#include "defs.h"
#include "xlat/getrandom_flags.h"

SYS_FUNC(getrandom)
{
	if (exiting(tcp)) {
		if (syserror(tcp))
			s_push_addr(tcp->u_arg[0]);
		else
			s_push_str(tcp->u_arg[0], tcp->u_rval);
		s_push_int_lu(tcp->u_arg[1]);
		s_push_flags_int(getrandom_flags, tcp->u_arg[2], "GRND_???");
	}
	return 0;
}
