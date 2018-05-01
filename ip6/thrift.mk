MAKE_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
THRIFT_DIR = $(MAKE_ROOT)/thrift
C_GLIB_DIR = $(THRIFT_DIR)/lib/c_glib
BIFROST_TUTORIAL = $(THRIFT_DIR)/tutorial/c_glib
THRIFT_TUTORIAL = $(THRIFT_DIR)/tutorial_thrift/c_glib

MAKE_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
srcExt = c
srcDir = $(MAKE_ROOT)/apps
objDir = $(MAKE_ROOT)/obj
binDir = $(srcDir)/bin
libDir = $(MAKE_ROOT)/lib
dpdkDir = $(libDir)/dpdk_backend
thriftDir = $(MAKE_ROOT)/thrift

# add the raw socket flag to differentiate from other builds
CFLAGS += -DDEFAULT
# a list of c files we do not want to compile
filter:= $(dpdkDir)/dpdk_server.c
filter+= $(dpdkDir)/dpdk_client.c
filter+= $(dpdkDir)/dpdk_common.c
filter+= $(dpdkDir)/dpdk_common.h

sources := $(shell find "$(libDir)" -name '*.$(srcExt)')
sources_filtered := $(filter-out $(filter), $(sources))
srcDirs := $(shell find . -name '*.$(srcExt)' -exec dirname {} \; | uniq)
objects := $(patsubst $(MAKE_ROOT)/%.$(srcExt), $(objDir)/%.o, $(sources_filtered))

all: $(apps)
	@$(MAKE) -C $(C_GLIB_DIR)
	@$(MAKE) -C $(BIFROST_TUTORIAL)
	@$(MAKE) -C $(THRIFT_TUTORIAL)
	# -rm -rf $(objDir)

$(apps):  % : $(binDir)/%

.SECONDARY: $(objects)

$(binDir)/%: buildrepo $(objects)
	@mkdir -p `dirname $@`
	@$(eval appObject = $(@:$(binDir)/%=%))
	@$(CC) $(srcDir)/$(appObject).c $(objects) $(LDFLAGS) $(DFLAGS)-o $@

$(objDir)/%.o: %.$(srcExt)
	@echo "Building $@ ... "
	@$(CC) $(CFLAGS) $(MAKE_ROOT)/$< -o $@

clean:
	@echo "Cleaning..."
	@$(MAKE) -C $(C_GLIB_DIR) clean
	@$(MAKE) -C $(BIFROST_TUTORIAL) clean
	@$(MAKE) -C $(THRIFT_TUTORIAL) clean
	-rm -rf $(objDir)
	-rm -rf $(binDir)/*

buildrepo:
	@$(call make-repo)

define make-repo
   for dir in $(srcDirs);\
   do\
	mkdir -p $(objDir)/$$dir;\
   done
endef