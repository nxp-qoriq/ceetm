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

#include "dpaa1_ceetm.h"
#include "dpaa2_ceetm.h"

/* DPAA SoC identifier.
 * If this is not available, assume the board is DPAA2.
 */
#define DPAA_SOC_ID_FILE	"/sys/devices/soc0/soc_id"

#define SVR_LS1043A_FAMILY	0x87920000
#define SVR_LS1046A_FAMILY	0x87070000
#define SVR_MASK		0xffff0000

enum dpaa_version {
	DPAA_1,
	DPAA_2,
};

static enum dpaa_version detect_dpaa_version(void)
{
	FILE *svr_file = fopen(DPAA_SOC_ID_FILE, "r");
	unsigned int svr_ver, dpaa_svr_family = 0;

	if (svr_file) {
		if (fscanf(svr_file, "svr:%x", &svr_ver) > 0)
			dpaa_svr_family = svr_ver & SVR_MASK;
		fclose(svr_file);
	}

	switch (dpaa_svr_family) {
	case SVR_LS1043A_FAMILY:
	case SVR_LS1046A_FAMILY:
		return DPAA_1;
	default:
		return DPAA_2;
	}
}

static int ceetm_parse_qopt(struct qdisc_util *qu, int argc, char **argv,
		struct nlmsghdr *n)
{
	enum dpaa_version ver = detect_dpaa_version();

	switch (ver) {
	case DPAA_1:
		return dpaa1_ceetm_parse_qopt(qu, argc, argv, n);
	case DPAA_2:
	default:
		return dpaa2_ceetm_parse_qopt(qu, argc, argv, n);
	}
}

static int ceetm_print_qopt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	enum dpaa_version ver = detect_dpaa_version();

	switch (ver) {
	case DPAA_1:
		return dpaa1_ceetm_print_qopt(qu, f, opt);
	case DPAA_2:
	default:
		return dpaa2_ceetm_print_qopt(qu, f, opt);
	}
}

static int ceetm_parse_copt(struct qdisc_util *qu, int argc, char **argv,
		struct nlmsghdr *n)
{
	enum dpaa_version ver = detect_dpaa_version();

	switch (ver) {
	case DPAA_1:
		return dpaa1_ceetm_parse_copt(qu, argc, argv, n);
	case DPAA_2:
	default:
		return dpaa2_ceetm_parse_copt(qu, argc, argv, n);
	}
}

static int ceetm_print_copt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	enum dpaa_version ver = detect_dpaa_version();

	switch (ver) {
	case DPAA_1:
		return dpaa1_ceetm_print_copt(qu, f, opt);
	case DPAA_2:
	default:
		return dpaa2_ceetm_print_copt(qu, f, opt);
	}
}

static int ceetm_print_xstats(struct qdisc_util *qu, FILE *f, struct rtattr *xstats)
{
	enum dpaa_version ver = detect_dpaa_version();

	switch (ver) {
	case DPAA_1:
		return dpaa1_ceetm_print_xstats(qu, f, xstats);
	case DPAA_2:
	default:
		return dpaa2_ceetm_print_xstats(qu, f, xstats);
	}
}

struct qdisc_util ceetm_qdisc_util = {
	.id	 	= "ceetm",
	.parse_qopt	= ceetm_parse_qopt,
	.print_qopt	= ceetm_print_qopt,
	.parse_copt	= ceetm_parse_copt,
	.print_copt	= ceetm_print_copt,
	.print_xstats	= ceetm_print_xstats,
};
