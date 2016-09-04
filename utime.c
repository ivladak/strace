#include "defs.h"

#include DEF_MPERS_TYPE(utimbuf_t)

#include <utime.h>

typedef struct utimbuf utimbuf_t;

#include MPERS_DEFS

static int fill_utimbuf(struct s_arg *arg, long addr, void *fn_data)
{
	utimbuf_t u;

	if (umove((struct tcb *)fn_data, addr, &u))
		return -1;

	s_insert_time("actime", u.actime);
	s_insert_time("modtime", u.modtime);

	return 0;
}

SYS_FUNC(utime)
{
	s_push_path("filename");
	s_push_addr_type("times", S_TYPE_struct, fill_utimbuf, tcp);

	return RVAL_DECODED;
}
