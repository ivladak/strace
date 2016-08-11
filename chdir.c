#include "defs.h"

SYS_FUNC(chdir)
{
	s_push_path("path");

	return RVAL_DECODED;
}
