#include "defs.h"

s_syscall_t *s_syscall;

inline void
s_push_value_int (s_type_t type, uint64_t value)
{
	s_arg_t *arg = s_arg_new(s_syscall, type);
	arg->value_int = value;
}


#define S_MACRO(INT, ENUM) \
inline void \
s_push_int_ ## ENUM (INT value) \
{ \
	s_push_value_int(S_TYPE_ ## ENUM, (uint64_t) value); \
}

S_MACRO(char, char)
S_MACRO(int, int)
S_MACRO(long, long)
S_MACRO(long long, longlong)
S_MACRO(unsigned, unsigned)
S_MACRO(unsigned long, unsigned_long)
S_MACRO(unsigned long long, unsigned_longlong)

S_MACRO(int, int_octal)
S_MACRO(long, long_octal)
S_MACRO(long long, longlong_octal)

S_MACRO(long, addr)

#undef S_MACRO

inline void
s_push_path (long addr)
{
	s_push_value_int(S_TYPE_path, (uint64_t) addr);
}

inline void
s_push_flags (const struct xlat *x, uint64_t flags, const char *dflt)
{
	s_arg_t *arg = s_arg_new(s_syscall, S_TYPE_flags);
	arg->value_p = malloc(sizeof(s_flags_t));
	s_flags_t *p = arg->value_p;
	p->x = x;
	p->flags = flags;
	p->dflt = dflt;
}

#define S_MACRO(NAME, TYPE) \
inline void \
s_push_flags_ ## NAME (const struct xlat *x, TYPE flags, \
			  const char *dflt) \
{ \
	s_push_flags(x, flags, dflt); \
}

S_MACRO(int, unsigned)
S_MACRO(long, unsigned long)
S_MACRO(64, uint64_t)

#undef S_MACRO

