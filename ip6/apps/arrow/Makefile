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
ARR_MEM_TEST  = arrow_mem_test
ARR_MEM_BENCH = arrow_mem_bench

all: $(ARR_IPC_BENCH) $(ARR_MEM_BENCH) $(ARR_MEM_TEST)


$(ARR_IPC_BENCH):
	@echo "Building $@"
	@$(CC) -o $@ $@.cc $(CFLAGS) $(LIBS)

$(ARR_MEM_TEST):
	@echo "Building $@"
	@$(CC) -o $@ $@.cc $(CFLAGS) $(LIBS)

$(ARR_MEM_BENCH):
	@echo "Building $@"
	@$(CC) -o $@ $@.cc $(CFLAGS) $(LIBS)

clean:
	@echo "Cleaning..."
	@$(RM)  $(ARR_IPC_BENCH) $(ARR_MEM_TEST) $(ARR_MEM_BENCH)