/*
 * Copyright (c) 2015 Dmitry V. Levin <ldv@altlinux.org>
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

#include <assert.h>

#ifdef HAVE_LINUX_SECCOMP_H
# include <linux/seccomp.h>
#endif
#include "xlat/seccomp_ops.h"
#include "xlat/seccomp_filter_flags.h"

#ifdef HAVE_LINUX_FILTER_H
# include <linux/filter.h>
# include "xlat/bpf_class.h"
# include "xlat/bpf_miscop.h"
# include "xlat/bpf_mode.h"
# include "xlat/bpf_op_alu.h"
# include "xlat/bpf_op_jmp.h"
# include "xlat/bpf_rval.h"
# include "xlat/bpf_size.h"
# include "xlat/bpf_src.h"

# ifndef SECCOMP_RET_ACTION
#  define SECCOMP_RET_ACTION 0x7fff0000U
# endif
# include "xlat/seccomp_ret_action.h"
#endif

struct bpf_filter {
	uint16_t code;
	uint8_t jt;
	uint8_t jf;
	uint32_t k;
};

static const size_t BPF_STR_BUF_SIZE = 4096;

#define _SNAPPEND(_str, _pos, _size, _func, ...) \
	do { \
		size_t __size = (_size); \
		size_t __pos = (_pos); \
		\
		if (__pos < __size) { \
			int __new_pos; \
			\
			__new_pos = _func(_str + __pos, __size - __pos, __VA_ARGS__); \
			\
			if (__new_pos >= 0) \
				_pos += __new_pos; \
		} \
	} while (0)

#ifdef HAVE_LINUX_FILTER_H

static size_t
decode_bpf_code(char *buf, size_t len, uint16_t code)
{
	uint16_t i = code & ~BPF_CLASS(code);
	size_t pos = 0;

	_SNAPPEND(buf, pos, len, sprintxval, bpf_class, BPF_CLASS(code), "BPF_???");
	switch (BPF_CLASS(code)) {
		case BPF_LD:
		case BPF_LDX:
			_SNAPPEND(buf, pos, len, snprintf, "|");
			_SNAPPEND(buf, pos, len, sprintxval, bpf_size, BPF_SIZE(code),
				"BPF_???");
			_SNAPPEND(buf, pos, len, snprintf, "|");
			_SNAPPEND(buf, pos, len, sprintxval, bpf_mode, BPF_MODE(code),
				"BPF_???");
			break;
		case BPF_ST:
		case BPF_STX:
			if (i)
				_SNAPPEND(buf, pos, len, snprintf, "|%#x /* %s */", i,
					"BPF_???");
			break;
		case BPF_ALU:
			_SNAPPEND(buf, pos, len, snprintf, "|");
			_SNAPPEND(buf, pos, len, sprintxval, bpf_src, BPF_SRC(code),
				"BPF_???");
			_SNAPPEND(buf, pos, len, snprintf, "|");
			_SNAPPEND(buf, pos, len, sprintxval, bpf_op_alu, BPF_OP(code),
				"BPF_???");
			break;
		case BPF_JMP:
			_SNAPPEND(buf, pos, len, snprintf, "|");
			_SNAPPEND(buf, pos, len, sprintxval, bpf_src, BPF_SRC(code),
				"BPF_???");
			_SNAPPEND(buf, pos, len, snprintf, "|");
			_SNAPPEND(buf, pos, len, sprintxval, bpf_op_jmp, BPF_OP(code),
				"BPF_???");
			break;
		case BPF_RET:
			_SNAPPEND(buf, pos, len, snprintf, "|");
			_SNAPPEND(buf, pos, len, sprintxval, bpf_rval, BPF_RVAL(code),
				"BPF_???");
			i &= ~BPF_RVAL(code);
			if (i)
				_SNAPPEND(buf, pos, len, snprintf, "|%#x /* %s */", i,
					"BPF_???");
			break;
		case BPF_MISC:
			_SNAPPEND(buf, pos, len, snprintf, "|");
			_SNAPPEND(buf, pos, len, sprintxval, bpf_miscop, BPF_MISCOP(code),
				"BPF_???");
			i &= ~BPF_MISCOP(code);
			if (i)
				_SNAPPEND(buf, pos, len, snprintf, "|%#x /* %s */", i,
					"BPF_???");
			break;
	}

	return pos;
}

#endif /* HAVE_LINUX_FILTER_H */

static char *
decode_bpf_stmt(const struct bpf_filter *filter)
{
	const size_t len = BPF_STR_BUF_SIZE;
	char *buf = malloc(len);
	size_t pos = 0;

	if (!buf)
		return NULL;

#ifdef HAVE_LINUX_FILTER_H
	_SNAPPEND(buf, pos, len, snprintf, "BPF_STMT(");
	_SNAPPEND(buf, pos, len, decode_bpf_code, filter->code);
	_SNAPPEND(buf, pos, len, snprintf, ", ");
	if (BPF_CLASS(filter->code) == BPF_RET) {
		unsigned int action = SECCOMP_RET_ACTION & filter->k;
		unsigned int data = filter->k & ~action;

		_SNAPPEND(buf, pos, len, sprintxval, seccomp_ret_action, action,
			"SECCOMP_RET_???");
		if (data)
			_SNAPPEND(buf, pos, len, snprintf, "|%#x)", data);
		else
			_SNAPPEND(buf, pos, len, snprintf, ")");
	} else {
		_SNAPPEND(buf, pos, len, snprintf, "%#x)", filter->k);
	}
#else
	_SNAPPEND(buf, pos, len, snprintf, "BPF_STMT(%#x, %#x)", filter->code,
		filter->k);
#endif /* HAVE_LINUX_FILTER_H */

	if (pos > len) {
		free(buf);
		buf = NULL;
	}

	return buf;
}

static char *
decode_bpf_jump(const struct bpf_filter *filter)
{
	const size_t len = BPF_STR_BUF_SIZE;
	char *buf = malloc(len);
	size_t pos = 0;

	if (!buf)
		return NULL;

#ifdef HAVE_LINUX_FILTER_H
	_SNAPPEND(buf, pos, len, snprintf, "BPF_JUMP(");
	_SNAPPEND(buf, pos, len, decode_bpf_code, filter->code);
	_SNAPPEND(buf, pos, len, snprintf, ", %#x, %#x, %#x)",
		filter->k, filter->jt, filter->jf);
#else
	_SNAPPEND(buf, pos, len, snprintf, "BPF_JUMP(%#x, %#x, %#x, %#x)",
		filter->code, filter->k, filter->jt, filter->jf);
#endif /* HAVE_LINUX_FILTER_H */

	if (pos > len) {
		free(buf);
		buf = NULL;
	}

	return buf;
}

#ifndef BPF_MAXINSNS
# define BPF_MAXINSNS 4096
#endif

static void
s_insert_bpf_code(const char *name, uint16_t code)
{
	static const struct xlat empty_xlat[] = { XLAT_END };
	uint16_t i = code & ~BPF_CLASS(code);

	s_insert_xlat_int(name, bpf_class, BPF_CLASS(code), "BPF_???");
	switch (BPF_CLASS(code)) {
		case BPF_LD:
		case BPF_LDX:
			s_append_xlat_int_val(NULL, bpf_size, BPF_SIZE(code), "BPF_???");
			s_append_xlat_int_val(NULL, bpf_mode, BPF_MODE(code), "BPF_???");
			break;
		case BPF_ST:
		case BPF_STX:
			if (i)
				s_append_xlat_int_val(NULL, empty_xlat, i, "BPF_???");
			break;
		case BPF_ALU:
			s_append_xlat_int_val(NULL, bpf_src, BPF_SRC(code), "BPF_???");
			s_append_xlat_int_val(NULL, bpf_op_alu, BPF_OP(code), "BPF_???");
			break;
		case BPF_JMP:
			s_append_xlat_int_val(NULL, bpf_src, BPF_SRC(code), "BPF_???");
			s_append_xlat_int_val(NULL, bpf_op_jmp, BPF_OP(code), "BPF_???");
			break;
		case BPF_RET:
			s_append_xlat_int_val(NULL, bpf_rval, BPF_RVAL(code), "BPF_???");
			i &= ~BPF_RVAL(code);
			if (i)
				s_append_xlat_int_val(NULL, empty_xlat, i, "BPF_???");
			break;
		case BPF_MISC:
			s_append_xlat_int_val(NULL, bpf_miscop, BPF_MISCOP(code),
				"BPF_???");
			i &= ~BPF_MISCOP(code);
			if (i)
				s_append_xlat_int_val(NULL, empty_xlat, i, "BPF_???");
			break;
		default:
			s_append_xlat_int_val(NULL, empty_xlat, code, "BPF_???");
	}
}

static int
fill_bpf_filter(struct s_arg *arg, void *buf, size_t len, void *data)
{
	const struct bpf_filter *filter = buf;
	unsigned int *pn = data;
	struct s_struct *struct_arg = S_ARG_TO_TYPE(arg, struct);

	assert(arg->type == S_TYPE_struct);

	s_insert_bpf_code("code", filter->code);
	s_insert_x("jt", filter->jt);
	s_insert_x("jf", filter->jf);
	s_insert_x("k", filter->k);

	if (filter->jt || filter->jf)
		s_struct_set_own_aux_str(struct_arg, decode_bpf_jump(filter));
	else
		s_struct_set_own_aux_str(struct_arg, decode_bpf_stmt(filter));

	if (++(*pn) >= BPF_MAXINSNS)
		return -1;

	return 0;
}

void
s_insert_seccomp_fprog(const char *name, unsigned long addr, unsigned short len)
{
	if (abbrev(current_tcp)) {
		s_insert_addr(name, addr);
	} else {
		unsigned int insns = 0;

		s_insert_array_type(name, addr, len, sizeof(struct bpf_filter),
			S_TYPE_struct, fill_bpf_filter, &insns);
	}
}

void
s_push_seccomp_fprog(const char *name, unsigned short len)
{
	if (abbrev(current_tcp)) {
		s_push_addr(name);
	} else {
		unsigned int insns = 0;

		s_push_array_type(name, len, sizeof(struct bpf_filter),
			S_TYPE_struct, fill_bpf_filter, &insns);
	}
}

#include "seccomp_fprog.h"

static ssize_t
fetch_fill_seccomp_filter(struct s_arg *arg, unsigned long addr, void *fn_data)
{
	struct seccomp_fprog fprog;
	ssize_t ret;

	if ((ret = fetch_seccomp_fprog(current_tcp, addr, &fprog)) > 0) {
		s_insert_u("len", fprog.len);
		s_insert_seccomp_fprog("filter", fprog.filter, fprog.len);
	}

	return ret;
}

void
s_push_seccomp_filter(const char *name)
{
	s_push_fetch_fill_struct(name, fetch_fill_seccomp_filter, NULL);
}

SYS_FUNC(seccomp)
{
	unsigned int op = tcp->u_arg[0];

	s_push_xlat_int("operation", seccomp_ops, "SECCOMP_SET_MODE_???");

	if (op == SECCOMP_SET_MODE_FILTER) {
		s_push_flags_int("flags", seccomp_filter_flags,
			   "SECCOMP_FILTER_FLAG_???");
		s_push_seccomp_filter("args");
	} else {
		s_push_u("flags");
		s_push_addr("args");
	}

	return RVAL_DECODED;
}
