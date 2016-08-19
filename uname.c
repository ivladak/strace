/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993-1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2012-2015 Dmitry V. Levin <ldv@altlinux.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "defs.h"

#include <sys/utsname.h>

ssize_t
fetch_fill_uname(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	struct utsname uname;

	if (umove_or_printaddr(current_tcp, addr, &uname))
		return -1;

#define PRINT_UTS_MEMBER(member) \
	s_insert_str_val(#member, uname.member, sizeof(uname.member));

	PRINT_UTS_MEMBER(sysname);
	PRINT_UTS_MEMBER(nodename);
	if (abbrev(current_tcp)) {
		s_insert_ellipsis();
		return sizeof(uname);
	}
	PRINT_UTS_MEMBER(release);
	PRINT_UTS_MEMBER(version);
	PRINT_UTS_MEMBER(machine);
#ifdef HAVE_STRUCT_UTSNAME_DOMAINNAME
	PRINT_UTS_MEMBER(domainname);
#endif

	return sizeof(uname);
}

SYS_FUNC(uname)
{
	if (entering(tcp))
		s_changeable_void("buf");
	else
		s_push_addr_type("buf", S_TYPE_struct, fetch_fill_uname, NULL);

	return 0;
}
