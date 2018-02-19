MAKE_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
srcExt = c
srcDir = $(MAKE_ROOT)/applications
objDir = $(srcDir)/obj
binDir = $(srcDir)/bin
libDir = $(MAKE_ROOT)/lib

# add the raw socket flag to differentiate from other builds
CFLAGS += -DRAW_SOCK
# a list of c files we do not want to compile
filter:= $(libDir)/dpdkstack.c

sources := $(shell find "$(libDir)" -name '*.$(srcExt)')
sources_filtered := $(filter-out $(filter), $(sources))
srcDirs := $(shell find . -name '*.$(srcExt)' -exec dirname {} \; | uniq)
objects := $(patsubst $(MAKE_ROOT)/%.$(srcExt), $(objDir)/%.o, $(sources_filtered))

all: $(apps)
	-rm -r $(objDir)

$(apps):  % : $(binDir)/%


$(binDir)/%: buildrepo $(objects)
	@mkdir -p `dirname $@`
	@$(eval appObject = $(@:$(binDir)/%=%))
	@$(CC) $(srcDir)/$(appObject).c $(objects) $(LDFLAGS) $(DFLAGS)-o $@

$(objDir)/%.o: %.$(srcExt)
	@echo "Building $@ ... "
	@$(CC) $(CFLAGS) $(MAKE_ROOT)/$< -o $@

clean:
	@echo "Cleaning..."
	-rm -r $(objDir)
	-rm -r $(binDir)/*

buildrepo:
	@$(call make-repo)

define make-repo
   for dir in $(srcDirs);\
   do\
	mkdir -p $(objDir)/$$dir;\
   done
endef