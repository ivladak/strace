#include "defs.h"

SYS_FUNC(readahead)
{
	s_push_fd("fd");
	s_push_lld("offset");
	s_push_lu("count");

	return RVAL_DECODED;
}
