#ifndef STRACE_STRUCTURED_INLINES_H
#define STRACE_STRUCTURED_INLINES_H

#include <linux/limits.h>

/* internal */
static inline void
s_insert_addr_arg(long value, struct s_arg *arg)
{
	s_addr_new_and_insert(value, arg);
}

/* internal */
/* possible optimization: allocate buf on stack */
static inline struct s_arg *
s_insert_addr_type(long value, long len, enum s_type type,
	s_fill_arg_fn fill_cb, void *fn_data)
{
	struct s_addr *addr = s_addr_new_and_insert(value, NULL);
	void *buf;

	buf = xmalloc(len);

	if (umoven(current_tcp, value, len, buf))
		goto s_insert_addr_type_cleanup;

	addr->val = s_arg_new(current_tcp, type);

	/* XXX Rewrite */
	switch (type) {
	case S_TYPE_struct:
	case S_TYPE_array:
		s_struct_enter(S_ARG_TO_TYPE(addr->val, struct));
		break;

	default:
		break;
	}

	fill_cb(addr->val, buf, len, fn_data);

s_insert_addr_type_cleanup:
	free(buf);

	return addr->val;
}


static inline void
s_insert_num(enum s_type type, uint64_t value)
{
	s_num_new_and_insert(type, value);
}

static inline void
s_push_num(enum s_type type)
{
	struct s_syscall *syscall = current_tcp->s_syscall;
	unsigned long long val;

	s_syscall_pop_all(syscall);
	s_syscall_cur_arg_advance(syscall, type, &val);
	s_insert_num(type, val);
}

#define DEF_PUSH_INT(TYPE, ENUM) \
	static inline void \
	s_insert_##ENUM(TYPE value) \
	{ \
		s_insert_num(S_TYPE_##ENUM, (uint64_t) value); \
	} \
	\
	static inline void \
	s_insert_##ENUM##_addr(long addr) \
	{ \
		TYPE val = 0; \
		struct s_arg *arg = NULL; \
		\
		if (!umove(current_tcp, addr, &val)) \
			arg = S_TYPE_TO_ARG(s_num_new(S_TYPE_##ENUM, val)); \
		\
		s_insert_addr_arg(addr, arg); \
	} \
	\
	static inline void \
	s_push_##ENUM(void) \
	{ \
		s_push_num(S_TYPE_##ENUM); \
	} \
	\
	static inline void \
	s_push_##ENUM##_addr(void) \
	{ \
		struct s_syscall *syscall = current_tcp->s_syscall; \
		\
		s_syscall_pop_all(syscall); \
		s_insert_##ENUM##_addr( \
			current_tcp->u_arg[syscall->cur_arg++]); \
	}

DEF_PUSH_INT(int, d)
DEF_PUSH_INT(long, ld)
DEF_PUSH_INT(long long, lld)

DEF_PUSH_INT(unsigned, u)
DEF_PUSH_INT(unsigned long, lu)
DEF_PUSH_INT(unsigned long long, llu)

DEF_PUSH_INT(int, o)
DEF_PUSH_INT(long, lo)
DEF_PUSH_INT(long long, llo)

DEF_PUSH_INT(int, x)
DEF_PUSH_INT(long, lx)
DEF_PUSH_INT(long long, llx)

DEF_PUSH_INT(int, fd)

#undef DEF_PUSH_INT


static inline void
s_insert_addr(long value)
{
	s_addr_new_and_insert(value, NULL);
}

static inline void
s_push_addr(void)
{
	s_syscall_pop_all(current_tcp->s_syscall);
	s_insert_addr(current_tcp->u_arg[current_tcp->s_syscall->cur_arg++]);
}


static inline void
s_insert_string(enum s_type type, long addr, long len, bool has_nul)
{
	struct s_str *str = s_str_new(type, addr, len, has_nul);

	s_insert_addr_arg(addr, str ? S_TYPE_TO_ARG(str) : NULL);
}

/* Equivalent to s_insert_path_addr */
static inline void
s_insert_path(long addr)
{
	s_insert_string(S_TYPE_path, addr, PATH_MAX, true);
}

/* Equivalent to s_push_path_addr */
static inline void
s_push_path(void)
{
	s_syscall_pop_all(current_tcp->s_syscall);
	s_insert_path(current_tcp->u_arg[current_tcp->s_syscall->cur_arg++]);
}

/* Equivalent to s_insert_str_addr */
static inline void
s_insert_str(long addr, long len)
{
	s_insert_string(S_TYPE_str, addr, len, len == -1);
}

/* Equivalent to s_push_str_addr */
static inline void
s_push_str(long len)
{
	s_syscall_pop_all(current_tcp->s_syscall);
	s_insert_str(current_tcp->u_arg[current_tcp->s_syscall->cur_arg++],
		len);
}


static inline void
s_insert_struct(void)
{
	struct s_struct *s = s_struct_new();
	s_struct_enter(s);
}

static inline void
s_insert_struct_addr(long addr)
{
	struct s_struct *s = s_struct_new();

	s_insert_addr_arg(addr, &s->arg);
	s_struct_enter(s);
}

/* Equivalent to s_push_struct_addr */
static inline void
s_push_struct(void)
{
	s_syscall_pop_all(current_tcp->s_syscall);
	s_insert_struct_addr(
		current_tcp->u_arg[current_tcp->s_syscall->cur_arg++]);
}


static inline void
s_insert_xlat(enum s_type type, const struct xlat *x, uint64_t val,
	const char *dflt, bool flags)
{
	s_xlat_new_and_insert(type, x, val, dflt, flags);
}

static inline void
s_push_xlat(const struct xlat *x, const char *dflt, bool flags,
	enum s_type type)
{
	unsigned long long val;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, type, &val);
	s_insert_xlat(type, x, val, dflt, flags);
}

static inline void
s_append_xlat_val(enum s_type type, const struct xlat *x, uint64_t val,
	const char *dflt, bool flags)
{
	s_xlat_append(type, x, val, dflt, flags);
}

static inline void
s_append_xlat(const struct xlat *x, const char *dflt, bool flags,
	enum s_type type)
{
	unsigned long long val;

	s_syscall_cur_arg_advance(current_tcp->s_syscall, type, &val);
	s_append_xlat_val(type, x, val, dflt, flags);
}

#define DEF_XLAT_FUNCS(WHAT, TYPE, ENUM, FLAGS_TYPE, FLAGS) \
	static inline void \
	s_insert_##WHAT##_##ENUM(const struct xlat *x, TYPE val, \
		const char *dflt) \
	{ \
		s_insert_xlat(S_TYPE_##FLAGS_TYPE, x, val, dflt, FLAGS); \
	} \
	\
	static inline void \
	s_insert_##WHAT##_##ENUM##_addr(const struct xlat *x, long addr, \
		const char *dflt) \
	{ \
		TYPE val = 0; \
		struct s_arg *arg = NULL; \
		\
		if (!umove(current_tcp, addr, &val)) \
			arg = S_TYPE_TO_ARG(s_xlat_new(S_TYPE_##FLAGS_TYPE, x, \
				val, dflt, FLAGS)); \
		\
		s_insert_addr_arg(addr, arg); \
	} \
	\
	static inline void \
	s_push_##WHAT##_##ENUM(const struct xlat *x, const char *dflt) \
	{ \
		s_push_xlat(x, dflt, FLAGS, S_TYPE_##FLAGS_TYPE); \
	} \
	\
	static inline void \
	s_push_##WHAT##_##ENUM##_addr(const struct xlat *x, const char *dflt) \
	{ \
		s_syscall_pop_all(current_tcp->s_syscall); \
		s_insert_##WHAT##_##ENUM##_addr(x, \
			current_tcp->u_arg[current_tcp->s_syscall->cur_arg++], \
			dflt); \
	} \
	\
	static inline void \
	s_append_##WHAT##_##ENUM##_val(const struct xlat *x, TYPE val, \
		const char *dflt) \
	{ \
		s_append_xlat_val(S_TYPE_##FLAGS_TYPE, x, val, dflt, FLAGS); \
	} \
	\
	static inline void \
	s_append_##WHAT##_##ENUM(const struct xlat *x, const char *dflt) \
	{ \
		s_append_xlat(x, dflt, FLAGS, S_TYPE_##FLAGS_TYPE); \
	}

#define DEF_XLAT(TYPE, ENUM, FLAGS_TYPE) \
	DEF_XLAT_FUNCS(flags, TYPE, ENUM, FLAGS_TYPE, true) \
	DEF_XLAT_FUNCS(xlat, TYPE, ENUM, FLAGS_TYPE, false)

DEF_XLAT(unsigned, int, xlat)
DEF_XLAT(unsigned long, long, xlat_l)
DEF_XLAT(uint64_t, 64, xlat_ll)

#undef DEF_XLAT
#undef DEF_XLAT_FUNCS

static inline void
s_changeable(void)
{
	s_last_is_changeable(current_tcp);
}

static inline void
s_changeable_void(void)
{
	s_arg_next(current_tcp, S_TYPE_changeable_void);
	current_tcp->s_syscall->cur_arg++;
	if (entering(current_tcp)) {
		s_changeable();
	}
}

static inline void
s_push_empty(enum s_type type)
{
	unsigned long long val;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, type, &val);
}

#endif /* #ifndef STRACE_STRUCTURED_INLINES_H */
