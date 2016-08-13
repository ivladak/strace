#include "defs.h"

SYS_FUNC(sethostname)
{
	s_push_str("name", tcp->u_arg[1]);
	s_push_lu("len");

	return RVAL_DECODED;
}

#if defined(ALPHA)
SYS_FUNC(gethostname)
{
	if (entering(tcp)) {
		s_changeable_void("name");
		s_push_lu("len");
	} else {
		if (syserror(tcp))
			s_push_addr("name");
		else
			s_push_str("name", -1);
	}
	return 0;
}
#endif /* ALPHA */
