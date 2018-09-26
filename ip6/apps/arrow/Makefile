MAKE_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
#DDC_DIR = $(MAKE_ROOT)
ARROW_DIR = $(MAKE_ROOT)/../../../includes/arrow/cpp/
BB_LIB = -L$(MAKE_ROOT)/../../ -lbluebridge
BB_SRC = $(MAKE_ROOT)/../../
ARROW_LIB = $(THRIFT_DIR)/lib/c_glib/src

GEN_TEMPLATE = thrift --gen c_glib

CC = gcc
INCLUDES += -I$(BB_SRC) -I$(ARROW_DIR)/src
LIBS = $(INCLUDES) $(ARROW_DIR)/debug/debug/libarrow.a
LIBS += $(BB_LIB) -lstdc++ -lm -lgtest -lpthread -lbenchmark
CFLAGS =

ARR_IPC_BENCH = arrow_ipc_bench

all: $(ARR_IPC_BENCH)


$(ARR_IPC_BENCH):
	@echo "Building $(ARR_SERVER)"
	@$(CC) -o $@ $@.cc $(CFLAGS) $(LIBS)
clean:
	@echo "Cleaning..."
	@$(RM) -r $(GLIB_DIR)
	@$(RM) ddc_$(ARR_SERVER) tcp_$(ARR_SERVER) $(MEM_SERVER) $(DDC_CLIENT) $(TCP_CLIENT)
