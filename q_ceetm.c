/* Copyright 2014-2017 Freescale Semiconductor Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the above-listed copyright holders nor the
 * names of any contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>

#include "q_ceetm.h"

static void explain()
{
	fprintf(stderr, "Usage:\n"
		"... qdisc add ... ceetm type root [rate R [ceil C] [overhead O]]\n"
		"... class add ... ceetm type root (tbl T | rate R [ceil C])\n"
		"... qdisc add ... ceetm type prio qcount Q\n"
		"... qdisc add ... ceetm type wbfs qcount Q qweight W1 ... Wn "
		"[cr CR] [er ER]\n"
		"\n"
		"Update configurations:\n"
		"... qdisc change ... ceetm type root [rate R [ceil C] [overhead O]]\n"
		"... class change ... ceetm type root (tbl T | rate R [ceil C])\n"
		"... class change ... ceetm type prio [cr CR] [er ER]\n"
		"... qdisc change ... ceetm type wbfs [cr CR] [er ER]\n"
		"... class change ... ceetm type wbfs qweight W\n"
		"\n"
		"Qdisc types:\n"
		"root - configure a LNI linked to a FMan port\n"
		"prio - configure a channel's Priority Scheduler with up to "
		"eight classes\n"
		"wbfs - configure a Weighted Bandwidth Fair Scheduler with "
		"four or eight classes\n"
		"\n"
		"Class types:\n"
		"root - configure a shaped or unshaped channel\n"
		"prio - configure an independent class queue\n"
		"\n"
		"Options:\n"
		"R - the CR of the LNI's or channel's dual-rate shaper "
		"(required for shaping scenarios)\n"
		"C - the ER of the LNI's or channel's dual-rate shaper "
		"(optional for shaping scenarios, defaults to 0)\n"
		"O - per-packet size overhead used in rate computations "
		"(required for shaping scenarios, recommended value is 24 "
		"i.e. 12 bytes IFG + 8 bytes Preamble + 4 bytes FCS)\n"
		"T - the token bucket limit of an unshaped channel used as "
		"fair queuing weight (required for unshaped channels)\n"
		"CR/ER - boolean marking if the class group or prio class "
		"queue contributes to CR/ER shaping (1) or not (0) (optional, "
		"at least one needs to be enabled for shaping scenarios, both "
		"default to 1 for prio class queues)\n"
		"Q - the number of class queues connected to the channel "
		"(from 1 to 8) or in a class group (either 4 or 8)\n"
		"W - the weights of each class in the class group measured "
		"in a log scale with values from 1 to 248 (when adding a wbfs "
		"qdisc, either four or eight, depending on the size of the "
		"class group; when updating a wbfs class, only one)\n"
		);
}

static int ceetm_parse_qopt(struct qdisc_util *qu, int argc, char **argv,
		struct nlmsghdr *n)
{
	struct tc_ceetm_qopt opt;
	bool overhead_set = false;
	bool rate_set = false;
	bool ceil_set = false;
	bool cr_set = false;
	bool er_set = false;
	bool qweight_set = false;
	struct rtattr *tail;
	int i;
	memset(&opt, 0, sizeof(opt));

	while (argc > 0) {
		if (strcmp(*argv, "type") == 0) {
			if (opt.type) {
				fprintf(stderr, "type already specified.\n");
				return -1;
			}

			NEXT_ARG();

			if (matches(*argv, "root") == 0)
				opt.type = CEETM_ROOT;

			else if (matches(*argv, "prio") == 0)
				opt.type = CEETM_PRIO;

			else if (matches(*argv, "wbfs") == 0)
				opt.type = CEETM_WBFS;

			else {
				fprintf(stderr, "Illegal type argument.\n");
				return -1;
			}

		} else if (strcmp(*argv, "qcount") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc "
						"type before the qcount.\n");
				return -1;

			} else if (opt.type == CEETM_ROOT) {
				fprintf(stderr, "qcount belongs to prio and "
						"wbfs qdiscs only.\n");
				return -1;
			}

			if (opt.qcount) {
				fprintf(stderr, "qcount already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u16(&opt.qcount, *argv, 10) || opt.qcount == 0) {
				fprintf(stderr, "Illegal qcount argument.\n");
				return -1;
			}

			if (opt.type == CEETM_PRIO &&
					opt.qcount > CEETM_MAX_PRIO_QCOUNT) {
				fprintf(stderr, "qcount must be between 1 and "
					"%d for prio qdiscs\n",
					CEETM_MAX_PRIO_QCOUNT);
				return -1;
			}

			if (opt.type == CEETM_WBFS &&
					opt.qcount != CEETM_MIN_WBFS_QCOUNT &&
					opt.qcount != CEETM_MAX_WBFS_QCOUNT) {
				fprintf(stderr, "qcount must be either %d or "
					"%d for wbfs qdiscs\n",
					CEETM_MIN_WBFS_QCOUNT,
					CEETM_MAX_WBFS_QCOUNT);
				return -1;
			}

		} else if (strcmp(*argv, "rate") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
							"before the rate.\n");
				return -1;

			} else if (opt.type != CEETM_ROOT) {
				fprintf(stderr, "rate belongs to root qdiscs "
						"only.\n");
				return -1;
			}

			if (rate_set) {
				fprintf(stderr, "rate already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_rate(&opt.rate, *argv)) {
				fprintf(stderr, "Illegal rate argument.\n");
				return -1;
			}

			rate_set = true;

		} else if (strcmp(*argv, "ceil") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
							"before the ceil.\n");
				return -1;

			} else if (opt.type != CEETM_ROOT) {
				fprintf(stderr, "ceil belongs to root qdiscs "
						"only.\n");
				return -1;
			}

			if (ceil_set) {
				fprintf(stderr, "ceil already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_rate(&opt.ceil, *argv)) {
				fprintf(stderr, "Illegal ceil argument.\n");
				return -1;
			}

			ceil_set = true;

		} else if (strcmp(*argv, "overhead") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
							"before the overhead.\n");
				return -1;

			} else if (opt.type != CEETM_ROOT) {
				fprintf(stderr, "overhead belongs to root "
						"qdiscs only.\n");
				return -1;
			}

			if (overhead_set) {
				fprintf(stderr, "overhead already specified\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u16(&opt.overhead, *argv, 10)) {
				fprintf(stderr, "Illegal overhead argument\n");
				return -1;
			}

			overhead_set = true;

		} else if (strcmp(*argv, "cr") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
						"before the cr.\n");
				return -1;

			} else if (opt.type != CEETM_WBFS) {
				fprintf(stderr, "cr belongs to wbfs qdiscs "
						"only.\n");
				return -1;
			}

			if (cr_set) {
				fprintf(stderr, "cr already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u16(&opt.cr, *argv, 10) || opt.cr > 1) {
				fprintf(stderr, "Illegal cr argument.\n");
				return -1;
			}

			cr_set = true;

		} else if (strcmp(*argv, "er") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
						"before the er.\n");
				return -1;

			} else if (opt.type != CEETM_WBFS) {
				fprintf(stderr, "er belongs to wbfs qdiscs "
						"only.\n");
				return -1;
			}

			if (er_set) {
				fprintf(stderr, "er already specified\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u16(&opt.er, *argv, 10) || opt.er > 1) {
				fprintf(stderr, "Illegal er argument\n");
				return -1;
			}

			er_set = true;

		} else if (strcmp(*argv, "qweight") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the qdisc type "
						"before the qweight.\n");
				return -1;

			} else if (opt.type != CEETM_WBFS) {
				fprintf(stderr, "qweight belongs to wbfs "
						"qdiscs only.\n");
				return -1;
			}

			if (!opt.qcount) {
				fprintf(stderr, "Please specify the qcount "
						"before the qweight.\n");
				return -1;
			}

			if (qweight_set) {
				fprintf(stderr, "qweight already specified.\n");
				return -1;
			}

			for (i = 0; i < opt.qcount; i++) {
				NEXT_ARG();
				if (get_u8(&opt.qweight[i], *argv, 10) ||
					opt.qweight[i] == 0 ||
					opt.qweight[i] > CEETM_MAX_WBFS_VALUE) {
					fprintf(stderr, "Illegal qweight "
							"argument: must be "
							"between 1 and 248.\n");
					return -1;
				}
			}

			qweight_set = true;

		} else if (strcmp(*argv, "help") == 0) {
			explain();
			return -1;

		} else {
			fprintf(stderr, "Illegal argument.\n");
			explain();
			return -1;
		}

		argc--; argv++;
	}

	if (!opt.type) {
		fprintf(stderr, "Please specify the qdisc type.\n");
		return -1;
	}

	if (opt.type == CEETM_ROOT && (ceil_set || overhead_set) && !rate_set) {
		fprintf(stderr, "rate is mandatory for a shaped root qdisc.\n");
		return -1;
	}

	if (opt.type == CEETM_PRIO && !opt.qcount) {
		fprintf(stderr, "qcount is mandatory for a prio qdisc.\n");
		return -1;
	}

	if (opt.type == CEETM_ROOT && rate_set)
		opt.shaped = 1;
	else
		opt.shaped = 0;

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 1024, TCA_CEETM_QOPS, &opt, sizeof(opt));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;

	return 0;
}

static int ceetm_parse_copt(struct qdisc_util *qu, int argc, char **argv,
		struct nlmsghdr *n)
{
	struct tc_ceetm_copt opt;
	struct rtattr *tail;
	memset(&opt, 0, sizeof(opt));
	bool tbl_set = false;
	bool rate_set = false;
	bool ceil_set = false;
	bool cr_set = false;
	bool er_set = false;

	while (argc > 0) {
		if (strcmp(*argv, "type") == 0) {
			if (opt.type) {
				fprintf(stderr, "type already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (matches(*argv, "root") == 0) {
				opt.type = CEETM_ROOT;

			} else if (matches(*argv, "prio") == 0) {
				opt.type = CEETM_PRIO;

			} else if (matches(*argv, "wbfs") == 0) {
				opt.type = CEETM_WBFS;

			} else {
				fprintf(stderr, "Illegal type argument.\n");
				return -1;
			}

		} else if (strcmp(*argv, "rate") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the class "
						"type before the rate.\n");
				return -1;

			} else if (opt.type != CEETM_ROOT) {
				fprintf(stderr, "rate belongs to root classes "
						"only.\n");
				return -1;
			}

			if (rate_set) {
				fprintf(stderr, "rate already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_rate(&opt.rate, *argv)) {
				fprintf(stderr, "Illegal rate argument.\n");
				return -1;
			}

			rate_set = true;

		} else if (strcmp(*argv, "ceil") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the class type "
						"before the ceil.\n");
				return -1;

			} else if (opt.type != CEETM_ROOT) {
				fprintf(stderr, "ceil belongs to root classes "
						"only.\n");
				return -1;
			}

			if (ceil_set) {
				fprintf(stderr, "ceil already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_rate(&opt.ceil, *argv)) {
				fprintf(stderr, "Illegal ceil argument.\n");
				return -1;
			}

			ceil_set = true;

		} else if (strcmp(*argv, "tbl") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the class type "
						"before the tbl.\n");
				return -1;

			} else if (opt.type != CEETM_ROOT) {
				fprintf(stderr, "tbl belongs to root classes "
						"only.\n");
				return -1;
			}

			if (tbl_set) {
				fprintf(stderr, "tbl already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u16(&opt.tbl, *argv, 10)) {
				fprintf(stderr, "Illegal tbl argument.\n");
				return -1;
			}

			tbl_set = true;

		} else if (strcmp(*argv, "cr") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the class type "
						"before the cr.\n");
				return -1;

			} else if (opt.type != CEETM_PRIO) {
				fprintf(stderr, "cr belongs to prio classes "
						"only.\n");
				return -1;
			}

			if (cr_set) {
				fprintf(stderr, "cr already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u16(&opt.cr, *argv, 10) || opt.cr > 1) {
				fprintf(stderr, "Illegal cr argument.\n");
				return -1;
			}

			cr_set = true;

		} else if (strcmp(*argv, "er") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the class type "
						"before the er.\n");
				return -1;

			} else if (opt.type != CEETM_PRIO) {
				fprintf(stderr, "er belongs to prio classes "
						"only.\n");
				return -1;
			}

			if (er_set) {
				fprintf(stderr, "er already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u16(&opt.er, *argv, 10) || opt.er > 1) {
				fprintf(stderr, "Illegal er argument.\n");
				return -1;
			}

			er_set = true;

		} else if (strcmp(*argv, "qweight") == 0) {
			if (!opt.type) {
				fprintf(stderr, "Please specify the class type "
						"before the qweight.\n");
				return -1;

			} else if (opt.type != CEETM_WBFS) {
				fprintf(stderr, "qweight belongs to wbfs "
						"classes only.\n");
				return -1;
			}

			if (opt.weight) {
				fprintf(stderr, "qweight already specified.\n");
				return -1;
			}

			NEXT_ARG();
			if (get_u8(&opt.weight, *argv, 10) ||
				opt.weight == 0 ||
				opt.weight > CEETM_MAX_WBFS_VALUE) {
				fprintf(stderr, "Illegal qweight "
						"argument: must be "
						"between 1 and 248.\n");
				return -1;
			}

		} else {
			fprintf(stderr, "Illegal argument.\n");
			return -1;
		}

		argc--; argv++;
	}

	if (!opt.type) {
		fprintf(stderr, "Please specify the class type.\n");
		return -1;
	}

	if (opt.type == CEETM_ROOT && !tbl_set && !rate_set) {
		fprintf(stderr, "Either tbl or rate must be specified for "
				"root classes.\n");
		return -1;
	}

	if (opt.type == CEETM_ROOT && tbl_set && rate_set) {
		fprintf(stderr, "Both tbl and rate can not be used for root "
				"classes.\n");
		return -1;
	}

	if (opt.type == CEETM_ROOT && ceil_set && !rate_set) {
		fprintf(stderr, "rate is mandatory for shaped root classes.\n");
		return -1;
	}

	if (opt.type == CEETM_PRIO && !(cr_set && er_set)) {
		fprintf(stderr, "Both cr and er are mandatory when altering a "
				"prio class.\n");
		return -1;
	}

	if (opt.type == CEETM_PRIO && !opt.cr && !opt.er) {
		fprintf(stderr, "Either cr or er must be enabled for a shaped "
				"prio class.\n");
		return -1;
	}

	if (opt.type == CEETM_WBFS && !opt.weight) {
		fprintf(stderr, "qweight is mandatory for wbfs classes.\n");
		return -1;
	}

	if (rate_set)
		opt.shaped = 1;
	else
		opt.shaped = 0;

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, TCA_OPTIONS, NULL, 0);
	addattr_l(n, 2024, TCA_CEETM_COPT, &opt, sizeof(opt));
	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *) tail;

	return 0;
}

int ceetm_print_qopt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_CEETM_MAX + 1];
	struct tc_ceetm_qopt *qopt = NULL;
	char buf[64];

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_CEETM_MAX, opt);

	if (tb[TCA_CEETM_QOPS]) {
		if (RTA_PAYLOAD(tb[TCA_CEETM_QOPS]) < sizeof(*qopt))
			fprintf(stderr, "CEETM: too short opt\n");
		else
			qopt = RTA_DATA(tb[TCA_CEETM_QOPS]);
	}

	if (!qopt)
		return 0;

	if (qopt->type == CEETM_ROOT) {
		fprintf(f, "type root");

		if (qopt->shaped) {
			print_rate(buf, sizeof(buf), qopt->rate);
			fprintf(f, " shaped rate %s ", buf);

			print_rate(buf, sizeof(buf), qopt->ceil);
			fprintf(f, "ceil %s ", buf);

			fprintf(f, "overhead %u ", qopt->overhead);

		} else {
			fprintf(f, " unshaped");
		}

	} else if (qopt->type == CEETM_PRIO) {
		fprintf(f, "type prio %s qcount %u ",
				qopt->shaped ? "shaped" : "unshaped",
				qopt->qcount);

	} else if (qopt->type == CEETM_WBFS) {
		fprintf(f, "type wbfs ");

		if (qopt->shaped) {
			fprintf(f, "shaped cr %d er %d ", qopt->cr, qopt->er);
		} else {
			fprintf(f, "unshaped ");
		}

		fprintf(f, "qcount %u", qopt->qcount);
	}

	return 0;
}

int ceetm_print_copt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_CEETM_MAX + 1];
	struct tc_ceetm_copt *copt = NULL;
	char buf[64];

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_CEETM_MAX, opt);

	if (tb[TCA_CEETM_COPT]) {
		if (RTA_PAYLOAD(tb[TCA_CEETM_COPT]) < sizeof(*copt))
			fprintf(stderr, "CEETM: too short opt\n");
		else
			copt = RTA_DATA(tb[TCA_CEETM_COPT]);
	}

	if (!copt)
		return 0;

	if (copt->type == CEETM_ROOT) {
		fprintf(f, "type root ");

		if (copt->shaped) {
			print_rate(buf, sizeof(buf), copt->rate);
			fprintf(f, "shaped rate %s ", buf);

			print_rate(buf, sizeof(buf), copt->ceil);
			fprintf(f, "ceil %s ", buf);

		} else {
			fprintf(f, "unshaped tbl %d", copt->tbl);
		}

	} else if (copt->type == CEETM_PRIO) {
		fprintf(f, "type prio ");

		if (copt->shaped) {
			fprintf(f, "shaped cr %d er %d", copt->cr, copt->er);
		} else {
			fprintf(f, "unshaped");
		}

	} else if (copt->type == CEETM_WBFS) {
		fprintf(f, "type wbfs qweight %d", copt->weight);
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
	fprintf(f, "ern drops %llu congested %llu frames %llu bytes %llu\n",
			st->ern_drop_count, st->cgr_congested_count,
			st->frame_count, st->byte_count);
	return 0;
}

struct qdisc_util ceetm_qdisc_util = {
	.id	 	= "ceetm",
	.parse_qopt	= ceetm_parse_qopt,
	.print_qopt	= ceetm_print_qopt,
	.parse_copt	= ceetm_parse_copt,
	.print_copt	= ceetm_print_copt,
	.print_xstats	= ceetm_print_xstats,
};
