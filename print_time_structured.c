#include "defs.h"
#include "print_time_structured.h"

void
s_insert_timespec_addr(const char *name, long addr)
{
	s_insert_addr_type(name, addr, S_TYPE_struct, s_fetch_fill_timespec, NULL);
}

void
s_push_timespec(const char *name)
{
	s_push_addr_type(name, S_TYPE_struct, s_fetch_fill_timespec, NULL);
}

void
s_insert_timeval_addr(const char *name, long addr)
{
	s_insert_addr_type(name, addr, S_TYPE_struct, s_fetch_fill_timeval, NULL);
}

void
s_push_timeval(const char *name)
{
	s_push_addr_type(name, S_TYPE_struct, s_fetch_fill_timeval, NULL);
}

void
s_push_itimerval(const char *name)
{
	s_push_addr_type(name, S_TYPE_struct, s_fetch_fill_itimerval, NULL);
}

#ifdef ALPHA

void
s_push_itimerval32(const char *name)
{
	s_push_addr_type(name, S_TYPE_struct, s_fetch_fill_itimerval32, NULL);
}

#endif /* ALPHA */
