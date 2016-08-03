#define DECL_PUSH_INT(TYPE, ENUM) extern void s_push_int_ ## ENUM(TYPE value)

DECL_PUSH_INT(int, d);
DECL_PUSH_INT(long, ld);
DECL_PUSH_INT(long long, lld);

DECL_PUSH_INT(unsigned, u);
DECL_PUSH_INT(unsigned long, lu);
DECL_PUSH_INT(unsigned long long, llu);

DECL_PUSH_INT(int, o);
DECL_PUSH_INT(long, lo);
DECL_PUSH_INT(long long, llo);

DECL_PUSH_INT(int, x);
DECL_PUSH_INT(long, lx);
DECL_PUSH_INT(long long, llx);

#undef DECL_PUSH_INT

extern void s_push_addr(long value);

extern void s_push_path(long addr);

#define DECL_PUSH_FLAGS(TYPE, ENUM) \
	extern void s_push_flags_ ## ENUM(const struct xlat *, TYPE, const char *)

DECL_PUSH_FLAGS(unsigned, int);
DECL_PUSH_FLAGS(unsigned long, long);
DECL_PUSH_FLAGS(uint64_t, 64);

#undef DECL_PUSH_FLAGS

extern void s_push_str(long addr, long len);
