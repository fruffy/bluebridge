#include <pthread.h>
#include <sched.h>
#include <signal.h>

#include "proc.h"

int pin_thread(pthread_t thread, int core_id)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}

int pin_current_process(int core_id)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

void register_signal_handlers(sighandler_t handler)
{
    sigset_t sigset;
    struct sigaction siginfo = {
        .sa_handler = handler,
        .sa_flags = SA_RESTART,
    };

    sigemptyset(&sigset);
    siginfo.sa_mask = sigset;

    sigaction(SIGINT, &siginfo, NULL);
    sigaction(SIGTERM, &siginfo, NULL);
    sigaction(SIGALRM, &siginfo, NULL);

    //special case for SIGPIPE. gotta ignore it.. cant handle it
    siginfo.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &siginfo, NULL);
}

