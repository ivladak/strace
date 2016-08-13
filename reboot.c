#include "defs.h"

#include "xlat/bootflags1.h"
#include "xlat/bootflags2.h"
#include "xlat/bootflags3.h"

SYS_FUNC(reboot)
{
	const unsigned int cmd = tcp->u_arg[2];

	s_push_flags_int("magic", bootflags1, "LINUX_REBOOT_MAGIC_???");
	s_push_flags_int("magic2", bootflags2, "LINUX_REBOOT_MAGIC_???");
	s_push_flags_int("cmd", bootflags3, "LINUX_REBOOT_CMD_???");

	if (cmd == LINUX_REBOOT_CMD_RESTART2)
		s_push_str("arg", -1);

	return RVAL_DECODED;
}
