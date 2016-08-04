#include "defs.h"

SYS_FUNC(iopl)
{
	s_push_d();

	return RVAL_DECODED;
}
