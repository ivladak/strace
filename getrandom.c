#include "defs.h"
#include "xlat/getrandom_flags.h"

SYS_FUNC(getrandom)
{
	if (exiting(tcp)) {
		if (syserror(tcp))
			s_push_addr();
		else
			s_push_str(tcp->u_rval);

		s_push_lu();
		s_push_flags_int(getrandom_flags, "GRND_???");
	}
	return 0;
}
