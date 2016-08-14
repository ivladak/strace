#include "defs.h"

static void
decode_renameat(void)
{
	s_push_dirfd("oldfd");
	s_push_path("oldname");
	s_push_dirfd("newfd");
	s_push_path("newname");
}

SYS_FUNC(renameat)
{
	decode_renameat();

	return RVAL_DECODED;
}

#include <linux/fs.h>
#include "xlat/rename_flags.h"

SYS_FUNC(renameat2)
{
	decode_renameat();
	s_push_flags_int("flags", rename_flags, "RENAME_??");

	return RVAL_DECODED;
}
