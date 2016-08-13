#include "defs.h"

#ifdef BFIN

#include <bfin_sram.h>

#include "xlat/sram_alloc_flags.h"

SYS_FUNC(sram_alloc)
{
	s_push_lu("size");
	s_push_flags_long("flags", sram_alloc_flags, "???_SRAM");

	return RVAL_DECODED | RVAL_HEX;
}

#endif /* BFIN */
