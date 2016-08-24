/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2000 PocketPenguins Inc.  Linux for Hitachi SuperH
 *                    port by Greg Banks <gbanks@pocketpenguins.com>
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
#include <asm/mman.h>
#include <sys/mman.h>

unsigned long
get_pagesize(void)
{
	static unsigned long pagesize;

	if (!pagesize)
		pagesize = sysconf(_SC_PAGESIZE);
	return pagesize;
}

SYS_FUNC(brk)
{
	s_push_addr("addr");

	return RVAL_DECODED | RVAL_HEX;
}

#include "xlat/mmap_prot.h"
#include "xlat/mmap_flags.h"

static void
print_mmap(struct tcb *tcp, long *u_arg, unsigned long long offset)
{
	const unsigned long addr = u_arg[0];
	const unsigned long len = u_arg[1];
	const unsigned long prot = u_arg[2];
	const unsigned long flags = u_arg[3];
	const int fd = u_arg[4];

	s_insert_addr("addr", addr);
	s_insert_lu("length", len);
	s_insert_flags_long("prot", mmap_prot, prot, "PROT_???");
#ifdef MAP_TYPE
	s_insert_xlat_long("flags", mmap_flags, flags & MAP_TYPE, "MAP_???");
	s_append_flags_long_val("flags", mmap_flags, flags & ~MAP_TYPE, NULL);
#else
	s_insert_flags_long("flags", mmap_flags, "MAP_???");
#endif
	s_insert_fd("fd", fd);
	s_insert_llx("offset", offset);
}

/* Syscall name<->function correspondence is messed up on many arches.
 * For example:
 * i386 has __NR_mmap == 90, and it is "old mmap", and
 * also it has __NR_mmap2 == 192, which is a "new mmap with page offsets".
 * But x86_64 has just one __NR_mmap == 9, a "new mmap with byte offsets".
 * Confused? Me too!
 */

#if defined AARCH64 || defined ARM \
 || defined I386 || defined X86_64 || defined X32 \
 || defined M68K \
 || defined S390 || defined S390X
/* Params are pointed to by u_arg[0], offset is in bytes */
SYS_FUNC(old_mmap)
{
	long u_arg[6];
# if defined AARCH64 || defined X86_64
	/* We are here only in a 32-bit personality. */
	unsigned int narrow_arg[6];
	if (umove_or_printaddr(tcp, tcp->u_arg[0], &narrow_arg))
		return RVAL_DECODED | RVAL_HEX;
	unsigned int i;
	for (i = 0; i < 6; i++)
		u_arg[i] = narrow_arg[i];
# else
	if (umove_or_printaddr(tcp, tcp->u_arg[0], &u_arg))
		return RVAL_DECODED | RVAL_HEX;
# endif
	print_mmap(tcp, u_arg, (unsigned long) u_arg[5]);

	return RVAL_DECODED | RVAL_HEX;
}
#endif /* old_mmap architectures */

#if defined(S390)
/* Params are pointed to by u_arg[0], offset is in pages */
SYS_FUNC(old_mmap_pgoff)
{
	long u_arg[5];
	int i;
	unsigned narrow_arg[6];
	unsigned long long offset;
	if (umoven(tcp, tcp->u_arg[0], sizeof(narrow_arg), narrow_arg) == -1)
		return 0;
	for (i = 0; i < 5; i++)
		u_arg[i] = (unsigned long) narrow_arg[i];
	offset = narrow_arg[5];
	offset *= get_pagesize();
	print_mmap(tcp, u_arg, offset);

	return RVAL_DECODED | RVAL_HEX;
}
#endif

/* Params are passed directly, offset is in bytes */
SYS_FUNC(mmap)
{
	unsigned long long offset =
#if HAVE_STRUCT_TCB_EXT_ARG
		tcp->ext_arg[5];	/* try test/x32_mmap.c */
#else
		(unsigned long) tcp->u_arg[5];
#endif
	/* Example of kernel-side handling of this variety of mmap:
	 * arch/x86/kernel/sys_x86_64.c::SYSCALL_DEFINE6(mmap, ...) calls
	 * sys_mmap_pgoff(..., off >> PAGE_SHIFT); i.e. off is in bytes,
	 * since the above code converts off to pages.
	 */
	print_mmap(tcp, tcp->u_arg, offset);

	return RVAL_DECODED | RVAL_HEX;
}

/* Params are passed directly, offset is in pages */
SYS_FUNC(mmap_pgoff)
{
	/* Try test/mmap_offset_decode.c */
	unsigned long long offset;
	offset = (unsigned long) tcp->u_arg[5];
	offset *= get_pagesize();
	print_mmap(tcp, tcp->u_arg, offset);

	return RVAL_DECODED | RVAL_HEX;
}

/* Params are passed directly, offset is in 4k units */
SYS_FUNC(mmap_4koff)
{
	unsigned long long offset;
	offset = (unsigned long) tcp->u_arg[5];
	offset <<= 12;
	print_mmap(tcp, tcp->u_arg, offset);

	return RVAL_DECODED | RVAL_HEX;
}

SYS_FUNC(munmap)
{
	s_push_addr("addr");
	s_push_lu("length");

	return RVAL_DECODED;
}

SYS_FUNC(mprotect)
{
	s_push_addr("addr");
	s_push_lu("len");
	s_push_flags_long("prot", mmap_prot, "PROT_???");

	return RVAL_DECODED;
}

#include "xlat/mremap_flags.h"

SYS_FUNC(mremap)
{
	unsigned long flags;

	s_push_addr("old_address");
	s_push_lu("old_size");
	s_push_lu("new_size");

	flags = s_get_cur_arg(S_TYPE_lu);
	s_insert_flags_long("flags", mremap_flags, flags, "MREMAP_???");
#ifdef MREMAP_FIXED
	if ((flags & (MREMAP_MAYMOVE | MREMAP_FIXED)) ==
	    (MREMAP_MAYMOVE | MREMAP_FIXED)) {
		s_push_addr("new_address");
	}
#endif
	return RVAL_DECODED | RVAL_HEX;
}

#include "xlat/madvise_cmds.h"

SYS_FUNC(madvise)
{
	s_push_addr("addr");
	s_push_lu("len");
	s_push_xlat_int("advice", madvise_cmds, "MADV_???");

	return RVAL_DECODED;
}

#include "xlat/mlockall_flags.h"

SYS_FUNC(mlockall)
{
	s_push_flags_int("flags", mlockall_flags, "MCL_???");

	return RVAL_DECODED;
}

#include "xlat/mctl_sync.h"

SYS_FUNC(msync)
{
	s_push_addr("addr");
	s_push_lu("length");
	s_push_flags_int("flags", mctl_sync, "MS_???");

	return RVAL_DECODED;
}

#include "xlat/mlock_flags.h"

SYS_FUNC(mlock2)
{
	s_push_addr("addr");
	s_push_lu("len");
	s_push_flags_int("flags", mlock_flags, "MLOCK_???");

	return RVAL_DECODED;
}

static int
fill_mincore_vec(struct s_arg *arg, void *buf, size_t len, void *fn_data)
{
	struct s_num *num = S_ARG_TO_TYPE(arg, num);
	uint8_t *val = buf;

	num->val = *val;

	return 0;
}

SYS_FUNC(mincore)
{
	if (entering(tcp)) {
		s_push_addr("addr");
		s_push_lu("length");
		s_changeable_void("vec");
	} else {
		const unsigned long page_size = get_pagesize();
		const unsigned long page_mask = page_size - 1;
		unsigned long len = tcp->u_arg[1];

		len = len / page_size + (len & page_mask ? 1 : 0);

		s_push_array_type("vec", len, 1, S_TYPE_hhu, fill_mincore_vec,
			NULL);
	}
	return 0;
}

#if defined ALPHA || defined IA64 || defined M68K \
 || defined SPARC || defined SPARC64
SYS_FUNC(getpagesize)
{
	return RVAL_DECODED | RVAL_HEX;
}
#endif

SYS_FUNC(remap_file_pages)
{
	s_push_addr("addr");
	s_push_lu("size");
	s_push_flags_long("prot", mmap_prot, "PROT_???");
	s_push_lu("pgoff");
#ifdef MAP_TYPE
	const unsigned long flags = s_get_cur_arg(S_TYPE_lu);
	s_insert_xlat_long("flags", mmap_flags, flags & MAP_TYPE, "MAP_???");
	s_append_flags_long_val("flags", mmap_flags, flags & ~MAP_TYPE, NULL);
#else
	s_push_flags_long("flags", mmap_flags, "MAP_???");
#endif

	return RVAL_DECODED;
}

#if defined(POWERPC)
static bool
print_protmap_entry(struct tcb *tcp, void *elem_buf, size_t elem_size, void *data)
{
	tprintf("%#08x", * (unsigned int *) elem_buf);

	return true;
}

SYS_FUNC(subpage_prot)
{
	unsigned long addr = tcp->u_arg[0];
	unsigned long len = tcp->u_arg[1];
	unsigned long nmemb = len >> 16;
	unsigned long map = tcp->u_arg[2];

	printaddr(addr);
	tprintf(", %lu, ", len);

	unsigned int entry;
	print_array(tcp, map, nmemb, &entry, sizeof(entry),
		    umoven_or_printaddr, print_protmap_entry, 0);

	return RVAL_DECODED;
}
#endif
