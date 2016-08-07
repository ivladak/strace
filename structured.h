#ifndef STRACE_STRUCTURED_H
#define STRACE_STRUCTURED_H

#include "queue.h"

/* Some trivial utility macros for field manipulation. */

#define POW2(val) (1ULL << (val))
#define IS_POW2(x) ((x) && !((x) & ((x) - 1)))
#define MASK(bits) (POW2(bits) - 1)
#define FIELD(value, len, shift) (((value) & MASK(len)) << (shift))
#define FIELD_MASK(offs, size) (MASK(size) << (offs))
#define GET_FIELD(val, size, start) (((val) >> (start)) & MASK(size))

/* Simple wrapper for lg2 in order to avoid manual calculation of field sizes */
#define LOG2(x) \
	((x <= POW2(1)) ? 1 : \
	 (x <= POW2(2)) ? 2 : \
	 (x <= POW2(3)) ? 3 : \
	 (x <= POW2(4)) ? 4 : \
	 (x <= POW2(5)) ? 5 : \
	 (x <= POW2(6)) ? 6 : \
	 (x <= POW2(7)) ? 7 : \
	 (x <= POW2(8)) ? 8 : ~0)


enum s_type_size {
	S_TYPE_SIZE_c,
	S_TYPE_SIZE_h,
	S_TYPE_SIZE_i,
	S_TYPE_SIZE_l,
	S_TYPE_SIZE_ll,

	S_TYPE_SIZE_COUNT
};

#define S_TYPE_SIZE_FIELD_OFFS 0
#define S_TYPE_SIZE_FIELD_SIZE LOG2(S_TYPE_SIZE_COUNT)

#define S_TYPE_SIZE(val) \
	((enum s_type_size)GET_FIELD(val, S_TYPE_SIZE_FIELD_SIZE, \
		S_TYPE_SIZE_FIELD_OFFS))
#define S_TYPE_SIZE_FIELD(val) \
	FIELD(val, S_TYPE_SIZE_FIELD_SIZE, S_TYPE_SIZE_FIELD_OFFS)


enum s_type_sign {
	S_TYPE_SIGN_implicit, /* for char */
	S_TYPE_SIGN_signed,
	S_TYPE_SIGN_unsigned,

	S_TYPE_SIGN_COUNT
};

#define S_TYPE_SIGN_FIELD_OFFS \
	(S_TYPE_SIZE_FIELD_OFFS + S_TYPE_SIZE_FIELD_SIZE)
#define S_TYPE_SIGN_FIELD_SIZE LOG2(S_TYPE_SIGN_COUNT)

#define S_TYPE_SIGN(val) \
	(enum s_type_sign()GET_FIELD(val, S_TYPE_SIGN_FIELD_SIZE, \
		S_TYPE_SIGN_FIELD_OFFS))
#define S_TYPE_SIGN_FIELD(val) \
	FIELD(val, S_TYPE_SIGN_FIELD_SIZE, S_TYPE_SIGN_FIELD_OFFS)


enum s_type_fmt {
	S_TYPE_FMT_default, /* Some default representation */
	S_TYPE_FMT_hex,
	S_TYPE_FMT_octal,
	S_TYPE_FMT_char,

	S_TYPE_FMT_COUNT
};

#define S_TYPE_FMT_FIELD_OFFS \
	(S_TYPE_SIGN_FIELD_OFFS + S_TYPE_SIGN_FIELD_SIZE)
#define S_TYPE_FMT_FIELD_SIZE LOG2(S_TYPE_FMT_COUNT)

#define S_TYPE_FMT(val) \
	((enum s_type_fmt)GET_FIELD(val, S_TYPE_FMT_FIELD_SIZE, \
		S_TYPE_FMT_FIELD_OFFS))
#define S_TYPE_FMT_FIELD(val) \
	FIELD(val, S_TYPE_FMT_FIELD_SIZE, S_TYPE_FMT_FIELD_OFFS)


enum s_type_kind {
	S_TYPE_KIND_num,
	S_TYPE_KIND_str,
	S_TYPE_KIND_addr,
	S_TYPE_KIND_fd,
	S_TYPE_KIND_path,
	S_TYPE_KIND_flags,
	S_TYPE_KIND_changeable,
	S_TYPE_KIND_changeable_void,

	S_TYPE_KIND_COUNT
};

#define S_TYPE_KIND_FIELD_OFFS \
	(S_TYPE_FMT_FIELD_OFFS + S_TYPE_FMT_FIELD_SIZE)
#define S_TYPE_KIND_FIELD_SIZE LOG2(S_TYPE_KIND_COUNT)

#define S_TYPE_KIND(val) \
	((enum s_type_kind)GET_FIELD(val, S_TYPE_KIND_FIELD_SIZE, \
		S_TYPE_KIND_FIELD_OFFS))
#define S_TYPE_KIND_FIELD(val) \
	FIELD(val, S_TYPE_KIND_FIELD_SIZE, S_TYPE_KIND_FIELD_OFFS)

#define S_TYPE_DEF(_size, _sign, _fmt, _kind) \
	(S_TYPE_SIZE_FIELD(S_TYPE_SIZE_##_size) | \
		S_TYPE_SIGN_FIELD(S_TYPE_SIGN_##_sign) | \
		S_TYPE_FMT_FIELD(S_TYPE_FMT_##_fmt) | \
		S_TYPE_KIND_FIELD(S_TYPE_KIND_##_kind))

enum s_type {
	S_TYPE_c        = S_TYPE_DEF(c,  implicit, default, num),
	S_TYPE_d        = S_TYPE_DEF(i,  signed,   default, num),
	S_TYPE_ld       = S_TYPE_DEF(l,  signed,   default, num),
	S_TYPE_lld      = S_TYPE_DEF(ll, signed,   default, num),

	S_TYPE_u        = S_TYPE_DEF(i,  unsigned, default, num),
	S_TYPE_lu       = S_TYPE_DEF(l,  unsigned, default, num),
	S_TYPE_llu      = S_TYPE_DEF(ll, unsigned, default, num),

	S_TYPE_x        = S_TYPE_DEF(i,  unsigned, hex,     num),
	S_TYPE_lx       = S_TYPE_DEF(l,  unsigned, hex,     num),
	S_TYPE_llx      = S_TYPE_DEF(ll, unsigned, hex,     num),

	S_TYPE_o        = S_TYPE_DEF(i,  unsigned, octal,   num),
	S_TYPE_lo       = S_TYPE_DEF(l,  unsigned, octal,   num),
	S_TYPE_llo      = S_TYPE_DEF(ll, unsigned, octal,   num),

	S_TYPE_str      = S_TYPE_DEF(l,  unsigned, default, str),
	S_TYPE_addr     = S_TYPE_DEF(l,  unsigned, default, addr),
	S_TYPE_fd       = S_TYPE_DEF(i,  unsigned, default, fd),
	S_TYPE_path     = S_TYPE_DEF(l,  unsigned, default, path),

	S_TYPE_flags    = S_TYPE_DEF(i,  unsigned, default, flags),
	S_TYPE_flags_l  = S_TYPE_DEF(l,  unsigned, default, flags),
	S_TYPE_flags_ll = S_TYPE_DEF(ll, unsigned, default, flags),

	S_TYPE_changeable =
		S_TYPE_DEF(i,  implicit, default, changeable),
	/** the value is write-only */
	S_TYPE_changeable_void =
		S_TYPE_DEF(i,  implicit, default, changeable_void),
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
