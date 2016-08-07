#ifndef STRACE_STRUCTURED_INLINES_H
#define STRACE_STRUCTURED_INLINES_H

static inline void
s_push_value_int(enum s_type type, uint64_t value)
{
	s_num_new_and_push(type, value);
}

static inline bool
s_push_arg(enum s_type type, bool printnum)
{
	struct s_syscall *syscall = current_tcp->s_syscall;
	unsigned long long val;
	int new_arg;

	if (!printnum) {
		if (S_TYPE_SIZE(type) == S_TYPE_SIZE_ll) {
			new_arg = getllval(current_tcp, &val, syscall->cur_arg);
		} else {
			new_arg = syscall->cur_arg + 1;
			val = current_tcp->u_arg[syscall->cur_arg];
		}
	} else {
		new_arg = syscall->cur_arg + 1;
		if (umove_or_printaddr(current_tcp,
			current_tcp->u_arg[syscall->cur_arg], &val)
		)
			return false;
	}

	s_push_value_int(type, val);

	syscall->cur_arg = new_arg;
	return true;
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
s_push_addr(void)
{
	s_push_arg(S_TYPE_addr, false);
}

static inline void
s_push_addr_val(long value)
{
	s_push_value_int(S_TYPE_addr, (uint64_t) value);
}

static inline void
s_push_path(void)
{
	s_push_arg(S_TYPE_path, false);
}

static inline void
s_push_path_val(long addr)
{
	s_push_value_int(S_TYPE_path, (uint64_t) addr);
}

static inline void
s_push_flags_val(const struct xlat *x, uint64_t flags, const char *dflt)
{
	s_flags_new_and_push(x, flags, dflt);
}

static inline void
s_push_flags(const struct xlat *x, const char *dflt)
{
	s_push_flags_val(x,
		current_tcp->u_arg[current_tcp->s_syscall->cur_arg++], dflt);
}

#define DEF_PUSH_FLAGS(TYPE, ENUM) \
	static inline void \
	s_push_flags_val_ ## ENUM(const struct xlat *x, TYPE flags, \
	                      const char *dflt) \
	{ \
		s_push_flags_val(x, flags, dflt); \
	} \
	\
	static inline void \
	s_push_flags_ ## ENUM(const struct xlat *x, const char *dflt) \
	{ \
		s_push_flags_val(x, \
			current_tcp->u_arg[current_tcp->s_syscall->cur_arg++], \
			dflt); \
	}

DEF_PUSH_FLAGS(unsigned, int)
DEF_PUSH_FLAGS(unsigned long, long)
DEF_PUSH_FLAGS(uint64_t, 64)

#undef DEF_PUSH_FLAGS

static inline void
s_push_str_val(long addr, long len)
{
	s_str_new_and_push(addr, len);
}

static inline void
s_push_str(long len)
{
	s_push_str_val(current_tcp->u_arg[current_tcp->s_syscall->cur_arg++],
		len);
}

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


#endif /* #ifndef STRACE_STRUCTURED_INLINES_H */
