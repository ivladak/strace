#include "defs.h"

SYS_FUNC(iopl)
{
	s_push_d("level");

	return RVAL_DECODED;
}
