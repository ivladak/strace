#ifndef STRACE_PRINTSIGINFO_H
#define STRACE_PRINTSIGINFO_H

extern void printsiginfo(const siginfo_t *);

extern void s_insert_siginfo(const char *name, unsigned long addr);
extern void s_push_siginfo(const char *name);
extern void s_push_siginfo_array(const char *name, unsigned long nmemb);

#endif /* !STRACE_PRINTSIGINFO_H */
