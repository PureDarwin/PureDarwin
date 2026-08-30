/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

/* TCP-pacing implementation and helper functions */

#include "tcp_includes.h"

static uint64_t
microuptime_ns(void)
{
	uint64_t abstime = mach_absolute_time();
	uint64_t ns = 0;

	absolutetime_to_nanoseconds(abstime, &ns);

	return ns;
}

/* Compute interval to use for specified (size) amount of data */
static uint32_t
tcp_pacer_get_packet_interval(struct tcpcb *tp, uint64_t size)
{
	uint64_t rate = tp->t_pacer.rate;
	uint64_t interval;

	if (rate == 0) {
		os_log_error(OS_LOG_DEFAULT,
		    "%s: pacer rate shouldn't be 0, CCA is %s (cwnd=%u, smoothed rtt=%u ms)",
		    __func__, CC_ALGO(tp)->name, tp->snd_cwnd, tp->t_srtt >> TCP_RTT_SHIFT);

		return 0;
	}

	interval = (size * NSEC_PER_SEC) / rate;

	if (interval > UINT32_MAX) {
		interval = UINT32_MAX;
	}

	return (uint32_t)interval;
}

/*
 * Computes packet's (of length pkt_len) tx_time according to the TCP-connection
 * state. Also, returns the delay between now and the tx_time in milli-seconds.
 * All values are in nano-seconds.
 */
uint32_t
tcp_pacer_get_packet_tx_time(struct tcpcb *tp, int pkt_len, uint64_t *tx_time)
{
	uint64_t now = microuptime_ns();

	if (pkt_len < 0) {
		pkt_len = 0;
	}

	if (tp->t_pacer.packet_tx_time == 0) {
		tp->t_pacer.packet_tx_time = now;
		tp->t_pacer.current_size = pkt_len;
	} else {
		if (tp->t_pacer.current_size >= tp->t_pacer.tso_burst_size) {
			/*
			 * Increment tx_time by packet_interval and
			 * reset current_size to this packet's len
			 */
			tp->t_pacer.packet_tx_time +=
			    tcp_pacer_get_packet_interval(tp, tp->t_pacer.current_size);
			tp->t_pacer.current_size = pkt_len;
			if (now > tp->t_pacer.packet_tx_time) {
				/*
				 * If current time is bigger, then application
				 * has already paced the packet. Also, we can't
				 * set tx_time in the past.
				 */
				tp->t_pacer.packet_tx_time = now;
			}
		} else {
			tp->t_pacer.current_size += pkt_len;
		}
	}

	if (now < tp->t_pacer.packet_tx_time) {
		*tx_time = tp->t_pacer.packet_tx_time;
	} else {
		*tx_time = now;
	}

	/*
	 * tcp_pacer_get_packet_interval() guarantees that the below substraction
	 * is less than UINT32_MAX.
	 */
	return (uint32_t)(*tx_time - now) / NSEC_PER_MSEC;
}

#define MSEC_PER_SEC       (1000)  /* milliseconds per second */
uint64_t
tcp_compute_measured_rate(const struct tcpcb *tp)
{
	uint32_t srtt = tp->t_srtt;
	uint64_t rate;

	if (srtt == 0) {
		/* Can't pace when it's at 0 */
		return 0;
	}

	rate = tp->snd_cwnd;

	/* Multiply by MSEC_PER_SEC as srtt is in milliseconds */
	rate *= MSEC_PER_SEC;
	rate = (rate << TCP_RTT_SHIFT) / srtt;

	return rate;
}

#define BURST_SHIFT (12)        /* 1/(2^12) = 0.000244s, we allow a burst queue of at least 250us */
void
tcp_update_pacer_state(struct tcpcb *tp)
{
	struct inpcb *inp = tp->t_inpcb;
	uint32_t burst;
	uint64_t rate;

	rate = tcp_compute_measured_rate(tp);
	/* Use 200% rate when in slow start */
	if (tp->snd_cwnd < tp->snd_ssthresh) {
		rate *= 2;
	}

	if (inp->inp_max_pacing_rate != UINT64_MAX) {
		if (inp->inp_max_pacing_rate < rate) {
			rate = inp->inp_max_pacing_rate;
		}
	}
	burst = (uint32_t)(rate >> BURST_SHIFT);

	tp->t_pacer.rate = rate;
	tp->t_pacer.tso_burst_size = max(tp->t_maxseg, burst);
}
