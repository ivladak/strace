#include "defs.h"

#include DEF_MPERS_TYPE(utimbuf_t)

#include <utime.h>

typedef struct utimbuf utimbuf_t;

#include MPERS_DEFS

static int
fill_utimbuf(struct s_arg *arg, void *buf, unsigned long len, void *fn_data)
{
	utimbuf_t *u = buf;

	s_insert_time("actime", u->actime);
	s_insert_time("modtime", u->modtime);

	return 0;
}

SYS_FUNC(utime)
{
	s_push_path("filename");
	s_push_fill_struct("times", sizeof(utimbuf_t), fill_utimbuf, tcp);

	return RVAL_DECODED;
}
