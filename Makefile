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
# */

TOPDIR := $(shell pwd)
export TOPDIR

#------------------------------------------------------------------------------
#  Include Definitions
#------------------------------------------------------------------------------
.PHONY: all
.NOTPARALLEL :
all: lib module

lib :
	make -w -C usr/lib -f Makefile
module :
	make -w -C ceetm_module -f Makefile

#--------------------------------------------------------------
.PHONY: clean
clean:
	make -w -C usr/lib -f Makefile clean
	make -w -C ceetm_module -f Makefile clean
