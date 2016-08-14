#include "defs.h"

#include <fcntl.h>

#include "xlat/access_flags.h"

static int
decode_access(void)
{
	s_push_path("pathname");
	s_push_flags_int("mode", access_flags, "?_OK");

	return RVAL_DECODED;
}

SYS_FUNC(access)
{
	return decode_access();
}

SYS_FUNC(faccessat)
{
	s_push_dirfd("dirfd");
	return decode_access();
}
