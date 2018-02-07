#ifndef __DEBUG_H_
#define __DEBUG_H_

#include <errno.h>
#include <stdio.h>
#include <assert.h>

#define UNREFERENCED_PARAM(x)        ((void)(x))

#define TRACE_CONFIG(f, m...) fprintf(stdout, f, ##m)

#ifdef DBGERR

#define TRACE_ERROR(f, m...) { \
	fprintf(stdout, "[%10s:%4d] " f, __FUNCTION__, __LINE__, ##m);	\
	}

#else

#define TRACE_ERROR(f, m...)	(void)0

#endif /* DBGERR */

#ifdef DBGMSG

#define TRACE_DBG(f, m...) {\
	fprintf(stderr, "[%10s:%4d] " \
			f, __FUNCTION__, __LINE__, ##m);   \
	}

#else

#define TRACE_DBG(f, m...)   (void)0

#endif /* DBGMSG */

#ifdef INFO

#define TRACE_INFO(f, m...) {                                         \
	fprintf(stdout, "[%10s:%4d] " f,__FUNCTION__, __LINE__, ##m);    \
    }

#else

#define TRACE_INFO(f, m...) (void)0

#endif /* INFO */

#ifdef DBGFUNC

#define TRACE_FUNC(n, f, m...) {                                         \
	fprintf(stderr, "%6s: %10s:%4d] " \
			f, n, __FUNCTION__, __LINE__, ##m);    \
	}

#else

#define TRACE_FUNC(f, m...) (void)0

#endif /* DBGFUNC */

#ifdef UDP
#define TRACE_UDP(f, m...) TRACE_FUNC("UDP", f, ##m)
#else
#define TRACE_UDP(f, m...)   (void)0
#endif /* UDP */

#endif /* __DEBUG_H_ */
