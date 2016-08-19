/*
 * Copyright (c) 2000 Wichert Akkerman <wakkerma@debian.org>
 * Copyright (c) 2011 Denys Vlasenko <dvlasenk@redhat.com>
 * Copyright (c) 2005-2015 Dmitry V. Levin <ldv@altlinux.org>
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

/* these constants are the same as in <linux/capability.h> */
enum {
#include "caps0.h"
};

#include "xlat/cap_mask0.h"

/* these constants are CAP_TO_INDEX'ed constants from <linux/capability.h> */
enum {
#include "caps1.h"
};

#include "xlat/cap_mask1.h"

/* these constants are the same as in <linux/capability.h> */
enum {
	_LINUX_CAPABILITY_VERSION_1 = 0x19980330,
	_LINUX_CAPABILITY_VERSION_2 = 0x20071026,
	_LINUX_CAPABILITY_VERSION_3 = 0x20080522
};

#include "xlat/cap_version.h"

typedef struct user_cap_header_struct {
	uint32_t version;
	int pid;
} cap_user_header_t;

typedef struct user_cap_data_struct {
	uint32_t effective;
	uint32_t permitted;
	uint32_t inheritable;
} cap_user_data_t;

static void
push_cap_bits(const char *name, const uint32_t lo, const uint32_t hi)
{
	if (lo && !hi) {
		s_insert_flags_int(name, cap_mask0, lo, "CAP_???");
	} else if (hi && !lo) {
		s_insert_flags_int(name, cap_mask1, hi, "CAP_???");
	} else if (!lo && !hi) {
		s_insert_flags_int(name, cap_mask0, lo, "CAP_???");
	} else {
		s_insert_struct(name);
		s_insert_flags_int("low", cap_mask0, lo, "CAP_???");
		s_insert_flags_int("high", cap_mask1, hi, "CAP_???");
		s_struct_finish();
	}
}

static long
fetch_fill_cap_user_data(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	struct user_cap_data_struct data[2];
	unsigned int len;
	unsigned h = (unsigned long) fn_data;

	if (!h) {
		return -1;
	}

	if (_LINUX_CAPABILITY_VERSION_2 == h ||
	    _LINUX_CAPABILITY_VERSION_3 == h)
		len = 2;
	else
		len = 1;

	if (s_umoven_verbose(current_tcp, addr, len * sizeof(data[0]), data))
		return -1;

	push_cap_bits("effective", data[0].effective, len > 1 ? data[1].effective : 0);
	push_cap_bits("permitted", data[0].permitted, len > 1 ? data[1].permitted : 0);
	push_cap_bits("inheritable", data[0].inheritable, len > 1 ? data[1].inheritable : 0);

	return 0;
}

static int
fill_cap_user_header(struct s_arg *arg, void *buf, unsigned long len,
	void *fn_data)
{
	unsigned *version = (unsigned *) fn_data;
	cap_user_header_t *h = buf;

	s_insert_xlat_int("version", cap_version, h->version,
		"_LINUX_CAPABILITY_VERSION_???");
	*version = h->version;
	s_insert_d("pid", h->pid);

	return 0;
}

SYS_FUNC(capget)
{
	unsigned *version = 0;

	if (entering(tcp)) {
		s_push_addr_type_sized("hdrp", sizeof(cap_user_header_t), S_TYPE_struct,
			fill_cap_user_header, &version);
		current_tcp->_priv_data = version;
		s_changeable_void("datap");
	} else {
		s_push_addr_type("datap", S_TYPE_struct, fetch_fill_cap_user_data,
			(void *) current_tcp->_priv_data);
	}
	return 0;
}

SYS_FUNC(capset)
{
	unsigned *version = 0;
	s_push_addr_type_sized("hdrp", sizeof(cap_user_header_t),
		S_TYPE_struct, fill_cap_user_header, &version);
	s_push_addr_type("datap", S_TYPE_struct, fetch_fill_cap_user_data,
		(void *) version);

	return RVAL_DECODED;
}
