#include "defs.h"

SYS_FUNC(truncate)
{
	s_push_path("path");
	s_push_lu("length");

	return RVAL_DECODED;
}

SYS_FUNC(truncate64)
{
	s_push_path("path");
	s_push_llu("length");

	return RVAL_DECODED;
}

SYS_FUNC(ftruncate)
{
	s_push_fd("fd");
	s_push_lu("length");

	return RVAL_DECODED;
}

SYS_FUNC(ftruncate64)
{
	s_push_fd("fd");
	s_push_llu("length");

	return RVAL_DECODED;
}
