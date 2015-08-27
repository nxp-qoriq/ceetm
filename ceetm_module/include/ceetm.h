/**************************************************************************
 * Copyright 2013, Freescale Semiconductor, Inc. All rights reserved.
 ***************************************************************************/
/*
 * File:	ceetm.h
 *
 * Description: Header file for CEETM Drievr Definations.
 *
 * Authors:	Sachin Saxena <sachin.saxena@freescale.com>
 *
 * History
 *  Version     Date		Author			Change Description *
 *  1.0		15-10-2013	Sachin Saxena		Initial Version
 */
 /****************************************************************************/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <net/pkt_sched.h>

#include <linux/skbuff.h>
#include <linux/fsl_qman.h>

#define CEETM_SUCCESS	0

/*sub portal index of DPAA 1G port = cell index + 2*/
#define CEETM_OFFSET_1G	2
/*sub portal index of DPAA OH port = cell index + 9*/
#define CEETM_OFFSET_OH	9

#define CEETM_CONTEXT_A	0x1a00000080000000

#define ceetm_err(fmt, arg...)  \
	printk(KERN_ERR"[CPU %d ln %d fn %s] - " fmt, smp_processor_id(), \
	__LINE__, __func__, ##arg)

#ifdef CEETM_DEBUG
#define ceetm_dbg(fmt, arg...)  \
	printk(KERN_INFO"[CPU %d ln %d fn %s] - " fmt, smp_processor_id(), \
	__LINE__, __func__, ##arg)
#else
#define ceetm_dbg(fmt, arg...)
#endif
#ifdef CEETM_SCH_DEBUG
#define ceetm_sch_dbg(fmt, arg...)  \
	printk(KERN_INFO"[CPU %d ln %d fn %s] - " fmt, smp_processor_id(), \
	__LINE__, __func__, ##arg)
#else
#define ceetm_sch_dbg(fmt, arg...)
#endif

/* Class level	: For Classes attached to root Qdisc Level = 0 */
/*		: For Inner Classes attached to root Classe Level = 1 */
/*		: For Leaf Classes attached to PRIO QDISC Level = 2 */
/*		: For Leaf Classes attached to WBFS QDISC Level = 3 */
enum ceetm_class_level {
	CEETM_ROOT_CLASS,
	CEETM_INNER_CLASS,
	CEETM_PRIO_LEAF_CLASS,
	CEETM_WBFS_LEAF_CLASS
};
#define IS_LEAF(x) ((x == CEETM_PRIO_LEAF_CLASS) || (x == CEETM_WBFS_LEAF_CLASS))

/* Interior & leaf nodes */
struct ceetm_class {
	struct Qdisc_class_common common;
	/* usage count of this class */
	int refcnt;

	/* topology */
	int level;
	void		*parent;	/* parent class/qdisc pointer */
	struct Qdisc	*child_qdisc;	/* Ptr to child Qdisc */
	/* Configtion parameters, specific to class types */
	union {
		struct ceetm_root_node {
			unsigned char shaping_en;
			/* Following paramerters are required,
			   if Shaping_en == TRUE, i.e Shaping Enabled */
			unsigned int rate; /* Committed Rate  */
			unsigned int ceil; /* Peak Rate */
			unsigned int mpu; /* Minimum Packet Size */
			unsigned int overhead; /* Required for Shaping
						  overhead caluclation */
		} root;
		struct ceetm_inner_node {
			/* For Shaped Classes */
			unsigned int rate; /* Committed Rate  */
			unsigned int ceil; /* Peak Rate */
			/* Weight is required for unshaped  classes */
			unsigned int weight;
		} inner;
		struct ceetm_prio_leaf {
			unsigned int  priority;
			unsigned char cr_eligible;
			unsigned char er_eligible;
		} prio;
		struct ceetm_wbfs_leaf {
			unsigned int  weight;
		} wbfs;
	} cfg;
	void	*hw_handle; /* Hardware FQ instance Handle */
	void	*cq; /* Hardware Class Queue instance Handle */
	struct tcf_proto *filter_list; /* class attached filters */
	int filter_cnt;
};
#define CEETM_PRIO_NUM		8

#define CEETM_WBFS_GRP_A		1
#define CEETM_WBFS_GRP_B		2
#define CEETM_WBFS_GRP_both	3

#define CEETM_WBFS_MIN_Q		4
#define CEETM_WBFS_MAX_Q		8

enum ceetm_wbfs_instance {
	NO_WBFS_EXISTS = 0,
	ONE_WBFS_EXISTS,
	MAX_WBFS_EXISTS
};

struct ceetm_sched {
	struct Qdisc_class_hash clhash;
	int		type;	/* Root, PRIO or WBFS  */
	union {
		struct root_links {
			struct ceetm_class un_shaped;
			struct ceetm_class shaped;
		} root;
		struct prio_links {
			/* We may attach WBFS to a PRIO scheduler class.
			   Maximum value can be 2. i.e, two
			   instances of WBFS scheduler Qdisc with 4 leaf
			   nodes in each instance. */
			int wbfs_grp;
			struct ceetm_class queues[CEETM_PRIO_NUM];
		} prio;
		struct wbfs_links {
			int		num_valid_q;
			unsigned char	cr_eligible;
			unsigned char	er_eligible;
			struct ceetm_class grp_q[CEETM_WBFS_MAX_Q];
		} wbfs;
	} un;
	void	*hw_handle; /* Hardware instance Handle */
	struct ceetm_class *parent; /* For inner Qdiscs */
	struct tcf_proto *filter_list; /* filters for qdisc itself */
	int filter_cnt;
};

struct ceetm_fq {
	struct qman_fq		egress_fq;
	int			congested;
	/* Related Device structure */
	struct net_device	*net_dev;
	/* Queue Statistics */
	uint64_t    ulEnqueuePkts;	/* Total number of packets received */
	uint64_t    ulDroppedPkts;	/* Total number of packets dropped
						due to Buffer overflow */
	/** FQ lock **/
	spinlock_t	lock;
};

/****************** Function Declarations ******************/

extern void ceetm_inc_drop_cnt(void *handle);
extern void ceetm_inc_enqueue_cnt(void *handle);
extern void ceetm_get_fq_stats(void *handle, uint64_t *enq, uint64_t *drp);
extern int ceetm_enqueue_pkt(void *handle, struct sk_buff *skb);
extern void ceetm_cfg_lni(struct net_device *dev, struct ceetm_sched *q);
extern void ceetm_cfg_channel(void *handle,
				uint32_t mtu,
				struct ceetm_sched *q);
extern void ceetm_cfg_prio_leaf_q(void *handle,
			uint32_t idx,
			struct ceetm_class *cl,
			struct net_device *dev);
extern void ceetm_cfg_wbfs_leaf_q(void *handle,
			int	grp,
			uint32_t idx,
			struct ceetm_class *cl,
			struct net_device *dev);
extern int qman_ceetm_channel_set_group_cr_er_eligibility(
		struct ceetm_sched *p_q,
		int grp,
		u16 cr_eligibility,
		u16 er_eligibility);
extern int ceetm_cfg_wbfs_grp(void *handle, int	grp, uint32_t pri);
extern int ceetm_release_lni(void *handle);
extern int ceetm_release_channel(void *handle);
extern int ceetm_release_wbfs_cq(void *handle);
extern void ceetm_release_cq(void *handle);
