#include "defs.h"
#include "print_time.h"

void
s_insert_timespec_addr(const char *name, long addr)
{
	s_insert_addr_type(name, addr, S_TYPE_struct,
		MPERS_FUNC_NAME(s_fetch_fill_timespec), NULL);
}

void
s_push_timespec(const char *name)
{
	s_push_addr_type(name, S_TYPE_struct,
		MPERS_FUNC_NAME(s_fetch_fill_timespec), NULL);
}
