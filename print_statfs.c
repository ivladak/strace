/*
 * Copyright (c) 2014-2016 Dmitry V. Levin <ldv@altlinux.org>
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
#include "statfs.h"
#include "xlat/fsmagic.h"
#include "xlat/statfs_flags.h"

ssize_t
fill_struct_statfs(struct s_arg *arg, unsigned long addr, void *fn_data)
{
#ifdef HAVE_STRUCT_STATFS
	struct strace_statfs b;
	ssize_t ret;

	if ((ret = fetch_struct_statfs(current_tcp, addr, &b)) < 1)
		return -1;

	s_insert_xlat_64_sorted("f_type", fsmagic, ARRAY_SIZE(fsmagic),
		b.f_type);
	s_insert_llu("f_bsize", b.f_bsize);
	s_insert_llu("f_blocks", b.f_blocks);
	s_insert_llu("f_bfree", b.f_bfree);
	s_insert_llu("f_bavail", b.f_bavail);
	s_insert_llu("f_files", b.f_files);
	s_insert_llu("f_ffree", b.f_ffree);
# if defined HAVE_STRUCT_STATFS_F_FSID_VAL \
  || defined HAVE_STRUCT_STATFS_F_FSID___VAL
	s_insert_struct("f_fsid");
#  ifdef HAVE_STRUCT_STATFS_F_FSID___VAL
	s_insert_array("__val");
#  else
	s_insert_array("val");
#  endif
	s_insert_llu(NULL, b.f_fsid[0]);
	s_insert_llu(NULL, b.f_fsid[1]);
	s_array_finish();
	s_struct_finish();
# endif
	s_insert_llu("f_namelen", b.f_namelen);
# ifdef HAVE_STRUCT_STATFS_F_FRSIZE
	s_insert_llu("f_frsize", b.f_frsize);
# endif
# ifdef HAVE_STRUCT_STATFS_F_FLAGS
	s_insert_flags_64("f_flags", statfs_flags, b.f_flags, "ST_???");
# endif

	return ret;
#else
	return -2;
#endif
}

ssize_t
fill_struct_statfs64(struct s_arg *arg, unsigned long addr, void *fn_data)
{
#ifdef HAVE_STRUCT_STATFS64
	struct strace_statfs b;
	ssize_t ret;

	if ((ret = fetch_struct_statfs64(current_tcp, addr,
	    (unsigned long)fn_data, &b) < 0))
		return -1;

	s_insert_xlat_64_sorted("f_type", fsmagic, ARRAY_SIZE(fsmagic),
		b.f_type);
	s_insert_llu("f_bsize", b.f_bsize);
	s_insert_llu("f_blocks", b.f_blocks);
	s_insert_llu("f_bfree", b.f_bfree);
	s_insert_llu("f_bavail", b.f_bavail);
	s_insert_llu("f_files", b.f_files);
	s_insert_llu("f_ffree", b.f_ffree);
# if defined HAVE_STRUCT_STATFS64_F_FSID_VAL \
  || defined HAVE_STRUCT_STATFS64_F_FSID___VAL
	s_insert_struct("f_fsid");
#  ifdef HAVE_STRUCT_STATFS64_F_FSID___VAL
	s_insert_array("__val");
#  else
	s_insert_array("val");
#  endif
	s_insert_llu(NULL, b.f_fsid[0]);
	s_insert_llu(NULL, b.f_fsid[1]);
	s_array_finish();
	s_struct_finish();
# endif
	s_insert_llu("f_namelen", b.f_namelen);
# ifdef HAVE_STRUCT_STATFS64_F_FRSIZE
	s_insert_llu("f_frsize", b.f_frsize);
# endif
# ifdef HAVE_STRUCT_STATFS64_F_FLAGS
	s_insert_flags_64("f_flags", statfs_flags, b.f_flags, "ST_???");
# endif

	return ret;
#else
	return -2;
#endif
}
