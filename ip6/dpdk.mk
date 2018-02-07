#   BSD LICENSE
# 
#   Copyright(c) 2010-2013 Intel Corporation. All rights reserved.
#   All rights reserved.
# 
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
# 
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
# 
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
srcExt = c
srcDir = $(ROOT_DIR)/applications
objDir = $(srcDir)/obj
binDir = $(srcDir)/bin
libDir = $(ROOT_DIR)/lib
app := server testing test_rmem
#userfaultfd_measure_pagefault dsm_wc rmem_test 

sources := $(shell find "$(libDir)" -name '*.$(srcExt)')

RTE_SDK=$(ROOT_DIR)/../dpdk
ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif
# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = testing

COMMON_DIR  = ../../common

# all source are stored in SRCS-y
SRCS-y := ${srcDir}/testing.c ${sources}

#CFLAGS += -O0
#CFLAGS += -O3
#CFLAGS += -g
#CFLAGS += -DPRIVATE_COUNTER
#CFLAGS += -DSHARED_COUNTER
#CFLAGS += -DSHARED_LOCK
#CFLAGS += -DUDP_DBG
#CFLAGS += -DTHROTTLE_TX
#CFLAGS += $(WERROR_FLAGS)
CFLAGS += -Wno-unused-parameter

CFLAGS += -I${COMMON_DIR}

include $(RTE_SDK)/mk/rte.extapp.mk