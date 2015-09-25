#/**************************************************************************
# * Copyright 2013, Freescale Semiconductor, Inc. All rights reserved.
# ***************************************************************************/
#/*
# * File:	Makefile
# *
# */

PWD := $(shell pwd)
include $(KBUILD_OUTPUT)/.config

CC	= $(CROSS_COMPILE)gcc
AR	= $(CROSS_COMPILE)ar
LD	= $(CROSS_COMPILE)ld
EXTRA_CFLAGS := -mno-spe -mspe=no -mabi=no-spe

ifneq ($(CONFIG_FSL_DPA),y)
 fatal := $(error DPAA support is missing in Kernel. Try to fix)
endif

include $(KERNEL_PATH)/drivers/net/ethernet/freescale/fman/ncsw_config.mk

EXTRA_CFLAGS += -I$(KERNEL_PATH)/drivers/net/ethernet/freescale/fman/src/wrapper/
EXTRA_CFLAGS += -I$(KERNEL_PATH)/drivers/net/ethernet/freescale/dpa
EXTRA_CFLAGS += -I$(KERNEL_PATH)/drivers/net/ethernet/freescale/
EXTRA_CFLAGS += -I$(KERNEL_PATH)/drivers/net/bonding
EXTRA_CFLAGS += -I$(KERNEL_PATH)/include
EXTRA_CFLAGS += -I$(KERNEL_PATH)/net/sched/

# Enable following CFLAG for debug prints
#EXTRA_CFLAGS += -DCEETM_DEBUG=1
#EXTRA_CFLAGS += -DCEETM_SCH_DEBUG=1

obj-m += ceetm.o
ceetm-y := sch_ceetm.o ceetm_qman.o

all:
	make ARCH=powerpc -C $(KERNEL_PATH) M=$(PWD) modules

clean:
	make ARCH=powerpc -C $(KERNEL_PATH) M=$(PWD) clean
