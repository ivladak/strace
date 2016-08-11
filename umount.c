#include "defs.h"
#include "xlat/umount_flags.h"

SYS_FUNC(umount2)
{
	s_push_path("target");
	s_push_flags_int("flags", umount_flags, "MNT_???");

	return RVAL_DECODED;
}
