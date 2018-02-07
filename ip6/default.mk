ROOT_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
srcExt = c
srcDir = $(ROOT_DIR)/applications
objDir = $(srcDir)/obj
binDir = $(srcDir)/bin
libDir = $(ROOT_DIR)/lib
app := server testing rmem_test busexmp

# a list of c files we do not want to compile
filter:= $(libDir)/dpdkstack.c

sources := $(shell find "$(libDir)" -name '*.$(srcExt)')
sources_filtered := $(filter-out $(filter), $(sources))
srcDirs := $(shell find . -name '*.$(srcExt)' -exec dirname {} \; | uniq)
objects := $(patsubst $(ROOT_DIR)/%.$(srcExt), $(objDir)/%.o, $(sources_filtered))

all: $(app)
	-rm -r $(objDir)

$(app):  % : $(binDir)/%


$(binDir)/%: buildrepo $(objects)
	@mkdir -p `dirname $@`
	@$(eval appObject = $(@:$(binDir)/%=%))
	@$(CC) $(srcDir)/$(appObject).c $(objects) $(LDFLAGS) $(DFLAGS)-o $@

$(objDir)/%.o: %.$(srcExt)
	@echo "Building $@ ... "
	@$(CC) $(CFLAGS) $(ROOT_DIR)/$< -o $@

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