#include "defs.h"
#include "xlat/getrandom_flags.h"

SYS_FUNC(getrandom)
{
	if (entering(tcp)) {
		s_changeable_void("buf");
		s_push_lu("count");
		s_push_flags_int("flags", getrandom_flags, "GRND_???");
	} else {
		if (syserror(tcp))
			s_push_addr(NULL);
		else
			s_push_str(NULL, tcp->u_rval);
	}
	return 0;
}
