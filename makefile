MAKE_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
MSG_DIR        := $(MAKE_ROOT)/ip6
APP_DIR        := $(MSG_DIR)/apps
THRIFT_APP_DIR := $(APP_DIR)/thrift
THRIFT_DIR     := includes/thrift
ARROW_APP_DIR := $(APP_DIR)/arrow
ARROW_DIR     := includes/arrow/cpp/debug

CC = gcc
CFLAGS += -c -Wextra -Wall -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS += -std=gnu11 -pedantic
# performance flags
CFLAGS += -O3
# CFLAGS += -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS := -lpcap -pthread -lm -lrt

apps := testing server event_server bb_disk rmem_test
#rmem_test

# Cannot do "@find $(MAKE_ROOT)/ip6/ -path -name '*.o*' -delete"
# it deletes the object files in the thrift .libs which requires
# a complete rebuild of thrift to repopulate.

export apps CC CFLAGS LDFLAGS
all: lib
	@$(MAKE) -C $(APP_DIR) -f app.mk clean
	@$(MAKE) -C $(APP_DIR) -f app.mk all

classic:
	@echo "Running classic build in $(MAKE_ROOT)"
	@$(MAKE) -C $(MSG_DIR) -f classic.mk clean
	@$(MAKE) -C $(MSG_DIR) -f classic.mk all

lib:
	@echo "Running lib build in $(MAKE_ROOT)"
	@$(MAKE) -C $(MSG_DIR) -f lib.mk clean
	@$(MAKE) -C $(MSG_DIR) -f lib.mk all

libdpdk:
	@echo "Running the libdpdk build in $(MAKE_ROOT)"
	@$(MAKE) -C $(MSG_DIR) -f libdpdk.mk clean
	@rm -rf libbluebridgedpdk.a
	@$(MAKE) -C $(MSG_DIR) -f libdpdk.mk O=./
	@$(MAKE) -C $(MSG_DIR) -f libdpdk.mk clean

dpdk: libdpdk $(apps)
	@find $(MAKE_ROOT)/ip6/ -name '*.o*' -delete
	@rm -rf $(MAKE_ROOT)/ip6/build

$(apps):
	# binary name
	$(MAKE) -C $(APP_DIR) -f dpdk.mk O=dpdk/ APP=$@
	@find $(MAKE_ROOT)/ip6/ -name '*.o*' -delete

thrift:
	@echo "Creating the thrift build in $(MAKE_ROOT)"
	@$(MAKE) -C $(THRIFT_DIR)

thrift-apps: lib
	@$(MAKE) -C $(THRIFT_APP_DIR) clean
	@$(MAKE) -C $(THRIFT_APP_DIR)

thrift-all: all thrift thrift-apps

thrift-clean:
	@echo "Cleaning thrift build in $(MAKE_ROOT)"
	@$(MAKE) -C $(THRIFT_APP_DIR) clean
	@$(MAKE) -C $(THRIFT_DIR) clean

arrow:
	@echo "Creating the arrow build in $(MAKE_ROOT)"
	cd $(ARROW_DIR); cmake -DARROW_BUILD_BENCHMARKS=ON ..
	@$(MAKE) -C $(ARROW_DIR)

arrow-apps: lib
	@$(MAKE) -C $(ARROW_APP_DIR) clean
	@$(MAKE) -C $(ARROW_APP_DIR)

arrow-all: all arrow arrow-apps

arrow-clean:
	@echo "Cleaning thrift build in $(MAKE_ROOT)"
	@$(MAKE) -C $(THRIFT_APP_DIR) clean
	@$(MAKE) -C $(THRIFT_DIR) clean

clean: thrift-clean
	@$(MAKE) -C $(MSG_DIR) -f classic.mk clean
	@$(MAKE) -C $(MSG_DIR) -f lib.mk clean
	@$(MAKE) -C $(MSG_DIR) -f libdpdk.mk clean
	@$(MAKE) -C $(APP_DIR) -f app.mk clean


