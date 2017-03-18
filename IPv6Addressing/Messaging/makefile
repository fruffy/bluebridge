srcExt = c
srcDir = src
objDir = obj
binDir = bin
libDir = lib
app := client server


CC = gcc
#CFlags += -c -Wextra -Werror -Wall -g3
CFlags += -c -Wextra -Wall -g3
sources := $(shell find "$(srcDir)/$(libDir)" -name '*.$(srcExt)')
srcDirs := $(shell find . -name '*.$(srcExt)' -exec dirname {} \; | uniq)
objects := $(patsubst %.$(srcExt),$(objDir)/%.o,$(sources))

all: $(app)

$(app):  % : $(binDir)/%

$(binDir)/%: buildrepo $(objects)
	@mkdir -p `dirname $@`
	@$(eval appObject = $(@:bin/%=%))
	@$(CC) $(srcDir)/$(appObject).c $(objects) $(LDFlags) -o $@

$(objDir)/%.o: %.$(srcExt)
	@echo "Building $@ ... "
	@$(CC) $(CFlags) $< -o $@

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