#include "defs.h"

SYS_FUNC(chdir)
{
	s_push_path();

	return RVAL_DECODED;
}
