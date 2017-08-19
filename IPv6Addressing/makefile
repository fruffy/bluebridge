srcExt = c
srcDir = applications
objDir = $(srcDir)/obj
binDir = $(srcDir)/bin
libDir = lib
app := server testing test_rmem
#userfaultfd_measure_pagefault dsm_wc test_rmem 

# CC = gcc
# #CFlags += -c -Wextra -Werror -Wall -g3
# CFLAGS += -c -Wextra -Wall -g3 -std=gnu11
# LDFLAGS := -lpcap -pthread -lm

sources := $(shell find "$(libDir)" -name '*.$(srcExt)')
srcDirs := $(shell find . -name '*.$(srcExt)' -exec dirname {} \; | uniq)
objects := $(patsubst %.$(srcExt), $(objDir)/%.o, $(sources))

all: $(app)
	-rm -r $(objDir)

$(app):  % : $(binDir)/%


$(binDir)/%: buildrepo $(objects)
	@mkdir -p `dirname $@`
	@$(eval appObject = $(@:$(binDir)/%=%))
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