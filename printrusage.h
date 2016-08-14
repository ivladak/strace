#ifndef STRACE_PRINTRUSAGE_H
#define STRACE_PRINTRUSAGE_H

extern void s_insert_rusage(const char *name, unsigned long addr);
extern void s_push_rusage(const char *name);

#ifdef ALPHA
extern ssize_t fetch_fill_rusage32(struct s_arg *arg, long addr, void *fn_data);
extern void s_insert_rusage32(const char *name, unsigned long addr);
extern void s_push_rusage32(const char *name);
#endif /* ALPHA */

#endif /* !defined(STRACE_PRINTRUSAGE_H) */
