#include "defs.h"

#ifdef HAVE_LINUX_FALLOC_H
# include <linux/falloc.h>
#endif

#include "xlat/falloc_flags.h"

SYS_FUNC(fallocate)
{
	s_push_fd("fd");
	s_push_flags_int("mode", falloc_flags, "FALLOC_FL_???");
	s_push_llu("offset");
	s_push_llu("len");

	return RVAL_DECODED;
}
