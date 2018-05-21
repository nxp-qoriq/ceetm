/* Copyright 2014-2016 Freescale Semiconductor Inc.
 * Copyright 2017-2018 NXP
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

#include "include/utils.h"
#include "tc/tc_util.h"

/* Maximum number of CEETM CQs that can be linked to a channel (prio qdisc) */
#define CEETM_MAX_PRIO_QCOUNT	8
#define CEETM_MAX_WBFS_QCOUNT	8
#define CEETM_MIN_WBFS_QCOUNT	4
#define CEETM_MAX_WBFS_VALUE	248

#define DPAA2_CEETM_MIN_WEIGHT	100
#define DPAA2_CEETM_MAX_WEIGHT	24800

enum {
	DPAA2_CEETM_TCA_UNSPEC,
	DPAA2_CEETM_TCA_COPT,
	DPAA2_CEETM_TCA_QOPS,
	DPAA2_CEETM_TCA_MAX,
};

/* CEETM configuration types */
enum dpaa2_ceetm_type {
	CEETM_ROOT = 1,
	CEETM_PRIO,
};

enum {
	STRICT_PRIORITY = 0,
	WEIGHTED_A,
	WEIGHTED_B,
};

struct dpaa2_ceetm_shaping_cfg {
	__u64 cir; /* committed information rate */
	__u64 eir; /* excess information rate */
	__u16 cbs; /* committed burst size */
	__u16 ebs; /* excess burst size */
	__u8 coupled; /* shaper coupling */
};

/* CEETM Qdisc configuration parameters */
struct dpaa2_ceetm_tc_qopt {
	enum dpaa2_ceetm_type type;
	__u16 shaped;
	__u8 prio_group_A;
	__u8 prio_group_B;
	__u8 separate_groups;
};

/* CEETM Class configuration parameters */
struct dpaa2_ceetm_tc_copt {
	enum dpaa2_ceetm_type type;
	struct dpaa2_ceetm_shaping_cfg shaping_cfg;
	__u16 shaped;
	__u8 mode;
	__u16 weight;
};

/* CEETM stats */
struct dpaa2_ceetm_tc_xstats {
	__u64 ceetm_dequeue_bytes;
	__u64 ceetm_dequeue_frames;
	__u64 ceetm_reject_bytes;
	__u64 ceetm_reject_frames;
};

int dpaa2_ceetm_parse_qopt(struct qdisc_util *qu, int argc, char **argv,
 			  struct nlmsghdr *n);
int dpaa2_ceetm_print_qopt(struct qdisc_util *qu, FILE *f,
 			  struct rtattr *opt);
int dpaa2_ceetm_parse_copt(struct qdisc_util *qu, int argc, char **argv,
 			  struct nlmsghdr *n);
int dpaa2_ceetm_print_copt(struct qdisc_util *qu, FILE *f,
 			  struct rtattr *opt);
int dpaa2_ceetm_print_xstats(struct qdisc_util *qu, FILE *f,
				    struct rtattr *xstats);

