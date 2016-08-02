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

S_MACRO(long, addr)

#undef S_MACRO
