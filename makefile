MAKE_DIR = $(PWD)

MSG_DIR    := $(MAKE_DIR)/IPv6Addressing/


CC = gcc
CFLAGS += -c -Wextra -Wall -Wall -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS += -std=gnu11 -pedantic
# CFLAGS += -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS := -lpcap -pthread -lm

export MAKE_DIR CC CFLAGS LDFLAGS

all:
	@$(MAKE) -C $(MSG_DIR)



.PHONY: clean
clean:
	@$(MAKE) -C $(MSG_DIR) clean
