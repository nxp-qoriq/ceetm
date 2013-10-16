/**************************************************************************
 * Copyright 2013, Freescale Semiconductor, Inc. All rights reserved.
 ***************************************************************************/
/*
 * File:	pkt_sched.h
 *
 * Description: Common header file for CEETM QDisc & User space TC application
 *
 * Authors:	Sachin Saxena <sachin.saxena@freescale.com>
 *
 * History
 *  Version     Date		Author			Change Description *
 *  1.0		15-10-2013	Sachin Saxena		Initial Version
 */
 /****************************************************************************/

#include <linux/types.h>

#ifndef FALSE
#define		FALSE	0	 /*!<  Boolean false, non-true, zero */
#define		TRUE	(!FALSE) /*!<  Boolean true, non-false, non-zero */
#endif

/* CEETM Qdsic Types*/
#define TC_CEETM_NUM_MAX_Q	8
enum {
	CEETM_Q_ROOT = 1,/* Root Qdisc which has Priority Scheduler
			   and Shaper at Logical Network Interface */
	CEETM_Q_PRIO,	/* Inner Qdisc which has Priority Scheduler
			   functionality */
	CEETM_Q_WBFS	/* Inner Qdisc which has Weighted Fair
			   Queueing functionality */
};
/* CEETM Qdisc configuration parameters */
struct tc_ceetm_qopt {
	__u32	type;
	__u32	rate;
	__u32	ceil;
	__u16	mpu;
	__u16	overhead;
	__u16	queues;
	__u8	weight[TC_CEETM_NUM_MAX_Q];
	/* Each byte 0-7 indicates the Comitted Rate
	   / Excess Rate Eligibilty of respective queue */
	__u8	cr_map[TC_CEETM_NUM_MAX_Q];
	__u8	er_map[TC_CEETM_NUM_MAX_Q];
};
/* CEETM Class configuration parameters */
struct tc_ceetm_copt {
	__u32	rate;
	__u32	ceil;
	__u32	weight;
};
enum {
	TCA_CEETM_UNSPEC,
	TCA_CEETM_COPT,
	TCA_CEETM_INIT,
	__TCA_CEETM_MAX,
};
/* CEETM stats */
struct tc_ceetm_xstats {
	__u64		enqueue;
	__u64		drop;
	__u64		dequeue;
	__u64		deq_bytes;
};

#define TCA_CEETM_MAX (__TCA_CEETM_MAX - 1)
