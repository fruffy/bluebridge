MAKE_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
MSG_DIR    := $(MAKE_ROOT)/ip6
APP_DIR    := $(MSG_DIR)/apps

CC = gcc
CFLAGS += -c -Wextra -Wall -Wall -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS += -std=gnu11 -pedantic
# performance flags
CFLAGS += -oFast
# CFLAGS += -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS := -lpcap -pthread -lm -lrt

apps := testing server event_server bb_disk
#rmem_test

MAKE_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
srcExt = c
binDir = bin
LDFLAGS+= -L../ -lbluebridge
# add the raw socket flag to differentiate from other builds
CFLAGS += -DDEFAULT

sources := $(shell find "." -name '*.$(srcExt)')

all: $(apps)

$(apps):
	@mkdir -p `dirname $@`
	@$(eval appObject = $(@:$(binDir)/%=%))   # strip bin/ from the app file
	$(CC) -o bin/$@ $(appObject).c $(LDFLAGS) $(DFLAGS)

clean:
	@echo "Cleaning..."
	-rm -rf $(binDir)/*
	-rm -rf $(apps)
