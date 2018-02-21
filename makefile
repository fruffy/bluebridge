MAKE_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
MSG_DIR    := $(MAKE_ROOT)/ip6/

CC = gcc
CFLAGS += -c -Wextra -Wall -Wall -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS += -std=gnu11 -pedantic
CFLAGS += -oFast # performance flags
# CFLAGS += -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS := -lpcap -pthread -lm -lrt

apps := testing server event_server bb_disk
#rmem_test

export apps CC CFLAGS LDFLAGS
all:
		@echo "Running default build in $(MAKE_ROOT)"
		@find $(MAKE_ROOT)/ip6/ -name '*.o*' -delete
		@$(MAKE) -C $(MSG_DIR) -f default.mk
		@find $(MAKE_ROOT)/ip6/ -name '*.o*' -delete

dpdk: $(apps)
		@find $(MAKE_ROOT)/ip6/ -name '*.o*' -delete
		@rm -rf $(MAKE_ROOT)/ip6/build

$(apps):
	@find $(MAKE_ROOT)/ip6/ -name '*.o*' -delete
	# binary name
	$(MAKE) -C $(MSG_DIR) -f dpdk.mk O=applications/dpdk/ APP=$@
	@find $(MAKE_ROOT)/ip6/ -name '*.o*' -delete


.PHONY: clean dpdk
clean:
		@$(MAKE) -C $(MSG_DIR) clean
		@find $(MAKE_ROOT)/ip6/ -name '*.o*' -delete
		@rm -rf $(MAKE_ROOT)/ip6/build

