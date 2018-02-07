MAKE_DIR = $(PWD)
ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
MSG_DIR    := $(ROOT_DIR)/ip6/

CC = gcc
CFLAGS += -c -Wextra -Wall -Wall -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS += -std=gnu11 -pedantic
CFGLAS += -oFast # performance flags
CFLAGS += -DRAW_SOCK
# CFLAGS += -Wstrict-prototypes -Wmissing-prototypes

LDFLAGS := -lpcap -pthread -lm -lrt

export MAKE_DIR CC CFLAGS LDFLAGS
all:
		@echo $(ROOT_DIR)
		@rm -rf $(ROOT_DIR)/ip6/build
		@$(MAKE) -C $(MSG_DIR) -f default.mk
		@find $(ROOT_DIR)/ip6/ -name '*.o*' -delete

dpdk:
		@find $(ROOT_DIR)/ip6/ -name '*.o*' -delete
		@rm -rf $(ROOT_DIR)/ip6/build
		@$(MAKE) -C $(MSG_DIR) -f dpdk.mk
		@find $(ROOT_DIR)/ip6/ -name '*.o*' -delete

.PHONY: clean dpdk
clean:
		@$(MAKE) -C $(MSG_DIR) clean
		@find $(ROOT_DIR)/ip6/ -name '*.o*' -delete
		@rm -rf $(ROOT_DIR)/ip6/build

