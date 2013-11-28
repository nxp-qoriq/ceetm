#/**************************************************************************
# * Copyright 2013, Freescale Semiconductor, Inc. All rights reserved.
# ***************************************************************************/
#/*
# * File:       Makefile
# *
# */

CC := $(CROSS_COMPILE)gcc

ARCH = powerpc

ifeq ($(ARCH), x86_64)
CFLAGS = -DX86_64 -D__LP64__
else
CFLAGS =
endif

SHARED_LIBS ?= y
INCLUDE := -I../include/

TCSO :=
TCSO += q_ceetm.so

LDLIBS += -L. -ltc -lm

ifeq ($(SHARED_LIBS),y)
LDLIBS += -ldl
LDFLAGS += -Wl,-export-dynamic
endif

CFLAGS += -DCONFIG_GACT -DCONFIG_GACT_PROB

all: $(TCSO)

clean:
	rm -f *.o *.so;

q_ceetm.so: q_ceetm.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDE) -shared -fpic -o q_ceetm.so q_ceetm.c
