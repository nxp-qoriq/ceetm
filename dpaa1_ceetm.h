/* Copyright 2014-2016 Freescale Semiconductor Inc.
 * Copyright 2017-2018 NXP
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include "include/utils.h"
#include "tc/tc_util.h"

/* Maximum number of CEETM CQs that can be linked to a channel (prio qdisc) */
#define CEETM_MAX_PRIO_QCOUNT	8
#define CEETM_MAX_WBFS_QCOUNT	8
#define CEETM_MIN_WBFS_QCOUNT	4
#define CEETM_MAX_WBFS_VALUE	248

enum {
	TCA_CEETM_UNSPEC,
	TCA_CEETM_COPT,
	TCA_CEETM_QOPS,
	__TCA_CEETM_MAX,
};

#define TCA_CEETM_MAX (__TCA_CEETM_MAX - 1)

/* CEETM configuration types */
enum {
	DPAA1_CEETM_ROOT = 1,
	DPAA1_CEETM_PRIO,
	DPAA1_CEETM_WBFS
};

/* CEETM Qdisc configuration parameters */
struct tc_ceetm_qopt {
	__u32 type;
	__u16 shaped;
	__u16 qcount;
	__u16 overhead;
	__u32 rate;
	__u32 ceil;
	__u16 cr;
	__u16 er;
	__u8 qweight[CEETM_MAX_WBFS_QCOUNT];
};

/* CEETM Class configuration parameters */
struct tc_ceetm_copt {
	__u32 type;
	__u16 shaped;
	__u32 rate;
	__u32 ceil;
	__u16 tbl;
	__u16 cr;
	__u16 er;
	__u8 weight;
};

/* CEETM stats */
struct tc_ceetm_xstats {
	__u32 ern_drop_count;
	__u32 cgr_congested_count;
	__u64 frame_count;
	__u64 byte_count;
};

int dpaa1_ceetm_parse_qopt(struct qdisc_util *qu, int argc, char **argv,
 			  struct nlmsghdr *n);
int dpaa1_ceetm_print_qopt(struct qdisc_util *qu, FILE *f,
 			  struct rtattr *opt);
int dpaa1_ceetm_parse_copt(struct qdisc_util *qu, int argc, char **argv,
 			  struct nlmsghdr *n);
int dpaa1_ceetm_print_copt(struct qdisc_util *qu, FILE *f,
 			  struct rtattr *opt);
int dpaa1_ceetm_print_xstats(struct qdisc_util *qu, FILE *f,
				    struct rtattr *xstats);

