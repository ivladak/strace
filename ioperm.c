#include "defs.h"

SYS_FUNC(ioperm)
{
	s_push_addr(tcp->u_arg[0]);
	s_push_addr(tcp->u_arg[1]);
	s_push_int_d(tcp->u_arg[2]);

	return RVAL_DECODED;
}
