#include <stdlib.h>

void toeplitz_init(uint8_t *key_vals);

/* sip, dip, sp, dp: in network byte order */
int
GetRSSCPUCore(uint32_t sip, uint32_t dip,
              uint16_t sp, uint16_t dp, int num_queues);

int
GetSourcePortForCore(char *src_ip, char *dst_ip,
                     uint16_t dst_p, uint16_t src_start,
                     int dest_queues, int des_core);
