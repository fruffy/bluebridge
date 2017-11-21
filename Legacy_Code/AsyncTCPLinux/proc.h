#ifndef _PROC_H
#define _PROC_H

#include <stdlib.h>

#define rdtscll(val) do { \
            unsigned int __a,__d; \
            __asm__ __volatile__("rdtsc" : "=a" (__a), "=d" (__d)); \
            (val) = ((unsigned long long)__a) | (((unsigned long long)__d)<<32); \
        } while(0)

int pin_thread(pthread_t thread, int core_id);
int pin_current_process(int core_id);

void register_signal_handlers(sighandler_t handler);

#endif /* _PROC_H */
