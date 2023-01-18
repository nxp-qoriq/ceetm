/* Copyright 2014-2016 Freescale Semiconductor Inc.
 * Copyright 2017-2018 NXP
 *
 * SPDX-License-Identifier: GPL-2.0
 */
#include <stdio.h>
#include <stdlib.h>

#include "dpaa2_ceetm.h"

/* TODO:
 * - shaping per queue (LX2) setup - prio / wbfs class
 */

static void explain(void)
{
	fprintf(stderr, "Usage:\n"
		"... qdisc add ... ceetm type root\n"
		"... class add ... ceetm type root [cir CIR] [eir EIR] [cbs CBS] [ebs EBS] [coupled C]\n"
		"... qdisc add ... ceetm type prio [prioA PRIO] [prioB PRIO] [separate SEPARATE]\n"
		"... class add ... ceetm type prio [mode MODE] [weight W]\n"
		"\n"
		"Update configurations:\n"
		"... class change ... ceetm type root [cir CIR] [eir EIR] [cbs CBS] [ebs EBS] [coupled C]\n"
		"\n"
		"Qdisc types:\n"
		"root - associate a LNI to the DPNI\n"
		"prio - configure the LNI channel's Priority Scheduler with up to eight classes\n"
		"\n"
		"Class types:\n"
		"root - configure the LNI channel\n"
		"prio - configure an independent or weighted class queue\n"
		"\n"
		"Options:\n"
		"CIR - the committed information rate of the LNI channel\n"
		"	dual-rate shaper (required for shaping scenarios)\n"
		"EIR - the excess information rate of the LNI channel\n"
		"	dual-rate shaper (optional for shaping scenarios, default 0)\n"
		"CBS - the committed burst size of the LNI channel\n"
		"	dual-rate shaper (required for shaping scenarios)\n"
		"EBS - the excess of the LNI channel\n"
		"	dual-rate shaper (optional for shaping scenarios, default 0)\n"
		"C - shaper coupled, if both CIR and EIR are finite, once the\n"
		"	CR token bucket is full, additional CR tokens are instead\n"
		"	added to the ER token bucket\n"
		"PRIO - priority of the weighted group A / B of queues\n"
		"SEPARATE - groups A and B are separate\n"
		"MODE - scheduling mode of class queue, can be:\n"
		"	STRICT_PRIORITY\n"
		"	WEIGHTED_A\n"
		"	WEIGHTED_B\n"
		"W - the weight of the class queue in the weighted group\n"
		);
}

int dpaa2_ceetm_parse_qopt(struct qdisc_util *qu, int argc, char **argv,
		struct nlmsghdr *n)
{
	struct dpaa2_ceetm_tc_qopt opt;
	bool prioA_set = false;
	bool prioB_set = false;
	bool separate_set = false;
	struct rtattr *tail;
	memset(&opt, 0, sizeof(opt));

	while (argc > 0) {
		if (strcmp(*argv, "type") == 0) {
			if (opt.type) {
				fprintf(stderr, "type already specified.\n");
				return -1;
			}

			NEXT_ARG();

			if (matches(*argv, "root") == 0)
				opt.type = DPAA2_CEETM_ROOT;

			else if (matches(*argv, "prio") == 0)
				opt.type = DPAA2_CEETM_PRIO;

			else {
				fprintf(stderr, "Illegal type argument.\n");
				return -1;
			}

		} else if (strcmp(*argv, "prioA") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
						"before prioA.\n");
				return -1;

			} else if (opt.type != DPAA2_CEETM_PRIO) {
				fprintf(stderr, "prioA belongs to prio "
						"qdiscs only.\n");
				return -1;
			}

			if (prioA_set) {
				fprintf(stderr, "prioA already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u8(&opt.prio_group_A, *argv, 10)) {
				fprintf(stderr, "Illegal prioA argument\n");
				return -1;
			}

			prioA_set = true;
		} else if (strcmp(*argv, "prioB") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
						"before prioB.\n");
				return -1;

			} else if (opt.type != DPAA2_CEETM_PRIO) {
				fprintf(stderr, "prioB belongs to prio "
						"qdiscs only.\n");
				return -1;
			}

			if (prioB_set) {
				fprintf(stderr, "prioB already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u8(&opt.prio_group_B, *argv, 10)) {
				fprintf(stderr, "Illegal prioB argument\n");
				return -1;
			}

			prioB_set = true;
		} else if (strcmp(*argv, "separate") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
							"before separate.\n");
				return -1;

			} else if (opt.type != DPAA2_CEETM_PRIO) {
				fprintf(stderr, "separate belongs to prio qdiscs "
						"only.\n");
				return -1;
			}

			if (separate_set) {
				fprintf(stderr, "separate already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u8(&opt.separate_groups, *argv, 10)) {
				fprintf(stderr, "Illegal separate argument. Use 0/1.\n");
				return -1;
			}

			separate_set = true;

		} else if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;

		} else {
			fprintf(stderr, "Illegal argument - %s.\n", *argv);
			explain();
			return -1;
		}

		argc--; argv++;
	}

	if (!opt.type) {
		fprintf(stderr, "Please specify the qdisc type.\n");
		return -1;
	}

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 1024, DPAA2_CEETM_TCA_QOPS, &opt, sizeof(opt));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;

	return 0;
}

int dpaa2_ceetm_parse_copt(struct qdisc_util *qu, int argc, char **argv,
		struct nlmsghdr *n)
{
	struct dpaa2_ceetm_tc_copt opt;
	struct rtattr *tail;
	memset(&opt, 0, sizeof(opt));
	bool cir_set = false;
	bool eir_set = false;
	bool cbs_set = false;
	bool ebs_set = false;
	bool coupled_set = false;
	bool mode_set = false;
	bool weight_set = false;

	while (argc > 0) {
		if (strcmp(*argv, "type") == 0) {
			if (opt.type) {
				fprintf(stderr, "type already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (matches(*argv, "root") == 0) {
				opt.type = DPAA2_CEETM_ROOT;

			} else if (matches(*argv, "prio") == 0) {
				opt.type = DPAA2_CEETM_PRIO;

			} else {
				fprintf(stderr, "Illegal type argument.\n");
				return -1;
			}

		} else if (strcmp(*argv, "cir") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the class "
						"type before the CIR.\n");
				return -1;

			} else if (opt.type != DPAA2_CEETM_ROOT) {
				fprintf(stderr, "CIR belongs to root classes "
						"only.\n");
				return -1;
			}

			if (cir_set) {
				fprintf(stderr, "CIR already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_rate64(&opt.shaping_cfg.cir, *argv)) {
				fprintf(stderr, "Illegal CIR argument.\n");
				return -1;
			}

			cir_set = true;
		} else if (strcmp(*argv, "eir") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
							"before the EIR.\n");
				return -1;

			} else if (opt.type != DPAA2_CEETM_ROOT) {
				fprintf(stderr, "EIR belongs to root qdiscs "
						"only.\n");
				return -1;
			}

			if (eir_set) {
				fprintf(stderr, "EIR already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_rate64(&opt.shaping_cfg.eir, *argv)) {
				fprintf(stderr, "Illegal EIR argument.\n");
				return -1;
			}

			eir_set = true;
		} else if (strcmp(*argv, "cbs") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
							"before the CBS.\n");
				return -1;

			} else if (opt.type != DPAA2_CEETM_ROOT) {
				fprintf(stderr, "CBS belongs to root qdiscs "
						"only.\n");
				return -1;
			}

			if (cbs_set) {
				fprintf(stderr, "CBS already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u16(&opt.shaping_cfg.cbs, *argv, 10)) {
				fprintf(stderr, "Illegal CBS argument.\n");
				return -1;
			}

			/* TODO: CBS min / max validation */
			cbs_set = true;
		} else if (strcmp(*argv, "ebs") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
							"before the EBS.\n");
				return -1;

			} else if (opt.type != DPAA2_CEETM_ROOT) {
				fprintf(stderr, "EBS belongs to root qdiscs "
						"only.\n");
				return -1;
			}

			if (ebs_set) {
				fprintf(stderr, "EBS already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u16(&opt.shaping_cfg.ebs, *argv, 10)) {
				fprintf(stderr, "Illegal EBS argument.\n");
				return -1;
			}

			/* TODO: EBS min / max validation */
			ebs_set = true;
		} else if (strcmp(*argv, "coupled") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
							"before the coupling.\n");
				return -1;

			} else if (opt.type != DPAA2_CEETM_ROOT) {
				fprintf(stderr, "coupled belongs to root qdiscs "
						"only.\n");
				return -1;
			}

			if (coupled_set) {
				fprintf(stderr, "coupled already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u8(&opt.shaping_cfg.coupled, *argv, 10)) {
				fprintf(stderr, "Illegal coupled argument. Specify 0/1.\n");
				return -1;
			}

			coupled_set = true;

		} else if (strcmp(*argv, "mode") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the class type "
						"before the mode.\n");
				return -1;

			} else if (opt.type != DPAA2_CEETM_PRIO) {
				fprintf(stderr, "mode belongs to prio "
						"classes only.\n");
				return -1;
			}

			if (mode_set) {
				fprintf(stderr, "mode already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (matches(*argv, "STRICT_PRIORITY") == 0)
				opt.mode = STRICT_PRIORITY;
			else if (matches(*argv, "WEIGHTED_A") == 0)
				opt.mode = WEIGHTED_A;
			else if (matches(*argv, "WEIGHTED_B") == 0)
				opt.mode = WEIGHTED_B;
			else {
				fprintf(stderr, "Illegal mode "
						"argument: must be either\n"
						"STRICT_PRIORITY\n"
						"WEIGHTED_A\n"
						"WEIGHTED_B\n");
				return -1;
			}

			mode_set = true;

		} else if (strcmp(*argv, "weight") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the class type "
						"before the weight.\n");
				return -1;

			} else if (opt.type != DPAA2_CEETM_PRIO) {
				fprintf(stderr, "weight belongs to prio "
						"classes only.\n");
				return -1;
			}

			if (weight_set) {
				fprintf(stderr, "weight already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u16(&opt.weight, *argv, 10) ||
				opt.weight == 0) {
				fprintf(stderr, "Illegal weight "
						"argument: must be "
						"between 100 and 24800.\n");
				return -1;
			}

			weight_set = true;

		} else {
			fprintf(stderr, "Illegal argument - %s.\n", *argv);
			return -1;
		}

		argc--; argv++;
	}

	if (!opt.type) {
		fprintf(stderr, "Please specify the class type.\n");
		return -1;
	}

	/* TODO: more validation for all scenarios */
	if ((!cir_set || !eir_set) && coupled_set && opt.shaping_cfg.coupled == 1) {
		fprintf(stderr, "Coupled can be set to 1 only if CIR and EIR are set.\n");
		return -1;
	}

	if (cir_set || eir_set)
		opt.shaped = 1;
	else
		opt.shaped = 0;

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 2024, DPAA2_CEETM_TCA_COPT, &opt, sizeof(opt));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;

	return 0;
}

int dpaa2_ceetm_print_qopt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[DPAA2_CEETM_TCA_MAX];
	struct dpaa2_ceetm_tc_qopt *qopt = NULL;

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, DPAA2_CEETM_TCA_MAX - 1, opt);

	if (tb[DPAA2_CEETM_TCA_QOPS]) {
		if (RTA_PAYLOAD(tb[DPAA2_CEETM_TCA_QOPS]) < sizeof(*qopt))
			fprintf(stderr, "CEETM: too short opt\n");
		else
			qopt = RTA_DATA(tb[DPAA2_CEETM_TCA_QOPS]);
	}

	if (!qopt)
		return 0;

	if (qopt->type == DPAA2_CEETM_ROOT) {
		fprintf(f, "type root ");
	} else if (qopt->type == DPAA2_CEETM_PRIO) {
		fprintf(f, "type prio prioA %d prioB %d separate %d",
				qopt->prio_group_A, qopt->prio_group_B,
				qopt->separate_groups);
	}

	return 0;
}

int dpaa2_ceetm_print_copt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[DPAA2_CEETM_TCA_MAX];
	struct dpaa2_ceetm_tc_copt *copt = NULL;
	char buf[64];

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, DPAA2_CEETM_TCA_MAX - 1, opt);

	if (tb[DPAA2_CEETM_TCA_COPT]) {
		if (RTA_PAYLOAD(tb[DPAA2_CEETM_TCA_COPT]) < sizeof(*copt))
			fprintf(stderr, "CEETM: too short opt\n");
		else
			copt = RTA_DATA(tb[DPAA2_CEETM_TCA_COPT]);
	}

	if (!copt)
		return 0;

	if (copt->type == DPAA2_CEETM_ROOT) {
		fprintf(f, "type root ");

		if (copt->shaped) {
			print_rate(buf, sizeof(buf), copt->shaping_cfg.cir);
			fprintf(f, "CIR %s ", buf);

			print_rate(buf, sizeof(buf), copt->shaping_cfg.eir);
			fprintf(f, "EIR %s ", buf);

			fprintf(f, "CBS %d EBS %d ", copt->shaping_cfg.cbs, copt->shaping_cfg.ebs);

			fprintf(f, "coupled %d ", copt->shaping_cfg.coupled);
		} else {
			fprintf(f, "unshaped ");
		}

	} else if (copt->type == DPAA2_CEETM_PRIO) {
		fprintf(f, "type prio ");

		switch(copt->mode) {
		case STRICT_PRIORITY:
			fprintf(f, "mode STRICT_PRIORITY ");
			break;
		case WEIGHTED_A:
			fprintf(f, "mode WEIGHTED_A ");
			break;
		case WEIGHTED_B:
			fprintf(f, "mode WEIGTHED_B ");
			break;
		}

		if (copt->mode != STRICT_PRIORITY)
			fprintf(f, "weight %d ", copt->weight);
	}

	return 0;
}

int dpaa2_ceetm_print_xstats(struct qdisc_util *qu, FILE *f, struct rtattr *xstats)
{
	struct dpaa2_ceetm_tc_xstats *st;

	if (xstats == NULL)
		return 0;

	if (RTA_PAYLOAD(xstats) < sizeof(*st))
		return -1;

	st = RTA_DATA(xstats);
	fprintf(f, "ceetm:\ndeq bytes %llu\ndeq frames %llu\nrej bytes %llu\nrej frames %llu\n",
			st->ceetm_dequeue_bytes, st->ceetm_dequeue_frames,
			st->ceetm_reject_bytes, st->ceetm_reject_frames);
	return 0;
}
