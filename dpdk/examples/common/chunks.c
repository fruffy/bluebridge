#define _LARGEFILE64_SOURCE
#define _SVID_SOURCE
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <dirent.h>

#include "chunks.h"

int load_chunks(char *chk_dir, struct chunk_metadata *chunks,
                uint64_t *chk_count, int chk_limit)
{
    struct dirent **chunk_list;
    unsigned int num_chunks, i;
    uint64_t chunk_count  = 0;

    num_chunks = scandir(chk_dir, &chunk_list, NULL, alphasort);
    if ((int) num_chunks < 0) {
        fprintf(stderr, "scandir %s", chk_dir);
        errno = EINVAL;
        return -1;
    }

    for (i = 0; i < num_chunks && i < chk_limit; i++) {

        struct dirent *entry = chunk_list[i];
        char file_name[255];
        int fd = -1;
        uint64_t total_read = 0;

        if (strcmp(entry->d_name, ".") == 0)
            continue;
        else if (strcmp(entry->d_name, "..") == 0)
            continue;

        sprintf(file_name, "%s%s", chk_dir, entry->d_name);
        fd = open(file_name, O_RDONLY);
        if (fd < 0) {
            perror("open");
            continue;
        } else {
            chunks[chunk_count].size = lseek64(fd, 0, SEEK_END);
            lseek64(fd, 0, SEEK_SET);
        }

        chunks[chunk_count].data = (uint8_t *) malloc(chunks[chunk_count].size);
        if (!chunks[chunk_count].data) {
            fprintf(stderr, "Failed to allocate memory for %s\n", file_name);
            perror("malloc");
            continue;
        }

        while (1) {
            int ret = read(fd, chunks[chunk_count].data + total_read,
                           chunks[chunk_count].size - total_read);

            if (ret <= 0) {
                break;
            }

            total_read += ret;
        }

        if (total_read < chunks[chunk_count].size) {
            free(chunks[chunk_count].data);
            continue;
        }

        close(fd);

        printf("Loaded chunk %s: %lu\n", entry->d_name,
                chunks[chunk_count].size);

        chunk_count++;
    }

    *chk_count = chunk_count;
    return 0;
}

void free_chunks(struct chunk_metadata *chunks, uint64_t chunk_count)
{
    unsigned int i;

    for (i = 0; i < chunk_count; i++) {
        free(chunks[i].data);
    }
}

