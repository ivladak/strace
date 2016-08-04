#ifndef STRACE_STRUCTURED_H
#define STRACE_STRUCTURED_H

#include "queue.h"

typedef enum s_type {
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
} s_type_t;

/* syscall */

struct s_syscall;

typedef struct s_arg {
	struct s_syscall *syscall;
	const char *name; /* if is a field of a struct */
	s_type_t type;
	union {
		void *value_p;
		uint64_t value_int;
	};

	STAILQ_ENTRY(s_arg) entry;
} s_arg_t;

STAILQ_HEAD(args_queue, s_arg);

typedef struct s_syscall {
	struct tcb *tcp;
	int cur_arg;
	struct args_queue args;
	s_type_t ret_type;
} s_syscall_t;

/* struct */

typedef struct s_struct {
	struct args_queue args;
} s_struct_t;


/* complex arguments */

typedef struct s_flags {
	const struct xlat *x;
	uint64_t flags;
	const char *dflt;
} s_flags_t;

typedef struct s_str {
	char *str;
	long addr;
} s_str_t;

/* prototypes */

extern void s_val_free(s_arg_t *arg);
extern s_arg_t *s_arg_new(struct tcb *tcp, s_type_t type);
extern s_syscall_t *s_syscall_new(struct tcb *tcp);
extern void s_syscall_free(struct tcb *tcp);
extern void s_syscall_print(struct tcb *tcp);

#endif /* #ifndef STRACE_STRUCTURED_H */
