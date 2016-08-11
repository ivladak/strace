#include "defs.h"

SYS_FUNC(umask)
{
	s_push_lo("mask");

	return RVAL_DECODED | RVAL_OCTAL;
}
