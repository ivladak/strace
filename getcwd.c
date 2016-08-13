#include "defs.h"

SYS_FUNC(getcwd)
{
	if (entering(tcp)) {
		s_changeable_void("buf");
		s_push_lu("size");
	} else {
		if (syserror(tcp))
			s_push_addr("buf");
		else
			s_push_pathn("buf", tcp->u_rval);
	}
	return 0;
}
