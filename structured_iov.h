#ifndef STRACE_STRUCTURED_IOV_H
#define STRACE_STRUCTURED_IOV_H

#include "defs.h"

extern void s_insert_iov_upto(const char *name, unsigned long addr,
	unsigned long len, enum iov_decode decode_iov, unsigned long data_size);
extern void s_insert_iov(const char *name, unsigned long addr,
	unsigned long len, enum iov_decode decode_iov);
extern void s_push_iov_upto(const char *name, unsigned long len,
	enum iov_decode decode_iov, unsigned long data_size);
extern void s_push_iov(const char *name, unsigned long len,
	enum iov_decode decode_iov);

#endif /* !STRACE_STRUCTURED_IOV_H */
