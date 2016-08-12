#include "defs.h"

SYS_FUNC(fstatfs)
{
	if (entering(tcp)) {
		s_push_fd("fd");
		s_changeable_void("buf");
	} else {
		s_insert_addr_type("buf", tcp->u_arg[1], S_TYPE_struct,
			fill_struct_statfs, NULL);
	}
	return 0;
}
