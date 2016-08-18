#ifndef STRACE_PRINTRUSAGE_H
#define STRACE_PRINTRUSAGE_H

#ifdef ALPHA
extern void s_insert_rusage32(const char *name, unsigned long addr);
extern void s_push_rusage32(const char *name);
#endif /* ALPHA */

#endif /* !defined(STRACE_PRINTRUSAGE_H) */
