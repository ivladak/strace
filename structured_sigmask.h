#ifndef STRACE_STRUCTURED_SIGMASK_H
#define STRACE_STRUCTURED_SIGMASK_H

#include <signal.h>
#include <stdint.h>

#include "structured.h"


#ifndef NSIG
# warning NSIG is not defined, using 32
# define NSIG 32
#elif NSIG < 32
# error NSIG < 32
#endif

struct s_sigmask {
	struct s_arg arg;

	unsigned int bytes;
	union {
		uint8_t sigmask[NSIG >> 3];
		uint32_t sigmask32[NSIG >> 5];
	};
};

typedef void (*s_print_sigmask_fn)(int bit, const char *str, bool set,
	void *data);

extern struct s_sigmask *s_sigmask_new(const char *name, const void *sig_mask,
	unsigned int bytes);
extern struct s_sigmask *s_sigmask_new_and_insert(const char *name,
	const void *sig_mask, unsigned int bytes);

extern void s_insert_sigmask(const char *name, const void *sig_mask,
	unsigned int bytes);
extern void s_insert_sigmask_addr(const char *name, unsigned long addr,
	unsigned int bytes);
/* Equivalent to s_push_sigmask_addr */
extern void s_push_sigmask(const char *name, unsigned int bytes);

/** Helper for processing sigmask during output, similar to s_process_xlat */
extern void s_process_sigmask(struct s_sigmask *arg, s_print_sigmask_fn cb,
	void *cb_data);

#endif /* !defined(STRACE_STRUCTURED_SIGMASK_H) */
