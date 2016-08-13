#include "defs.h"

#include <sys/swap.h>

#include "xlat/swap_flags.h"

SYS_FUNC(swapon)
{
	s_push_path("path");
	s_push_xlat_flags_int("swapflags", swap_flags, NULL,
		SWAP_FLAG_PRIO_MASK, "SWAP_FLAG_???", NULL);

	return RVAL_DECODED;
}
