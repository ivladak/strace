#ifndef STRACE_STRUCTURED_H
#define STRACE_STRUCTURED_H

#include <stdbool.h>
#include <stdint.h>

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


enum s_syscall_type {
	S_SCT_SYSCALL,
	S_SCT_SIGNAL,
};

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

	S_TYPE_FMT_uid,
	S_TYPE_FMT_gid,
	S_TYPE_FMT_time,
	S_TYPE_FMT_dirfd,
	S_TYPE_FMT_signo,
	S_TYPE_FMT_scno,
	S_TYPE_FMT_wstatus,
	S_TYPE_FMT_rlim32,
	S_TYPE_FMT_rlim64,
	S_TYPE_FMT_ptrace_uaddr,
	S_TYPE_FMT_sa_handler,

	S_TYPE_FMT_umask,
	S_TYPE_FMT_umode_t,
	S_TYPE_FMT_mode_t,

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
	S_TYPE_KIND_xlat,
	S_TYPE_KIND_sigmask,
	S_TYPE_KIND_struct,
	S_TYPE_KIND_array,
	S_TYPE_KIND_ellipsis,
	S_TYPE_KIND_changeable,

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

	S_TYPE_uid      = S_TYPE_DEF(i,  unsigned, uid,     num),
	S_TYPE_gid      = S_TYPE_DEF(i,  unsigned, gid,     num),
	S_TYPE_uid16    = S_TYPE_DEF(h,  unsigned, uid,     num),
	S_TYPE_gid16    = S_TYPE_DEF(h,  unsigned, gid,     num),
	S_TYPE_time     = S_TYPE_DEF(i,  unsigned, time,    num),

	S_TYPE_signo    = S_TYPE_DEF(i,  unsigned, signo,   num),
	S_TYPE_scno     = S_TYPE_DEF(i,  unsigned, scno,    num),
	S_TYPE_wstatus  = S_TYPE_DEF(i,  unsigned, wstatus, num),
	S_TYPE_rlim32   = S_TYPE_DEF(i,  unsigned, rlim32,  num),
	S_TYPE_rlim64   = S_TYPE_DEF(ll, unsigned, rlim64,  num),

	S_TYPE_umask    = S_TYPE_DEF(l,  unsigned, umask,   num),
	S_TYPE_umode_t  = S_TYPE_DEF(h,  unsigned, umode_t, num),
	S_TYPE_mode_t   = S_TYPE_DEF(i,  unsigned, mode_t,  num),

	S_TYPE_sa_handler = S_TYPE_DEF(l,  unsigned, sa_handler, num),

	S_TYPE_str      = S_TYPE_DEF(l,  unsigned, default, str),
	S_TYPE_addr     = S_TYPE_DEF(l,  unsigned, default, addr),
	S_TYPE_fd       = S_TYPE_DEF(i,  unsigned, default, fd),
	S_TYPE_dirfd    = S_TYPE_DEF(i,  unsigned, dirfd,   fd),

	S_TYPE_path     = S_TYPE_DEF(l,  unsigned, default, path),

	S_TYPE_ptrace_uaddr = S_TYPE_DEF(l,  unsigned, ptrace_uaddr, addr),

	S_TYPE_xlat     = S_TYPE_DEF(i,  unsigned, hex,     xlat),
	S_TYPE_xlat_l   = S_TYPE_DEF(l,  unsigned, hex,     xlat),
	S_TYPE_xlat_ll  = S_TYPE_DEF(ll, unsigned, hex,     xlat),

	S_TYPE_xlat_d   = S_TYPE_DEF(i,  unsigned, default, xlat),
	S_TYPE_xlat_ld  = S_TYPE_DEF(l,  unsigned, default, xlat),
	S_TYPE_xlat_lld = S_TYPE_DEF(ll, unsigned, default, xlat),

	S_TYPE_sigmask  = S_TYPE_DEF(l,  unsigned, default, sigmask),

	S_TYPE_struct   = S_TYPE_DEF(i,  unsigned, default, struct),
	S_TYPE_array    = S_TYPE_DEF(i,  unsigned, default, array),

	S_TYPE_ellipsis = S_TYPE_DEF(i,  unsigned, default, ellipsis),

	S_TYPE_changeable =
		S_TYPE_DEF(i,  implicit, default, changeable),
};

#define S_ARG_TO_TYPE(_arg, _type) __containerof(_arg, struct s_##_type, arg)
#define S_TYPE_TO_ARG(_p) (&((_p)->arg))

/* syscall */

struct s_syscall;

struct s_arg {
	struct s_syscall *syscall;
	const char *name;
	enum s_type type;
	int arg_num;

	STAILQ_ENTRY(s_arg) entry;
	STAILQ_ENTRY(s_arg) chg_entry;
};

STAILQ_HEAD(args_queue, s_arg);

struct s_args {
	struct args_queue args;
	SLIST_ENTRY(s_args) entry;
};

SLIST_HEAD(s_args_list, s_args);

struct s_syscall {
	struct tcb *tcp;
	int cur_arg;
	int last_arg;
	struct s_arg *last_changeable;
	struct s_args args;
	struct args_queue changeable_args;
	enum s_type ret_type;
	enum s_syscall_type type;

	struct s_args_list insertion_stack;
};

/* struct */

struct s_struct {
	struct s_arg arg;

	struct s_args args;
	/** Auxiliary non-standard string representation */
	union {
		const char *aux_str;
		char *own_aux_str;
	};
	bool own;
};

/* complex arguments */

struct s_num {
	struct s_arg arg;

	uint64_t val;
};

struct s_xlat {
	struct s_arg arg;

	const struct xlat *x;
	uint64_t val;
	const char *dflt;
	bool flags;

	/* poor man's chaining in order to avoid creating separate list head. */
	struct s_xlat *next;
	struct s_xlat *last;
};

struct s_str {
	struct s_arg arg;

	char *str;
	long len;
	bool has_nul;
};

struct s_addr {
	struct s_arg arg;

	unsigned long addr;
	struct s_arg *val;
};

struct s_ellipsis {
	struct s_arg arg;
};

/* Stored in structured_sigmask.h due to inclusion of signal.h */
struct s_sigmask;

struct s_changeable {
	struct s_arg arg;

	struct s_arg *entering;
	struct s_arg *exiting;
};

struct s_printer {
	void (*print_before)(struct tcb *tcp);
	void (*print_entering)(struct tcb *tcp);
	void (*print_exiting)(struct tcb *tcp);
	void (*print_after)(struct tcb *tcp);
	void (*print_resumed)(struct tcb *tcp);
	void (*print_tv)(struct tcb *tcp, struct timeval *tv);
	void (*print_unavailable_entering)(struct tcb *tcp, int scno_good);
	void (*print_unavailable_exiting)(struct tcb *tcp);
	void (*print_signal)(struct tcb *tcp);
};

enum syscall_print_xlat_bits {
	SPXF_FIRST_BIT,
	SPXF_DEFAULT_BIT,
};

enum syscall_print_xlat_flags {
	SPXF_FIRST    = POW2(SPXF_FIRST_BIT),
	SPXF_DEFAULT  = POW2(SPXF_DEFAULT_BIT),
};

typedef void (*s_print_xlat_fn)(enum s_type type, uint64_t value, uint64_t mask,
	const char *str, uint32_t flags, void *fn_data);
/** Returns amount of bytes read. */
typedef ssize_t (*s_fetch_fill_arg_fn)(struct s_arg *arg, unsigned long addr,
	void *fn_data);
typedef int (*s_fill_arg_fn)(struct s_arg *arg, void *buf, size_t len,
	void *fn_data);

/* prototypes */

extern void *s_arg_to_type(struct s_arg *arg);

extern struct s_arg *s_arg_new(struct tcb *tcp, enum s_type type,
	const char *name);
extern void s_arg_free(struct s_arg *arg);
extern struct s_arg *s_arg_next(struct tcb *tcp, enum s_type type,
	const char *name);
extern void s_arg_insert(struct s_syscall *syscall, struct s_arg *arg,
	int force_arg);

extern struct s_num *s_num_new(enum s_type type, const char *name,
	uint64_t value);
extern struct s_str *s_str_new(enum s_type type, const char *name,
	long addr, long len, bool has_nul);
extern struct s_addr *s_addr_new(const char *name, long addr,
	struct s_arg *arg);
extern struct s_xlat *s_xlat_new(enum s_type type, const char *name,
	const struct xlat *x, uint64_t xlat, const char *dflt, bool flags);
extern struct s_struct *s_struct_new(enum s_type type, const char *name);
extern struct s_ellipsis *s_ellipsis_new(void);
extern struct s_changeable *s_changeable_new(const char *name,
	struct s_arg *entering, struct s_arg *exiting);

extern struct s_arg *s_arg_new_init(struct tcb *tcp, enum s_type type,
	const char *name);
extern bool s_arg_equal(struct s_arg *arg1, struct s_arg *arg2);

extern struct s_num *s_num_new_and_insert(enum s_type type, const char *name,
	uint64_t value);
extern struct s_addr *s_addr_new_and_insert(const char *name, long addr,
	struct s_arg *arg);
extern struct s_xlat *s_xlat_new_and_insert(enum s_type type,
	const char *name, const struct xlat *x, uint64_t val, const char *dflt,
	bool flags);
extern struct s_struct *s_struct_new_and_insert(enum s_type type,
	const char *name);
extern struct s_ellipsis *s_ellipsis_new_and_insert(void);
extern struct s_changeable *s_changeable_new_and_insert(const char *name,
	struct s_arg *entering, struct s_arg *exiting);

extern struct s_xlat *s_xlat_append(enum s_type type, const char *name,
	const struct xlat *x, uint64_t val, const char *dflt, bool flags);
extern struct s_struct *s_struct_set_aux_str(struct s_struct *s,
	const char *aux_str);
extern struct s_struct *s_struct_set_own_aux_str(struct s_struct *s,
	char *aux_str);

extern struct args_queue *s_struct_enter(struct s_struct *s);
extern struct args_queue *s_syscall_insertion_point(struct s_syscall *s);
extern struct args_queue *s_syscall_pop(struct s_syscall *s);
extern struct args_queue *s_syscall_pop_all(struct s_syscall *s);

extern struct s_syscall *s_syscall_new(struct tcb *tcp,
	enum s_syscall_type sc_type);
extern void s_last_is_changeable(struct tcb *tcp);
extern void s_syscall_free(struct tcb *tcp);

extern int s_syscall_cur_arg_advance(struct s_syscall *syscall,
	enum s_type type, unsigned long long *val);

extern void s_process_xlat(struct s_xlat *arg, s_print_xlat_fn cb,
	void *cb_data);

extern void s_syscall_print_before(struct tcb *tcp);
extern void s_syscall_print_entering(struct tcb *tcp);
extern void s_syscall_init_exiting(struct tcb *tcp);
extern void s_syscall_print_exiting(struct tcb *tcp);
extern void s_syscall_print_after(struct tcb *tcp);
extern void s_syscall_print_resumed(struct tcb *tcp);
extern void s_syscall_print_tv(struct tcb *tcp, struct timeval *tv);
extern void s_syscall_print_unavailable_entering(struct tcb *tcp,
	int scno_good);
extern void s_syscall_print_unavailable_exiting(struct tcb *tcp);
extern void s_syscall_print_signal(struct tcb *tcp, const void *si,
	unsigned sig);

#endif /* #ifndef STRACE_STRUCTURED_H */
