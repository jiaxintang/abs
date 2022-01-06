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
 * @(#) $Header: /cvsroot/nsnam/ns-2/queue/cocoa.h,v 1.19 2004/10/28 23:35:37 haldar Exp $ (LBL)
 */

#ifndef ns_cocoa_h
#define ns_cocoa_h

#include <string.h>
#include "queue.h"
#include "red.h"
#include "config.h"

extern int cocoa_id_;
class CoCoA;

class CoCoASyncTimer : public TimerHandler {

public:
	CoCoASyncTimer (CoCoA* q) : TimerHandler() {

		queue_ = q;
	}

	double interval_;

protected:
	virtual void expire(Event *e);
	CoCoA* queue_;
};

class CoCoAStatTimer : public TimerHandler {

public:
	CoCoAStatTimer (CoCoA* q) : TimerHandler() {

		queue_ = q;
	}

protected:
	virtual void expire(Event *e);
	CoCoA* queue_;
};

/*
 * A bounded, cocoa queue
 */
class CoCoA : public Queue {
  public:
	CoCoA() :  syncTimer_(this), statTimer_(this) { 
		q_ = new PacketQueue; 
		pq_ = q_;
		bind_bool("drop_front_", &drop_front_);
		bind_bool("summarystats_", &summarystats);
		bind_bool("queue_in_bytes_", &qib_);  // boolean: q in bytes?
		bind("mean_pktsize_", &mean_pktsize_);
		//		_RENAMED("drop-front_", "drop_front_");
		
		bind("init_interval_", &init_interval_);
		bind("min_li_thresh_", &min_li_thresh_);

		id_ = cocoa_id_++;
		previous_pkt_drop_ts_ = 0;

		syncTimer_.interval_ = init_interval_;
		syncTimer_.resched(init_interval_);
		statTimer_.resched(SINTV_/NUM_STAT_INTV);
	
		udpq_ = new PacketQueue();	      // for udp who consumes the bandwidth
	
		cur_bytes_sent_ = 0;
		cur_bytes_recv_ = 0;
		lowest_qlen_ = 9999;
		mavg_qlen_ = 0;
		highest_qlen_ = 0;
		avg_qlen_ = 0;
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
		previous_li_stt_ = 0.0;

		was_buffersize_enlarged_ = false;
		li_pkts_sent_ = 0;
		standing_queue_ = 0;
		li_spare_time_ = 0.0;
		li_spare_start_timestamp_ = 0.0;
		min_queue_ = 9999;
	}
	~CoCoA() {
		delete q_;
		delete udpq_;
	}

	void reportAndFetch();
	void updateFeatures();
	void update_timeout(Packet*);
	void cleanTHPs();
	void updatePktSent(int fid, int len);
	void findAndDelete(Packet*);
	void dropPkt(Packet*);
	void dropEnquedPkt(Packet*);
	void enquePkt(Packet*);
	void adjustInterval();


  protected:
	void reset();
	int command(int argc, const char*const* argv); 
	void enque(Packet*);
	Packet* deque();
	void shrink_queue();	// To shrink queue and drop excessive packets.

	PacketQueue *q_;	/* underlying FIFO queue */
	int drop_front_;	/* drop-from-front (rather than from tail) */
	int summarystats;
	void print_summarystats();
	int qib_;       	/* bool: queue measured in bytes? */
	int mean_pktsize_;	/* configured mean packet size in bytes */

	PacketQueue *udpq_;	/* for udp who consumes the bandwidth */

	double previous_pkt_drop_ts_;	/* the timestamp of previous packet drop */
	double previous_li_stt_;	/* the start time of previous li */
	int min_queue_;			/* minimal queue length in LI */
	double min_li_thresh_;		/* the threshold for computing LI */

	int id_;		// @author Jesson Liu
	CoCoASyncTimer syncTimer_;
	CoCoAStatTimer statTimer_;
	double init_interval_;
	/* for ML features*/
	double lowest_qlen_;
	double mavg_qlen_;
	double avg_qlen_;
	double highest_qlen_;
	int cur_bytes_sent_;
	int cur_bytes_recv_;
	int num_drops_;

	int num_qlen_sampled_;

	double lowest_qdelay_;
	double highest_qdelay_;
	double avg_qdelay_;
	double mavg_qdelay_;
	long num_qdelay_;

	double rtts_;
	int rtt_raw_;
	int prev_bytes_recv_;
	double spare_time_;
	double spare_start_timestamp_;
	double previous_spare_start_timestamp_;
	int standing_queue_;
	double detect_interval_;
	int num_spare_;


	bool was_buffersize_enlarged_;
	double li_spare_time_;
	double li_spare_start_timestamp_;
	int li_pkts_sent_;

	vector<struct timeout_info*>* timeouts_;

	vector<struct flow_thp_info*>* thps_;
};

#endif
