/*
 * Copyright (c) 2002-2005 Roland McGrath <roland@redhat.com>
 * Copyright (c) 2004 Ulrich Drepper <drepper@redhat.com>
 * Copyright (c) 2005-2016 Dmitry V. Levin <ldv@altlinux.org>
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

#ifdef HAVE_SYS_XATTR_H
# include <sys/xattr.h>
#endif

#include "xlat/xattrflags.h"

#ifndef XATTR_SIZE_MAX
# define XATTR_SIZE_MAX 65536
#endif

static void
s_push_xattr_val(unsigned long size)
{
	if (size > XATTR_SIZE_MAX)
		s_push_addr("value");
	else
		s_push_str_ex("value", size, QUOTE_OMIT_TRAILING_0);

	s_push_lu("size");
}

SYS_FUNC(setxattr)
{
	s_push_path("path");
	s_push_str("name", -1);
	s_push_xattr_val(tcp->u_arg[3]);
	s_push_flags_int("flags", xattrflags, "XATTR_???");
	return RVAL_DECODED;
}

SYS_FUNC(fsetxattr)
{
	s_push_fd("fd");
	s_push_str("name", -1);
	s_push_xattr_val(tcp->u_arg[3]);
	s_push_flags_int("flags", xattrflags, "XATTR_???");
	return RVAL_DECODED;
}

SYS_FUNC(getxattr)
{
	if (entering(tcp)) {
		s_push_path("path");
		s_push_str("name", -1);
		s_changeable_void("value");
		s_changeable_void("size");
	} else {
		s_push_xattr_val(tcp->u_rval);
	}
	return 0;
}

SYS_FUNC(fgetxattr)
{
	if (entering(tcp)) {
		s_push_fd("fd");
		s_push_str("name", -1);
		s_changeable_void("value");
		s_changeable_void("size");
	} else {
		s_push_xattr_val(tcp->u_rval);
	}
	return 0;
}

static void
print_xattr_list(void)
{
	if (!(current_tcp->u_arg[2]) || syserror(current_tcp))
		s_push_addr("list");
	else
		s_push_str("list", current_tcp->u_rval);

	s_push_lu("size");
}

SYS_FUNC(listxattr)
{
	if (entering(tcp)) {
		s_push_path("path");
		s_changeable_void("list");
		s_changeable_void("size");
	} else {
		print_xattr_list();
	}
	return 0;
}

SYS_FUNC(flistxattr)
{
	if (entering(tcp)) {
		s_push_fd("fd");
		s_changeable_void("list");
		s_changeable_void("size");
	} else {
		print_xattr_list();
	}
	return 0;
}

SYS_FUNC(removexattr)
{
	s_push_path("path");
	s_push_str("name", -1);
	return RVAL_DECODED;
}

SYS_FUNC(fremovexattr)
{
	s_push_fd("fd");
	s_push_str("name", -1);
	return RVAL_DECODED;
}
