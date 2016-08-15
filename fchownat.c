#include "defs.h"

SYS_FUNC(fchownat)
{
	s_push_dirfd("dirfd");
	s_push_path("pathname");
	s_push_uid_1("owner");
	s_push_gid_1("group");
	s_push_flags_int("flags", at_flags, "AT_???");

	return RVAL_DECODED;
}
