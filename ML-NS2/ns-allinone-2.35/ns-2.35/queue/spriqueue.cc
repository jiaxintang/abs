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

/*#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/common/priqueue.cc,v 1.17 2004/10/28 23:35:37 haldar Exp $ (LBL)";
#endif*/

#include "spriqueue.h"
#include "ip.h"
#include "tcp.h"
#include "flags.h"
#include "math.h"
#include "random.h"

#define max(arg1,arg2) (arg1>arg2 ? arg1 : arg2)
#define min(arg1,arg2) (arg1<arg2 ? arg1 : arg2)

int spri_queue_id_ = 0;
//double SINTV_ = 0.1;

static class SPriQueueClass : public TclClass {
 public:
	SPriQueueClass() : TclClass("Queue/SPriQueue") {}
	TclObject* create(int, const char*const*) {
		return (new SPriQueue);
	}
} class_spriqueue;

//void SPriQueue::reset()
//{
//	Queue::reset();
//}
/*
int 
SPriQueue::command(int argc, const char*const* argv) 
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
 		if (strcmp(argv[1], "bottleneck") == 0) {
 			cong_switch=1;
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
*/
/*
 * drop-tail
 */
void SPriQueue::enque(Packet* p)
{
	int initial_cwnd = 2;
	int priority = num_queue_-1;
	int pktsize = (hdr_cmn::access(p))->size();

	int rtt = (hdr_tcp::access(p))->rtt_ml();
	int seqno = (hdr_tcp::access(p))->seqno();
	short is_linux_tcp = (hdr_tcp::access(p))->is_linux_tcp();
	int fid = (hdr_ip::access(p))->flowid();
	short flag = 0;
	double baseline = (rtt_upperbound_-rtt_lowerbound_)/num_queue_;
	struct rtt_info* syn_tobeenque = NULL;

	
//	printf("%lf: fid(%d)'s #%d pkt\n", Scheduler::instance().clock(), fid, seqno);
	if(seqno==0){
		// record the timestamp of syn packets
		for(int i=0; i<rtts_->size(); i++){

			struct rtt_info* cur_rttinfo = (*rtts_)[i];
			if(fid==cur_rttinfo->fid){

				printf("Error in rtt info lookup, flow #.%d existed rtt table\n", fid);
				flag = -1;
				syn_tobeenque = cur_rttinfo;
				break;
			}
		}
		
		if(flag != -1){
			struct rtt_info* new_rtt = new struct rtt_info;
			new_rtt->fid = fid;
			new_rtt->startTS = Scheduler::instance().clock();
			new_rtt->rtt = 0;
			new_rtt->inst_rtt = 0;
			new_rtt->qid = -1;
			new_rtt->isSet = false;
			syn_tobeenque = new_rtt;
			
			flag = 1;
//			printf("add new flow rtt info: %d\n", new_rtt->fid);
		}else{

			// In the case of syn packets dropped, update to the latest timestamp
			syn_tobeenque->startTS = Scheduler::instance().clock();
			syn_tobeenque->rtt = 0;
			syn_tobeenque->inst_rtt = 0;
			syn_tobeenque->qid = -1;
			syn_tobeenque->isSet = false;
		}

		priority = -1;
	
//	} else if(seqno==1) {		/* used only in TCP reno*/
	} else if((seqno==2 && is_linux_tcp==1) || (seqno==1 && is_linux_tcp==0) && isChecked(fid)==0){	/* compatible with multiple TCP variants*/
		// record RTT
//		printf("in seqno\n");
		bool found = false;
		for(int i=0; i<rtts_->size(); i++){

			struct rtt_info* cur_rttinfo = (*rtts_)[i];
			if(fid == cur_rttinfo->fid){

				if(!cur_rttinfo->isSet){
					cur_rttinfo->rtt = Scheduler::instance().clock()-cur_rttinfo->startTS;
//					printf("flow(%d)'s rtt is %lf\n", fid, cur_rttinfo->rtt);
					cur_rttinfo->isSet = true;
				}

				// Set priority
				priority = getPushedQueue(cur_rttinfo->rtt);
				cur_rttinfo->qid = priority;
				found = true;
				break;
			}
		}

		if(!found){

			printf("Error in rtt info lookup, syn rtt not found\n");
			flag = -2; 
//		}else{
//
//			(*flows_per_queue_)[priority]++;
		}
	}else{

		bool found = false;
		for(int i=0; i<rtts_->size(); i++){

			struct rtt_info* cur_rttinfo = (*rtts_)[i];
			if(fid == cur_rttinfo->fid){

//				printf("flow(%d)'s rtt is %lf\n", fid, cur_rttinfo->rtt);
				if(cur_rttinfo->qid>=0){

					priority = cur_rttinfo->qid;
//					printf("flow #%d's pkt to queue #%d\n", cur_rttinfo->fid, priority);
				}else{
					// Set priority
					priority = getPushedQueue(cur_rttinfo->rtt);
					cur_rttinfo->qid = priority;
				}
				found = true;
				break;
			}
		}

		if(!found){
	
			printf("Error in rtt info lookup, packet rtt not found, is tcp pkt(%d)?\tflow #%d's %d-th pkt\n", (hdr_cmn::access(p))->ptype()==PT_TCP, fid, seqno);
			flag = -3;
		}else{

//			printf("enque flow #%d's packet #%d to queue #%d\n", fid, seqno, priority);
		}
	}
	//queue length exceeds the queue limit
	
	if(priority==-1){

		synque_->enque(p);
	
		if(seqno == 0 && syn_tobeenque != NULL){
	
			rtts_->push_back(syn_tobeenque);
		}

	}else{

//-----unused	double bdp = (rtt_lowerbound_+baseline*(priority+1))*1000*1000*1000/TCP_MTU/8;
//		double bdp = getQueueRepresentativeRTT(priority)*1000*1000*1000/TCP_MTU/8;
		double bdp = getQueueRepresentativeRTT(priority)*1000*1000*1000/TCP_MTU/8/num_queue_;
			
		double total_qlen = TotalByteLength();
	
		if(que_buffer_[priority] > 0.0){
	
			bdp = que_buffer_[priority]*qlim_;
	//		printf("BUFFSIZE@%lf:\t%lf\n", Scheduler::instance().clock(), bdp);
		}
//		printf("R_RTT:%lf\n", getQueueRepresentativeRTT(priority));	
//		printf("%lf: current buffer size for queue #%d is %lf, while total length is %lf\n", Scheduler::instance().clock(), priority, bdp*TCP_MTU, total_qlen);
	
		if(!delay_based_dropping_){
			if((getQueueByteLength(priority)+pktsize)>(bdp*TCP_MTU) || total_qlen >= (qlim_*TCP_MTU)){
				
				drop(p);
				num_drops_++;
	//			if(syn_tobeenque){
	//	
	//				delete syn_tobeenque;
	//			}
				return;
			}
		}else{
			// drop packet based on average queueing delay
			double td = getAvgQueueingDelay(priority); 
			if(td > 0 && td > 2.0*getQueueRepresentativeRTT(priority)){
				drop(p);
				/*if(priority==3)
					printf("queue No.3 drop one packet, cur_len:%d\n", getQueueLength(priority));
				*/
				num_drops_++;
				return;
			}
		}
	
//		if(seqno == 0 && syn_tobeenque != NULL){
//	
//			rtts_->push_back(syn_tobeenque);
//		}
		//Enqueue packet
		q_[priority]->enque(p);
		
		cur_bytes_recv_[priority] += pktsize;
		struct pkt_info *tp = new pkt_info;
		tp->pkt = (void*)p;
		tp->ingress_timestamp = Scheduler::instance().clock();
		pkts_->push_back(tp);
	
		/* update RTT */
		rtt_raw_[priority] = rtt;
		updateRTTInfo(fid, rtt*1.0/1000/1000);
	
		recv_num_++;
	}
}

//AG if queue size changes, we drop excessive packets...
/*void SPriQueue::shrink_queue() 
{
         int qlimBytes = qlim_ * mean_pktsize_;
	if (debug_)
 		printf("shrink-queue: time %5.2f qlen %d, qlim %d\n",
  			Scheduler::instance().clock(),
 			q_->length(), qlim_);
         while ((!qib_ && q_->length() > qlim_) || 
             (qib_ && q_->byteLength() > qlimBytes)) {
                 if (drop_front_) { // remove from head of queue 
                         Packet *pp = q_->deque();
                         drop(pp);
                 } else {
                         Packet *pp = q_->tail();
                         q_->remove(pp);
                         drop(pp);
                 }
         }
}*/

Packet* SPriQueue::deque()
{
	Packet* p = NULL;
	if(synque_->length()>0){

		p=synque_->deque();
	}else{
		int queue_id = 0;
	        if(TotalLength()>0)
		{
			if(weighted_rr_ || avgbw_wrr_){
				/*-- weighted round-robin --*/
	//			if(q_[cur_queue_]->length()>0){
	//
	//				if((*weight_counters_)[cur_queue_]<(*weights_)[cur_queue_]){
	//	
	//					p=q_[cur_queue_]->deque();
	//					queue_id = cur_queue_;
	//					(*weight_counters_)[cur_queue_]++;
	//				}else{
	//
	//					// reach threshold, go to the next queue
	//					int temp_cur = (cur_queue_+1) % num_queue_;
	//					for(int c=2; c<=num_queue_; c++){
	//	
	//						if(q_[temp_cur]->length()>0){
	//		
	//							p=q_[temp_cur]->deque();
	//							queue_id = temp_cur;
	//							//return (p);
	//							break;
	//						}
	//						temp_cur = (cur_queue_+c) % num_queue_;
	//	
	//					}
	//					cur_queue_ = temp_cur;
	//				}
	//				
	//			}
				bool nosend = true;
				int last_queue_id = 0;
				for(int i=0; i<num_queue_; i++){
	
					if(q_[i]->length()>0 && (*weight_counters_)[i]>0){
						
						p=q_[i]->deque();
						queue_id = i;
						(*weight_counters_)[i] -= 1;
						nosend = false;
						break;
					}else if(q_[i]->length()>0 && (*weight_counters_)[i] == 0){
						//TODO	
						//last_queue_id = (i+1)%num_queue_;
						last_queue_id = i;
					}else if(q_[i]->length()==0){
						continue;
					}
				}
	
				if(nosend){
					// if all the weights become 0, reset them
					resetWeight();
					if(last_queue_id >=0){
	
						p = q_[last_queue_id]->deque();
						queue_id = last_queue_id;
						(*weight_counters_)[last_queue_id] -= 1;
					}
				}
	
	
			}else{
				/* --pure round-robin-- */		
				if(q_[cur_queue_]->length()>0)
				{
					p=q_[cur_queue_]->deque();
		//			printf("@spri-%d-%lf\t%d\t%d\t%d\n", id_, Scheduler::instance().clock(), q_[0]->length(), recv_num_, drop_num_);
		
		//				printf("qlen@spri-%d-%lf\t%d\n", id_, Scheduler::instance().clock(), q_[0]->length());
		
		//			printf("%d@thp:\t%d\n", id_, recv_num_);
		
					queue_id = cur_queue_;
					cur_queue_ = (cur_queue_+1) % num_queue_;
					//return (p);
				}else{
		
					int temp_cur = (cur_queue_+1) % num_queue_;
					for(int c=2; c<=num_queue_; c++){
		
						if(q_[temp_cur]->length()>0){
		
							p=q_[temp_cur]->deque();
							queue_id = temp_cur;
							//return (p);
							break;
						}
						temp_cur = (cur_queue_+c) % num_queue_;
					}
					cur_queue_ = temp_cur;
				}
			}
			
			/* --strict priority-- */
	/*		if(q_[cur_queue_]->length()>0){
				
				p=q_[cur_queue_]->deque();
				queue_id = cur_queue_;
			//	cur_queue_ = (cur_queue_+1) % num_queue_;
			}else{
	
				cur_queue_ = (cur_queue_+1) % num_queue_;
			}
	*/
		}
		
		if(p){
	
			for(int i=0; i<pkts_->size(); i++){
	
	
				struct pkt_info *tp = (*pkts_)[i];
				if(tp->pkt == p){
	
					double que_delay = Scheduler::instance().clock() - tp->ingress_timestamp;
					pkts_delay_->push_back(que_delay);
					if(que_delay < lowest_qdelay_[queue_id]){
	
						lowest_qdelay_[queue_id] = que_delay;
					}
	
					if(que_delay > highest_qdelay_[queue_id]){
	
						highest_qdelay_[queue_id] = que_delay;
					}
	
					avg_qdelay_[queue_id] = (avg_qdelay_[queue_id]*num_qdelay_[queue_id]+que_delay)/(num_qdelay_[queue_id]+1);
					num_qdelay_[queue_id] = num_qdelay_[queue_id] + 1;
	
					float weight = 0.5;
					mavg_qdelay_[queue_id] = weight*que_delay + (1-weight)*mavg_qdelay_[queue_id];
					pkts_->erase(pkts_->begin()+i, pkts_->begin()+i+1);
					delete tp;
					break;
				}
			}
			int pkt_size = (hdr_cmn::access(p))->size();
			int fid = (hdr_ip::access(p))->flowid();
			updatePktSent(fid, queue_id, pkt_size);
			cur_bytes_sent_[queue_id] += pkt_size;
		}
	}
	return p;
}

void SPriQueue::reportAndFetch(){

	int num_active_flows = thps_->size();

	const int SEND_BUFFER_SIZE = sizeof(long)+QUEUE_FEATURE*num_queue_	/*queue info*/
				+sizeof(int)*4 /*tail info*/
				+(sizeof(int)+sizeof(float)+sizeof(double)*2)*num_active_flows;	/*flow info*/
	const int RECV_BUFFER_SIZE = 4*(num_queue_+1);
//	const int RECV_BUFFER_SIZE = sizeof(que_buffer_[0])*num_queue_+sizeof(float);

	//Socket
	int sock_cli = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(serv_port_);
	
	const char* sip = serv_ip_;
	servaddr.sin_addr.s_addr = inet_addr(sip);
	printf("Connect: %s: %u\n", serv_ip_, serv_port_);

	if(::connect(sock_cli, (struct sockaddr *)&servaddr, sizeof(servaddr))) {

		printf("@%lf: connection fails, no features uploaded and no actions assigned\n", Scheduler::instance().clock());
		return;
	}else{

		char sendbuf[SEND_BUFFER_SIZE];
		char recvbuf[RECV_BUFFER_SIZE];

		memset(&sendbuf, 0 , sizeof(sendbuf));
		memset(&recvbuf, 0 , sizeof(recvbuf));

		printf("REPORT@%lf:\n", Scheduler::instance().clock());
		long num_que = num_queue_;
		int offset = 0;
		memcpy(sendbuf+offset, &num_que, sizeof(num_que));
		offset += sizeof(num_que);
		// parse features into strings;
		for(int i=0; i<num_queue_; i++){
			
			double cur_thr = cur_bytes_sent_[i]*8.0/syncTimer_.interval_/1000.0/1000.0;	//Mbps
//			double cur_thr_in = cur_bytes_recv_[i]*8.0/syncTimer_.interval_/1000.0/1000.0;
			previous_thp_[i] = cur_thr;

			memcpy(sendbuf+offset, &lowest_qdelay_[i], sizeof(lowest_qdelay_[i]));
			offset += sizeof(lowest_qdelay_[i]);
			memcpy(sendbuf+offset, &highest_qdelay_[i], sizeof(highest_qdelay_[i]));
			offset += sizeof(highest_qdelay_[i]);
			memcpy(sendbuf+offset, &avg_qdelay_[i], sizeof(avg_qdelay_[i]));
			offset += sizeof(avg_qdelay_[i]);
			memcpy(sendbuf+offset, &mavg_qdelay_[i], sizeof(mavg_qdelay_[i]));
			offset += sizeof(mavg_qdelay_[i]);
			
			memcpy(sendbuf+offset, &lowest_qlen_[i], sizeof(lowest_qlen_[i]));
			offset += sizeof(lowest_qlen_[i]);
			memcpy(sendbuf+offset, &highest_qlen_[i], sizeof(highest_qlen_[i]));
			offset += sizeof(highest_qlen_[i]);
			memcpy(sendbuf+offset, &avg_qlen_[i], sizeof(avg_qlen_[i]));
			offset += sizeof(avg_qlen_[i]);
			memcpy(sendbuf+offset, &mavg_qlen_[i], sizeof(mavg_qlen_[i]));
			offset += sizeof(mavg_qlen_[i]);
			memcpy(sendbuf+offset, &cur_thr, sizeof(cur_thr));
			offset += sizeof(cur_thr);
			
			//RTT
			memcpy(sendbuf+offset, &que_rtts_[i], sizeof(que_rtts_[i]));
			offset += sizeof(que_rtts_[i]);
			
			
			memcpy(sendbuf+offset, &cur_bytes_sent_[i], sizeof(cur_bytes_sent_[i]));
			offset += sizeof(cur_bytes_sent_[i]);
			memcpy(sendbuf+offset, &cur_bytes_recv_[i], sizeof(cur_bytes_recv_[i]));
			offset += sizeof(cur_bytes_recv_[i]);

//			printf("\tqueue#%d: lqd(%lf) hqd(%lf) aqd(%lf) maqd(%lf)\n"
//				"\t\tlql(%lf) hql(%lf) aql(%lf) maql(%lf)\n"
//				"\t\tthrp(%lf) rtts(%lf) bytes_sent(%d) bytes_recv(%d)\n",
//				i, lowest_qdelay_[i], highest_qdelay_[i], avg_qdelay_[i], mavg_qdelay_[i],
//				lowest_qlen_[i], highest_qlen_[i], avg_qlen_[i], mavg_qlen_[i],
//				cur_thr, que_rtts_[i], cur_bytes_sent_[i], cur_bytes_recv_[i]);
			printf("\tqueue#%d: lqd(%lf) hqd(%lf) aqd(%lf) maqd(%lf)"
				" lql(%lf) hql(%lf) aql(%lf) maql(%lf)"
				" thrp(%lf) rtts(%lf) bytes_sent(%d) bytes_recv(%d)\n",
				i, lowest_qdelay_[i], highest_qdelay_[i], avg_qdelay_[i], mavg_qdelay_[i],
				lowest_qlen_[i], highest_qlen_[i], avg_qlen_[i], mavg_qlen_[i],
				cur_thr, que_rtts_[i], cur_bytes_sent_[i], cur_bytes_recv_[i]);
		}

		int num_flows = flows_->size();	
		int num_retrans = 0;

		for(int i=0; i<num_flows; i++){

			num_retrans +=(*flows_)[i]->num_retrans;
		}

                memcpy(sendbuf+offset, &num_retrans, sizeof(num_retrans));
                offset += sizeof(num_retrans);
                memcpy(sendbuf+offset, &num_drops_, sizeof(num_drops_));
                offset += sizeof(num_drops_);
                memcpy(sendbuf+offset, &num_flows, sizeof(num_flows));
                offset += sizeof(num_flows);
		printf("\t#retrans(%d) #drops(%d) #flows(%d)\n", num_retrans, num_drops_, num_flows);		

		
		memcpy(sendbuf+offset, &num_active_flows, sizeof(num_active_flows));
		offset += sizeof(num_active_flows);

		printf("\neach flow's throughput:\n");
//		printf("\n\n");					//-- for debug --
		for(int i=0; i<thps_->size(); i++){
			float cur_thp = (*thps_)[i]->pkt_sent*8.0/syncTimer_.interval_/1000.0/1000.0;	//Mbps
			int fid = (*thps_)[i]->fid;
			double synrtt = getRTTInfo(fid, false);
			double inst_rtt = getRTTInfo(fid, true);

			memcpy(sendbuf+offset, &fid, sizeof(fid));
			offset += sizeof(fid);
			memcpy(sendbuf+offset, &cur_thp, sizeof(cur_thp));
			offset += sizeof(cur_thp);
			memcpy(sendbuf+offset, &synrtt, sizeof(synrtt));
			offset += sizeof(synrtt);
			memcpy(sendbuf+offset, &inst_rtt, sizeof(inst_rtt));
			offset += sizeof(inst_rtt);

			//for debug
			int qid = getQueueInfo(fid);
			printf("%d-%f-%lf-%lf@%d ", fid, cur_thp, synrtt, inst_rtt, qid);
//--useless		printf("%d-%f-%lf-%lf ", fid, cur_thp, synrtt, inst_rtt);
//			printf("thp@%lf: %d %f %d\n", Scheduler::instance().clock(), fid, cur_thp, qid);	//-- for debug --
		}
		printf("\n\n");
		// Communication;
		::send(sock_cli, sendbuf, sizeof(sendbuf), 0);
		::recv(sock_cli, recvbuf, sizeof(recvbuf), 0);
		close(sock_cli);

		/* update infos */
		updateWeights();

		printf("RECV---: ");
		offset = 0;
		float ml_interv = 0.1;
		for(int i=0; i<num_queue_; i++) {
			memcpy(&que_buffer_[i], recvbuf+offset, sizeof(que_buffer_[i]));
			offset += sizeof(que_buffer_[i]);
			printf("que#%d's buffersize(%f) ", i, que_buffer_[i]);
		}
		memcpy(&ml_interv, recvbuf+offset, sizeof(ml_interv));
		offset += sizeof(ml_interv);
		printf("intv(%f)\n\n", ml_interv);

		//initialization
		for(int i=0; i<num_queue_; i++){
			cur_bytes_sent_[i] = 0;
			cur_bytes_recv_[i] = 0;
			
			lowest_qdelay_[i] = 9999;
			highest_qdelay_[i] = 0;
			avg_qdelay_[i] = 0;
			mavg_qdelay_[i] = 0;
			num_qdelay_[i] = 0;

			que_rtts_[i] = 0.0;
			rtt_raw_[i] = 0;

			lowest_qlen_[i] = 9999;
			highest_qlen_[i] = 0;
			mavg_qlen_[i] = 0;
			avg_qlen_[i] = 0;
			num_stat_[i] = 0;
		}

		cleanTHPs();
		syncTimer_.interval_ = ml_interv;
		SINTV_=ml_interv;
	}	
}

void SPriQueue::updateFeatures(){
//	double cur_thr_in = cur_bytes_recv_*8.0/syncTimer_.interval_/1000.0/1000.0;

	float weight = 0.5;

	for(int i=0; i<num_queue_; i++){

		mavg_qlen_[i] = weight*q_[i]->length() + (1-weight)*mavg_qlen_[i];
		
		if(q_[i]->length() > highest_qlen_[i]){
		
			highest_qlen_[i] = q_[i]->length();
		}
		if(q_[i]->length() < lowest_qlen_[i]){
		
			lowest_qlen_[i] = q_[i]->length();
		}
		
		avg_qlen_[i] = (num_stat_[i]*avg_qlen_[i]+q_[i]->length())/(num_stat_[i]+1);
		num_stat_[i] = num_stat_[i]+1;

		if( (prev_bytes_recv_[i]!=cur_bytes_recv_[i]) && (cur_bytes_recv_[i]!=0) ){
		
			if(rtt_raw_[i]>0 && que_rtts_[i]>0.0 ){
			
				que_rtts_[i] = 0.5*rtt_raw_[i] + 0.5*que_rtts_[i];       // unit: us
			}else if(rtt_raw_[i]>0 && que_rtts_[i] == 0.0){
			
				que_rtts_[i] = rtt_raw_[i];
			}
			
			prev_bytes_recv_[i] = cur_bytes_recv_[i];
		}

	}

	/* update infos */
//	updateWeights();


}
void SPriQueue::updateWeights(){

	// update number of active flows
	updateFlowsPerQueue();
	//update weights;
	double values[num_queue_];	
	for(int i=0; i<num_queue_; i++){

		values[i] = -1;
	}

	double basic_value = -1;
	double smallest = 9999999;
	double greatest = 0;
	printf("current factor @%lf is: \t", Scheduler::instance().clock());
	for(int i=0; i<num_queue_; i++){
		
		if(weighted_rr_ && ~avgbw_wrr_){	

//--need modify----	values[i] = (i+1)*1.0*(*flows_per_queue_)[i];
			values[i] = 1.0*(*flows_per_queue_)[i];
			if(values[i] !=0 && values[i]<smallest){
			
				//update the smallest value
				smallest = values[i];
				basic_value = smallest;
			}
		}else if(~weighted_rr_ && avgbw_wrr_){

			values[i] = getAvgThroughput(i);
			if(values[i] !=0 && values[i]>greatest){
			
				//update the greatest value
				greatest = values[i];
				basic_value = greatest;
			}
		}	
		printf("%lf ", values[i]);
	}
	printf("base:%lf\t", basic_value);

//		printf("current smallest non-zero queue is %d\n", smallest_nonzero_queue_id);
	if(basic_value > 0){

		printf("current weight @%lf is:\t", Scheduler::instance().clock());
/*		for(int i=0; i<smallest_nonzero_queue_id; i++){
			
			(*weights_)[i] = 0;
			(*weight_counters_)[i] = (*weights_)[i];
			printf("%d ", (*weights_)[i]);
		}
*/
		for(int i=0; i<num_queue_; i++){

////			(*weights_)[i] = (num_queue_*1.0-i)*(*flows_per_queue_)[i]/((num_queue_*1.0-largest_nonzero_queue_id)*(*flows_per_queue_)[largest_nonzero_queue_id]);
////			(*weights_)[i] = (i+1)*1.0*(*flows_per_queue_)[i]/((smallest_nonzero_queue_id+1)*1.0*(*flows_per_queue_)[smallest_nonzero_queue_id]);
			if(weighted_rr_ && ~avgbw_wrr_){
				//only consider the number of active flows and RTT
//--need modify---		(*weights_)[i] = (i+1)*1.0*(*flows_per_queue_)[i]/basic_value;
				(*weights_)[i] = 1.0*(*flows_per_queue_)[i]/basic_value;
			}else if(~weighted_rr_ && avgbw_wrr_){
				//only consider average bandwidth
				if(values[i]<0){
					(*weights_)[i] = 1;
				}else if(values[i]==0){
					(*weights_)[i] = default_weight_;
				}else{
					(*weights_)[i] = (int)(basic_value/values[i]);
				}
			}
			(*weight_counters_)[i] = (*weights_)[i];
			printf("%d ", (*weights_)[i]);

		}
	}else{

//		printf("current weight (fall back to round-robin):");
		for(int i=0; i<num_queue_; i++){

			(*weights_)[i] = 1;
			(*weight_counters_)[i] = (*weights_)[i];
//			printf("%d ", (*weights_)[i]);
		}
	}
	printf("\n");
	
//		// for next cycle
	for(int i=0; i<num_queue_; i++){

		(*flows_per_queue_)[i] = 0;	
	}
}

void SPriQueue::updateFlowsPerQueue(){

	// getting the number of active flows in each queue;
	flows_per_queue_->clear();
	for(int i=0; i<num_queue_; i++){
		
		flows_per_queue_->push_back(0);
	}

	for(int j=0; j<thps_->size(); j++){

		(*flows_per_queue_)[(*thps_)[j]->qid]++;
	}
	
}

double SPriQueue::getAvgThroughput(int qid){

	if(qid<0){

		printf("Error in getting average throughput\n");
		return -1;
	}

	if(qid>=flows_per_queue_->size()){

		printf("queue id is greater than current flows_per_queue_ vector\n");
		return -1;	
	}

	double cur_thr = cur_bytes_sent_[qid]*8.0/syncTimer_.interval_/1000.0/1000.0;	//Mbps
//	double cur_thr = previous_thp_[qid];
	if((*flows_per_queue_)[qid]==0){

		return 0;
	}else{

		double avg_thp = cur_thr/(*flows_per_queue_)[qid];	
		printf("(%lf,%d) ", cur_thr, (*flows_per_queue_)[qid]);
		return avg_thp;
	}
	
}

double SPriQueue::getAvgQueueingDelay(int qid){


	if(qid < num_queue_ && qid >=0) {
	
	//	double avgthp = getAvgThroughput(qid);
		if (avg_qdelay_[qid] <= 0){

		//	printf("thrp<=0\n");
			return -1;
		}
	//	return  getQueueByteLength(qid)*8.0/getAvgThroughput(qid);
		return avg_qdelay_[qid];
	} else {

		printf("Error index for get %d-th queue's average queueing delay\n", qid);
		return -1;
	}

}

void SPriQueue::cleanTHPs(){

/*	for (int i=0; i<thps_->size(); i++){

		(*thps_)[i]->pkt_sent = 0;
	}
*/
	thps_->clear();
}


void PrioSyncTimer::expire(Event*) {


	queue_->reportAndFetch();
	this->resched(interval_);
}

void PrioStatTimer::expire(Event*) {

	queue_->updateFeatures();
	this->resched(SINTV_/NUM_STAT_INTV);
}

int SPriQueue::isChecked(int fid){

	for(int i=0; i<rtts_->size(); i++){
		struct rtt_info* cur_rttinfo = (*rtts_)[i];
		if(fid == cur_rttinfo->fid){
			return cur_rttinfo->isSet;	
		}
	}
	return 0;
}


