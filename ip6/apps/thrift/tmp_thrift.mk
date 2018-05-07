MAKE_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
TCP_DIR = $(MAKE_ROOT)
#DDC_DIR = $(MAKE_ROOT)

C_GLIB_LIB = $(MAKE_ROOT)/../../thrift/lib/c_glib/.libs/libthrift_c_glib.a
BB_LIB = -L$(MAKE_ROOT)/../../ -lbluebridge
BB_SRC = $(MAKE_ROOT)/../../
C_GLIB_SRC = $(MAKE_ROOT)/../../thrift/lib/c_glib/src

GEN_TEMPLATE = thrift --gen c_glib

CC = gcc
INCLUDES += -I$(C_GLIB_SRC) -I$(BB_SRC) `pkg-config --cflags glib-2.0 gobject-2.0`
LIBS = $(INCLUDES)
LIBS += $(C_GLIB_LIB) $(BB_LIB) 
LIBS += `pkg-config --libs glib-2.0 gobject-2.0`
CFLAGS = 
# CFLAGS = -Wextra -Wall -Wshadow -Wpointer-arith -Wcast-qual
# CFLAGS += -std=gnu11
# performance flags

export C_GLIB_LIB BB_LIB BB_SRC C_GLIB_SRC LIBS CC CFLAGS GEN_TEMPLATE
all:
	@-rm -rf $(TCP_DIR)/gen-c_glib
	@-rm -rf $(DDC_DIR)/gen-c_glib
	@$(MAKE) -C $(TCP_DIR)
	#@$(MAKE) -C $(DDC_DIR)
	#-rm -rf $(objDir)

clean:
	@echo "Cleaning..."
	@$(MAKE) -C $(TCP_DIR) clean
	#@$(MAKE) -C $(DDC_DIR) clean

