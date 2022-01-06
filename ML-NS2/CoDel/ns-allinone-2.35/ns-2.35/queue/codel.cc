/*
 * Codel - The Controlled-Delay Active Queue Management algorithm
 * Copyright (C) 2011-2012 Kathleen Nichols <nichols@pollere.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions, and the following disclaimer,
 *	without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote products
 *	derived from this software without specific prior written permission.
 *
 * Alternatively, provided that this notice is retained in full, this
 * software may be distributed under the terms of the GNU General
 * Public License ("GPL") version 2, in which case the provisions of the
 * GPL apply INSTEAD OF those given above.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <math.h>
#include <sys/types.h>
#include "config.h"
#include "template.h"
#include "random.h"
#include "flags.h"
#include "delay.h"
#include "codel.h"

int codel_id_ = 0;

static class CoDelClass : public TclClass {
  public:
	CoDelClass() : TclClass("Queue/CoDel") {}
	TclObject* create(int, const char*const*) {
		return (new CoDelQueue);
	}
} class_codel;

CoDelQueue::CoDelQueue() : tchan_(0), syncTimer_(this), statTimer_(this)
{
	bind("interval_", &interval_);
	bind("target_", &target_);  // target min delay in clock ticks
	bind("curq_", &curq_);	  // current queue size in bytes
	bind("d_exp_", &d_exp_);	// current delay experienced in clock ticks
	bind("init_interval_", &init_interval_);
	bind("detect_interval_", &detect_interval_);
	q_ = new PacketQueue();	 // underlying queue
	udpq_ = new PacketQueue();
	pq_ = q_;
	reset();

	/* ML training parameters initialization */
	id_ = codel_id_++;
	syncTimer_.interval_ = init_interval_;
	syncTimer_.resched(init_interval_);
	statTimer_.resched(SINTV_/NUM_STAT_INTV);

	
	cur_bytes_sent_ = 0;
	cur_bytes_recv_ = 0;
	lowest_qlen_ = 9999;
	mavg_qlen_ = 0;
	highest_qlen_ = 0;
	avg_qlen_ = 0.0;
	num_qlen_sampled_ = 0;
	
	lowest_qdelay_ = 9999;
	highest_qdelay_ = 0;
	mavg_qdelay_ = 0;
	avg_qdelay_ = 0;
	num_qdelay_ = 0;
	
	num_drops_ = 0;
	prev_bytes_recv_ = 0;
	rtts_ = 0.0;
	rtt_raw_ = 0;
	timeouts_ = new vector<struct timeout_info*>;
	thps_ = new vector<struct flow_thp_info*>;
	spare_time_ = 0.0;
	num_spare_ = 0;
	spare_start_timestamp_ = 0.0;
	previous_spare_start_timestamp_ = 0.0;

}

void CoDelQueue::reset()
{
	curq_ = 0;
	d_exp_ = 0.;
	dropping_ = 0;
	first_above_time_ = -1;
	maxpacket_ = 256;
	count_ = 0;
	Queue::reset();
}

// Add a new packet to the queue.  The packet is dropped if the maximum queue
// size in pkts is exceeded. Otherwise just add a timestamp so dequeue can
// compute the sojourn time (all the work is done in the deque).

void CoDelQueue::enque(Packet* pkt)
{
	if((hdr_cmn::access(pkt)->ptype())==PT_UDP){
		if((udpq_->length()+1) <= qlim_){
		
			udpq_->enque(pkt);
		} else {

			drop(pkt);
		}
	}else{
		if(q_->length() >= qlim_) {
			// tail drop
			drop(pkt);
			num_drops_++;
		} else {
			HDR_CMN(pkt)->ts_ = Scheduler::instance().clock();
			q_->enque(pkt);
			
			if(q_->length()>5000){			
//				printf("cur queue length:%d, buffer size:%d, @%lf\n", q_->length(), qlim_, Scheduler::instance().clock());
			}
			/* @author jesson, for ML features */
			cur_bytes_recv_ += (hdr_cmn::access(pkt))->size();
			rtt_raw_ = (hdr_tcp::access(pkt))->rtt_ml();
			struct pkt_info *tp = new pkt_info;
			tp->pkt = (void*)pkt;
			tp->ingress_timestamp =  Scheduler::instance().clock();
			pkts_->push_back(tp);
			update_timeout(pkt);
		}
	}
}

// return the time of the next drop relative to 't'
double CoDelQueue::control_law(double t)
{
	return t + interval_ / sqrt(count_);
}

// Internal routine to dequeue a packet. All the delay and min tracking
// is done here to make sure it's done consistently on every dequeue.
dodequeResult CoDelQueue::dodeque()
{
	double now = Scheduler::instance().clock();
	dodequeResult r = { NULL, 0 };

	r.p = q_->deque();
	if (r.p == NULL) {
		curq_ = 0;
		first_above_time_ = 0;
	} else {
		// d_exp_ and curq_ are ns2 'traced variables' that allow the dynamic
		// queue behavior that drives CoDel to be captured in a trace file for
		// diagnostics and analysis.  d_exp_ is the sojourn time and curq_ is
		// the current q size in bytes.
		d_exp_ = now - HDR_CMN(r.p)->ts_;
		curq_ = q_->byteLength();

		if (maxpacket_ < HDR_CMN(r.p)->size_)
			// keep track of the max packet size.
			maxpacket_ = HDR_CMN(r.p)->size_;

		// To span a large range of bandwidths, CoDel essentially runs two
		// different AQMs in parallel. One is sojourn-time-based and takes
		// effect when target_ is larger than the time it takes to send a
		// TCP MSS packet. The 1st term of the "if" does this.
		// The other is backlog-based and takes effect when the time to send an
		// MSS packet is >= target_. The goal here is to keep the output link
		// utilization high by never allowing the queue to get smaller than
		// the amount that arrives in a typical interarrival time (one MSS-sized
		// packet arriving spaced by the amount of time it takes to send such
		// a packet on the bottleneck). The 2nd term of the "if" does this.
		if (d_exp_ < target_ || curq_ <= maxpacket_) {
			// went below - stay below for at least interval
			first_above_time_ = 0;
		} else {
			if (first_above_time_ == 0) {
				// just went above from below. if still above at first_above_time,
				// will say it’s ok to drop
				first_above_time_ = now + interval_;
			} else if (now >= first_above_time_) {
				r.ok_to_drop = 1;
			}
		}
	}
	return r;
}

// All of the work of CoDel is done here. There are two branches: In packet
// dropping state (meaning that the queue sojourn time has gone above target
// and hasn’t come down yet) check if it’s time to leave or if it’s time for
// the next drop(s). If not in dropping state, decide if it’s time to enter it
// and do the initial drop.

Packet* CoDelQueue::deque()
{
	Packet *p = NULL;
//	printf("beginning of CoDel's deque(), udpq_==NULL ? %d, queue length=%d\n", udpq_==NULL, q_->length());
	if(udpq_->length()>0){
//		printf("before udp deque\n");
		p=udpq_->deque();
//		printf("udp deque\n");
	}else{
//		printf("before any of else\n");
		double now = Scheduler::instance().clock();;
//		printf("before first dodeque()\n");
		dodequeResult r = dodeque();
//		printf("after first dodeque()\n");	
		if(r.p){
//			printf("in if\n");
			if (dropping_) {
				if (! r.ok_to_drop) {
					// sojourn time below target - leave dropping state
					dropping_ = 0;
				}
//				printf("in dropping_\n");
				// It’s time for the next drop. Drop the current packet and dequeue
				// the next.  If the dequeue doesn't take us out of dropping state,
				// schedule the next drop. A large backlog might result in drop
				// rates so high that the next drop should happen now, hence the
				// ‘while’ loop.
				while (now >= drop_next_ && dropping_) {
					findAndDelete(r.p);
					drop(r.p);
					num_drops_++;
					++count_;
					r = dodeque();
					if (! r.ok_to_drop) {
						// leave dropping state
						dropping_ = 0;
					} else {
						// schedule the next drop.
						drop_next_ = control_law(drop_next_);
					}
				}
		
			// If we get here we’re not in dropping state. 'ok_to_drop' means that the
			// sojourn time has been above target for interval so enter dropping state.
			} else if (r.ok_to_drop) {
//				printf("in else\n");
				findAndDelete(r.p);
				drop(r.p);
				num_drops_++;
				r = dodeque();
				dropping_ = 1;
		
				// If min went above target close to when it last went below,
				// assume that the drop rate that controlled the queue on the
				// last cycle is a good starting point to control it now.
				count_ = (count_ > 1 && now - drop_next_ < 16*interval_)? count_ - 1 : 1;
				drop_next_ = control_law(now);
			}
//			printf("before ML logic\n");	
			p = r.p;
			// ABS logic
			if(spare_start_timestamp_ > 0.0){
	
				double temp_interval = Scheduler::instance().clock()-spare_start_timestamp_;
				spare_time_ += temp_interval;
				previous_spare_start_timestamp_ = spare_start_timestamp_;
				spare_start_timestamp_ = 0.0;
	
				if(num_spare_==0){
	
					num_spare_++;
				}
			}
	
//			printf("before for\n");
			for(int i=0; i<pkts_->size(); i++) {
	
				struct pkt_info *tp = (*pkts_)[i];
				if(tp->pkt==p){
					// find and delete it
					double que_delay = Scheduler::instance().clock() - tp->ingress_timestamp;
					pkts_delay_->push_back(que_delay);
	
					if(que_delay < lowest_qdelay_){
	
						lowest_qdelay_ = que_delay;
					}
	
					if(que_delay > highest_qdelay_){
	
						highest_qdelay_ = que_delay;
					}
	
					avg_qdelay_ = (avg_qdelay_*num_qdelay_ + que_delay)/(num_qdelay_+1);
					++num_qdelay_;
	
					float weight = 0.5;
					mavg_qdelay_ = weight*que_delay + (1-weight)*mavg_qdelay_;
	
					pkts_->erase(pkts_->begin()+i, pkts_->begin()+i+1);
					delete tp;
					break;
				}
			}
//			printf("end for\n");
			cur_bytes_sent_ += (hdr_cmn::access(p))->size();		// for ML feature calculation
			int fid = (hdr_ip::access(p))->flowid();
			updatePktSent(fid, (hdr_cmn::access(p))->size());
		}else{
//			printf("in else\n"); 
			// ABS logic
			if(spare_start_timestamp_==0.0){
				spare_start_timestamp_ = Scheduler::instance().clock();
				if(spare_start_timestamp_-previous_spare_start_timestamp_>detect_interval_){
					num_spare_++;
				}
			}
		}

		return (r.p);
	}

	return (p);
}

int CoDelQueue::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();
	if (argc == 2) {
		if (strcmp(argv[1], "reset") == 0) {
			reset();
			return (TCL_OK);
		}
	} else if (argc == 3) {
		// attach a file for variable tracing
		if (strcmp(argv[1], "attach") == 0) {
			int mode;
			const char* id = argv[2];
			tchan_ = Tcl_GetChannel(tcl.interp(), (char*)id, &mode);
			if (tchan_ == 0) {
				tcl.resultf("CoDel trace: can't attach %s for writing", id);
				return (TCL_ERROR);
			}
			return (TCL_OK);
		}
		// connect CoDel to the underlying queue
		if (!strcmp(argv[1], "packetqueue-attach")) {
			delete q_;
			if (!(q_ = (PacketQueue*) TclObject::lookup(argv[2])))
				return (TCL_ERROR);
			else {
				pq_ = q_;
				return (TCL_OK);
			}
		}
	}
	return (Queue::command(argc, argv));
}

// Routine called by TracedVar facility when variables change values.
// Note that the tracing of each var must be enabled in tcl to work.
void
CoDelQueue::trace(TracedVar* v)
{
	const char *p;

	if (((p = strstr(v->name(), "curq")) == NULL) &&
		((p = strstr(v->name(), "d_exp")) == NULL) ) {
		fprintf(stderr, "CoDel: unknown trace var %s\n", v->name());
		return;
	}
	if (tchan_) {
		char wrk[500];
		double t = Scheduler::instance().clock();
		if(*p == 'c') {
			sprintf(wrk, "c %g %d", t, int(*((TracedInt*) v)));
		} else if(*p == 'd') {
			sprintf(wrk, "d %g %g %d %g", t, double(*((TracedDouble*) v)), count_,
					count_? control_law(0.)*1000.:0.);
		}
		int n = strlen(wrk);
		wrk[n] = '\n'; 
		wrk[n+1] = 0;
		(void)Tcl_Write(tchan_, wrk, n+1);
	}
}

void CoDelQueue::update_timeout(Packet* pkt){

			
	bool isfound = false;
	for(int i=0; i< timeouts_->size(); i++){
			
		if((*timeouts_)[i]->fid==(hdr_ip::access(pkt))->flowid()){
			
			(*timeouts_)[i]->num_timeout = (hdr_tcp::access(pkt))->num_timeout();
			isfound = true; 
		}
	}		       
				
	if(!isfound){   
			
		struct timeout_info* tempt = new timeout_info;
		tempt->fid = (hdr_ip::access(pkt))->flowid();
		tempt->num_timeout = (hdr_tcp::access(pkt))->num_timeout();
		timeouts_->push_back(tempt);
//	      printf("add new flow with id #%d\n", tempt->fid);
	}       
				
}  

void CoDelQueue::findAndDelete(Packet *p){

	for(int i=0; i<pkts_->size(); i++) {

		struct pkt_info *tp = (*pkts_)[i];
		if(tp->pkt==p){
			// find and delete it
			pkts_->erase(pkts_->begin()+i, pkts_->begin()+i+1);
			delete tp;
			break;
		}
	}
}

void CoDelQueue::reportAndFetch(){

	int num_active_flows = thps_->size();
	const int FEATURE_BUFFER_SIZE = CONSTANT_BUFFER_SIZE
					+ sizeof(int)	/* number of active flows */
					+ (sizeof(int)+sizeof(float))*num_active_flows;

	double cur_thr = cur_bytes_sent_*8.0/syncTimer_.interval_/1000000.0;	    //Mbps
	double cur_thr_in = cur_bytes_recv_*8.0/syncTimer_.interval_/1000000.0;
//      printf("@%lf: cur_thr:%d\n", Scheduler::instance().clock(), cur_pkt_sent_);

	//Socket
	int sock_cli = socket(AF_INET,SOCK_STREAM, 0);
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(serv_port_);
//      servaddr.sin_addr.s_addr = inet_addr("192.168.1.114");
	const char* sip = serv_ip_;
	servaddr.sin_addr.s_addr = inet_addr(sip);

//      printf("PORT:%u\n", serv_port_);
	printf("Connect: %s, %u\n", serv_ip_, serv_port_);

	if (::connect(sock_cli, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){

		printf("@%lf: connect fail, no features uploaded and no actions assigned\n"
			, Scheduler::instance().clock());
		return;
	}else{

//	      printf("before min(%lf) max(%lf)\n", edp_.th_min, edp_.th_max);

		char sendbuf[FEATURE_BUFFER_SIZE];
		char recvbuf[OPERATION_BUFFER_SIZE];

		int num_flows = flows_->size();
		int num_retrans = 0;
		for(int i=0; i<num_flows; i++){

			num_retrans +=(*flows_)[i]->num_retrans;
		}

		// parse features into strings;
		int offset = 0;
		memcpy(sendbuf+offset, &lowest_qdelay_, sizeof(lowest_qdelay_));
		offset += sizeof(lowest_qdelay_);
		memcpy(sendbuf+offset, &highest_qdelay_, sizeof(highest_qdelay_));
		offset += sizeof(highest_qdelay_);
		memcpy(sendbuf+offset, &avg_qdelay_, sizeof(avg_qdelay_));
		offset += sizeof(avg_qdelay_);
		memcpy(sendbuf+offset, &mavg_qdelay_, sizeof(mavg_qdelay_));
		offset += sizeof(mavg_qdelay_);

		memcpy(sendbuf+offset, &lowest_qlen_, sizeof(lowest_qlen_));
		offset += sizeof(lowest_qlen_);
		memcpy(sendbuf+offset, &highest_qlen_, sizeof(highest_qlen_));
		offset += sizeof(highest_qlen_);
		memcpy(sendbuf+offset, &avg_qlen_, sizeof(avg_qlen_));
		offset += sizeof(avg_qlen_);
		memcpy(sendbuf+offset, &mavg_qlen_, sizeof(mavg_qlen_));
		offset += sizeof(mavg_qlen_);
		memcpy(sendbuf+offset, &cur_thr, sizeof(cur_thr));
		offset += sizeof(cur_thr);

		//RTT
		memcpy(sendbuf+offset, &rtts_, sizeof(rtts_));
		offset += sizeof(rtts_);

		//Spare Time
		if(spare_start_timestamp_ > 0.0){

			spare_time_ += Scheduler::instance().clock() - spare_start_timestamp_;
			spare_start_timestamp_ = Scheduler::instance().clock();
		}
		double spare_percentage = spare_time_/syncTimer_.interval_*100;
		memcpy(sendbuf+offset, &spare_percentage, sizeof(spare_percentage));
		offset += sizeof(spare_percentage);

		memcpy(sendbuf+offset, &num_spare_, sizeof(num_spare_));
		offset += sizeof(num_spare_);

		memcpy(sendbuf+offset, &cur_bytes_sent_, sizeof(cur_bytes_sent_));
		offset += sizeof(cur_bytes_sent_);
		memcpy(sendbuf+offset, &cur_bytes_recv_, sizeof(cur_bytes_recv_));
		offset += sizeof(cur_bytes_recv_);


		memcpy(sendbuf+offset, &num_retrans, sizeof(num_retrans));
		offset += sizeof(num_retrans);
		memcpy(sendbuf+offset, &num_drops_, sizeof(num_drops_));
		offset += sizeof(num_drops_);
		memcpy(sendbuf+offset, &num_flows, sizeof(num_flows));
		offset += sizeof(num_flows);

		// for test
		printf("REPORT@%lf: lqd(%lf) hqd(%lf) aqd(%lf) maqd(%lf)\n"
			"\t\tlql(%lf) hql(%lf) aql(%lf) maql(%lf)\n"
			"\t\tthrp(%lf) rtts(%lf) spare_time(%lf\%) num_spare(%d) \n"
			"\t\tbytes_sent(%d) bytes_recv(%d) #retrans(%d) #drops(%d) #flows(%d)\n",
			Scheduler::instance().clock(),
			lowest_qdelay_, highest_qdelay_, avg_qdelay_, mavg_qdelay_,
			lowest_qlen_, highest_qlen_, avg_qlen_, mavg_qlen_,
			cur_thr, rtts_, spare_percentage, num_spare_, cur_bytes_sent_, cur_bytes_recv_,
			num_retrans, num_drops_, num_flows);

		memcpy(sendbuf+offset, &num_active_flows, sizeof(num_active_flows));
		offset += sizeof(num_active_flows);
		
		printf("\neach flow's throughput:\n");
		for(int i=0; i<num_active_flows; i++){

			float fthp = (*thps_)[i]->pkt_sent*8.0/syncTimer_.interval_/1000.0/1000.0;	//Mbps
			int fid = (*thps_)[i]->fid;
			
			memcpy(sendbuf+offset, &fid, sizeof(fid));
			offset += sizeof(fid);
			memcpy(sendbuf+offset, &fthp, sizeof(fthp));
			offset += sizeof(fthp);
	
			printf("%d-%f ", fid, fthp);
		}
		printf("\n");
		
		// Communication;
		::send(sock_cli, sendbuf, sizeof(sendbuf),0);
//		::recv(sock_cli, recvbuf, sizeof(recvbuf),0);
		close(sock_cli);
//
//		// parse operation into CoDel parameters;
//
//		float min_thr = 0.005;
//		float max_thr = 0.005;
//		float wq = 0.1;
//		float ml_interv = 0.1;
//		float maxp = 10.0;
//
//		offset = 0;
//		memcpy(&min_thr, recvbuf, sizeof(min_thr));
//		offset += sizeof(min_thr);
//		memcpy(&max_thr, recvbuf+offset, sizeof(max_thr));
//		offset += sizeof(max_thr);
//		memcpy(&wq, recvbuf+offset, sizeof(wq));
//		offset += sizeof(wq);
//		memcpy(&ml_interv, recvbuf+offset, sizeof(ml_interv));
//		offset += sizeof(ml_interv);
//		memcpy(&maxp, recvbuf+offset, sizeof(maxp));
//		offset += sizeof(maxp);
//
//
//		// initializing
//	//      memset(sendbuf, 0, sizeof(sendbuf));
//	//      memset(recvbuf, 0, sizeof(recvbuf));
//	//      delete &sendbuf;
//	//      delete &recvbuf;
//
//
		cur_bytes_sent_ = 0;
		cur_bytes_recv_ = 0;
		rtts_ = 0.0;
//
////	      setMaxThr(int(max_thr*qlim_));
////	      setMinThr(int(min_thr*qlim_));
//		setMaxThr(int(max_thr* (qlim_ * edp_.mean_pktsize) ));
//		setMinThr(int(min_thr* (qlim_ * edp_.mean_pktsize) ));
//		setWQ(wq);
//		setMaxp(maxp);
//		syncTimer_.interval_ = ml_interv;
//		SINTV_=ml_interv;
//
//		//for test
//		printf("RECV---: min(%f) max(%f) wq(%f) intv(%f) maxq(%f)\n", min_thr, max_thr, wq, ml_interv, edp_.max_p_inv);
////	      printf("set min(%lf) max(%lf)\n", edp_.th_min, edp_.th_max);
		//reset queueing delay
		lowest_qdelay_=9999;
		highest_qdelay_=0;
		avg_qdelay_=0;
		mavg_qdelay_=0;
		num_qdelay_=0;

		lowest_qlen_ = 9999;
		mavg_qlen_ = 0;
		avg_qlen_ = 0;
		num_qlen_sampled_ = 0;
		highest_qlen_ = 0;
		spare_time_ = 0.0;
		num_spare_ = 0;

		cleanTHPs();

		// print timeout
/*	      int num_timeouts = 0;
		for(int k=0; k<timeouts_->size(); k++){

			num_timeouts += (*timeouts_)[k]->num_timeout;
		}
		printf("Total Timeout Occured:%d\n", num_timeouts);
*/

/*	      std::sort(pkts_delay_->begin(), pkts_delay_->end(), sortFun);

		if(pkts_delay_->size()>0){
			int loc = (int)(0.95*pkts_delay_->size());
			printf("95th queue delay@%lf:\t%lf\n", Scheduler::instance().clock(), (*pkts_delay_)[loc]);
		}
*/
	}
}

void CoDelQueue::updateFeatures(){

	float weight = 0.5;

	mavg_qlen_ = weight*q_->length() + (1-weight)*mavg_qlen_;

	if(q_->length() > highest_qlen_){

		highest_qlen_ = q_->length();
	}
	if(q_->length() < lowest_qlen_){

		lowest_qlen_ = q_->length();
	}

	avg_qlen_ = (avg_qlen_*num_qlen_sampled_++ + q_->length())/num_qlen_sampled_;

	if( (prev_bytes_recv_!=cur_bytes_recv_) && (cur_bytes_recv_!=0) ){

		if(rtt_raw_>0 && rtts_>0.0 ){

			rtts_ = 0.5*rtt_raw_ + 0.5*rtts_;       // unit: us 
		}else if(rtt_raw_>0 && rtts_ == 0.0){

			rtts_ = rtt_raw_;
		}
		prev_bytes_recv_ = cur_bytes_recv_;
	}
}

void CoDelQueue::cleanTHPs(){

	thps_->clear();
}

void CoDelQueue::updatePktSent(int fid, int len){

	bool found = false;
	for(int i=0; i<thps_->size(); i++){

		if((*thps_)[i]->fid == fid){

			(*thps_)[i]->pkt_sent += len;
			found = true;
		}
	}

	if(!found){

		struct flow_thp_info *new_thp = new struct flow_thp_info;
		new_thp->fid = fid;
		new_thp->pkt_sent = 0;

		thps_->push_back(new_thp);
	}
}

void StatTimer::expire(Event*){

	queue_->updateFeatures();
	this->resched(SINTV_/NUM_STAT_INTV);
}


void SyncTimer::expire(Event*){

	queue_->reportAndFetch();
	this->resched(interval_);
}
