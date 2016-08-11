#include "defs.h"

SYS_FUNC(ioperm)
{
	s_push_lx("from");
	s_push_lx("num");
	s_push_d("turn_on");

	return RVAL_DECODED;
}
