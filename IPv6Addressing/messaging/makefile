srcExt = c
srcDir = src
objDir = obj
binDir = bin
libDir = lib
app := server userfaultfd_measure_pagefault testing dsm_wc test_rmem


# CC = gcc
# #CFlags += -c -Wextra -Werror -Wall -g3
# CFLAGS += -c -Wextra -Wall -g3 -std=gnu11
# LDFLAGS := -lpcap -pthread -lm

sources := $(shell find "$(srcDir)/$(libDir)" -name '*.$(srcExt)')
srcDirs := $(shell find . -name '*.$(srcExt)' -exec dirname {} \; | uniq)
objects := $(patsubst %.$(srcExt),$(objDir)/%.o,$(sources))

all: $(app)

$(app):  % : $(binDir)/%

$(binDir)/%: buildrepo $(objects)
	@mkdir -p `dirname $@`
	@$(eval appObject = $(@:bin/%=%))
	@$(CC) $(srcDir)/$(appObject).c $(objects) $(LDFLAGS) -o $@ `pkg-config --cflags --libs glib-2.0`

$(objDir)/%.o: %.$(srcExt)
	@echo "Building $@ ... "
	@$(CC) $(CFLAGS) $< -o $@

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