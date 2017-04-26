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
	CEETM_ROOT = 1,
	CEETM_PRIO,
	CEETM_WBFS
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

