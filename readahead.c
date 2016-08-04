#include "defs.h"

SYS_FUNC(readahead)
{
	s_push_fd();
	s_push_lld();
	s_push_lu();

	return RVAL_DECODED;
}
