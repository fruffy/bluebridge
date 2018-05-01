MAKE_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
srcExt = c
binDir = bin

# we are including the bluebridge library here
LDFLAGS+= -L../ -lbluebridge
# add the raw socket flag to differentiate from other builds
CFLAGS += -DDEFAULT

sources := $(shell find "." -name '*.$(srcExt)')

all: $(apps)

$(apps):
	@mkdir -p $(binDir)
	@$(eval appObject = $(@:$(binDir)/%=%))   # strip bin/ from the app file
	$(CC) -o bin/$@ $(appObject).c $(LDFLAGS) $(DFLAGS)

clean:
	@echo "Cleaning..."
	@-rm -rf $(binDir)/*
