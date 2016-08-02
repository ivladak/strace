#include "defs.h"

SYS_FUNC(umask)
{
	s_push_int_lo(tcp->u_arg[0]);

	return RVAL_DECODED | RVAL_OCTAL;
}
