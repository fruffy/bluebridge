MAKE_DIR = $(PWD)
ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
MSG_DIR    := $(ROOT_DIR)/IPv6Addressing/

CC = gcc-6
CFLAGS += -c -Wextra -Wall -Wall -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS += -std=gnu11 -pedantic
CFGLAS += -oFast # performance flags

# CFLAGS += -Wstrict-prototypes -Wmissing-prototypes

LDFLAGS := -lpcap -pthread -lm -lrt

export MAKE_DIR CC CFLAGS LDFLAGS
all:
		echo $(ROOT_DIR)
		rm -rf $(ROOT_DIR)/IPv6Addressing/build
		@$(MAKE) -C $(MSG_DIR)
		#@$(MAKE) -C $(MSG_DIR) -f default.mk
		find $(ROOT_DIR)/IPv6Addressing/ -name '*.o*' -delete

.PHONY: clean
clean:
		@$(MAKE) -C $(MSG_DIR) clean
		find $(ROOT_DIR)/IPv6Addressing/ -name '*.o*' -delete
		rm -rf $(ROOT_DIR)/IPv6Addressing/build

