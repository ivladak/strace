/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
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
#include <fcntl.h>
#include <sys/uio.h>

#include "netlink_structured.h"
#include "structured_iov.h"

SYS_FUNC(read)
{
	if (entering(tcp)) {
		s_push_fd("fd");
		s_changeable_void("buf");
		s_push_lu("count");
	} else {
		if (syserror(tcp))
			s_push_addr("buf");
		else
			s_push_str("buf", tcp->u_rval);
	}
	return 0;
}

SYS_FUNC(write)
{
	unsigned long buf;
	unsigned long count;

	s_push_fd("fd");

	buf = s_get_cur_arg(S_TYPE_addr);
	count = s_get_cur_arg(S_TYPE_lu);

	s_insert_str("buf", buf, count);
	s_insert_lu("count", count);

	return RVAL_DECODED;
}

struct print_iovec_config {
	enum iov_decode decode_iov;
	unsigned long data_size;
};

static bool
print_iovec(struct tcb *tcp, void *elem_buf, size_t elem_size, void *data)
{
	const unsigned long *iov;
	unsigned long iov_buf[2], len;
	struct print_iovec_config *c = data;

        if (elem_size < sizeof(iov_buf)) {
		iov_buf[0] = ((unsigned int *) elem_buf)[0];
		iov_buf[1] = ((unsigned int *) elem_buf)[1];
		iov = iov_buf;
	} else {
		iov = elem_buf;
	}

	tprints("{iov_base=");

	len = iov[1];

	switch (c->decode_iov) {
		case IOV_DECODE_STR:
			if (len > c->data_size)
				len = c->data_size;
			c->data_size -= len;
			printstr(tcp, iov[0], len);
			break;
		case IOV_DECODE_NETLINK:
			if (len > c->data_size)
				len = c->data_size;
			c->data_size -= len;
			decode_netlink(tcp, iov[0], iov[1]);
			break;
		default:
			printaddr(iov[0]);
			break;
	}

	tprintf(", iov_len=%lu}", iov[1]);

	return true;
}

/*
 * data_size limits the cumulative size of printed data.
 * Example: recvmsg returing a short read.
 */
void
tprint_iov_upto(struct tcb *tcp, unsigned long len, unsigned long addr,
		enum iov_decode decode_iov, unsigned long data_size)
{
	unsigned long iov[2];
	struct print_iovec_config config =
		{ .decode_iov = decode_iov, .data_size = data_size };

	print_array(tcp, addr, len, iov, current_wordsize * 2,
		    umoven_or_printaddr, print_iovec, &config);
}

void
tprint_iov(struct tcb *tcp, unsigned long len, unsigned long addr,
	   enum iov_decode decode_iov)
{
	tprint_iov_upto(tcp, len, addr, decode_iov, (unsigned long) -1L);
}



static int
fill_iovec(struct s_arg *arg, void *buf, size_t elem_size, void *fn_data)
{
	struct {
		unsigned long data;
		unsigned long len;
	} iov;
	unsigned long len;
	struct print_iovec_config *c = fn_data;

	/* Personality-related stuff */
        if (elem_size < sizeof(iov)) {
		iov.data = ((unsigned int *) buf)[0];
		iov.len = ((unsigned int *) buf)[1];
	} else {
		iov.data = ((unsigned long *) buf)[0];
		iov.len = ((unsigned long *) buf)[1];
	}

	len = iov.len;

	switch (c->decode_iov) {
		case IOV_DECODE_STR:
			if (len > c->data_size)
				len = c->data_size;
			c->data_size -= len;
			s_insert_str("iov_base", iov.data, len);
			break;
		case IOV_DECODE_NETLINK:
			if (len > c->data_size)
				len = c->data_size;
			c->data_size -= len;
			s_insert_netlink("iov_base", iov.data, len);
			break;
		default:
			s_insert_addr("iov_base", iov.data);
			break;
	}

	s_insert_lu("iov_len", iov.len);

	return true;
}

void
s_insert_iov_upto(const char *name, unsigned long addr, unsigned long len,
	enum iov_decode decode_iov, unsigned long data_size)
{
	struct print_iovec_config config =
		{ .decode_iov = decode_iov, .data_size = data_size };

	s_insert_array_type(name, addr, len, current_wordsize * 2,
		S_TYPE_struct, fill_iovec, &config);
}

void
s_insert_iov(const char *name, unsigned long addr, unsigned long len,
	enum iov_decode decode_iov)
{
	s_insert_iov_upto(name, addr, len, decode_iov, (unsigned long) -1L);
}

void
s_push_iov_upto(const char *name, unsigned long len, enum iov_decode decode_iov,
	unsigned long data_size)
{
	struct print_iovec_config config =
		{ .decode_iov = decode_iov, .data_size = data_size };

	s_push_array_type(name, len, current_wordsize * 2,
		S_TYPE_struct, fill_iovec, &config);
}

void
s_push_iov(const char *name, unsigned long len, enum iov_decode decode_iov)
{
	s_push_iov_upto(name, len, decode_iov, (unsigned long) -1L);
}

SYS_FUNC(readv)
{
	if (entering(tcp)) {
		s_push_fd("fd");
		s_changeable_void("iov");
		s_push_lu("iovcnt");
	} else {
		unsigned long iov = s_get_cur_arg(S_TYPE_addr);
		unsigned long iovcnt = s_get_cur_arg(S_TYPE_lu);

		s_insert_iov_upto("iov", iov, iovcnt, IOV_DECODE_STR,
			tcp->u_rval);
	}
	return 0;
}

SYS_FUNC(writev)
{
	unsigned long iov;
	unsigned long iovcnt;

	s_push_fd("fd");

	iov = s_get_cur_arg(S_TYPE_addr);
	iovcnt = s_get_cur_arg(S_TYPE_lu);

	s_insert_iov("iov", iov, iovcnt, IOV_DECODE_STR);
	s_insert_lu("iovcnt", iovcnt);

	return RVAL_DECODED;
}

SYS_FUNC(pread)
{
	if (entering(tcp)) {
		s_push_fd("fd");
		s_changeable_void("buf");
		s_push_lu("count");
		s_push_lld("offset");
	} else {
		if (syserror(tcp))
			s_push_addr("buf");
		else
			s_push_str("buf", tcp->u_rval);
	}
	return 0;
}

SYS_FUNC(pwrite)
{
	unsigned long buf;
	unsigned long count;

	s_push_fd("fd");

	buf = s_get_cur_arg(S_TYPE_addr);
	count = s_get_cur_arg(S_TYPE_lu);

	s_insert_str("buf", buf, count);
	s_insert_lu("count", count);
	s_push_lld("offset");

	return RVAL_DECODED;
}

#include "xlat/rwf_flags.h"

static int
do_preadv(bool flags)
{
	if (entering(current_tcp)) {
		s_push_fd("fd");
		s_changeable_void("iov");
		s_push_lu("iovcnt");
		s_push_lld_lu_pair("pos");
		if (flags)
			s_push_flags_int("flags", rwf_flags, "RWF_???");
	} else {
		unsigned long iov = s_get_cur_arg(S_TYPE_addr);
		unsigned long iovcnt = s_get_cur_arg(S_TYPE_lu);

		s_insert_iov_upto("iov", iov, iovcnt, IOV_DECODE_STR,
			current_tcp->u_rval);
	}
	return 0;
}

SYS_FUNC(preadv)
{
	return do_preadv(false);
}

SYS_FUNC(preadv2)
{
	return do_preadv(true);
}

static int
do_pwritev(bool flags)
{
	unsigned long iov;
	unsigned long iovcnt;

	s_push_fd("fd");

	iov = s_get_cur_arg(S_TYPE_addr);
	iovcnt = s_get_cur_arg(S_TYPE_lu);

	s_insert_iov("iov", iov, iovcnt, IOV_DECODE_STR);
	s_insert_lu("iovcnt", iovcnt);
	s_push_lld_lu_pair("pos");
	if (flags)
		s_push_flags_int("flags", rwf_flags, "RWF_???");

	return RVAL_DECODED;
}

SYS_FUNC(pwritev)
{
	return do_pwritev(false);
}

SYS_FUNC(pwritev2)
{
	return do_pwritev(true);
}

#include "xlat/splice_flags.h"

SYS_FUNC(tee)
{
	s_push_fd("fd_in");
	s_push_fd("fd_out");
	s_push_lu("len");
	s_push_flags_int("flags", splice_flags, "SPLICE_F_???");

	return RVAL_DECODED;
}

SYS_FUNC(splice)
{
	s_push_fd("fd_in");
	s_push_lld_addr("off_in");
	s_push_fd("fd_out");
	s_push_lld_addr("off_out");
	s_push_lu("len");
	s_push_flags_int("flags", splice_flags, "SPLICE_F_???");

	return RVAL_DECODED;
}

SYS_FUNC(vmsplice)
{
	unsigned long iov;
	unsigned long nr_regs;

	s_push_fd("fd");

	iov = s_get_cur_arg(S_TYPE_addr);
	nr_regs = s_get_cur_arg(S_TYPE_lu);

	s_insert_iov("iov", iov, nr_regs, IOV_DECODE_STR);
	s_insert_lu("nr_regs", nr_regs);
	s_push_flags_int("flags", splice_flags, "SPLICE_F_???");

	return RVAL_DECODED;
}
