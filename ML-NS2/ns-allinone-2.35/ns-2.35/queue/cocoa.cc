/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1994 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/queue/cocoa.cc,v 1.17 2004/10/28 23:35:37 haldar Exp $ (LBL)";
#endif

#include "cocoa.h"


int cocoa_id_ = 0;

static class CoCoAClass : public TclClass {
 public:
	CoCoAClass() : TclClass("Queue/CoCoA") {}
	TclObject* create(int, const char*const*) {
		return (new CoCoA);
	}
} class_drop_tail;

void CoCoA::reset()
{
	Queue::reset();
}

int 
CoCoA::command(int argc, const char*const* argv) 
{
	if (argc==2) {
		if (strcmp(argv[1], "printstats") == 0) {
			print_summarystats();
			return (TCL_OK);
		}
 		if (strcmp(argv[1], "shrink-queue") == 0) {
 			shrink_queue();
 			return (TCL_OK);
 		}
	}
	if (argc == 3) {
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
	return Queue::command(argc, argv);
}

/*
 * cocoa
 */
void CoCoA::enque(Packet* p)
{
	if((hdr_cmn::access(p)->ptype())==PT_UDP){

		if((udpq_->length()+1)<=qlim_){

			udpq_->enque(p);
		}else{

			drop(p);
		}

	}else{
		if (summarystats) {
			Queue::updateStats(qib_?q_->byteLength():q_->length());
		}
	
		int qlimBytes = qlim_ * mean_pktsize_;
		double now = Scheduler::instance().clock();
		if ((!qib_ && (q_->length() + 1) >= qlim_) ||
	  	(qib_ && (q_->byteLength() + hdr_cmn::access(p)->size()) >= qlimBytes)){
	

//			if(li_spare_time_>0.0 && !was_buffersize_enlarged_){
			if(spare_time_>0.0 && !was_buffersize_enlarged_){
//				double spare_percentage = li_spare_time_/(now - previous_pkt_drop_ts_)*100;
				//Spare Time
				if(spare_start_timestamp_ > 0.0){
			
					spare_time_ += Scheduler::instance().clock() - spare_start_timestamp_;
					spare_start_timestamp_ = Scheduler::instance().clock();
				}
				double spare_percentage = spare_time_/syncTimer_.interval_;
//				printf("spare_percentage:%lf, at %lf\n", spare_percentage, now);
				qlim_ += li_pkts_sent_*(spare_percentage/(1-spare_percentage));
				enquePkt(p);
				was_buffersize_enlarged_ = true;
			}else if(was_buffersize_enlarged_){

				dropPkt(p);
				
//			}else if((now - previous_pkt_drop_ts_)>min_li_thresh_){
			}else if((now - previous_li_stt_)>min_li_thresh_){

				adjustInterval();
//				printf("# of standing_queue_/buffer_size@%lf: %d/%d\n", Scheduler::instance().clock(), standing_queue_, qlim_);
				if(qlim_-standing_queue_>10){
				
					qlim_ -= standing_queue_;
				}else{
					qlim_ = 10;
				}
				drop(p);
				num_drops_++;
				for(int i=0; i< standing_queue_; i++){

					Packet *tp = q_->deque_tail();
//					printf("deque_from_tail@%lf\n", now);
					if(tp){
					
						dropEnquedPkt(tp);
						num_drops_++;
					}else{

						break;
					}
				}
			}else{
				// start a new interval
				dropPkt(p);
			}
	
		} else {
			enquePkt(p);
		}
	}
}

void CoCoA::dropEnquedPkt(Packet *p){
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
	drop(p);
}

void CoCoA::enquePkt(Packet* p){

	q_->enque(p);
	cur_bytes_recv_ += (hdr_cmn::access(p))->size();
	rtt_raw_ = (hdr_tcp::access(p))->rtt_ml();
	struct pkt_info *tp = new pkt_info;
	tp->pkt = (void*) p;
	tp->ingress_timestamp = Scheduler::instance().clock();
	pkts_->push_back(tp);
	update_timeout(p);
}

void CoCoA::update_timeout(Packet* pkt){


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
//		printf("add new flow with id #%d\n", tempt->fid);
	}
				
}

void CoCoA::dropPkt(Packet* p){

	adjustInterval();
	drop(p);
	previous_pkt_drop_ts_ = Scheduler::instance().clock();
//	printf("drop a packet at %lf\n", previous_pkt_drop_ts_);
	num_drops_++;
}

void CoCoA::adjustInterval(){

	double now = Scheduler::instance().clock();
//	if((now - previous_pkt_drop_ts_)>min_li_thresh_){
	if((now - previous_li_stt_)>min_li_thresh_){
		
		if(min_queue_<9999){
			standing_queue_ = min_queue_;
		}else{
			standing_queue_ = q_->length();
		}
		previous_pkt_drop_ts_ = now;
		//TODO reset
		was_buffersize_enlarged_ = false;
		li_pkts_sent_ = 0;
		li_spare_time_ = 0.0;
		li_spare_start_timestamp_ = 0;
		min_queue_ = 9999;
		previous_li_stt_ = now;
	}
}

//AG if queue size changes, we drop excessive packets...
void CoCoA::shrink_queue() 
{
	int qlimBytes = qlim_ * mean_pktsize_;
	if (debug_)
		printf("shrink-queue: time %5.2f qlen %d, qlim %d\n",
 			Scheduler::instance().clock(),
			q_->length(), qlim_);
	while ((!qib_ && q_->length() > qlim_) || 
	    (qib_ && q_->byteLength() > qlimBytes)) {
		if (drop_front_) { /* remove from head of queue */
			Packet *pp = q_->deque();
			drop(pp);
		} else {
			Packet *pp = q_->tail();
			q_->remove(pp);
			drop(pp);
		}
	}
}

Packet* CoCoA::deque()
{
	Packet *p;
	if(udpq_->length()>0){

		p=udpq_->deque();
	}else{
		if (summarystats && &Scheduler::instance() != NULL) {
			Queue::updateStats(qib_?q_->byteLength():q_->length());
		}
		p=q_->deque();
		if(p){
			li_pkts_sent_++;
			min_queue_ = min_queue_ < q_->length() ? min_queue_ : q_->length();
			if(spare_start_timestamp_ > 0.0){
				
				double temp_interval = Scheduler::instance().clock()-spare_start_timestamp_;
				spare_time_ += temp_interval;
				previous_spare_start_timestamp_ = spare_start_timestamp_;
				spare_start_timestamp_ = 0.0;
				
				if(num_spare_==0){
					
					num_spare_++;
				}
			}

			if(li_spare_start_timestamp_ > 0.0){
				
				double temp_interval = Scheduler::instance().clock()-li_spare_start_timestamp_;
				li_spare_time_ += temp_interval;
				li_spare_start_timestamp_ = 0.0;
			}

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


			cur_bytes_sent_ +=  (hdr_cmn::access(p))->size();
			int fid = (hdr_ip::access(p))->flowid();
			updatePktSent(fid, (hdr_cmn::access(p))->size());
		}else{
			if(spare_start_timestamp_==0.0){
				spare_start_timestamp_ = Scheduler::instance().clock();
				if(spare_start_timestamp_-previous_spare_start_timestamp_>detect_interval_){
					num_spare_++;
				}
			}

			if(li_spare_start_timestamp_==0.0){
				li_spare_start_timestamp_ = Scheduler::instance().clock();
			}
		}
	}

	return p;
}

void CoCoA::print_summarystats()
{
	//double now = Scheduler::instance().clock();
	printf("True average queue: %5.3f", true_ave_);
	if (qib_)
		printf(" (in bytes)");
	printf(" time: %5.3f\n", total_time_);
}


void CoCoA::findAndDelete(Packet *p){

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

void CoCoA::cleanTHPs(){

	thps_->clear();
}

void CoCoA::updatePktSent(int fid, int len){

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

void CoCoAStatTimer::expire(Event*){

	queue_->updateFeatures();
	this->resched(SINTV_/NUM_STAT_INTV);
}


void CoCoASyncTimer::expire(Event*){

	queue_->reportAndFetch();
	this->resched(interval_);
}

void CoCoA::updateFeatures(){

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


void CoCoA::reportAndFetch(){

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
	}
}
