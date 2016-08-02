#define S_MACRO(INT, ENUM) extern void s_push_int_ ## ENUM (INT value);

S_MACRO(char, char)
S_MACRO(int, int)
S_MACRO(long, long)
S_MACRO(long long, longlong)
S_MACRO(unsigned, unsigned)
S_MACRO(unsigned long, unsigned_long)
S_MACRO(unsigned long long, unsigned_longlong)

S_MACRO(long, addr)

#undef S_MACRO

extern void s_push_path(long addr);
