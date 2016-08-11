#ifndef STRACE_STRUCTURED_INLINES_H
#define STRACE_STRUCTURED_INLINES_H

#include <linux/limits.h>

/* internal */
static inline void
s_insert_addr_arg(long value, struct s_arg *arg)
{
	s_addr_new_and_insert(value, arg);
}

static inline void
s_push_value_int(enum s_type type, uint64_t value)
{
	s_num_new_and_insert(type, value);
}

static inline bool
s_push_arg(enum s_type type, bool printnum)
{
	struct s_syscall *syscall = current_tcp->s_syscall;
	unsigned long long val;

	s_syscall_pop_all(syscall);

	if (!printnum) {
		s_syscall_cur_arg_advance(syscall, type, &val);
		s_push_value_int(type, val);
	} else {
		if (umove_or_printaddr(current_tcp,
			current_tcp->u_arg[syscall->cur_arg], &val)
		)
		syscall->cur_arg++;
	}

	return !printnum;
}



#define DEF_PUSH_INT(TYPE, ENUM) \
	static inline void \
	s_push_int_ ## ENUM(TYPE value) \
	{ \
		s_push_value_int(S_TYPE_ ## ENUM, (uint64_t) value); \
	} \
	\
	static inline bool \
	s_push_num_ ## ENUM(void) \
	{ \
		return s_push_arg(S_TYPE_ ## ENUM, true);\
	} \
	static inline bool \
	s_push_int_num_ ## ENUM(const long addr) \
	{ \
		TYPE num; \
		if (umove_or_printaddr(current_tcp, addr, &num)) \
			return false; \
		s_push_int_ ## ENUM(num); \
		current_tcp->s_syscall->cur_arg++; \
		return true; \
	} \
	\
	static inline void \
	s_push_ ## ENUM(void) \
	{ \
		s_push_arg(S_TYPE_ ## ENUM, false); \
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
s_push_xlat_val(const struct xlat *x, uint64_t val, const char *dflt,
	bool flags)
{
	s_xlat_new_and_insert(x, val, dflt, flags);
}

static inline void
s_push_xlat(const struct xlat *x, const char *dflt, bool flags,
	enum s_type type)
{
	unsigned long long val;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, type, &val);
	s_push_xlat_val(x, val, dflt, flags);
}

static inline void
s_append_xlat_val(const struct xlat *x, uint64_t val, const char *dflt,
	bool flags)
{
	s_xlat_append(x, val, dflt, flags);
}

static inline void
s_append_xlat(const struct xlat *x, const char *dflt, bool flags,
	enum s_type type)
{
	unsigned long long val;

	s_syscall_cur_arg_advance(current_tcp->s_syscall, type, &val);
	s_append_xlat_val(x, val, dflt, flags);
}

#define DEF_XLAT_FUNCS(ACT, TYPE, ENUM, FLAGS_TYPE) \
	static inline void \
	s_##ACT##_flags_val_ ## ENUM(const struct xlat *x, TYPE flags, \
	                      const char *dflt) \
	{ \
		s_##ACT##_xlat_val(x, flags, dflt, true); \
	} \
	\
	static inline void \
	s_##ACT##_flags_ ## ENUM(const struct xlat *x, const char *dflt) \
	{ \
		s_##ACT##_xlat(x, dflt, true, S_TYPE_ ## FLAGS_TYPE); \
	} \
	\
	static inline void \
	s_##ACT##_xlat_val_ ## ENUM(const struct xlat *x, TYPE flags, \
	                      const char *dflt) \
	{ \
		s_##ACT##_xlat_val(x, flags, dflt, false); \
	} \
	\
	static inline void \
	s_##ACT##_xlat_ ## ENUM(const struct xlat *x, const char *dflt) \
	{ \
		s_##ACT##_xlat(x, dflt, false, S_TYPE_ ## FLAGS_TYPE); \
	}

#define DEF_XLAT(TYPE, ENUM, FLAGS_TYPE) \
	DEF_XLAT_FUNCS(push, TYPE, ENUM, FLAGS_TYPE) \
	DEF_XLAT_FUNCS(append, TYPE, ENUM, FLAGS_TYPE)

DEF_XLAT(unsigned, int, xlat)
DEF_XLAT(unsigned long, long, xlat_l)
DEF_XLAT(uint64_t, 64, xlat_ll)

#undef DEF_XLAT
#undef DEF_XLAT_FUNCS

static inline void
s_push_fd(void)
{
	s_push_arg(S_TYPE_fd, false);
}

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
