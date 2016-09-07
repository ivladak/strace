#ifndef STRACE_PRINT_TIME_H
#define STRACE_PRINT_TIME_H

#ifdef ALPHA
void s_push_timeval32(const char *name);
void s_push_itimerval32(const char *name);
#endif /* ALPHA */

#endif /* !defined(STRACE_PRINT_TIME_H) */
