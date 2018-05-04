MAKE_ROOT:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
TCP_DIR = $(MAKE_ROOT)/tcp
DDC_DIR = $(MAKE_ROOT)/ddc

srcExt = c

all:
	@$(MAKE) -C $(TCP_DIR)
	@$(MAKE) -C $(DDC_DIR)
	#-rm -rf $(objDir)

clean:
	@echo "Cleaning..."
	@$(MAKE) -C $(TCP_DIR) clean
	@$(MAKE) -C $(DDC_DIR) clean
