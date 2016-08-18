/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993-1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2003-2016 Dmitry V. Levin <ldv@altlinux.org>
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

#ifdef STRACE_UID_SIZE
# if STRACE_UID_SIZE != 16
#  error invalid STRACE_UID_SIZE
# endif

# define SIZEIFY(x)		SIZEIFY_(x,STRACE_UID_SIZE)
# define SIZEIFY_(x,size)	SIZEIFY__(x,size)
# define SIZEIFY__(x,size)	x ## size

# define printuid	SIZEIFY(printuid)
# define sys_chown	SIZEIFY(sys_chown)
# define sys_fchown	SIZEIFY(sys_fchown)
# define sys_getgroups	SIZEIFY(sys_getgroups)
# define sys_getresuid	SIZEIFY(sys_getresuid)
# define sys_getuid	SIZEIFY(sys_getuid)
# define sys_setfsuid	SIZEIFY(sys_setfsuid)
# define sys_setgroups	SIZEIFY(sys_setgroups)
# define sys_setresuid	SIZEIFY(sys_setresuid)
# define sys_setreuid	SIZEIFY(sys_setreuid)
# define sys_setuid	SIZEIFY(sys_setuid)
#endif /* STRACE_UID_SIZE */

#include "defs.h"

#ifdef STRACE_UID_SIZE

# define s_push_uid_addr   s_push_uid16_addr
# define s_push_gid_addr   s_push_gid16_addr
# define s_push_uid        SIZEIFY(s_push_uid)
# define s_push_gid        SIZEIFY(s_push_gid)

# define S_TYPE_gid        SIZEIFY(S_TYPE_gid)

# if !NEED_UID16_PARSERS
#  undef STRACE_UID_SIZE
# endif
#else
# define STRACE_UID_SIZE 32
#endif

#ifdef STRACE_UID_SIZE

# undef uid_t
# define uid_t		uid_t_(STRACE_UID_SIZE)
# define uid_t_(size)	uid_t__(size)
# define uid_t__(size)	uint ## size ## _t

SYS_FUNC(getuid)
{
	return RVAL_UDECIMAL | RVAL_DECODED;
}

SYS_FUNC(setfsuid)
{
	s_push_uid("fsuid");

	return RVAL_UDECIMAL | RVAL_DECODED;
}

SYS_FUNC(setuid)
{
	s_push_uid("uid");

	return RVAL_DECODED;
}

SYS_FUNC(getresuid)
{
	if (entering(tcp)) {
		s_changeable_void("ruid");
		s_changeable_void("euid");
		s_changeable_void("suid");
	} else {
		s_push_uid_addr("ruid");
		s_push_uid_addr("euid");
		s_push_uid_addr("suid");
	}

	return 0;
}

SYS_FUNC(setreuid)
{
	s_push_uid("ruid");
	s_push_uid("euid");

	return RVAL_DECODED;
}

SYS_FUNC(setresuid)
{
	s_push_uid("ruid");
	s_push_uid("euid");
	s_push_uid("suid");

	return RVAL_DECODED;
}

SYS_FUNC(chown)
{
	s_push_path("path");
	s_push_uid("owner");
	s_push_gid("group");

	return RVAL_DECODED;
}

SYS_FUNC(fchown)
{
	s_push_fd("fd");
	s_push_uid("owner");
	s_push_gid("group");

	return RVAL_DECODED;
}

void
printuid(const char *text, const unsigned int uid)
{
	if ((uid_t) -1U == (uid_t) uid)
		tprintf("%s-1", text);
	else
		tprintf("%s%u", text, (uid_t) uid);
}

static int
fill_group(struct s_arg *arg, void *buf, size_t len, void *fn_data)
{
	S_ARG_TO_TYPE(arg, num)->val = (uid_t)(*(uid_t *)buf);

	return 0;
}

static void
s_push_groups(const char *name, const long len)
{
	static unsigned long ngroups_max;
	if (!ngroups_max)
		ngroups_max = sysconf(_SC_NGROUPS_MAX);

	if ((len < 0) || ((unsigned long)len > ngroups_max)) {
		s_push_addr(name);
		return;
	}

	s_push_array_type(name, len, sizeof(uid_t), S_TYPE_gid, fill_group,
		NULL);
}

SYS_FUNC(setgroups)
{
	const unsigned int len = tcp->u_arg[0];

	s_push_d("len");
	s_push_groups("list", len);
	return RVAL_DECODED;
}

SYS_FUNC(getgroups)
{
	if (entering(tcp)) {
		s_push_d("len");
		s_changeable_void("list");
	} else {
		s_push_groups("list", tcp->u_rval);
	}

	return 0;
}

#endif /* STRACE_UID_SIZE */
