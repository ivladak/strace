#ifndef STRACE_STRUCTURED_INLINES_H
#define STRACE_STRUCTURED_INLINES_H

#include <linux/limits.h>

static inline int
s_umoven_verbose(struct tcb *tcp, const long addr, const unsigned int len,
	void *our_addr)
{
	if (!addr || !verbose(tcp) || (exiting(tcp) && syserror(tcp)) ||
	    umoven(tcp, addr, len, our_addr) < 0)
		return -1;

	return 0;
}

#define s_umove_verbose(pid, addr, objp)	\
	s_umoven_verbose((pid), (addr), sizeof(*(objp)), (void *) (objp))

#define DEF_UMOVE_LONG(NAME, TYPE) \
	static inline int \
	s_umove_##NAME(struct tcb *tcp, unsigned long addr, TYPE *val) \
	{ \
		int ret; \
		\
		if (current_wordsize > sizeof(int)) { \
			unsigned long long tmp; \
			\
			if (!(ret = s_umove_verbose(tcp, addr, &tmp))) \
				*val = tmp; \
		} else { \
			unsigned int tmp; \
			\
			if (!(ret = s_umove_verbose(tcp, addr, &tmp))) \
				*val = tmp; \
		} \
		\
		return ret; \
	}

DEF_UMOVE_LONG(slong, long)
DEF_UMOVE_LONG(ulong, unsigned long)


static inline void
s_insert_ellipsis(void)
{
	s_ellipsis_new_and_insert();
}

static inline void
s_insert_addr(const char *name, long value)
{
	s_addr_new_and_insert(name, value, NULL);
}

static inline void
s_push_addr(const char *name)
{
	unsigned long long addr;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, S_TYPE_addr, &addr);
	s_insert_addr(name, addr);
}

/* internal */
static inline void
s_insert_addr_arg(const char *name, unsigned long value, struct s_arg *arg)
{
	s_addr_new_and_insert(name, value, arg);
}

static inline void
s_push_addr_addr(const char *name)
{
	unsigned long long addr;
	unsigned long addr_addr = 0;
	struct s_arg *arg = NULL;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, S_TYPE_addr, &addr);

	if (!s_umove_ulong(current_tcp, addr, &addr_addr))
		arg = S_TYPE_TO_ARG(s_addr_new(name, addr_addr, NULL));

	s_insert_addr_arg(name, addr, arg);
}

struct s_fetch_wrapper_args {
	s_fill_arg_fn fill_fn;
	void *fill_fn_data;
	size_t data_size;
	void *buf;
};

/** Generic data fetch wrapper, gets data from tracee and calls fill wrapper. */
static inline ssize_t
s_fetch_wrapper(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	struct s_fetch_wrapper_args *args = fn_data;
	void *buf = NULL;
	void *outbuf = args->buf;
	ssize_t ret = -1;

	if (!outbuf) {
		if (use_alloca(args->data_size)) {
			outbuf = alloca(args->data_size);
		} else {
			buf = outbuf = xmalloc(args->data_size);
		}
	}

	if (!umoven(current_tcp, addr, args->data_size, outbuf)) {
		args->fill_fn(arg, outbuf, args->data_size, args->fill_fn_data);
		ret = args->data_size;
	} else {
		ret = -1;
	}

	free(buf);

	return ret;
}

/* internal */
/**
 * @return Amount of bytes read from tracee or -1 on error.
 */
static inline ssize_t
s_insert_addr_type(const char *name, unsigned long value, enum s_type type,
	s_fetch_fill_arg_fn fetch_fill_cb, void *fn_data)
{
	struct s_addr *addr = s_addr_new_and_insert(name, value, NULL);
	ssize_t ret = -1;

	if (!value)
		return -1;

	addr->val = s_arg_new_init(current_tcp, type, name);

	/* XXX Rewrite */
	switch (type) {
	case S_TYPE_struct:
	case S_TYPE_array:
		s_struct_enter(S_ARG_TO_TYPE(addr->val, struct));
		break;

	default:
		break;
	}

	ret = fetch_fill_cb(addr->val, value, fn_data);

	switch (type) {
	case S_TYPE_struct:
	case S_TYPE_array:
		s_syscall_pop(current_tcp->s_syscall);
		break;

	default:
		break;
	}

	if (ret < 0) {
		s_arg_free(addr->val);
		addr->val = NULL;
	}

	return ret;
}

/**
 * @return Amount of bytes read from tracee or -1 on error.
 */
static inline ssize_t
s_push_addr_type(const char *name, enum s_type type,
	s_fetch_fill_arg_fn fetch_fill_cb, void *fn_data)
{
	struct s_syscall *syscall = current_tcp->s_syscall;
	unsigned long long addr;

	s_syscall_pop_all(syscall);
	s_syscall_cur_arg_advance(syscall, S_TYPE_addr, &addr);

	return s_insert_addr_type(name, addr, type, fetch_fill_cb, fn_data);
}

static inline ssize_t
s_insert_addr_type_sized(const char *name, unsigned long value,
	unsigned long size, enum s_type type, s_fill_arg_fn fill_cb, void *fn_data)
{
	struct s_fetch_wrapper_args fetcher_args = {
		.fill_fn = fill_cb,
		.fill_fn_data = fn_data,
		.data_size = size,
	};

	return s_insert_addr_type(name, value, type, s_fetch_wrapper, &fetcher_args);
}

static inline ssize_t
s_push_addr_type_sized(const char *name, unsigned long size, enum s_type type,
	s_fill_arg_fn fill_cb, void *fn_data)
{
	struct s_syscall *syscall = current_tcp->s_syscall;
	unsigned long long addr;

	s_syscall_pop_all(syscall);
	s_syscall_cur_arg_advance(syscall, S_TYPE_addr, &addr);

	return s_insert_addr_type_sized(name, addr, size, type, fill_cb, fn_data);
}

struct s_array_fetch_wrapper_args {
	s_fill_arg_fn fill_fn;
	void *fill_fn_data;
	size_t nmemb;
	size_t memb_size;
	void *buf;
	enum s_type type;
};

static inline ssize_t
s_array_fetch_wrapper(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	const struct s_array_fetch_wrapper_args *args = fn_data;
	const size_t size = args->nmemb * args->memb_size;
	const unsigned long end_addr = addr + size;
	const unsigned long abbrev_end =
		(abbrev(current_tcp) && max_strlen < args->nmemb) ?
			addr + args->memb_size * max_strlen : end_addr;
	void *buf = NULL;
	void *outbuf = args->buf;
	ssize_t res = 0;
	unsigned long cur;
	struct s_arg *cur_arg;
	bool last = false;

	if (!args->nmemb)
		return 0;
	if (end_addr <= addr || size / args->memb_size != args->nmemb)
		return -1;

	if (!outbuf) {
		if (use_alloca(args->memb_size)) {
			outbuf = alloca(args->memb_size);
		} else {
			buf = outbuf = xmalloc(args->memb_size);
		}
	}

	for (cur = addr; cur < end_addr; cur += args->memb_size) {
		if (s_umoven_verbose(current_tcp, cur, args->memb_size, outbuf))
		{
			if (!res)
				res = -1;

			/* s_insert_ellipsis(); */
			s_insert_addr(NULL, cur);

			break;
		}

		if ((cur >= abbrev_end) || last) {
			s_insert_ellipsis();
			break;
		}

		cur_arg = s_arg_new_init(current_tcp, args->type, NULL);

		s_arg_insert(current_tcp->s_syscall, cur_arg, -1);

		/* XXX Rewrite */
		switch (args->type) {
		case S_TYPE_struct:
		case S_TYPE_array:
			s_struct_enter(S_ARG_TO_TYPE(cur_arg, struct));
			break;

		default:
			break;
		}

		if (args->fill_fn(cur_arg, outbuf, args->memb_size,
		    args->fill_fn_data) < 0)
			last = true;

		switch (args->type) {
		case S_TYPE_struct:
		case S_TYPE_array:
			s_syscall_pop(current_tcp->s_syscall);
			break;

		default:
			break;
		}

		res += args->memb_size;
	}

	free(buf);

	return res;
}

/**
 * Differs from a_insert_addr_type_sized in that it interprets pointed not as
 * single item but as an array of items.
 */
static inline ssize_t
s_insert_array_type(const char *name, unsigned long value, size_t nmemb,
	size_t elem_size, enum s_type type, s_fill_arg_fn fill_cb, void *fn_data)
{
	struct s_array_fetch_wrapper_args fetcher_args = {
		.fill_fn = fill_cb,
		.fill_fn_data = fn_data,
		.nmemb = nmemb,
		.memb_size = elem_size,
		.type = type,
	};

	return s_insert_addr_type(name, value, S_TYPE_array, s_array_fetch_wrapper,
		&fetcher_args);
}

static inline ssize_t
s_push_array_type(const char *name, size_t nmemb, size_t elem_size,
	enum s_type type, s_fill_arg_fn fill_cb, void *fn_data)
{
	struct s_syscall *syscall = current_tcp->s_syscall;
	unsigned long long addr;

	s_syscall_pop_all(syscall);
	s_syscall_cur_arg_advance(syscall, S_TYPE_addr, &addr);

	return s_insert_array_type(name, addr, nmemb, elem_size, type, fill_cb,
		fn_data);
}


static inline void
s_insert_num(enum s_type type, const char *name, uint64_t value)
{
	s_num_new_and_insert(type, name, value);
}

static inline void
s_push_num(enum s_type type, const char *name)
{
	struct s_syscall *syscall = current_tcp->s_syscall;
	unsigned long long val;

	s_syscall_pop_all(syscall);
	s_syscall_cur_arg_advance(syscall, type, &val);
	s_insert_num(type, name, val);
}

#define DEF_PUSH_INT(TYPE, ENUM, UMOVE_FUNC) \
	static inline void \
	s_insert_##ENUM(const char *name, TYPE value) \
	{ \
		s_insert_num(S_TYPE_##ENUM, name, (uint64_t) value); \
	} \
	\
	static inline void \
	s_insert_##ENUM##_addr(const char *name, unsigned long addr) \
	{ \
		TYPE val = 0; \
		struct s_arg *arg = NULL; \
		\
		if (!UMOVE_FUNC(current_tcp, addr, &val)) \
			arg = S_TYPE_TO_ARG(s_num_new(S_TYPE_##ENUM, name, \
				val)); \
		\
		s_insert_addr_arg(name, addr, arg); \
	} \
	\
	static inline void \
	s_push_##ENUM(const char *name) \
	{ \
		s_push_num(S_TYPE_##ENUM, name); \
	} \
	\
	static inline void \
	s_push_##ENUM##_addr(const char *name) \
	{ \
		struct s_syscall *syscall = current_tcp->s_syscall; \
		unsigned long long addr; \
		\
		s_syscall_pop_all(syscall); \
		s_syscall_cur_arg_advance(syscall, S_TYPE_addr, &addr); \
		s_insert_##ENUM##_addr(name, addr); \
	}

DEF_PUSH_INT(int, d, s_umove_verbose)
DEF_PUSH_INT(long, ld, s_umove_slong)
DEF_PUSH_INT(long long, lld, s_umove_verbose)

DEF_PUSH_INT(unsigned, u, s_umove_verbose)
DEF_PUSH_INT(unsigned long, lu, s_umove_ulong)
DEF_PUSH_INT(unsigned long long, llu, s_umove_verbose)

DEF_PUSH_INT(unsigned, o, s_umove_verbose)
DEF_PUSH_INT(unsigned long, lo, s_umove_ulong)
DEF_PUSH_INT(unsigned long long, llo, s_umove_verbose)

DEF_PUSH_INT(unsigned, x, s_umove_verbose)
DEF_PUSH_INT(unsigned long, lx, s_umove_ulong)
DEF_PUSH_INT(unsigned long long, llx, s_umove_verbose)

DEF_PUSH_INT(int, uid, s_umove_verbose)
DEF_PUSH_INT(int, gid, s_umove_verbose)
DEF_PUSH_INT(short, uid16, s_umove_verbose)
DEF_PUSH_INT(short, gid16, s_umove_verbose)
DEF_PUSH_INT(long, time, s_umove_verbose)
DEF_PUSH_INT(int, fd, s_umove_verbose)
DEF_PUSH_INT(int, dirfd, s_umove_verbose)
DEF_PUSH_INT(int, signo, s_umove_verbose)
DEF_PUSH_INT(int, scno, s_umove_verbose)
DEF_PUSH_INT(int, wstatus, s_umove_verbose)
DEF_PUSH_INT(unsigned, rlim32, s_umove_verbose)
DEF_PUSH_INT(unsigned long long, rlim64, s_umove_verbose)

DEF_PUSH_INT(unsigned long,  umask,   s_umove_ulong)
DEF_PUSH_INT(unsigned short, umode_t, s_umove_verbose)
DEF_PUSH_INT(unsigned int,   mode_t,  s_umove_verbose)
DEF_PUSH_INT(unsigned short, short_mode_t,  s_umove_verbose)
DEF_PUSH_INT(unsigned long,  sa_handler, s_umove_ulong)
DEF_PUSH_INT(unsigned int,   dev_t,   s_umove_verbose)

#undef DEF_PUSH_INT


static inline void
s_push_ptrace_uaddr(const char *name)
{
	unsigned long long addr;
	struct s_addr *addr_arg;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, S_TYPE_addr, &addr);
	addr_arg = s_addr_new_and_insert(name, addr, NULL);

	addr_arg->arg.type = S_TYPE_ptrace_uaddr;
}


static inline void
s_insert_string(enum s_type type, const char *name, unsigned long addr,
	long len, bool has_nul)
{
	struct s_str *str = s_str_new(type, name, addr, len, has_nul);

	s_insert_addr_arg(name, addr, str ? S_TYPE_TO_ARG(str) : NULL);
}

/* Equivalent to s_insert_path_addr */
static inline void
s_insert_path(const char *name, unsigned long addr)
{
	s_insert_string(S_TYPE_path, name, addr, PATH_MAX, true);
}

/* Equivalent to s_push_path_addr */
static inline void
s_push_path(const char *name)
{
	unsigned long long addr;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, S_TYPE_addr, &addr);
	s_insert_path(name, addr);
}

/* Equivalent to s_insert_path_addr */
static inline void
s_insert_pathn(const char *name, unsigned long addr, size_t size)
{
	s_insert_string(S_TYPE_path, name, addr, size, true);
}

/* Equivalent to s_push_path_addr */
static inline void
s_push_pathn(const char *name, size_t size)
{
	unsigned long long addr;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, S_TYPE_addr, &addr);
	s_insert_pathn(name, addr, size);
}

/* Equivalent to s_insert_str_addr */
static inline void
s_insert_str(const char *name, unsigned long addr, long len)
{
	s_insert_string(S_TYPE_str, name, addr, len, len == -1);
}

static inline void
s_insert_str_val(const char *name, const char *str, long len)
{
	struct s_str *s = s_str_val_new(S_TYPE_str, name, str, len, true);

	s_arg_insert(current_tcp->s_syscall, &s->arg, -1);
}

/* Equivalent to s_push_str_addr */
static inline void
s_push_str(const char *name, long len)
{
	unsigned long long addr;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, S_TYPE_addr, &addr);
	s_insert_str(name, addr, len);
}


static inline void
s_insert_struct(const char *name)
{
	struct s_struct *s = s_struct_new_and_insert(S_TYPE_struct, name);
	s_struct_enter(s);
}

static inline void
s_insert_struct_addr(const char *name, unsigned long addr)
{
	struct s_struct *s = s_struct_new_and_insert(S_TYPE_struct, name);

	s_insert_addr_arg(name, addr, &s->arg);
	s_struct_enter(s);
}

/* Equivalent to s_push_struct_addr */
static inline void
s_push_struct(const char *name)
{
	unsigned long long addr;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, S_TYPE_addr, &addr);
	s_insert_struct_addr(name, addr);
}

static inline void
s_insert_array(const char *name)
{
	struct s_struct *s = s_struct_new_and_insert(S_TYPE_array, name);
	s_struct_enter(s);
}

static inline void
s_insert_array_addr(const char *name, unsigned long addr)
{
	struct s_struct *s = s_struct_new_and_insert(S_TYPE_array, name);

	s_insert_addr_arg(name, addr, &s->arg);
	s_struct_enter(s);
}

/* Equivalent to s_push_array_addr */
static inline void
s_push_array(const char *name)
{
	unsigned long long addr;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, S_TYPE_addr, &addr);
	s_insert_array_addr(name, addr);
}

static inline void
s_struct_finish(void)
{
	s_syscall_pop(current_tcp->s_syscall);
}

static inline void
s_array_finish(void)
{
	s_syscall_pop(current_tcp->s_syscall);
}


static inline void
s_insert_xlat(enum s_type type, const char *name, const struct xlat *x,
	uint64_t val, const char *dflt, bool flags, int8_t scale)
{
	s_xlat_new_and_insert(type, name, x, val, dflt, flags, scale);
}

static inline void
s_push_xlat(const char *name, const struct xlat *x, const char *dflt,
	bool flags, enum s_type type, int8_t scale)
{
	unsigned long long val;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, type, &val);
	s_insert_xlat(type, name, x, val, dflt, flags, scale);
}

static inline void
s_append_xlat_val(enum s_type type, const char *name, const struct xlat *x,
	uint64_t val, const char *dflt, bool flags, int8_t scale)
{
	s_xlat_append(type, name, x, val, dflt, flags, scale);
}

static inline void
s_append_xlat(const char *name, const struct xlat *x, const char *dflt,
	bool flags, enum s_type type, int8_t scale)
{
	unsigned long long val;

	s_syscall_cur_arg_advance(current_tcp->s_syscall, type, &val);
	s_append_xlat_val(type, name, x, val, dflt, flags, scale);
}

#define DEF_XLAT_FUNCS(WHAT, TYPE, ENUM, FLAGS_TYPE, FLAGS, UMOVE_FUNC) \
	static inline void \
	s_insert_##WHAT##_##ENUM(const char *name, const struct xlat *x, \
		TYPE val, const char *dflt) \
	{ \
		s_insert_xlat(S_TYPE_##FLAGS_TYPE, name, x, val, dflt, FLAGS, \
			0); \
	} \
	\
	static inline void \
	s_insert_##WHAT##_##ENUM##_scaled(const char *name, \
		const struct xlat *x, TYPE val, const char *dflt, int8_t scale)\
	{ \
		s_insert_xlat(S_TYPE_##FLAGS_TYPE, name, x, val, dflt, FLAGS, \
			scale); \
	} \
	\
	static inline void \
	s_insert_##WHAT##_##ENUM##_addr(const char *name, \
		const struct xlat *x, unsigned long addr, const char *dflt) \
	{ \
		TYPE val = 0; \
		struct s_arg *arg = NULL; \
		\
		if (!UMOVE_FUNC(current_tcp, addr, &val)) \
			arg = S_TYPE_TO_ARG(s_xlat_new(S_TYPE_##FLAGS_TYPE, \
				name, x, val, dflt, FLAGS, 0)); \
		\
		s_insert_addr_arg(name, addr, arg); \
	} \
	\
	static inline void \
	s_push_##WHAT##_##ENUM(const char *name, const struct xlat *x, \
		const char *dflt) \
	{ \
		s_push_xlat(name, x, dflt, FLAGS, S_TYPE_##FLAGS_TYPE, 0); \
	} \
	\
	static inline void \
	s_push_##WHAT##_##ENUM##_addr(const char *name, const struct xlat *x, \
		const char *dflt) \
	{ \
		unsigned long long addr; \
		\
		s_syscall_pop_all(current_tcp->s_syscall); \
		s_syscall_cur_arg_advance(current_tcp->s_syscall, S_TYPE_addr, \
			&addr); \
		s_insert_##WHAT##_##ENUM##_addr(name, x, addr, dflt); \
	} \
	\
	static inline void \
	s_append_##WHAT##_##ENUM##_val(const char *name, const struct xlat *x, \
		TYPE val, const char *dflt) \
	{ \
		s_append_xlat_val(S_TYPE_##FLAGS_TYPE, name, x, val, dflt, \
			FLAGS, 0); \
	} \
	\
	static inline void \
	s_append_##WHAT##_##ENUM(const char *name, const struct xlat *x, \
		const char *dflt) \
	{ \
		s_append_xlat(name, x, dflt, FLAGS, S_TYPE_##FLAGS_TYPE, 0); \
	} \
	\
	static inline void \
	s_append_##WHAT##_##ENUM##_val_scaled(const char *name, \
		const struct xlat *x, TYPE val, const char *dflt, \
		int8_t scale) \
	{ \
		s_append_xlat_val(S_TYPE_##FLAGS_TYPE, name, x, val, dflt, \
			FLAGS, scale); \
	} \
	\
	static inline void \
	s_append_##WHAT##_##ENUM##_scaled(const char *name, \
		const struct xlat *x, const char *dflt, uint8_t scale) \
	{ \
		s_append_xlat(name, x, dflt, FLAGS, S_TYPE_##FLAGS_TYPE, \
			scale); \
	}

#define DEF_XLAT(TYPE, ENUM, FLAGS_TYPE, UMOVE_FUNC) \
	DEF_XLAT_FUNCS(flags, TYPE, ENUM, FLAGS_TYPE, true, UMOVE_FUNC) \
	DEF_XLAT_FUNCS(xlat, TYPE, ENUM, FLAGS_TYPE, false, UMOVE_FUNC)

DEF_XLAT(unsigned, int, xlat, s_umove_verbose)
DEF_XLAT(unsigned, uint, xlat_u, s_umove_verbose)
DEF_XLAT(int, signed, xlat_d, s_umove_verbose)
DEF_XLAT(unsigned long, long, xlat_l, s_umove_ulong)
DEF_XLAT(uint64_t, 64, xlat_ll, s_umove_verbose)

#undef DEF_XLAT
#undef DEF_XLAT_FUNCS

static inline void
s_insert_xlat_flags(enum s_type ftype, enum s_type vtype, const char *name,
	const struct xlat *fx, const struct xlat *vx, uint64_t val,
	int64_t mask, const char *flags_dflt, const char *val_dflt,
	bool preceding_xlat, int8_t xlat_scale)
{
	uint64_t flags = val & ~mask;
	val = val & mask;

	if (!flags && !preceding_xlat) {
		s_insert_xlat(vtype, name, vx, val, val_dflt, false,
			xlat_scale);
	} else if (!val && preceding_xlat) {
		s_insert_xlat(ftype, name, fx, flags, flags_dflt,
			true, 0);
	} else {
		if (preceding_xlat) {
			s_insert_xlat(vtype, name, vx, val, val_dflt, false,
				xlat_scale);
			s_append_xlat_val(ftype, name, fx, flags, flags_dflt,
				true, 0);
		} else {
			s_insert_xlat(ftype, name, fx, flags, flags_dflt,
				true, 0);
			s_append_xlat_val(vtype, name, vx, val, val_dflt, false,
				xlat_scale);
		}
	}
}

static inline void
s_push_xlat_flags(enum s_type ftype, enum s_type vtype, const char *name,
	const struct xlat *fx, const struct xlat *vx,
	int64_t mask, const char *flags_dflt, const char *val_dflt,
	bool preceding_xlat, int8_t xlat_scale)
{
	unsigned long long val;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, ftype, &val);
	s_insert_xlat_flags(ftype, vtype, name, fx, vx, val, mask, flags_dflt,
		val_dflt, preceding_xlat, xlat_scale);
}

#define DEF_XLAT_FLAGS(ENUM, FLAGS_TYPE, VAL_TYPE) \
	static inline void \
	s_insert_xlat_flags_##ENUM(const char *name, const struct xlat *fx, \
		const struct xlat *vx, uint64_t val, int64_t mask, \
		const char *flags_dflt, const char *val_dflt, \
		bool preceding_xlat, int8_t xlat_scale) \
	{ \
		s_insert_xlat_flags(FLAGS_TYPE, VAL_TYPE, name, fx, vx, val, \
			mask, flags_dflt, val_dflt, preceding_xlat, \
			xlat_scale); \
	} \
	\
	static inline void \
	s_push_xlat_flags_##ENUM(const char *name, const struct xlat *fx, \
		const struct xlat *vx, int64_t mask, const char *flags_dflt, \
		const char *val_dflt, bool preceding_xlat, int8_t xlat_scale) \
	{ \
		s_push_xlat_flags(FLAGS_TYPE, VAL_TYPE, name, fx, vx, mask, \
			flags_dflt, val_dflt, preceding_xlat, xlat_scale); \
	}

DEF_XLAT_FLAGS(int,  S_TYPE_xlat,    S_TYPE_xlat_d)
DEF_XLAT_FLAGS(long, S_TYPE_xlat_l,  S_TYPE_xlat_ld)
DEF_XLAT_FLAGS(64,   S_TYPE_xlat_ll, S_TYPE_xlat_lld)

#undef DEF_XLAT_FLAGS

/* Special version for xlat_search replacement */
static inline void
s_insert_xlat_64_sorted(const char *name, const struct xlat *x, size_t n_memb,
	uint64_t val)
{
	s_insert_xlat(S_TYPE_xlat_ll, name, NULL, val,
		xlat_search(x, n_memb, val), false, 0);
}

static inline void
s_changeable(void)
{
	s_last_is_changeable(current_tcp);
}

static inline void
s_changeable_void(const char *name)
{
	s_syscall_cur_arg_advance(current_tcp->s_syscall, S_TYPE_addr, NULL);

	if (entering(current_tcp))
		s_changeable_new_and_insert(name, NULL, NULL);
}

static inline void
s_push_empty(enum s_type type)
{
	unsigned long long val;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, type, &val);
}

#endif /* #ifndef STRACE_STRUCTURED_INLINES_H */
