#/**************************************************************************
# * Copyright 2013, Freescale Semiconductor, Inc. All rights reserved.
# ***************************************************************************/
#/*
# * File:	Makefile
# *
# * Authors:	Sachin Saxena <sachin.saxena@freescale.com>
# * History
# *  Version     Date		Author			Change Description *
# *  1.0	15-10-2013	Sachin Saxena		Initial Code
# *  2.0	15-03-2016	Camelia Groza		Remove the kernel module
# */

TOPDIR := $(shell pwd)
export TOPDIR
#------------------------------------------------------------------------------
#  Include Definitions
#------------------------------------------------------------------------------
.PHONY: all
.NOTPARALLEL:
all:
	make -w -C lib -f Makefile
	mkdir -p bin
	cp lib/q_ceetm.so bin/.
#--------------------------------------------------------------
.PHONY: clean
clean:
	make -w -C lib -f Makefile clean
	rm -rf bin
