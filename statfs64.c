#include "defs.h"

SYS_FUNC(statfs64)
{
	const unsigned long size = tcp->u_arg[1];

	if (entering(tcp)) {
		s_push_path("path");
		s_push_lu("size");
		s_changeable_void("buf");
	} else {
		s_insert_addr_type("buf", tcp->u_arg[2], S_TYPE_struct,
			fill_struct_statfs64, (void *)size);
	}
	return 0;
}
