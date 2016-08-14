#ifndef STRACE_PRINTRESOURCE_H
#define STRACE_PRINTRESOURCE_H

#include "defs.h"

extern const char *sprint_rlim64(uint64_t lim);
#if !defined(current_wordsize) || current_wordsize == 4
extern const char *sprint_rlim32(uint32_t lim);
#endif

#endif /* !defined(STRACE_PRINTRESOURCE_H) */
