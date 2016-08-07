#ifndef STRACE_STRUCTURED_H
#define STRACE_STRUCTURED_H

#include "queue.h"

enum s_type {
	S_TYPE_c,
	S_TYPE_d,
	S_TYPE_ld,
	S_TYPE_lld,

	S_TYPE_u,
	S_TYPE_lu,
	S_TYPE_llu,

	S_TYPE_x,
	S_TYPE_lx,
	S_TYPE_llx,

	S_TYPE_o,
	S_TYPE_lo,
	S_TYPE_llo,

	S_TYPE_str,

	S_TYPE_addr,
	S_TYPE_fd,
	S_TYPE_path,
	S_TYPE_flags,

	S_TYPE_changeable,
	S_TYPE_changeable_void, /* the value didn't change */
};

/* syscall */

struct s_syscall;

struct s_arg {
	struct s_syscall *syscall;
	const char *name; /* if is a field of a struct */
	enum s_type type;
	union {
		void *value_p;
		uint64_t value_int;
	};

	STAILQ_ENTRY(s_arg) entry;
	STAILQ_ENTRY(s_arg) chg_entry;
};

STAILQ_HEAD(args_queue, s_arg);
STAILQ_HEAD(changeable_queue, s_arg);

struct s_syscall {
	struct tcb *tcp;
	int cur_arg;
	struct s_arg *last_changeable;
	struct args_queue args;
	struct changeable_queue changeable_args;
	enum s_type ret_type;
};

/* struct */

struct s_struct {
	struct args_queue args;
};

/* complex arguments */

struct s_flags {
	enum s_type type;
	const struct xlat *x;
	uint64_t flags;
	const char *dflt;
};

struct s_str {
	char *str;
	long addr;
};

struct s_changeable {
	struct s_arg *entering;
	struct s_arg *exiting;
};

struct s_printer {
	void (*print_entering)(struct tcb *tcp);
	void (*print_exiting)(struct tcb *tcp);
};

/* prototypes */

extern void s_val_free(struct s_arg *arg);

extern struct s_arg *s_arg_new(struct tcb *tcp, enum s_type type);
extern struct s_arg *s_arg_next(struct tcb *tcp, enum s_type type);

extern struct s_syscall *s_syscall_new(struct tcb *tcp);
extern void s_last_is_changeable(struct tcb *tcp);
extern void s_syscall_free(struct tcb *tcp);

extern void s_syscall_print_entering(struct tcb *tcp);
extern void s_syscall_print_exiting(struct tcb *tcp);

#endif /* #ifndef STRACE_STRUCTURED_H */
