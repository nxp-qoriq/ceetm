CC := $(CROSS_COMPILE)gcc

CFLAGS := -Wall -Wstrict-prototypes -Wmissing-prototypes \
	  -Wmissing-declarations -Wold-style-definition -Wformat=2
LDFLAGS += -Wl,-export-dynamic

# Point to the iproute2 headers that need to be included. Modify this path
# if you are not using flex-builder. Download the iproute2 sources for the
# desired version and point to those instead.
ifneq ($(IPROUTE2_DIR),)
CFLAGS += -I$(IPROUTE2_DIR)
endif

MODDESTDIR := $(DESTDIR)/usr/lib/tc

all: q_ceetm.so

q_ceetm.so: q_ceetm.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -fpic -o q_ceetm.so q_ceetm.c

install:
	install -d $(MODDESTDIR)
	install -m 755 q_ceetm.so $(MODDESTDIR)

.PHONY: clean
clean:
	rm -f *.o *.so

