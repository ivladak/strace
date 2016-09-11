#include "defs.h"

SYS_FUNC(fstatfs)
{
	if (entering(tcp)) {
		s_push_fd("fd");
		s_changeable_void("buf");
	} else {
		s_push_fetch_fill_struct("buf", fill_struct_statfs, NULL);
	}
	return 0;
}
