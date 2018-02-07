#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include "random.h"

int prand_seed(char *buf, int num)
{
    int fd = -1;
    int rc = -1;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        goto out;
    }

    while (num) {
        int bytes = read(fd, buf, num);

        if (bytes < 0) {
            goto out;
        }

        num -= bytes;
    }

    rc = 0;

out:
    if (fd) {
        close(fd);
    }

    return rc;
}

double prand_next(char *buf)
{
    return erand48((uint16_t *)buf);
}

/*
 * Pretty egregious use of a pseudo-random number generator.
 * By constantly re-seeding it with a per-request value, we
 * can re-generate the "random" data for a write request, but
 * still appear random on the wire for fault detection.
 */
void prandom_buffer(int seed, uint8_t *buf, int len)
{
    int* buf_int = (int *) buf;
    int buf_len  = len / 4;
    int i;

    assert(len % 4 == 0);

    srand(seed);

    for (i = 0; i < buf_len; i++) {
        buf_int[i] = rand();
    }
}
