#include "defs.h"
#include "xlat/umount_flags.h"

SYS_FUNC(umount2)
{
	s_push_path(tcp->u_arg[0]);
	s_push_flags_int(umount_flags, tcp->u_arg[1], "MNT_???");

	return RVAL_DECODED;
}
