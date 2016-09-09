/*
 * Copyright (c) 1993 Ulrich Pegelow <pegelow@moorea.uni-muenster.de>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2003-2006 Roland McGrath <roland@redhat.com>
 * Copyright (c) 2006-2015 Dmitry V. Levin <ldv@altlinux.org>
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

#include DEF_MPERS_TYPE(msqid_ds_t)

#include "ipc_defs.h"

#ifdef HAVE_SYS_MSG_H
/* The C library generally exports the struct the current kernel expects. */
# include <sys/msg.h>
typedef struct msqid_ds msqid_ds_t;
#elif defined HAVE_LINUX_MSG_H
/* The linux header might provide the right struct. */
# include <linux/msg.h>
typedef struct msqid64_ds msqid_ds_t;
#endif

#include MPERS_DEFS

#include "xlat/msgctl_flags.h"

static int
fill_msqid_ds(struct s_arg *arg, void *buf, unsigned long len, void *fn_data)
{
	msqid_ds_t *msqid_ds = buf;
	int cmd = (int) (long) fn_data;

	s_insert_struct("msg_perm");
	s_insert_uid("uid", msqid_ds->msg_perm.uid);
	s_insert_gid("gid", msqid_ds->msg_perm.gid);
	s_insert_umode_t("mode", msqid_ds->msg_perm.mode);

	if (cmd != IPC_STAT) {
		s_struct_finish();
		s_insert_ellipsis();
		return 0;
	}

	s_insert_u("key", msqid_ds->msg_perm.__key);
	s_insert_uid("cuid", msqid_ds->msg_perm.cuid);
	s_insert_gid("cgid", msqid_ds->msg_perm.cgid);
	s_struct_finish();

	s_insert_u("msg_stime", msqid_ds->msg_stime);
	s_insert_u("msg_rtime", msqid_ds->msg_rtime);
	s_insert_u("msg_ctime", msqid_ds->msg_ctime);
	s_insert_u("msg_qnum", msqid_ds->msg_qnum);
	s_insert_u("msg_qbytes", msqid_ds->msg_qbytes);
	s_insert_u("msg_lspid", msqid_ds->msg_lspid);
	s_insert_u("msg_lrpid", msqid_ds->msg_lrpid);

	return 0;
}

SYS_FUNC(msgctl)
{
	if (entering(tcp)) {
		s_push_d("msqid");
		s_insert_xlat_int("cmd", msgctl_flags, tcp->u_arg[1], "MSG_???");
	} else {
		const long addr = tcp->u_arg[indirect_ipccall(tcp) ? 3 : 2];
		int cmd = tcp->u_arg[1];
		if (cmd & IPC_64)
			cmd &= ~IPC_64;
		if (cmd == IPC_SET || cmd == IPC_STAT) {
			s_insert_addr_type_sized("buf", addr, sizeof(msqid_ds_t),
				S_TYPE_struct, fill_msqid_ds, (void *) (long) cmd);
		} else {
			s_insert_addr("buf", addr);
		}
	}
	return 0;
}
