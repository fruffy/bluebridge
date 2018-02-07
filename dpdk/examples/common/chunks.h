#ifndef _CHUNKS_H
#define _CHUNKS_H

#include <stdlib.h>
#include <inttypes.h>

struct chunk_metadata
{
    uint64_t id;
    uint64_t size;
    uint8_t *data;
};

int load_chunks(char *chk_dir, struct chunk_metadata *chunks,
                uint64_t *chk_count, int chk_limit);
void free_chunks(struct chunk_metadata *chunks, uint64_t chunk_count);

#endif /* _CHUNKS_H */
