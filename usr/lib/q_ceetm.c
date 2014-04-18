/**************************************************************************
 * Copyright 2013, Freescale Semiconductor, Inc. All rights reserved.
 ***************************************************************************/
/*
 * File:        q_ceetm.c
 *
 * Description: Userspace ceetm command handler.
 *
 * Authors:     Ganga <B46167@freescale.com>
 *
 *
 */
/******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "utils.h"
#include "tc_util.h"


static void explain(void)
{
		fprintf(stderr, "Usage:\n"
		"... qdisc add ... ceetm type root [rate R] [ceil C] [overhead "
		"O] [mpu M]\n"
		"... class add ... ceetm [rate T] [ceil C]\n"
		"... class add ... ceetm [weight W]\n"
		"... qdisc add ... ceetm type prio [cr_map c1 c2 c3 c4 c5 c6 c7"
		" c8] [er_map e1 e2 e3 e4 e5 e6 e7 e8]\n"
		"... qdisc add ... ceetm type wbfs [queues Q w1 w2 w3 w4 w5 w6 "
		"w7 w8] [cr_map c1] [er_map e1]\n"
		"\n type has values: root (Logical NW interface instance"
		" in CEETM)"
		"\n\t\t  wbfs  (Class with Weighted Fair "
		"Scheduler)\n"
		"\t\t  prio (Class with prio Qdisc)\n\n"
		" Depending upon class type a subset of above parameter list is"
		" applicable.\n"
		" ceil is optional\n"
		" cr_map and er_map are optional"
		"\n\n"
		" rate     rate allocated to this class\n"
		" mpu      minimum packet size used in rate computations\n"
		" overhead per-packet size overhead used in rate computations.\n"
		"\t Recommended value for overhead is 24 i.e. 12bytes IFG + 8bytes Preamble + 4bytes FCS.\n"
		" ceil     define Peak rate support upper class rate\n"
		" weight   Weightage(i.e. W) given to a class channel lies between [0, 8],\n"
		" where weight equals to zero will only allow one packet TX in each turn of the queues.\n"
		" cr_map   Boolean values for leaf nodes to contribute in"
		" committed rate shaping - 0 for NO, 1 for YES\n"
		" er_map   Boolean values for leaf nodes to contribute in "
		"excess rate shaping - 0 for NO, 1 for YES\n"
		" w[1..8]  how much weightage given to a leaf at once "
		"[1, 248]\n"
		" queues   in case of wbfs type class tells whether it has 4/8"
		" queues.{4 or 8}\n"
		);
}

static void explain1(int type_mode)
{
	if (type_mode == 1)
		fprintf(stderr, "Usage:\n"
		" ... qdisc add ... ceetm type root"
		" rate 1000mbit ceil 1000mbit mpu 64 overhead 24\n"
		);
	else if (type_mode == 0)
		fprintf(stderr, "Usage:\n"
		"a) ... class add ... ceetm rate"
		" 1000mbit ceil 1000mbit\nb) ... class add ..."
		" ceetm weight 1\n");
	else if (type_mode == 2)
		fprintf(stderr, "Usage:\n"
				" ... qdisc add ... ceetm type prio"
				" cr_map c1 c2 c3 c4 c5 c6 c7 c8 er_map e1 e2"
				" e3 e4 e5 e6 e7 e8\n");
	else if (type_mode == 3) {
		fprintf(stderr, "Usage:\n"
				"a) ... qdisc add ... ceetm type wbfs"
				" queues 8 w1 w2 w3 w4 w5 w6 w7 w8 cr_map c1"
				" er_map e1\n"
				"b) ... qdisc add ... ceetm type wbfs queues 4"
				" w1 w2 w3 w4 cr_map c1 er_map e1\n"
			);
	} else
		fprintf(stderr, "\nINCORRECT COMMAND LINE\n");
}

static int ceetm_parse_opt(struct qdisc_util *qu, int argc, char **argv,
		struct nlmsghdr *n)
{
	struct tc_ceetm_qopt opt;
	unsigned short mpu = 0;
	unsigned short overhead = 0;
	unsigned short queue = 0;
	int ceetm_weight_mode = 0;
	int ceetm_cr_mode = 0;
	int cr = 0;
	int ceetm_er_mode = 0;
	int er = 0;
	int ceetm_queue_mode = 0;
	int count_q = 0;
	int id_cr = 0;
	int id_er = 0;
	struct rtattr *tail;
	memset(&opt, 0, sizeof(opt));
	int i = 0;
	for (i ; i < 8 ; i++) {
		opt.cr_map[i] = (__u8)1;
		opt.er_map[i] = (__u8)1;
	}

	while (argc > 0) {
		if (strcmp(*argv, "type") == 0) {
			if (opt.type) {
				fprintf(stderr, "Double \"type\" spec\n");
				return -1;
			}
			NEXT_ARG();
			if (matches(*argv, "root") == 0)
				opt.type = CEETM_Q_ROOT;
			else if (matches(*argv, "prio") == 0)
				opt.type = CEETM_Q_PRIO;
			else if (matches(*argv, "wbfs") == 0)
				opt.type = CEETM_Q_WBFS;
			else {
				explain();
				return -1;
			}
		} else if (strcmp(*argv, "rate") == 0) {
			if (opt.type != CEETM_Q_ROOT) {
				if (opt.type)
					explain1(opt.type);
				else
					fprintf(stderr, "Error:"
							"Mention the "
							"type of qdisc after "
							"ceetm.\n");
				return -1;
			}
			NEXT_ARG();
			if (opt.rate) {
				fprintf(stderr, "Double \"rate\" spec\n");
				return -1;
			}
			if (get_rate(&opt.rate, *argv)) {
				if (opt.type)
					explain1(opt.type);
				else
					fprintf(stderr, "Error:"
							"Mention the "
							"type of qdisc after "
							"ceetm.\n");
				return -1;
			}
		} else if (strcmp(*argv, "mpu") == 0) {
			if (opt.type != CEETM_Q_ROOT) {
				if (opt.type)
					explain1(opt.type);
				else
					fprintf(stderr, "Error:"
							"Mention the "
							"type of qdisc after "
							"ceetm.\n");
				return -1;
			}
			if (opt.mpu) {
				fprintf(stderr, "Double \"mpu\" spec\n");
				return -1;
			}
			NEXT_ARG();
			if (get_u16(&mpu, *argv, 10)) {
				if (opt.type)
					explain1(opt.type);
				else
					fprintf(stderr, "Error:"
							"Mention the "
							"type of qdisc after "
							"ceetm.\n");
				return -1;
			}
			opt.mpu = mpu;
		} else if (strcmp(*argv, "overhead") == 0) {
			if (opt.type != CEETM_Q_ROOT) {
				if (opt.type)
					explain1(opt.type);
				else
					fprintf(stderr, "Error:"
							"Mention the "
							"type of qdisc after "
							"ceetm.\n");
				return -1;
			}
			if (opt.overhead) {
				fprintf(stderr, "Double \"overhead\" spec\n");
				return -1;
			}
			NEXT_ARG();
			if (get_u16(&overhead, *argv, 10)) {
				if (opt.type)
					explain1(opt.type);
				else
					fprintf(stderr, "Error:"
							"Mention the "
							"type of qdisc after "
							"ceetm.\n");
				return -1;
			}
			opt.overhead = overhead;
		} else if (strcmp(*argv, "ceil") == 0) {
			if (opt.type != CEETM_Q_ROOT) {
				if (opt.type)
					explain1(opt.type);
				else
					fprintf(stderr, "Error:"
							"Mention the "
							"type of qdisc after "
							"ceetm.\n");
				return -1;
			}
			NEXT_ARG();
			if (opt.ceil) {
				fprintf(stderr, "Double \"ceil\" spec\n");
				return -1;
			}
			if (get_rate(&opt.ceil, *argv)) {
				if (opt.type)
					explain1(opt.type);
				else
					fprintf(stderr, "Error:"
							"Mention the "
							"type of qdisc after "
							"ceetm.\n");
				return -1;
			}
		} else if (strcmp(*argv, "queues") == 0) {
			if (opt.type != CEETM_Q_WBFS) {
				if (opt.type)
					explain1(opt.type);
				else
					fprintf(stderr, "Error:"
							"Mention the "
							"type of qdisc after "
							"ceetm.\n");
				return -1;
			}
			if (opt.queues) {
				fprintf(stderr, "Double \"queues\" spec\n");
				return -1;
			}
			NEXT_ARG();
			if (get_u16(&queue, *argv, 0)) {
				explain();
				return -1;
			}
			if (queue != (__u8)4 && queue != (__u8)8) {
				if (opt.type)
					explain1(opt.type);
				else
					fprintf(stderr, "Error:"
							"Mention the "
							"type of qdisc after "
							"ceetm.\n");
				return -1;
			}
			opt.queues = queue;
			ceetm_queue_mode = 1;
		} else if (strcmp(*argv, "cr_map") == 0) {
			if (ceetm_cr_mode) {
				fprintf(stderr, "Error: duplicate cr_map\n");
				return -1;
			}
			if (opt.type == CEETM_Q_ROOT) {
				if (opt.type)
					explain1(opt.type);
				else
					fprintf(stderr, "Error:"
							"Mention the "
							"type of qdisc after "
							"ceetm.\n");
				return -1;
			}
			ceetm_queue_mode = 0;
			ceetm_er_mode = 0;
			ceetm_cr_mode = 1;
			cr = 1;
		} else if (strcmp(*argv, "er_map") == 0) {
			if (ceetm_er_mode) {
				fprintf(stderr, "Error: duplicate er_map\n");
				return -1;
			}
			if (opt.type == CEETM_Q_ROOT) {
				if (opt.type)
					explain1(opt.type);
				else
					fprintf(stderr, "Error:"
							"Mention the "
							"type of qdisc after "
							"ceetm.\n");
				return -1;
			}
			ceetm_queue_mode = 0;
			ceetm_cr_mode = 0;
			ceetm_er_mode = 1;
			er = 1;
		} else {
			if (ceetm_queue_mode) {
				__u32 num;
				if (opt.type == CEETM_Q_WBFS && !opt.queues) {
					if (opt.type)
						explain1(opt.type);
					else
						fprintf(stderr, "Error:"
							"Mention"
							 "the "
							"type of"
							"qdisc "
							"after "
							"ceetm."
							"\n"
							);
					return -1;
				}
				if (count_q >= opt.queues) {
					if (opt.type)
						explain1(opt.type);
					else
						fprintf(stderr, "Error:"
							"Mention"
							"the type of "
							"qdisc after "
							"ceetm.\n"
							);
					return -1;
				}
				if (get_u32(&num, *argv, 0)) {
					if (opt.type)
						explain1(opt.type);
					else
						fprintf(stderr, "Error:"
							"Mention"
							"the type of "
							"qdisc after"
							"ceetm.\n"
							);
					return -1;
				}
				if (num < 1 || num > 248) {
					fprintf(stderr, "Error:\n"
						"weight should "
						"be in the range "
						"[1, 248]\n"
						);
					return -1;
				}
				opt.weight[count_q++] = num;
				if (count_q >= opt.queues)
					ceetm_queue_mode = 0;
			} else if (ceetm_cr_mode) {
				__u8 num;
				if (get_u8(&num, *argv, 0)) {
					if (opt.type)
						explain1(opt.type);
					else
						fprintf(stderr, "Error:"
							"Mention"
							"the type of "
							"qdisc after"
							"ceetm.\n"
							);
					return -1;
				}
				if (num != (__u8)1 && num != (__u8)0) {
					fprintf(stderr, "Error:"
						"cr_map could "
						"have values either 1 "
						"or 0.\n"
						);
					return -1;
				}
				opt.cr_map[id_cr++] = num;
				if (opt.type == CEETM_Q_PRIO && id_cr >= 8 ||
						opt.type == CEETM_Q_WBFS && id_cr
						>= 1)
					ceetm_cr_mode = 0;
			} else if (ceetm_er_mode) {
				__u8 num;
				if (get_u8(&num, *argv, 0)) {
					if (opt.type)
						explain1(opt.type);
					else
						fprintf(stderr, "Error:"
							"Mention"
							"the type of "
							"qdisc after"
							"ceetm.\n"
							);
					return -1;
				}
				if (num != (__u8)1 && num != (__u8)0) {
					fprintf(stderr, "Error:"
						"er_map could "
						"have values either 1 "
						"or 0.\n"
						);
					return -1;
				}
				opt.er_map[id_er++] = num;
				if (opt.type == CEETM_Q_PRIO && id_er >= 8 ||
						opt.type == CEETM_Q_WBFS && id_er
						>= 1)
					ceetm_er_mode = 0;
			} else {
				explain();
				return -1;
			}
		}
		argc--; argv++;
	}
	if (opt.type == CEETM_Q_ROOT) {
		if (opt.rate) {
			if (!opt.overhead || !opt.mpu) {
				explain1(1);
				return -1;
			}
		} else {
			if (opt.overhead || opt.mpu || opt.ceil) {
				explain1(1);
				return -1;
			}
		}

	}
	if (opt.type == CEETM_Q_PRIO) {
		if (ceetm_cr_mode || ceetm_er_mode) {
			explain1(2);
			return -1;
		}
		if (cr) {
			if (id_cr < 8) {
				explain1(2);
				return -1;
			}
		}
		if (er) {
			if (id_er < 8) {
				explain1(2);
				return -1;
			}
		}
	}
	if (opt.type == CEETM_Q_WBFS) {
		if (ceetm_cr_mode || ceetm_er_mode) {
			explain1(3);
			return -1;
		}
		if (count_q != opt.queues) {
			explain1(3);
			return -1;
		}
		if (cr) {
			if (id_cr > 1) {
				explain1(3);
				return -1;
			}
		}
		if (er) {
			if (id_er > 1) {
				explain1(3);
				return -1;
			}
		}
	}
	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 1024, TCA_CEETM_INIT, &opt, sizeof(opt));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;

	return 0;
}

static int ceetm_parse_class_opt(struct qdisc_util *qu, int argc, char **argv,
		struct nlmsghdr *n)
{
	struct tc_ceetm_copt opt;
	struct rtattr *tail;
	__u32 weight = 0;
	int ceetm_weight_mode = 0;
	memset(&opt, 0, sizeof(opt));

	while (argc > 0) {
		if (strcmp(*argv, "rate") == 0) {
			if (ceetm_weight_mode) {
				explain1(0);
				return -1;
			}
			NEXT_ARG();
			if (opt.rate) {
				fprintf(stderr, "Double \"rate\" spec\n");
				return -1;
			}
			if (get_rate(&opt.rate, *argv)) {
				explain1(0);
				return -1;
			}
		} else if (strcmp(*argv, "ceil") == 0) {
			if (ceetm_weight_mode) {
				explain1(0);
				return -1;
			}
			NEXT_ARG();
			if (opt.ceil) {
				fprintf(stderr, "Double \"ceil\" spec\n");
				return -1;
			}
			if (get_rate(&opt.ceil, *argv)) {
				explain1(0);
				return -1;
			}
		} else if (strcmp(*argv, "weight") == 0) {
			if (opt.weight) {
				fprintf(stderr, "Double \"weight\" spec\n");
				return -1;
			}
			NEXT_ARG();
			if (get_u32(&weight, *argv, 0)) {
				explain1(0);
				return -1;
			}
			if (weight < 0 && weight > 8) {
				fprintf(stderr, "Value of \"weight\" is in between [0, 8].\n");
				return -1;
			}
			opt.weight = weight * 1000;
			ceetm_weight_mode = 1;
		} else {
			explain();
			return -1;
		}
		argc--; argv++;
	}
	if (opt.weight) {
		if (opt.rate || opt.ceil) {
			explain1(0);
			return -1;
		}
	} else {
		if (!opt.rate) {
			explain1(0);
			return -1;
		}
	}
	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 2024, TCA_CEETM_COPT, &opt, sizeof(opt));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;

	return 0;
}

int ceetm_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_CEETM_MAX+1];
	struct tc_ceetm_qopt *qopt = NULL;
	struct tc_ceetm_copt *copt = NULL;

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_CEETM_MAX, opt);

	if (tb[TCA_CEETM_INIT]) {
		if (RTA_PAYLOAD(tb[TCA_CEETM_INIT]) < sizeof(*qopt))
			fprintf(stderr, "CEETM: too short opt\n");
		else
			qopt = RTA_DATA(tb[TCA_CEETM_INIT]);
	}

	if (tb[TCA_CEETM_COPT]) {
		if (RTA_PAYLOAD(tb[TCA_CEETM_COPT]) < sizeof(*copt))
			fprintf(stderr, "CEETM: too short opt\n");
		else
			copt = RTA_DATA(tb[TCA_CEETM_COPT]);
	}
	if (qopt) {
		char buf[64];
		int i;
		if (qopt->type == CEETM_Q_ROOT) {
			fprintf(f, "type root ");
			if (qopt->rate) {
				print_rate(buf, sizeof(buf), qopt->rate);
				fprintf(f, "rate %s ", buf);
			}
			if (qopt->ceil) {
				print_rate(buf, sizeof(buf), qopt->ceil);
				fprintf(f, "ceil %s ", buf);
			}
			if (qopt->mpu)
				fprintf(f, "mpu %u ", qopt->mpu);
			if (qopt->overhead)
				fprintf(f, "overhead %u ", qopt->overhead);
		} else if (qopt->type == CEETM_Q_PRIO) {
			fprintf(f, "type prio ");

			fprintf(f, "cr_map ");
			for (i = 0; i <= 7; i++)
				fprintf(f, "%u ", qopt->cr_map[i]);

			fprintf(f, "er_map ");
			for (i = 0; i <= 7; i++)
				fprintf(f, "%u ", qopt->er_map[i]);
		} else if (qopt->type == CEETM_Q_WBFS) {
			fprintf(f, "type wbfs ");

			fprintf(f, "queues %u ", qopt->queues);

			fprintf(f, "weight ");
			for (i = 0; i < (__u32)qopt->queues; i++)
				fprintf(f, "%u ", qopt->weight[i]);

			fprintf(f, " cr_map ");
			fprintf(f, "%u ", qopt->cr_map[0]);

			fprintf(f, " er_map ");
			fprintf(f, "%u ", qopt->er_map[0]);
		}
	}
	if (copt) {
		char buf[64];
		if (copt->rate) {
			print_rate(buf, sizeof(buf), copt->rate);
			fprintf(f, "rate %s ", buf);
		}
		if (copt->ceil) {
			print_rate(buf, sizeof(buf), copt->ceil);
			fprintf(f, "ceil %s ", buf);
		}
		if (copt->weight)
			fprintf(f, "weight %u ", copt->weight);
	}

	return 0;
}


static int ceetm_print_xstats(struct qdisc_util *qu, FILE *f, struct rtattr *xstats)
{
	struct tc_ceetm_xstats *st;

	if (xstats == NULL)
		return 0;

	if (RTA_PAYLOAD(xstats) < sizeof(*st))
		return -1;

	st = RTA_DATA(xstats);
	fprintf(f, "enqueue %lu drop %lu dequeue %lu dequeue_bytes %lu\n",
			st->enqueue, st->drop, st->dequeue, st->deq_bytes);
	return 0;
}

struct qdisc_util ceetm_qdisc_util = {
	.id	 	= "ceetm",
	.parse_qopt	= ceetm_parse_opt,
	.print_qopt	= ceetm_print_opt,
	.print_xstats	= ceetm_print_xstats,
	.parse_copt	= ceetm_parse_class_opt,
	.print_copt	= ceetm_print_opt,
};
