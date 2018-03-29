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

# Cannot do "@find $(MAKE_ROOT)/ip6/ -path -name '*.o*' -delete"
# it deletes the object files in the thrift .libs which requires
# a complete rebuild of thrift to repopulate. 

export apps CC CFLAGS LDFLAGS
all:
		@echo "Running default build in $(MAKE_ROOT)"
		@$(MAKE) -C $(MSG_DIR) -f default.mk clean
		@$(MAKE) -C $(MSG_DIR) -f default.mk all

thrift:
		@echo "Running thrift build in $(MAKE_ROOT)"
		@$(MAKE) -C $(MSG_DIR) -f thrift.mk

thrift-clean:
		@echo "Cleaning thrift build in $(MAKE_ROOT)"
		@$(MAKE) -C $(MSG_DIR) -f thrift.mk clean

dpdk: $(apps)
		@find $(MAKE_ROOT)/ip6/ -path -name '*.o*' -delete
		@rm -rf $(MAKE_ROOT)/ip6/build

$(apps):
	# binary name
	@find $(MAKE_ROOT)/ip6/ -path -name '*.o*' -delete
	$(MAKE) -C $(MSG_DIR) -f dpdk.mk O=applications/dpdk/ APP=$@


.PHONY: clean dpdk
clean-all:
		@$(MAKE) -C $(MSG_DIR) -f default.mk clean
		@$(MAKE) -C $(MSG_DIR) -f thrift.mk clean
		@rm -rf $(MAKE_ROOT)/ip6/build

