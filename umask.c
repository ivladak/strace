#include "defs.h"

SYS_FUNC(umask)
{
	s_push_umode_t("mask");

	return RVAL_DECODED | RVAL_OCTAL;
}
