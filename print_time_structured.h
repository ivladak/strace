#ifndef STRACE_PRINT_TIME_H
#define STRACE_PRINT_TIME_H

void s_insert_timespec_addr(const char *name, long addr);
void s_push_timespec(const char *name);
void s_insert_timeval_addr(const char *name, long addr);
void s_push_timeval(const char *name);

void s_push_itimerval(const char *name);
#ifdef ALPHA
void s_push_itimerval32(const char *name);
#endif /* ALPHA */

#endif /* !defined(STRACE_PRINT_TIME_H) */
