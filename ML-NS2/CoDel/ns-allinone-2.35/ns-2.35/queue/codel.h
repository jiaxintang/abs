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

#ifndef ns_codel_h
#define ns_codel_h

// For socket
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>

#include "timer-handler.h"
#include "queue.h"
#include <stdlib.h>
#include "agent.h"
#include "template.h"
#include "trace.h"

#define NUM_STAT_INTV 10
#define CONSTANT_BUFFER_SIZE 112
#define OPERATION_BUFFER_SIZE 20

struct flow_thp_info{

	int fid;
	long pkt_sent;
};

// we need a multi-valued return and C doesn't help
struct dodequeResult { Packet* p; int ok_to_drop; };

class CoDelQueue;

struct timeout_info{

	int fid;
	int num_timeout;
};

class SyncTimer : public TimerHandler {

public:
	SyncTimer (CoDelQueue *q) : TimerHandler() {

		queue_ = q;
	}

	double interval_;
protected:
	virtual void expire(Event *e);
	CoDelQueue* queue_;
};

class StatTimer : public TimerHandler {

public:
	StatTimer (CoDelQueue *q) : TimerHandler() {

		queue_ = q;
	}

protected:
	virtual void expire(Event *e);
	CoDelQueue* queue_;
};

extern int codel_id_;

class CoDelQueue : public Queue {
public: 
	CoDelQueue();

	int getID() { return id_; }
	void reportAndFetch();
	void updateFeatures();
	void update_timeout(Packet*);
	void cleanTHPs();
	void updatePktSent(int fid, int len);

protected:
	// Stuff specific to the CoDel algorithm
	void enque(Packet* pkt);
	Packet* deque();

	// Static state (user supplied parameters)
	double target_;		 // target queue size (in time, same units as clock)
	double interval_;	 // width of moving time window over which to compute min

	// Dynamic state used by algorithm
	double first_above_time_; // when we went (or will go) continuously above
							// target for interval
	double drop_next_;	// time to drop next packet (or when we dropped last)
	int count_;			 // how many drops we've done since the last time
							// we entered dropping state.
	int dropping_;		// = 1 if in dropping state.
	int maxpacket_;		 // largest packet we've seen so far (this should be
							// the link's MTU but that's not available in NS)

	// NS-specific junk
	int command(int argc, const char*const* argv);
	void reset();
	void trace(TracedVar*); // routine to write trace records

	PacketQueue *q_;		// underlying FIFO queue
	Tcl_Channel tchan_;	 // place to write trace records
	TracedInt curq_;		// current qlen seen by arrivals
	TracedDouble d_exp_;	// delay seen by most recently dequeued packet

	/* for ML features */
	int id_;
	SyncTimer syncTimer_;
	StatTimer statTimer_;
	double init_interval_;

	PacketQueue *udpq_;		/* for udp who consumes the bandwidth */
	
	/* features */
	double lowest_qlen_;
	double mavg_qlen_;
	double highest_qlen_;
	int cur_bytes_sent_;
	int cur_bytes_recv_;
	int num_drops_;
	double avg_qlen_;
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
	double detect_interval_;
	int num_spare_;
	
	vector<struct timeout_info*>* timeouts_;	
	vector<struct flow_thp_info*>* thps_;

private:
	double control_law(double);
	dodequeResult dodeque();
	void findAndDelete(Packet *p);
};

#endif
