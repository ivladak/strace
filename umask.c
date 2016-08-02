#include "defs.h"

SYS_FUNC(umask)
{
	s_push_int_long_octal(tcp->u_arg[0]);

	return RVAL_DECODED | RVAL_OCTAL;
}
