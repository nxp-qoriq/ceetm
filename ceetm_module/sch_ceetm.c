/**************************************************************************
 * Copyright 2013, Freescale Semiconductor, Inc. All rights reserved.
 ***************************************************************************/
/*
 * File:	sch_ceetm.c
 *
 * Description: CEETM (Customer Edge Egress Traffic Management) Qdisc.
 *		============================================================
 *		CEETM is Hardware based Classfull Qdisc with multilevel Shaper &
 *		Scheduling support.
 *		It supports Aggregate Traffic Scheduling from multiple Virtual
 *		Links & Shape this Aggregate.
 *		This Qdisc plugins itself to existing Linux TC framework,
 *		when loaded.
 *
 * Authors:	Sachin Saxena <sachin.saxena@freescale.com>
 *
 * History
 *  Version     Date		Author			Change Description *
 *  1.0		15-10-2013	Sachin Saxena		Initial Changes
 */
 /****************************************************************************/

#include "include/ceetm.h"
#include "include/pkt_sched.h"
#include <linux/version.h>

MODULE_AUTHOR("Freescale Semiconductor, Inc");
MODULE_DESCRIPTION("CEETM QDISC");
MODULE_LICENSE("GPL");

/* find class in Root hash table using given handle */
static inline struct ceetm_class *ceetm_find(u32 handle, struct Qdisc *sch)
{
	struct net_device *dev = qdisc_dev(sch);
	struct Qdisc *root = dev->qdisc;
	struct ceetm_sched *q = qdisc_priv(root);
	struct Qdisc_class_common *clc;

	ceetm_sch_dbg("classid 0x%X\n", handle);
	clc = qdisc_class_find(&q->clhash, handle);
	if (clc == NULL)
		return NULL;
	return container_of(clc, struct ceetm_class, common);
}

/**
 * ceetm_classify - classify a packet into class
 *
 * It returns NULL if the packet should be dropped.
 * In all other cases leaf class is returned.
**/

static struct ceetm_class *ceetm_classify(struct sk_buff *skb,
					struct Qdisc *sch,
					int *qerr)
{
	struct ceetm_sched *q = qdisc_priv(sch);
	struct ceetm_class *cl = NULL;
	struct tcf_result res;
	struct tcf_proto *tcf;
	int result;

	ceetm_sch_dbg("sch %p,  handle 0x%x, parent 0x%X\n",
					sch, sch->handle, sch->parent);
	*qerr = NET_XMIT_SUCCESS | __NET_XMIT_BYPASS;
	tcf = q->filter_list;
	while (tcf && (result = tc_classify(skb, tcf, &res)) >= 0) {
#ifdef CONFIG_NET_CLS_ACT
		switch (result) {
		case TC_ACT_QUEUED:
		case TC_ACT_STOLEN:
			*qerr = NET_XMIT_SUCCESS | __NET_XMIT_STOLEN;
		case TC_ACT_SHOT:
			ceetm_sch_dbg("--- TC_ACT_SHOT\n");
			return NULL;
		}
#endif
		cl = (void *)res.class;
		if (!cl) {
			cl = ceetm_find(res.classid, sch);
			if (!cl)
				break;	/* filter selected invalid classid */
		}
		if (IS_LEAF(cl->level))
			return cl;	/* we hit leaf; return it */

		/* we have got inner class; apply inner filter chain */
		tcf = cl->filter_list;
	}
	if (!cl) {
		ceetm_sch_dbg("No match Found\n");
		return NULL;	/* bad default .. this is safe bet */
	}
	return cl;
}

#if (defined CONFIG_ASF_EGRESS_QOS) || (defined CONFIG_ASF_LINUX_QOS)
extern asf_qos_fn_hook *asf_qos_fn;
#endif
/**
 * ceetm_enqueue - Enqueue a packet into CEETM Frame Queue
 *
 * It dorps the packet if packet classification fails.
**/
static int ceetm_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	int	ret;
	struct ceetm_class *cl;

#if (defined CONFIG_ASF_LINUX_QOS)
	if (asf_qos_fn) {
		ret = asf_qos_fn(skb);
		if (!ret)
			return NET_XMIT_SUCCESS;
	}
#endif

	cl = ceetm_classify(skb, sch, &ret);

	if (!cl || !cl->hw_handle) {
		if (cl)
			ceetm_sch_dbg("class 0x%X donlt have FQ\n",
					cl->common.classid);
		else
			ceetm_sch_dbg("No valid Leaf Class found\n");
		goto drop;
	}
	ceetm_sch_dbg("Classify result : classid 0x%X\n",
				cl ? cl->common.classid : 0);
	if (ceetm_enqueue_pkt(cl->hw_handle, skb))
		goto drop;

	ceetm_inc_enqueue_cnt(cl->hw_handle);
	return CEETM_SUCCESS;
drop:
	/* Increment Drop Stats */
	if (cl && cl->hw_handle)
		ceetm_inc_drop_cnt(cl->hw_handle);
	if (skb)
		dev_kfree_skb_any(skb);

	return NET_XMIT_SUCCESS;
}

/* try to drop from each class (by prio) until one succeed */
static unsigned int ceetm_drop(struct Qdisc *sch)
{
	ceetm_sch_dbg(" No action required.\n");
	return CEETM_SUCCESS;
}

/* reset all classes */
/* always caled under BH & queue lock */
static void ceetm_reset(struct Qdisc *sch)
{
	ceetm_sch_dbg(" No action required.\n");
	return;
}

static const struct nla_policy ceetm_policy[TCA_CEETM_MAX + 1] = {
	[TCA_CEETM_COPT] = { .len = sizeof(struct tc_ceetm_copt) },
	[TCA_CEETM_INIT] = { .len = sizeof(struct tc_ceetm_qopt) },
};

static void ceetm_link_class(struct Qdisc *sch,
		struct Qdisc_class_hash *clhash,
		struct Qdisc_class_common *common)
{
	sch_tree_lock(sch);
	qdisc_class_hash_insert(clhash, common);
	sch_tree_unlock(sch);
	qdisc_class_hash_grow(sch, clhash);
}

static void ceetm_delink_class(struct Qdisc *sch,
		struct Qdisc_class_hash *clhash,
		struct Qdisc_class_common *common)
{
	sch_tree_lock(sch);
	qdisc_class_hash_remove(clhash, common);
	sch_tree_unlock(sch);
}
static void ceetm_cfg_root_class(struct Qdisc *sch,
			struct ceetm_sched *q,
			struct tc_ceetm_qopt *qopt)
{
	struct ceetm_class *un_shaped;
	struct ceetm_class *shaped;

	/* Configure Un-Shaped Class */
	un_shaped = &(q->un.root.un_shaped);
	un_shaped->common.classid = TC_H_MAKE(sch->handle, 0x1);
	un_shaped->refcnt = 1;
	un_shaped->level = CEETM_ROOT_CLASS;
	un_shaped->parent = (void *)sch;
	un_shaped->child_qdisc = NULL;
	un_shaped->cfg.root.shaping_en = FALSE;
	ceetm_link_class(sch, &q->clhash, &un_shaped->common);

	ceetm_sch_dbg("rate %d , ceil %d, mpu %d, overhead %d\n",
			qopt->rate, qopt->ceil, qopt->mpu, qopt->overhead);
	/* Configure Shaped Class */
	if (0 == qopt->rate)
		return;
	shaped = &(q->un.root.shaped);
	shaped->common.classid = TC_H_MAKE(sch->handle, 0x2);
	shaped->refcnt = 1;
	shaped->level = CEETM_ROOT_CLASS;
	shaped->parent = (void *)sch;
	shaped->child_qdisc = NULL;
	shaped->cfg.root.shaping_en = TRUE;
	shaped->cfg.root.rate = qopt->rate;
	shaped->cfg.root.ceil = qopt->ceil;
	shaped->cfg.root.mpu = qopt->mpu;
	shaped->cfg.root.overhead = qopt->overhead;
	ceetm_link_class(sch, &q->clhash, &shaped->common);
}

static int ceetm_cfg_prio_class(struct Qdisc *root,
			struct Qdisc *sch,
			struct ceetm_sched *q,
			struct tc_ceetm_qopt *qopt)
{
	struct ceetm_class *cl;
	struct ceetm_sched *root_q = qdisc_priv(root);
	int	i;

	/* Configure PRIO LEAF Classes */
	for (i = 0; i < CEETM_PRIO_NUM; i++) {
		cl = &(q->un.prio.queues[i]);
		cl->common.classid = TC_H_MAKE(sch->handle, (i + 1));
		cl->refcnt = 1;
		cl->level = CEETM_PRIO_LEAF_CLASS;
		cl->parent = (void *)sch;
		cl->child_qdisc = NULL;
		cl->hw_handle = NULL;
		cl->cq = NULL;
		cl->cfg.prio.priority = i;
		cl->cfg.prio.cr_eligible = qopt->cr_map[i];
		cl->cfg.prio.er_eligible = qopt->er_map[i];
		/* Allocate an Frame-Queue to this leaf class */
		ceetm_cfg_prio_leaf_q(q->hw_handle, i, cl, qdisc_dev(sch));
		if (NULL == cl->hw_handle) {
			ceetm_err("CEETM: unable to create equivalent"
					" Channel Queue instance.\n");
			i--;
			while (i >= 0) {
				ceetm_delink_class(root,
						&root_q->clhash,
						&cl->common);
				cl = &(q->un.prio.queues[i]);
				ceetm_release_cq(cl->hw_handle);
				i--;
			}
			/* Cleanup of acquired resource shall be done
			   by Channel Scheduler*/
			return -EINVAL;
		}
		ceetm_sch_dbg("Adding class classid 0x%X\n",
						cl->common.classid);
		ceetm_link_class(root, &root_q->clhash, &cl->common);
	}
	return CEETM_SUCCESS;
}

static int ceetm_cfg_wbfs_class(struct Qdisc *root,
			struct Qdisc *sch,
			struct ceetm_sched *p_q,
			struct tc_ceetm_qopt *qopt,
			int grp)
{
	struct ceetm_class *cl;
	struct ceetm_sched *root_q = qdisc_priv(root);
	struct ceetm_sched *q = qdisc_priv(sch);
	int i;

	/* Configure WBFS LEAF Classes */
	if (grp != CEETM_WBFS_GRP_both) {
		for (i = 0; i < CEETM_WBFS_MIN_Q; i++) {
			cl = &(q->un.wbfs.grp_q[i]);
			if (grp == CEETM_WBFS_GRP_A)
				cl->common.classid =
					TC_H_MAKE(sch->handle, (i + 1));
			else
				cl->common.classid = TC_H_MAKE(sch->handle,
						CEETM_WBFS_MIN_Q + (i + 1));

			cl->refcnt = 1;
			cl->level = CEETM_WBFS_LEAF_CLASS;
			cl->parent = (void *)sch;
			cl->child_qdisc = NULL;
			cl->cfg.wbfs.weight = qopt->weight[i];
			/* Allocate an Frame-Queue to this leaf class */
			ceetm_cfg_wbfs_leaf_q(p_q->hw_handle, grp, i,
						cl, qdisc_dev(sch));
			if (NULL == cl->hw_handle) {
				ceetm_err("CEETM: unable to create equivalent"
					" Channel Queue instance.\n");
				goto cleanup;
			}
			/* Add class handle in Root Qdisc */
			ceetm_link_class(root, &root_q->clhash, &cl->common);
		}
		q->un.wbfs.num_valid_q = CEETM_WBFS_MIN_Q;
	} else {
		for (i = 0; i < CEETM_WBFS_MAX_Q; i++) {
			cl = &(q->un.wbfs.grp_q[i]);
			cl->common.classid = TC_H_MAKE(sch->handle, (i + 1));
			cl->refcnt = 1;
			cl->level = CEETM_WBFS_LEAF_CLASS;
			cl->parent = (void *)sch;
			cl->child_qdisc = NULL;
			cl->cfg.wbfs.weight = qopt->weight[i];
			/* Allocate an Frame-Queue to this leaf class */
			ceetm_cfg_wbfs_leaf_q(p_q->hw_handle, grp, i,
						cl, qdisc_dev(sch));
			if (NULL == cl->hw_handle) {
				ceetm_err("CEETM: unable to create equivalent"
					" Channel Queue instance.\n");
				goto cleanup;
			}
			/* Add class handle in Root Qdisc */
			ceetm_link_class(root, &root_q->clhash, &cl->common);
		}
		q->un.wbfs.num_valid_q = CEETM_WBFS_MAX_Q;
	}
	/* CR & ER Eligibilty is at group level */
	q->un.wbfs.cr_eligible = qopt->cr_map[0];
	q->un.wbfs.er_eligible = qopt->er_map[0];
	ceetm_sch_dbg("CR %d ER %d\n", q->un.wbfs.cr_eligible,
					q->un.wbfs.er_eligible);
	if (qman_ceetm_channel_set_group_cr_er_eligibility(
				p_q, grp, q->un.wbfs.cr_eligible,
				q->un.wbfs.er_eligible)) {
		ceetm_err("CEETM: Could not set the eligibility.\n");
		goto cleanup;
	}
	return CEETM_SUCCESS;

cleanup:
	while (i >= 0) {
		ceetm_delink_class(root, &root_q->clhash, &cl->common);
		cl = &(q->un.wbfs.grp_q[i]);
		if (cl->hw_handle)
			ceetm_release_cq(cl->hw_handle);
		ceetm_release_wbfs_cq(p_q->hw_handle);
		i--;
	}
	return -EINVAL;
}

static int ceetm_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct net_device *dev = qdisc_dev(sch);
	struct ceetm_sched *q = qdisc_priv(sch);
	struct nlattr *tb[TCA_CEETM_INIT + 1];
	struct tc_ceetm_qopt *qopt;
	int err;

	if (!opt)
		return -EINVAL;

	err = nla_parse_nested(tb, TCA_CEETM_INIT, opt, ceetm_policy);
	if (err < 0)
		return err;

	if (tb[TCA_CEETM_INIT] == NULL) {
		ceetm_err("CEETM: hey probably you have bad tc tool ?\n");
		return -EINVAL;
	}
	if (TC_H_MIN(sch->handle)) {
		ceetm_err("CEETM: Invalid Qdisc Handle.\n");
		return -EINVAL;
	}
	qopt = nla_data(tb[TCA_CEETM_INIT]);
	/* Initialize the class hash list */
	err = qdisc_class_hash_init(&q->clhash);
	if (err < 0)
		return err;

	q->type = qopt->type;
	q->hw_handle = NULL;

	switch (q->type) {
	case CEETM_Q_ROOT:
	{
		ceetm_sch_dbg("Configuring CEETM_Q_ROOT.....\n");
		/* Validte inputs */
		if (sch->parent != TC_H_ROOT) {
			ceetm_err("CEETM: Invalid Qdisc Parent.\n");
			return -EINVAL;
		}
		ceetm_cfg_root_class(sch, q, qopt);
		q->parent = NULL;
		/* Configure Equivalent HW-CEETM instance*/
		ceetm_cfg_lni(dev, q);
		if (NULL == q->hw_handle) {
			ceetm_err("CEETM: unable to create LNI instance.\n");
			return -EINVAL;
		}
	}
	break;
	case CEETM_Q_PRIO:
	{
		struct Qdisc *root = dev->qdisc;
		struct ceetm_class *cl;
		struct ceetm_sched *root_q = qdisc_priv(root);

		ceetm_sch_dbg("-PRIO- sch %p,  handle 0x%x, parent 0x%X "
				"dev %s Root 0x%p\n", sch, sch->handle,
				sch->parent, dev->name, root);
		if (sch->parent == TC_H_ROOT) {
			ceetm_err("CEETM: PRIO cann't be Root Qdisc.\n");
			return -EINVAL;
		}
		/* Validate the parent class */
		cl = ceetm_find(sch->parent, root);

		if (!cl || cl->level != CEETM_INNER_CLASS) {
			ceetm_err("CEETM: Invalid Qdisc Parent.\n");
			return -EINVAL;
		}
		/* Link the Qdisc to parent class */
		if (cl->child_qdisc != NULL) {
			ceetm_err("CEETM: Invalid Parent."
				" Another Qdisc already attached.\n");
			return -EINVAL;
		}

		cl->child_qdisc = sch;
		q->parent = cl;
		q->un.prio.wbfs_grp = 0;
		q->hw_handle = NULL;
		/* Configure Equivalent HW-CEETM instance*/
		ceetm_cfg_channel(root_q->hw_handle, dev->mtu, q);
		if (NULL == q->hw_handle) {
			cl->child_qdisc = NULL;
			ceetm_err("CEETM: unable to create equivalent"
					" Channel Scheduler instance.\n");
			return -EINVAL;
		}
		if (ceetm_cfg_prio_class(root, sch, q, qopt)) {
			cl->child_qdisc = NULL;
			/* delete the allocated channel */
			ceetm_release_channel(q->hw_handle);
			return -EINVAL;
		}
	}
	break;
	case CEETM_Q_WBFS:
	{
		struct Qdisc *root = dev->qdisc;
		struct ceetm_sched *p_q;
		struct ceetm_class *cl;

		ceetm_sch_dbg("-WBFS- sch %p,  handle 0x%x, parent 0x%X dev %s "
				"Root 0x%p\n", sch, sch->handle,
					sch->parent, dev->name, root);
		if (sch->parent == TC_H_ROOT) {
			ceetm_err("CEETM: WBFS cann't be Root Qdisc.\n");
			return -EINVAL;
		}
		/* Validate the parent class */
		cl = ceetm_find(sch->parent, root);

		if (!cl || cl->level != CEETM_PRIO_LEAF_CLASS) {
			ceetm_err("CEETM: Invalid Qdisc Parent.\n");
			return -EINVAL;
		}
		/* Verify that WBFS qdisc has not claimed 1st priority class */
		if (1 == TC_H_MIN(cl->common.classid)) {
			ceetm_err("CEETM: Qdisc cann't be attached"
					" to First Priroity Class.\n");
			return -EINVAL;
		}
		/* Link the Qdisc to parent class */
		if (cl->child_qdisc != NULL) {
			ceetm_err("CEETM: Invalid Parent."
				" Another Qdisc already attached.\n");
			return -EINVAL;
		} else
			cl->child_qdisc = sch;
		/* Verify can we have scope for adding WBFS scheduler,
		   Since we can have either one 8 queues WBFS scheduler
		   or two WBFS with 4 queue each */
		p_q = qdisc_priv((struct Qdisc *) cl->parent);
		switch (p_q->un.prio.wbfs_grp) {
		case NO_WBFS_EXISTS:
			if (CEETM_WBFS_MAX_Q == qopt->queues) {
				p_q->un.prio.wbfs_grp = MAX_WBFS_EXISTS;
				if (ceetm_cfg_wbfs_grp(p_q->hw_handle,
						CEETM_WBFS_GRP_both,
						(TC_H_MIN(cl->common.classid) - 1)))
					return -EINVAL;
				if (ceetm_cfg_wbfs_class(root, sch, p_q,
						qopt, CEETM_WBFS_GRP_both))
					return -EINVAL;
			} else if (CEETM_WBFS_MIN_Q == qopt->queues) {
				p_q->un.prio.wbfs_grp = ONE_WBFS_EXISTS;
				if (ceetm_cfg_wbfs_grp(p_q->hw_handle,
						CEETM_WBFS_GRP_A,
						(TC_H_MIN(cl->common.classid) - 1)))
					return -EINVAL;
				if (ceetm_cfg_wbfs_class(root, sch, p_q,
						qopt, CEETM_WBFS_GRP_A))
					return -EINVAL;
			} else {
				ceetm_err("CEETM: Invalid Number of Queues\n");
				return -EINVAL;
			}
		break;
		case ONE_WBFS_EXISTS:
			if (CEETM_WBFS_MIN_Q == qopt->queues) {
				p_q->un.prio.wbfs_grp = MAX_WBFS_EXISTS;
				if (ceetm_cfg_wbfs_grp(p_q->hw_handle,
						CEETM_WBFS_GRP_B,
						(TC_H_MIN(cl->common.classid) - 1)))
					return -EINVAL;
				if (ceetm_cfg_wbfs_class(root, sch, p_q,
						qopt, CEETM_WBFS_GRP_B))
					return -EINVAL;
			} else {
				ceetm_err("CEETM: Invalid Number of Queues\n");
				return -EINVAL;
			}
		break;
		default:
			ceetm_err("CEETM: No more WBFS sch can be attched"
						" to given parent.\n");
			return -EINVAL;
		}

		q->parent = cl;
	}
	break;
	default:
		ceetm_err("CEETM: Invalid Qdisc type...bad tc tool !\n");
		return -EINVAL;
	}

	/* Initialize filter count */
	q->filter_cnt = 0;
	/* Every thing has been setup now configure the Hardware */

	return CEETM_SUCCESS;
}

static int ceetm_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	spinlock_t *root_lock = qdisc_root_sleeping_lock(sch);
	struct ceetm_sched *q = qdisc_priv(sch);
	struct nlattr *nest;
	struct tc_ceetm_qopt qopt;
	struct ceetm_class *cl;

	ceetm_sch_dbg("sch %p,  handle 0x%x, parent 0x%X\n",
				sch, sch->handle, sch->parent);
	memset(&qopt, 0, sizeof(qopt));
	spin_lock_bh(root_lock);

	qopt.type = q->type;
	switch (q->type) {
	case CEETM_Q_ROOT:
	{
		cl = &(q->un.root.shaped);
		qopt.rate = cl->cfg.root.rate;
		qopt.ceil = cl->cfg.root.ceil;
		qopt.mpu = cl->cfg.root.mpu;
		qopt.overhead = cl->cfg.root.overhead;
		ceetm_sch_dbg("rate %d , ceil %d, mpu %d, overhead %d\n",
				qopt.rate, qopt.ceil, qopt.mpu, qopt.overhead);
	}
	break;
	case CEETM_Q_PRIO:
	{
		int i;

		for (i = 0; i < CEETM_PRIO_NUM; i++) {
			cl = &(q->un.prio.queues[i]);
			qopt.cr_map[i] = cl->cfg.prio.cr_eligible;
			qopt.er_map[i] = cl->cfg.prio.er_eligible;
			ceetm_sch_dbg("DUMP: CR_EL[%d]  %d  ER_EL[%d] %d \n",
					i, qopt.cr_map[i], i, qopt.er_map[i]);
		}
	}
	break;
	case CEETM_Q_WBFS:
	{
		int i;

		qopt.queues = q->un.wbfs.num_valid_q;
		qopt.cr_map[0] = q->un.wbfs.cr_eligible;
		qopt.er_map[0] = q->un.wbfs.er_eligible;
		ceetm_sch_dbg("DUMP: CR_ELGIBLE[%d]	ER_ELGIBLE[%d]\n",
					qopt.cr_map[0], qopt.er_map[0]);
		for (i = 0; i < qopt.queues; i++) {
			cl = &(q->un.wbfs.grp_q[i]);
			qopt.weight[i] = cl->cfg.wbfs.weight;
			ceetm_sch_dbg("DUMP: Weight[%d] %d\n",
					i, qopt.weight[i]);
		}
	}
	break;
	default:
		ceetm_err("CEETM: Invalid Qdisc type...bad tc tool !\n");
		return -EINVAL;
	}

	nest = nla_nest_start(skb, TCA_OPTIONS);
	if (nest == NULL)
		goto nla_put_failure;
	if (nla_put(skb, TCA_CEETM_INIT, sizeof(qopt), &qopt))
		goto nla_put_failure;
	nla_nest_end(skb, nest);

	spin_unlock_bh(root_lock);
	return skb->len;

nla_put_failure:
	spin_unlock_bh(root_lock);
	nla_nest_cancel(skb, nest);
	return -1;
}

static int ceetm_dump_class(struct Qdisc *sch, unsigned long arg,
			  struct sk_buff *skb, struct tcmsg *tcm)
{
	spinlock_t *root_lock = qdisc_root_sleeping_lock(sch);
	struct ceetm_class *cl = (struct ceetm_class *)arg;
	struct nlattr *nest;
	struct tc_ceetm_copt opt;

	ceetm_sch_dbg("sch %p,  handle 0x%x, parent 0x%X\n",
				sch, sch->handle, sch->parent);
	spin_lock_bh(root_lock);
	if (cl->level != CEETM_INNER_CLASS)
		tcm->tcm_parent = ((struct Qdisc *)cl->parent)->handle;
	else
		tcm->tcm_parent =
			((struct ceetm_class *)cl->parent)->common.classid;

	tcm->tcm_handle = cl->common.classid;
	if (cl->child_qdisc)
		tcm->tcm_info = cl->child_qdisc->handle;

	memset(&opt, 0, sizeof(opt));
	if (cl->level == CEETM_WBFS_LEAF_CLASS)
		opt.weight = cl->cfg.wbfs.weight;
	else if (cl->level == CEETM_INNER_CLASS) {
		opt.rate = cl->cfg.inner.rate;
		opt.ceil = cl->cfg.inner.ceil;
		opt.weight = cl->cfg.inner.weight;
	} else if (cl->level == CEETM_ROOT_CLASS) {
		opt.rate = cl->cfg.root.rate;
		opt.ceil = cl->cfg.root.ceil;
	}

	nest = nla_nest_start(skb, TCA_OPTIONS);
	if (nest == NULL)
		goto nla_put_failure;
	if (nla_put(skb, TCA_CEETM_COPT, sizeof(opt), &opt))
		goto nla_put_failure;
	nla_nest_end(skb, nest);
	spin_unlock_bh(root_lock);
	return skb->len;

nla_put_failure:
	spin_unlock_bh(root_lock);
	nla_nest_cancel(skb, nest);
	return -1;
}

static int
ceetm_dump_class_stats(struct Qdisc *sch,
			unsigned long arg,
			struct gnet_dump *d)
{
	struct ceetm_class *cl = (struct ceetm_class *)arg;
	struct tc_ceetm_xstats xstats;

	if (cl->hw_handle) {
		ceetm_get_fq_stats(cl->hw_handle, &xstats.enqueue,
						&xstats.drop);
		qman_ceetm_cq_get_dequeue_statistics(
			(struct qm_ceetm_cq *)cl->cq, 1,
			&xstats.dequeue, &xstats.deq_bytes);
		return gnet_stats_copy_app(d, &xstats, sizeof(xstats));
	}
	return CEETM_SUCCESS;
}

static int ceetm_graft(struct Qdisc *sch, unsigned long arg, struct Qdisc *new,
		     struct Qdisc **old)
{
	return CEETM_SUCCESS;
}

static unsigned long ceetm_get(struct Qdisc *sch, u32 classid)
{
	struct ceetm_class *cl = ceetm_find(classid, sch);

	ceetm_sch_dbg("sch %p,  handle 0x%x, parent 0x%X  cl=%p\n",
				sch, sch->handle, sch->parent, cl);
	if (cl)
		cl->refcnt++;
	return (unsigned long)cl;
}

static void ceetm_destroy_class(struct Qdisc *sch, struct ceetm_class *cl)
{
	ceetm_sch_dbg("sch %p,  handle 0x%x, parent 0x%X\n",
					sch, sch->handle, sch->parent);
	ceetm_sch_dbg("===> ClassID : 0x%X Level: %d\n",
				cl->common.classid, cl->level);
	if (cl->child_qdisc != NULL) {
		struct ceetm_sched *q = qdisc_priv(cl->child_qdisc);
		/* Mark parent as NULL to indicate that inner QDISC
		   funcitons has been called by parent Class */
		ceetm_sch_dbg("child_Qdisc %p\n", cl->child_qdisc);
		q->parent = NULL;
		qdisc_destroy(cl->child_qdisc);
		cl->child_qdisc = NULL;
	}
	tcf_destroy_chain(&cl->filter_list);

	if (cl->level == CEETM_INNER_CLASS) /* All others are static ones */
		kfree(cl);
	else if (cl->hw_handle) {
		ceetm_release_cq(cl->hw_handle);
		cl->hw_handle = NULL;
	}
}

static void ceetm_destroy(struct Qdisc *sch)
{
	struct ceetm_sched *q = qdisc_priv(sch);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
	struct hlist_node *n;
#endif
	struct hlist_node *next;
	struct ceetm_class *cl;
	unsigned int i;

	ceetm_sch_dbg("sch %p,  handle 0x%x, parent 0x%X  Next Qdisc = %p\n",
			sch, sch->handle, sch->parent, sch->next_sched);
	/* Due to H/W limitiation, we are only supporting
	   QDISC Deletion via ROOT QDISC only */
	/* Since we have added all the sub classes entry in root qdisc,
	   each sub class/qdiscs in the Hierarchy shall be deleted when
	   Root Qdisc destroy is called. So no need to handle class destory
	   in other types of CEETM QDISC destroy */
	if (q->type != CEETM_Q_ROOT) {
		/* Only delete QDISC resources as sub classes/ qdiscs of it's
		   shall be handled by Root Qdisc deletion */
		ceetm_sch_dbg("Not a ROOT QDISC\n");
		if (q->parent == NULL) {
			ceetm_sch_dbg("OK Parent is NULL\n");
			tcf_destroy_chain(&q->filter_list);
			q->filter_list = NULL;
			qdisc_class_hash_destroy(&q->clhash);
			/* Release QMAN resources */
			if (ceetm_release_channel(q->hw_handle))
				ceetm_err("Error in releasing"
				" CEETM Channel Scheduler.\n");
			q->hw_handle =NULL;
			return;
		} else {
			ceetm_err("Deletion Allowed via Root Qdisc only!\n");
			return;
		}
	}
	/* Only a Root QDISC shall reach here */
	/* Unbind filter  */
	tcf_destroy_chain(&q->filter_list);
	q->filter_list = NULL;

	/* If CEETM ROOT typw Qdisc then, Destroy attached Class Resourcees */
	for (i = 0; i < q->clhash.hashsize; i++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
		hlist_for_each_entry(cl, n, &q->clhash.hash[i], common.hnode)
#else
		hlist_for_each_entry(cl, &q->clhash.hash[i], common.hnode)
#endif
		tcf_destroy_chain(&cl->filter_list);
	}
	for (i = 0; i < q->clhash.hashsize; i++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
		hlist_for_each_entry_safe(cl, n, next, &q->clhash.hash[i],
					common.hnode)
#else
		hlist_for_each_entry_safe(cl, next, &q->clhash.hash[i],
					common.hnode)
#endif
		ceetm_destroy_class(sch, cl);
	}
	qdisc_class_hash_destroy(&q->clhash);
	/* Finally Destroy the CEETM LNI*/
	if (ceetm_release_lni(q->hw_handle))
		ceetm_err("Error in releasing CEETM LNI.\n");
}

static int ceetm_delete(struct Qdisc *sch, unsigned long arg)
{
	return CEETM_SUCCESS;
}

static void ceetm_put(struct Qdisc *sch, unsigned long arg)
{
	struct ceetm_class *cl = (struct ceetm_class *)arg;
	ceetm_sch_dbg("classid 0x%X\n", cl->common.classid);
	cl->refcnt--;
}

static int ceetm_change_class(struct Qdisc *sch, u32 classid,
			    u32 parentid, struct nlattr **tca,
			    unsigned long *arg)
{
	int err = -EINVAL;
	struct ceetm_sched *q = qdisc_priv(sch);
	struct ceetm_class *cl = (struct ceetm_class *)*arg, *parent;
	struct nlattr *opt = tca[TCA_OPTIONS];
	struct nlattr *tb[__TCA_CEETM_MAX];
	struct tc_ceetm_copt *copt;

	ceetm_sch_dbg("-PRIO-- sch %p,  handle 0x%x, parent 0x%X\n",
					sch, sch->handle, sch->parent);
	/* extract all subattrs from opt attr */
	if (!opt)
		goto failure;

	if (cl) {
		ceetm_err("CEETM: Changing class attribute not supported.\n");
		return -EINVAL;
	}
	err = nla_parse_nested(tb, TCA_CEETM_COPT, opt, ceetm_policy);
	if (err < 0)
		goto failure;

	err = -EINVAL;
	if (tb[TCA_CEETM_COPT] == NULL)
		goto failure;

	parent = ceetm_find(parentid, sch);
	if (!parent || parent->level != CEETM_ROOT_CLASS)
		goto failure;
	/* check for valid classid */
	if (!classid || TC_H_MAJ(classid ^ sch->handle))
		/* It appears that TC framework already did this check
		   if (ceetm_find(classid, sch))
		 */
		goto failure;

	copt = nla_data(tb[TCA_CEETM_COPT]);
	if (parent->cfg.root.shaping_en) {
		/* Rate & Ceil values must be there */
		if (copt->rate == 0)
			goto failure;
	} else {
		/* Weight values must be there */
		if (copt->weight == 0)
			goto failure;
	}
	err = -ENOBUFS;
	cl = kzalloc(sizeof(*cl), GFP_KERNEL);
	if (!cl)
		goto failure;

	cl->common.classid = classid;
	cl->refcnt = 1;
	cl->level = CEETM_INNER_CLASS;
	cl->parent = (void *)parent;
	cl->child_qdisc = NULL;
	cl->cfg.inner.rate = copt->rate;
	cl->cfg.inner.ceil = copt->ceil;
	cl->cfg.inner.weight = copt->weight;
	/* Add class handle in Root Qdisc */
	ceetm_link_class(sch, &q->clhash, &cl->common);
	*arg = (unsigned long)cl;
	return CEETM_SUCCESS;

failure:
	return err;
}

static struct tcf_proto **ceetm_find_tcf(struct Qdisc *sch, unsigned long arg)
{
	struct ceetm_sched *q = qdisc_priv(sch);
	struct ceetm_class *cl = (struct ceetm_class *)arg;
	struct tcf_proto **fl = cl ? &cl->filter_list : &q->filter_list;

	ceetm_sch_dbg("sch %p,  handle 0x%x, parent 0x%X classid 0x%X\n",
			sch, sch->handle, sch->parent,
			cl ? cl->common.classid : 0xFFFFFFFF);
	return fl;
}

static unsigned long ceetm_bind_filter(struct Qdisc *sch, unsigned long parent,
				     u32 classid)
{
	struct ceetm_class *cl = ceetm_find(classid, sch);

	ceetm_sch_dbg("sch %p,  handle 0x%x, classid 0x%X\n",
					sch, sch->handle, classid);
	if (cl && IS_LEAF(cl->level))
		cl->filter_cnt++;

	return (unsigned long)cl;
}

static void ceetm_unbind_filter(struct Qdisc *sch, unsigned long arg)
{
	struct ceetm_class *cl = (struct ceetm_class *)arg;

	ceetm_sch_dbg("sch %p,  handle 0x%x, parent 0x%X classid 0x%X\n",
			sch, sch->handle, sch->parent,
			cl ? cl->common.classid : 0xFFFFFFFF);
	if (cl)
		cl->filter_cnt--;
}

static void ceetm_walk(struct Qdisc *sch, struct qdisc_walker *arg)
{
	struct ceetm_sched *q = qdisc_priv(sch);
	struct ceetm_class *cl;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
	struct hlist_node *n;
#endif
	unsigned int i;

	ceetm_sch_dbg("sch %p,  handle 0x%x, parent 0x%X\n",
				sch, sch->handle, sch->parent);
	if (arg->stop)
		return;

	for (i = 0; i < q->clhash.hashsize; i++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
		hlist_for_each_entry(cl, n, &q->clhash.hash[i], common.hnode) {
#else
		hlist_for_each_entry(cl, &q->clhash.hash[i], common.hnode) {
#endif
			if (arg->count < arg->skip) {
				arg->count++;
				continue;
			}
			if (arg->fn(sch, (unsigned long)cl, arg) < 0) {
				arg->stop = 1;
				return;
			}
			arg->count++;
		}
	}
}

static struct Qdisc *ceetm_leaf(struct Qdisc *sch, unsigned long arg)
{
	struct ceetm_class *cl = (struct ceetm_class *)arg;

	ceetm_sch_dbg("sch %p,  handle 0x%x, parent 0x%X\n",
					sch, sch->handle, sch->parent);
	return cl->child_qdisc;
}

static const struct Qdisc_class_ops ceetm_class_ops = {
	.graft		=	ceetm_graft,
	.leaf		=	ceetm_leaf,
	.get		=	ceetm_get,
	.put		=	ceetm_put,
	.change		=	ceetm_change_class,
	.delete		=	ceetm_delete,
	.walk		=	ceetm_walk,
	.tcf_chain	=	ceetm_find_tcf,
	.bind_tcf	=	ceetm_bind_filter,
	.unbind_tcf	=	ceetm_unbind_filter,
	.dump		=	ceetm_dump_class,
	.dump_stats	=	ceetm_dump_class_stats,
};

static struct Qdisc_ops ceetm_qdisc_ops __read_mostly = {
	.cl_ops		=	&ceetm_class_ops,
	.id		=	"ceetm",
	.priv_size	=	sizeof(struct ceetm_sched),
	.enqueue	=	ceetm_enqueue,
	.drop		=	ceetm_drop,
	.init		=	ceetm_init,
	.reset		=	ceetm_reset,
	.destroy	=	ceetm_destroy,
	.dump		=	ceetm_dump,
	.owner		=	THIS_MODULE,
};

static int __init ceetm_module_init(void)
{
	ceetm_sch_dbg("CEETM QDISC resgitered\n");
	return register_qdisc(&ceetm_qdisc_ops);
}
static void __exit ceetm_module_exit(void)
{
	ceetm_sch_dbg("CEETM QDISC un-resgitered\n");
	unregister_qdisc(&ceetm_qdisc_ops);
}

module_init(ceetm_module_init)
module_exit(ceetm_module_exit)
