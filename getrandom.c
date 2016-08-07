#include "defs.h"
#include "xlat/getrandom_flags.h"

SYS_FUNC(getrandom)
{
	if (entering(tcp)) {
		s_changeable_void();
		s_push_lu();
		s_push_flags_int(getrandom_flags, "GRND_???");
	} else {
		if (syserror(tcp))
			s_push_addr();
		else
			s_push_str_val(tcp->u_arg[0], tcp->u_rval);
	}
	return 0;
}
