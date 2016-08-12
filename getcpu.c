#include "defs.h"

SYS_FUNC(getcpu)
{
	if (entering(tcp)) {
		s_changeable_void("cpu");
		s_changeable_void("node");
		s_push_addr("tcache");
	} else {
		s_push_u_addr("cpu");
		s_push_u_addr("node");
	}
	return 0;
}
