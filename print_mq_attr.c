/*
 * Copyright (c) 2004 Ulrich Drepper <drepper@redhat.com>
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

#include DEF_MPERS_TYPE(mq_attr_t)

#ifdef HAVE_MQUEUE_H
# include <mqueue.h>
typedef struct mq_attr mq_attr_t;
#elif defined HAVE_LINUX_MQUEUE_H
# include <linux/types.h>
# include <linux/mqueue.h>
typedef struct mq_attr mq_attr_t;
#endif

#include MPERS_DEFS

static int
fillmqattr(struct s_arg *arg, void *buf, unsigned long len, void *fn_data)
{
	mq_attr_t *attr = buf;

	s_insert_open_modes("mq_flags", attr->mq_flags);
	s_insert_ld("mq_maxmsg", (long) attr->mq_maxmsg);
	s_insert_ld("mq_msgsize", (long) attr->mq_msgsize);
	s_insert_ld("mq_curmsgs", (long) attr->mq_curmsgs);

	return 0;
}

static ssize_t
fetchmqattr(struct s_arg *arg, unsigned long addr, void *fn_data)
{
#if defined HAVE_MQUEUE_H || defined HAVE_LINUX_MQUEUE_H
	mq_attr_t attr;
	if (s_umove_verbose(current_tcp, addr, &attr))
		return -1;

	return fillmqattr(arg, &attr, sizeof(attr), fn_data);
#else
	return -1;
#endif
}

MPERS_PRINTER_DECL(void, s_push_mqattr, const char *name)
{
	s_push_addr_type(name, S_TYPE_struct, fetchmqattr, NULL);
}
