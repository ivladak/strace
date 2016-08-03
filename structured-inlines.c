#include "defs.h"

inline void
s_push_value_int(s_type_t type, uint64_t value)
{
	s_arg_t *arg = s_arg_new(current_tcp, type);
	arg->value_int = value;
}


#define DEF_PUSH_INT(TYPE, ENUM) \
	inline void \
	s_push_int_ ## ENUM(TYPE value) \
	{ \
		s_push_value_int(S_TYPE_ ## ENUM, (uint64_t) value); \
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

inline void
s_push_addr(long value)
{
	s_push_value_int(S_TYPE_addr, (uint64_t) value);
}

inline void
s_push_path(long addr)
{
	s_push_value_int(S_TYPE_path, (uint64_t) addr);
}

inline void
s_push_flags(const struct xlat *x, uint64_t flags, const char *dflt)
{
	s_arg_t *arg = s_arg_new(current_tcp, S_TYPE_flags);
	arg->value_p = malloc(sizeof(s_flags_t));
	s_flags_t *p = arg->value_p;
	p->x = x;
	p->flags = flags;
	p->dflt = dflt;
}

#define DEF_PUSH_FLAGS(TYPE, ENUM) \
	inline void \
	s_push_flags_ ## ENUM(const struct xlat *x, TYPE flags, \
				  const char *dflt) \
	{ \
		s_push_flags(x, flags, dflt); \
	}

DEF_PUSH_FLAGS(unsigned, int)
DEF_PUSH_FLAGS(unsigned long, long)
DEF_PUSH_FLAGS(uint64_t, 64)

#undef DEF_PUSH_FLAGS
