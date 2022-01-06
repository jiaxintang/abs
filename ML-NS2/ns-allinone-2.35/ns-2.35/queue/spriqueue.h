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
 *
 * @(#) $Header: /cvsroot/nsnam/ns-2/queue/drop-tail.h,v 1.19 2004/10/28 23:35:37 haldar Exp $ (LBL)
 */

//#ifndef ns_drop_tail_h
//#define ns_drop_tail_h
#ifndef ns_priority_h
#define ns_priority_h

#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#include <string.h>
#include "queue.h"
#include "config.h"
#include <vector>

#define NUM_STAT_INTV 10
#define QUEUE_FEATURE 88
#define TCP_MTU 1500
extern int spri_queue_id_;
//extern double SINTV_;

struct rtt_info {

	int fid;
	double startTS;
	double rtt;
	double inst_rtt;
	int qid;
	bool isSet;
};

struct flow_thp_info {

	int fid;
	int qid;
	long pkt_sent;
};

/*
 * A bounded, drop-tail queue
 */
#include "timer-handler.h"

class SPriQueue;

class PrioSyncTimer : public TimerHandler { 
public:
//	PrioSyncTimer(SPriQueue *a, void (SPriQueue::*call_back)() ) 
//		: a_(a), call_back_(call_back) {};
	PrioSyncTimer(SPriQueue *q) : TimerHandler() {

		queue_ = q;
	}
	
	double interval_;

protected:
	virtual void expire (Event *e);
	SPriQueue *queue_; 
//	void (SPriQueue::*call_back_)();
}; 


class PrioStatTimer : public TimerHandler {

public:
	PrioStatTimer (SPriQueue *q) : TimerHandler() {

		queue_ = q;
	}

protected:
	virtual void expire(Event *e);
	SPriQueue* queue_;
};


class SPriQueue : public Queue {
  //friend class PrioSyncTimer;
  public:
	SPriQueue() : syncTimer_(this), statTimer_(this)
	{ 
/*		q_ = new PacketQueue; 
		q2_ = new PacketQueue;     //lwj
		q0_ = new PacketQueue;     //lwj
		pq_ = q_;
		bind_bool("drop_front_", &drop_front_);
		bind_bool("summarystats_", &summarystats);
		bind_bool("queue_in_bytes_", &qib_);  // boolean: q in bytes?
		bind("mean_pktsize_", &mean_pktsize_);
		//		_RENAMED("drop-front_", "drop_front_");
		cong_switch=0;
*/		
//		measure_timer_ = new PrioSyncTimer(this, &SPriQueue::everyRTT);
		thresh_=65;
		mean_pktsize_=1500;
		//marking_scheme_=PER_PORT_ECN;

		//Bind variables
		bind("thresh_",&thresh_);
		bind("mean_pktsize_", &mean_pktsize_);
		bind("num_queue_", &num_queue_);
		bind("init_interval_", &init_interval_);
		bind("default_weight_", &default_weight_);
		bind("rtt_lowerbound_", &rtt_lowerbound_);
		bind("rtt_upperbound_", &rtt_upperbound_);
		bind("weighted_rr_", &weighted_rr_);
		bind("delay_based_dropping_", &delay_based_dropping_);
		bind("avgbw_wrr_", &avgbw_wrr_);
		bind("omag_queuediff_", &omag_queuediff_);


    //	bind("marking_scheme_", &marking_scheme_);
  //	 bind_bool("drop_front_", &drop_front_);
			//Init queues
		q_=new PacketQueue*[num_queue_];
		synque_ = new PacketQueue;
		for(int i=0; i<num_queue_; i++)
		{
			q_[i]=new PacketQueue;
		}
		id_ = spri_queue_id_++;
		recv_num_ = 0;
		num_drops_ = 0;
		previous_qlen_ = 0;
		cur_queue_ = 0;
		rtts_ = new std::vector<struct rtt_info*>;
		thps_ = new std::vector<struct flow_thp_info*>;
		weights_ = new std::vector<int>;
		weight_counters_ = new std::vector<int>;
		flows_per_queue_ = new std::vector<int>;
	
//		syncTimer_ = new PrioSyncTimer(this);
		syncTimer_.interval_ = init_interval_;
		syncTimer_.resched(init_interval_);
		statTimer_.resched(SINTV_/NUM_STAT_INTV);

		for(int i=0; i<num_queue_; i++){

			cur_bytes_sent_.push_back(0);
			cur_bytes_recv_.push_back(0);
			
			que_buffer_.push_back(0);

			lowest_qlen_.push_back(9999);
			mavg_qlen_.push_back(0);
			highest_qlen_.push_back(0);
			avg_qlen_.push_back(0);

			lowest_qdelay_.push_back(9999);
			highest_qdelay_.push_back(0);
			mavg_qdelay_.push_back(0);
			avg_qdelay_.push_back(0);
			num_qdelay_.push_back(0);

			repqdelay_.push_back(rtt_lowerbound_*(pow(10, i)));

			num_stat_.push_back(0);
			
			prev_bytes_recv_.push_back(0);
			que_rtts_.push_back(0);
			rtt_raw_.push_back(0);
			
			weights_->push_back(1);
			weight_counters_->push_back(1);
			flows_per_queue_->push_back(0);

			previous_thp_.push_back(0);
		}
		num_drops_ = 0;
	}

	~SPriQueue() {
	/*delete q_;
	delete q2_;       //lwj
	delete q0_;       //lwj
	delete measure_timer_;     //lwj */
		for(int i=0;i<num_queue_;i++)
		{
			delete q_[i];
		}
		delete[] q_;
	}

	virtual int getQLength() {
		/*if(q_[0]->length() > 0){
			printf("dpri#%d@", id_);
		}*/
		return q_[0]->length();
	}
	void reportAndFetch();
	void updateFeatures();

  protected:
//	void reset();
//	int command(int argc, const char*const* argv); 
	int id_;
	int recv_num_;
	int num_drops_;
	int previous_qlen_;
	int num_queue_;
	std::vector<struct rtt_info*>* rtts_;
	std::vector<struct flow_thp_info*>* thps_;
	double init_interval_;
	PrioSyncTimer syncTimer_;
	PrioStatTimer statTimer_;

	vector<float> que_buffer_;

	vector<double> lowest_qdelay_;
	vector<double> highest_qdelay_;
	vector<double> avg_qdelay_;
	vector<double> mavg_qdelay_;
	vector<int> num_qdelay_;
	
	vector<double> lowest_qlen_;
	vector<double> avg_qlen_;
	vector<double> mavg_qlen_;
	vector<double> highest_qlen_;
	vector<int> cur_bytes_sent_;
	vector<int> cur_bytes_recv_;

	vector<long> num_stat_;	
	
	vector<double> repqdelay_;

	vector<int> prev_bytes_recv_;
	vector<double> que_rtts_;
	vector<int> rtt_raw_;

	vector<double> previous_thp_;

	vector<int> *weights_;
	vector<int> *weight_counters_;
	vector<int> *flows_per_queue_;

	int default_weight_;
	double rtt_lowerbound_;
	double rtt_upperbound_;
	int weighted_rr_;

	int delay_based_dropping_;
	int avgbw_wrr_;
	int omag_queuediff_;

	void enque(Packet*);
	Packet* deque();
//	void shrink_queue();	// To shrink queue and drop excessive packets.

	//PacketQueue *q_;	/* underlying FIFO queue */
	//PacketQueue *q2_;
	//PacketQueue *q0_;
//	int drop_front_;	/* drop-from-front (rather than from tail) */
//	int summarystats;
//	void print_summarystats();
//	int qib_;       	/* bool: queue measured in bytes? */
//	int mean_pktsize_;	/* configured mean packet size in bytes */
	
//	int cong_switch;
	 PacketQueue **q_;		// underlying multi-FIFO (CoS) queues
	PacketQueue *synque_;		// Jesson
		int mean_pktsize_;		// configured mean packet size in bytes
		int thresh_;			// single ECN marking threshold
		int cur_queue_;			// number of CoS queues. No more than MAX_QUEUE_NUM
//		int marking_scheme_;	// Disable ECN (0), Per-queue ECN (1) and Per-port ECN (2)
//	PrioSyncTimer*	measure_timer_;
//		void everyRTT();
	
	void cleanTHPs();
	
	void updateWeights();
	int isChecked(int);

	void updatePktSent(int fid, int qid, int len){

		bool found = false;
		for (int i=0; i<thps_->size(); i++){

			if((*thps_)[i]->fid==fid){

				(*thps_)[i]->pkt_sent += len;
				found = true;
			}
		}

		if(!found){

			struct flow_thp_info *new_thp = new struct flow_thp_info;
			new_thp->fid = fid;
			new_thp->qid = qid;
			new_thp->pkt_sent = 0;
			thps_->push_back(new_thp);
		}
	}

	double getRTTInfo(int fid, bool isInstant){

		double target_rtt = 0;
		for(int i=0; i<rtts_->size(); i++){
			
			struct rtt_info* cur_rttinfo = (*rtts_)[i];
			
			if(fid == cur_rttinfo->fid && cur_rttinfo->isSet){
				
				if(isInstant){

					target_rtt = cur_rttinfo->inst_rtt;
					break;
				}else{
					target_rtt = cur_rttinfo->rtt;
					break;
				}
//			}else if(!cur_rttinfo->isSet){
//				printf("Error in getting RTT for flow #%d, which is not set yet\n", fid);
			}
		}
		return target_rtt;
	}
	
	double getAvgThroughput(int queue_id);
	double getAvgQueueingDelay(int queue_id);
	void updateFlowsPerQueue();

	double getQueueRepresentativeRTT(int qid){

//		double rtt = -1;
		double rtt = 0;
		int order = 10;
		if(qid<0){
			
			printf("Error in getting queue %d's representative RTT\n", qid);
			return rtt;
		}	

/*		if(omag_queuediff_){
			if(qid < (num_queue_-1)){
				rtt = (repqdelay_[qid]+repqdelay_[qid+1])/2;
			
			} else {

				rtt = (repqdelay_[qid]+rtt_upperbound_>(repqdelay_[qid]*10) ?
						rtt_upperbound_ : (repqdelay_[qid]*10))/2;
			}
		}else{
			double baseline = (rtt_upperbound_-rtt_lowerbound_)/num_queue_;
			rtt = rtt_lowerbound_+qid*baseline;
		}
*/
		// average RTT in the queue;
		int num_flows_enqueued = 0;	
		for(int i=0; i<rtts_->size(); i++){

			struct rtt_info* cur_rttinfo = (*rtts_)[i];

			if(qid == cur_rttinfo->qid && cur_rttinfo->isSet){

//				printf("current rtt:%lf@%d\n", cur_rttinfo->rtt, qid);
				rtt += cur_rttinfo->rtt;
				num_flows_enqueued++;
			}
		}

		return num_flows_enqueued == 0 ? (repqdelay_[qid]+repqdelay_[qid+1])/2 : rtt/num_flows_enqueued;
	}

	int getPushedQueue(double rtt){

		int priority = num_queue_-1;

		if(rtt<rtt_lowerbound_){

			return 0;
		}


		if(omag_queuediff_){
			// order of magnitudes
			int order=10;
			for(int i=0; i<num_queue_; i++){

				if(rtt<rtt_lowerbound_){

					priority = 0;
					break;
				}else if(rtt>=repqdelay_[i] && rtt<repqdelay_[i+1]){

					priority = i;
					break;
				}
			}	
		}else{
			// linear
			double baseline = (rtt_upperbound_-rtt_lowerbound_)/num_queue_;
			for(int i=0; i<num_queue_; i++){
				
				if(rtt<rtt_lowerbound_){

					priority = 0;
					break;
				}else if(rtt>=(rtt_lowerbound_+baseline*i) && rtt<(rtt_lowerbound_+baseline*(i+1))){
//					printf("current rtt queue:%lf-#%d for #%d(%d)\n", rtt_lowerbound_+baseline*i, i, fid, seqno);
					priority = i;
					break;
				}
			}
		}

		return priority;
	}

	int getQueueInfo(int fid){

		int target_qid = 0;
		for(int i=0; i<rtts_->size(); i++){
			
			struct rtt_info* cur_rttinfo = (*rtts_)[i];
			
			if(fid == cur_rttinfo->fid && cur_rttinfo->isSet){
				

				target_qid = cur_rttinfo->qid;
				break;
			}
		}
		return target_qid;
	}

	void updateRTTInfo(int fid, double new_rtt){

		for(int i=0; i<rtts_->size(); i++){
			
			struct rtt_info* cur_rttinfo = (*rtts_)[i];
			
			if(fid == cur_rttinfo->fid && cur_rttinfo->isSet){
				
				cur_rttinfo->inst_rtt = new_rtt;
			}
		}
	}


	int TotalByteLength()
	{
		int bytelength=0;
		for(int i=0; i<num_queue_; i++)
			bytelength+=q_[i]->byteLength();
		return bytelength;
	}
	
	int getQueueByteLength(int index){
		
		if(index < num_queue_ && index >=0){

			return q_[index]->byteLength();
		} else {
			printf("Error index for get %d-th queue's byte length\n", index);
			return -1;
		}
	}
	
	int TotalLength(){

		int tl = 0;
		for(int i=0; i<num_queue_; i++){

			tl += q_[i]->length();
		}
		return tl;
	}
	
	int getQueueLength(int index){

		if(index < num_queue_ && index >=0) {

			return q_[index]->length();
		} else {
			
			printf("Error index for get %d-th queue's length in packets\n", index);
			return -1;
		}
	}
	
	int resetWeight(){

		for(int i=0; i<num_queue_; i++){

			(*weight_counters_)[i] = (*weights_)[i];
		}
	}

};

#endif
