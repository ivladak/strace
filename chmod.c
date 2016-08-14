#include "defs.h"

static void
decode_chmod(void)
{
	s_push_path("path");
	s_push_umode_t("mode");
}

SYS_FUNC(chmod)
{
	decode_chmod();

	return RVAL_DECODED;
}

SYS_FUNC(fchmodat)
{
	s_push_dirfd("dirfd");
	decode_chmod();

	return RVAL_DECODED;
}

SYS_FUNC(fchmod)
{
	s_push_fd("fd");
	s_push_umode_t("mode");

	return RVAL_DECODED;
}
