MAKE_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
#DDC_DIR = $(MAKE_ROOT)

C_GLIB_LIB = $(MAKE_ROOT)/../../thrift/lib/c_glib/.libs/libthrift_c_glib.a
BB_LIB = -L$(MAKE_ROOT)/../../ -lbluebridge
BB_SRC = $(MAKE_ROOT)/../../
C_GLIB_SRC = $(MAKE_ROOT)/../../thrift/lib/c_glib/src

GEN_TEMPLATE = thrift --gen c_glib

CC = gcc
INCLUDES += -I$(C_GLIB_SRC) -I$(BB_SRC) `pkg-config --cflags glib-2.0 gobject-2.0`
LIBS = $(INCLUDES)
LIBS += $(C_GLIB_LIB) $(BB_LIB) -lm
LIBS += `pkg-config --libs glib-2.0 gobject-2.0`
CFLAGS = 


GLIB_DIR = gen-c_glib
SHARED_GLIB = $(GLIB_DIR)/shared_types.c thrift_utils.c

MEM_SERVER = remote_mem_test
MEM_SERVER_GLIB =  $(GLIB_DIR)/$(MEM_SERVER).c $(GLIB_DIR)/$(MEM_SERVER)_types.c 

ARR_SERVER = simple_arr_comp
ARR_SERVER_GLIB = $(GLIB_DIR)/$(ARR_SERVER).c $(GLIB_DIR)/$(ARR_SERVER)_types.c 
DDC_CLIENT = ddc_client
TCP_CLIENT = tcp_client

DDC_CLIENT_DEPS = $(DDC_CLIENT).c $(MEM_SERVER_GLIB) $(ARR_SERVER_GLIB) mem_tests.c
TCP_CLIENT_DEPS = $(TCP_CLIENT).c $(MEM_SERVER_GLIB) $(ARR_SERVER_GLIB) mem_tests.c

all: $(SHARED_GLIB) $(MEM_SERVER) $(ARR_SERVER) $(DDC_CLIENT) $(TCP_CLIENT)

$(GLIB_DIR)/shared_types.c:
	@$(GEN_TEMPLATE) shared.thrift

gen-c_glib/$(MEM_SERVER).c:
	@$(GEN_TEMPLATE) $(MEM_SERVER).thrift

gen-c_glib/$(ARR_SERVER).c:
	@$(GEN_TEMPLATE) $(ARR_SERVER).thrift

$(ARR_SERVER): ddc_$(ARR_SERVER)_server.c tcp_$(ARR_SERVER)_server.c $(ARR_SERVER_GLIB)
	@echo "Building $(ARR_SERVER)"
	@$(CC) -o ddc_$(ARR_SERVER) ddc_$(ARR_SERVER)_server.c $(ARR_SERVER_GLIB) $(SHARED_GLIB) $(CFLAGS) $(LIBS)
	@$(CC) -o tcp_$(ARR_SERVER) tcp_$(ARR_SERVER)_server.c $(ARR_SERVER_GLIB) $(SHARED_GLIB) $(CFLAGS) $(LIBS) 
$(MEM_SERVER): $(MEM_SERVER)_server.c $(MEM_SERVER_GLIB)
	@echo "Building $(MEM_SERVER)"
	@$(CC) -o $@ $(MEM_SERVER)_server.c $(MEM_SERVER_GLIB) $(SHARED_GLIB) $(CFLAGS) $(LIBS)

$(DDC_CLIENT): $(DDC_CLIENT_DEPS)
	@echo "Building client"
	@$(CC) -o $@ $(DDC_CLIENT_DEPS) $(SHARED_GLIB) $(CFLAGS) $(LIBS)

$(TCP_CLIENT): $(TCP_CLIENT_DEPS)
	@echo "Building client"
	@$(CC) -o $@ $(TCP_CLIENT_DEPS) $(SHARED_GLIB) $(CFLAGS) $(LIBS)

clean:
	@echo "Cleaning..."
	@$(RM) -r $(GLIB_DIR)
	@$(RM) ddc_$(ARR_SERVER) tcp_$(ARR_SERVER) $(MEM_SERVER) $(DDC_CLIENT) $(TCP_CLIENT)