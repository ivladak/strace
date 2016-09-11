#include "defs.h"

SYS_FUNC(fstatfs64)
{
	const unsigned long size = tcp->u_arg[1];

	if (entering(tcp)) {
		s_push_fd("fd");
		s_push_lu("size");
		s_changeable_void("buf");
	} else {
		s_push_fetch_fill_struct("buf", fill_struct_statfs64, (void *)size);
	}
	return 0;
}
