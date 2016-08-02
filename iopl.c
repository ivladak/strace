#include "defs.h"

SYS_FUNC(iopl)
{
	s_push_int_d(tcp->u_arg[0]);

	return RVAL_DECODED;
}
