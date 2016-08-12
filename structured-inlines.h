#ifndef STRACE_STRUCTURED_INLINES_H
#define STRACE_STRUCTURED_INLINES_H

#include <linux/limits.h>

/* internal */
static inline void
s_insert_addr_arg(const char *name, long value, struct s_arg *arg)
{
	s_addr_new_and_insert(name, value, arg);
}

/* internal */
/* possible optimization: allocate buf on stack */
static inline struct s_arg *
s_insert_addr_type(const char *name, long value, enum s_type type,
	s_fill_arg_fn fill_cb, void *fn_data)
{
	struct s_addr *addr = s_addr_new_and_insert(name, value, NULL);
	int ret;

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

	ret = fill_cb(addr->val, value, fn_data);

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

	return addr->val;
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

#define DEF_PUSH_INT(TYPE, ENUM) \
	static inline void \
	s_insert_##ENUM(const char *name, TYPE value) \
	{ \
		s_insert_num(S_TYPE_##ENUM, name, (uint64_t) value); \
	} \
	\
	static inline void \
	s_insert_##ENUM##_addr(const char *name, long addr) \
	{ \
		TYPE val = 0; \
		struct s_arg *arg = NULL; \
		\
		if (!umove(current_tcp, addr, &val)) \
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
		\
		s_syscall_pop_all(syscall); \
		s_insert_##ENUM##_addr(name, \
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
s_insert_addr(const char *name, long value)
{
	s_addr_new_and_insert(name, value, NULL);
}

static inline void
s_push_addr(const char *name)
{
	s_syscall_pop_all(current_tcp->s_syscall);
	s_insert_addr(name,
		current_tcp->u_arg[current_tcp->s_syscall->cur_arg++]);
}


static inline void
s_insert_string(enum s_type type, const char *name, long addr, long len,
	bool has_nul)
{
	struct s_str *str = s_str_new(type, name, addr, len, has_nul);

	s_insert_addr_arg(name, addr, str ? S_TYPE_TO_ARG(str) : NULL);
}

/* Equivalent to s_insert_path_addr */
static inline void
s_insert_path(const char *name, long addr)
{
	s_insert_string(S_TYPE_path, name, addr, PATH_MAX, true);
}

/* Equivalent to s_push_path_addr */
static inline void
s_push_path(const char *name)
{
	s_syscall_pop_all(current_tcp->s_syscall);
	s_insert_path(name,
		current_tcp->u_arg[current_tcp->s_syscall->cur_arg++]);
}

/* Equivalent to s_insert_str_addr */
static inline void
s_insert_str(const char *name, long addr, long len)
{
	s_insert_string(S_TYPE_str, name, addr, len, len == -1);
}

/* Equivalent to s_push_str_addr */
static inline void
s_push_str(const char *name, long len)
{
	s_syscall_pop_all(current_tcp->s_syscall);
	s_insert_str(name,
		current_tcp->u_arg[current_tcp->s_syscall->cur_arg++], len);
}


static inline void
s_insert_struct(const char *name)
{
	struct s_struct *s = s_struct_new_and_insert(S_TYPE_struct, name);
	s_struct_enter(s);
}

static inline void
s_insert_struct_addr(const char *name, long addr)
{
	struct s_struct *s = s_struct_new_and_insert(S_TYPE_struct, name);

	s_insert_addr_arg(name, addr, &s->arg);
	s_struct_enter(s);
}

/* Equivalent to s_push_struct_addr */
static inline void
s_push_struct(const char *name)
{
	s_syscall_pop_all(current_tcp->s_syscall);
	s_insert_struct_addr(name,
		current_tcp->u_arg[current_tcp->s_syscall->cur_arg++]);
}

static inline void
s_insert_array(const char *name)
{
	struct s_struct *s = s_struct_new_and_insert(S_TYPE_array, name);
	s_struct_enter(s);
}

static inline void
s_insert_array_addr(const char *name, long addr)
{
	struct s_struct *s = s_struct_new_and_insert(S_TYPE_array, name);

	s_insert_addr_arg(name, addr, &s->arg);
	s_struct_enter(s);
}

/* Equivalent to s_push_array_addr */
static inline void
s_push_array(const char *name)
{
	s_syscall_pop_all(current_tcp->s_syscall);
	s_insert_array_addr(name,
		current_tcp->u_arg[current_tcp->s_syscall->cur_arg++]);
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
	uint64_t val, const char *dflt, bool flags)
{
	s_xlat_new_and_insert(type, name, x, val, dflt, flags);
}

static inline void
s_push_xlat(const char *name, const struct xlat *x, const char *dflt,
	bool flags, enum s_type type)
{
	unsigned long long val;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, type, &val);
	s_insert_xlat(type, name, x, val, dflt, flags);
}

static inline void
s_append_xlat_val(enum s_type type, const char *name, const struct xlat *x,
	uint64_t val, const char *dflt, bool flags)
{
	s_xlat_append(type, name, x, val, dflt, flags);
}

static inline void
s_append_xlat(const char *name, const struct xlat *x, const char *dflt,
	bool flags, enum s_type type)
{
	unsigned long long val;

	s_syscall_cur_arg_advance(current_tcp->s_syscall, type, &val);
	s_append_xlat_val(type, name, x, val, dflt, flags);
}

#define DEF_XLAT_FUNCS(WHAT, TYPE, ENUM, FLAGS_TYPE, FLAGS) \
	static inline void \
	s_insert_##WHAT##_##ENUM(const char *name, const struct xlat *x, \
		TYPE val, const char *dflt) \
	{ \
		s_insert_xlat(S_TYPE_##FLAGS_TYPE, name, x, val, dflt, FLAGS); \
	} \
	\
	static inline void \
	s_insert_##WHAT##_##ENUM##_addr(const char *name, \
		const struct xlat *x, long addr, const char *dflt) \
	{ \
		TYPE val = 0; \
		struct s_arg *arg = NULL; \
		\
		if (!umove(current_tcp, addr, &val)) \
			arg = S_TYPE_TO_ARG(s_xlat_new(S_TYPE_##FLAGS_TYPE, \
				name, x, val, dflt, FLAGS)); \
		\
		s_insert_addr_arg(name, addr, arg); \
	} \
	\
	static inline void \
	s_push_##WHAT##_##ENUM(const char *name, const struct xlat *x, \
		const char *dflt) \
	{ \
		s_push_xlat(name, x, dflt, FLAGS, S_TYPE_##FLAGS_TYPE); \
	} \
	\
	static inline void \
	s_push_##WHAT##_##ENUM##_addr(const char *name, const struct xlat *x, \
		const char *dflt) \
	{ \
		s_syscall_pop_all(current_tcp->s_syscall); \
		s_insert_##WHAT##_##ENUM##_addr(name, x, \
			current_tcp->u_arg[current_tcp->s_syscall->cur_arg++], \
			dflt); \
	} \
	\
	static inline void \
	s_append_##WHAT##_##ENUM##_val(const char *name, const struct xlat *x, \
		TYPE val, const char *dflt) \
	{ \
		s_append_xlat_val(S_TYPE_##FLAGS_TYPE, name, x, val, dflt, \
			FLAGS); \
	} \
	\
	static inline void \
	s_append_##WHAT##_##ENUM(const char *name, const struct xlat *x, \
		const char *dflt) \
	{ \
		s_append_xlat(name, x, dflt, FLAGS, S_TYPE_##FLAGS_TYPE); \
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
s_changeable_void(const char *name)
{
	s_arg_next(current_tcp, S_TYPE_changeable_void, name);
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
