#include "defs.h"

SYS_FUNC(ioperm)
{
	s_push_lx();
	s_push_lx();
	s_push_d();

	return RVAL_DECODED;
}
