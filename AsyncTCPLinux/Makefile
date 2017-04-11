CC=gcc
CFLAGS=-Wall -g -D_GNU_SOURCE
LDFLAGS=-lpthread

PROGS = asyncclient asyncserver

CFILES += proc.c
CFILES += net.c

PROG_AC = asyncclient.c $(CFILES)
PROG_AS = asyncserver.c $(CFILES)

all: ${PROGS}

asyncclient: $(PROG_AC)
	$(CC) $(CFLAGS) -o $@ $(PROG_AC) $(LDFLAGS)

asyncserver: $(PROG_AS)
	$(CC) $(CFLAGS) -o $@ $(PROG_AS) $(LDFLAGS)

clean:
	rm -f $(PROGS)
