#ifndef STRACE_SIGNAL_STRUCTURED_H
#define STRACE_SIGNAL_STRUCTURED_H

#include "defs.h"

extern void s_insert_sigset_addr_len_limit(const char *name, unsigned long addr,
	unsigned long len, unsigned long min_len);
extern void s_insert_sigset_addr_len(const char *name, unsigned long addr,
	unsigned long len);
extern void s_push_sigset_addr_len_limit(const char *name, unsigned long len,
	unsigned long min_len);
extern void s_push_sigset_addr_len(const char *name, unsigned long len);

#endif /* !defined(STRACE_SIGNAL_STRUCTURED_H) */
