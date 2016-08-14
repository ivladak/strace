#ifndef STRACE_SECCOMP_STRUCTURED_H
#define STRACE_SECCOMP_STRUCTURED_H

extern void s_insert_seccomp_fprog(const char *name, unsigned long addr,
	unsigned short len);
extern void s_push_seccomp_fprog(const char *name, unsigned short len);
extern void s_push_seccomp_filter(const char *name);

#endif /* !defined(STRACE_SECCOMP_STRUCTURED_H) */
