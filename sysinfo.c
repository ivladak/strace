/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993-1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2012 H.J. Lu <hongjiu.lu@intel.com>
 * Copyright (c) 2012 Denys Vlasenko <vda.linux@googlemail.com>
 * Copyright (c) 2014-2015 Dmitry V. Levin <ldv@altlinux.org>
 * Copyright (c) 2015 Elvira Khabirova <lineprinter0@gmail.com>
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
#include DEF_MPERS_TYPE(sysinfo_t)
#include <sys/sysinfo.h>
typedef struct sysinfo sysinfo_t;
#include MPERS_DEFS

SYS_FUNC(sysinfo)
{
	sysinfo_t si;

	if (entering(tcp))
		return 0;

	if (!s_umove_verbose(tcp, tcp->u_arg[0], &si)) {
		s_insert_struct("sysinfo");
		s_insert_llu("uptime", si.uptime);
		s_insert_array("loads");
		s_insert_llu("1 minute", zero_extend_signed_to_ull(si.loads[0]));
		s_insert_llu("5 minutes", zero_extend_signed_to_ull(si.loads[1]));
		s_insert_llu("15 minutes", zero_extend_signed_to_ull(si.loads[2]));
		s_array_finish();
		s_insert_llu("totalram", zero_extend_signed_to_ull(si.totalram));
		s_insert_llu("freeram", zero_extend_signed_to_ull(si.freeram));
		s_insert_llu("sharedram", zero_extend_signed_to_ull(si.sharedram));
		s_insert_llu("bufferram", zero_extend_signed_to_ull(si.bufferram));
		s_insert_llu("totalswap", zero_extend_signed_to_ull(si.totalswap));
		s_insert_llu("freeswap", zero_extend_signed_to_ull(si.freeswap));
		s_insert_u("procs", (unsigned) si.procs);
		s_insert_llu("totalhigh", zero_extend_signed_to_ull(si.totalhigh));
		s_insert_llu("freehigh", zero_extend_signed_to_ull(si.freehigh));
		s_insert_u("mem_unit", si.mem_unit);
		s_struct_finish();
	} else {
		s_push_addr("sysinfo");
	}

	return 0;
}
