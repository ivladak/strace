/* Functions and structures to replace strace's traditional
 * output with new structured output subsystem
 */

#include <assert.h>
#include <signal.h>
#include <stdarg.h>

#include "defs.h"
#include "printsiginfo.h"

#include "structured.h"
#include "structured_sigmask.h"

#include "structured_fmt_text.h"
#include "structured_fmt_text_z.h"
#include "structured_fmt_json.h"

/** List of printers used. */
struct s_printer *s_printers[] = {
	&s_printer_text,
	&s_printer_text_z,
	&s_printer_json,
	NULL
};

struct s_printer *s_printer_cur;

/* syscall representation */

#define S_TYPE_FUNC(S_TYPE_FUNC_NAME, S_TYPE_FUNC_ARG, S_TYPE_FUNC_RET, \
    S_TYPE_SWITCH) \
	S_TYPE_FUNC_RET \
	S_TYPE_FUNC_NAME(S_TYPE_FUNC_ARG arg) \
	{ \
		switch (S_TYPE_KIND(S_TYPE_SWITCH)) { \
		__ARG_TYPE_CASE(num); \
		__ARG_TYPE_CASE(str); \
		__ARG_TYPE_CASE(addr); \
		__ARG_TYPE_CASE(xlat); \
		__ARG_TYPE_CASE(sigmask); \
		__ARG_TYPE_CASE(struct); \
		__ARG_TYPE_CASE(ellipsis); \
		__ARG_TYPE_CASE(changeable); \
		\
		default: \
			assert(0); \
		} \
		\
		return (S_TYPE_FUNC_RET)0; \
	}

#define __ARG_TYPE_CASE(_type) \
	case S_TYPE_KIND_##_type: \
		return S_ARG_TO_TYPE(arg, _type)

S_TYPE_FUNC(s_arg_to_type, struct s_arg *, void *, arg->type)

#undef __ARG_TYPE_CASE

#define __ARG_TYPE_CASE(_type) \
	case S_TYPE_KIND_##_type: \
		return __offsetof(struct s_##_type, arg)

S_TYPE_FUNC(s_type_offs, enum s_type, size_t, arg)

#undef __ARG_TYPE_CASE

#define __ARG_TYPE_CASE(_type) \
	case S_TYPE_KIND_##_type: \
		return sizeof(struct s_##_type)

S_TYPE_FUNC(s_type_size, enum s_type, size_t, arg)

#undef __ARG_TYPE_CASE

struct s_arg *
s_type_to_arg(void *p, enum s_type type)
{
	return (struct s_arg *)((char *)p + s_type_offs(type));
}


struct s_arg *
s_arg_new(struct tcb *tcp, enum s_type type, const char *name)
{
	struct s_syscall *syscall = tcp->s_syscall;
	void *p = xcalloc(1, s_type_size(type));
	struct s_arg *arg = s_type_to_arg(p, type);

	arg->syscall = syscall;
	arg->type = type;
	arg->name = name;
	arg->arg_num = -1;

	return arg;
}

void
s_arg_insert(struct s_syscall *syscall, struct s_arg *arg, int force_arg)
{
	struct s_args_list *ins_point = s_syscall_insertion_point(syscall);

	if (entering(syscall->tcp) || !syscall->next_ins_changeable ||
	    (ins_point != &syscall->args)) {
		if (force_arg >= 0) {
			arg->arg_num = force_arg;
		} else if (ins_point == &syscall->args) {
			assert(syscall->next_ins_idx < MAX_ARGS);
			if (syscall->next_ins_idx > syscall->next_get_idx) {
				unsigned long long dummy;

				/* error_msg("s_arg_insert warning: performing "
					"insert without advance\n"); */
				s_syscall_cur_arg_advance(syscall, arg->type,
					&dummy);
			}
			assert(syscall->next_ins_idx <= syscall->next_get_idx);
			arg->arg_num = syscall->arg_idx[syscall->next_ins_idx];
			syscall->next_ins_idx++;
		} else {
			arg->arg_num = -1;
		}

		list_append(&ins_point->args, &arg->entry);

		if (arg->type == S_TYPE_changeable) {
			assert(!entering(syscall->tcp) ||
				!S_ARG_TO_TYPE(arg, changeable)->exiting);
			list_append(&syscall->changeable_args, &arg->chg_entry);
		}
	} else {
		struct s_arg *chg = syscall->next_ins_changeable;
		struct s_changeable *s_ch = S_ARG_TO_TYPE(chg, changeable);

		syscall->next_ins_changeable = list_next(chg, chg_entry);
		if (&syscall->next_ins_changeable->chg_entry ==
		    &syscall->changeable_args)
			syscall->next_ins_changeable = NULL;

		assert(!s_ch->exiting);
		s_ch->exiting = arg;
	}

	syscall->last_arg_inserted = arg;
}

void
s_arg_free(struct s_arg *arg)
{
	switch (arg->type) {
	case S_TYPE_str: {
		struct s_str *s_p = S_ARG_TO_TYPE(arg, str);

		if (s_p->str)
			free(s_p->str);

		break;
	}
	case S_TYPE_addr: {
		struct s_addr *p = S_ARG_TO_TYPE(arg, addr);

		if (p->val)
			s_arg_free(p->val);

		break;
	}
	case S_TYPE_struct: {
		struct s_struct *p = S_ARG_TO_TYPE(arg, struct);
		struct s_arg *arg;
		struct s_arg *tmp;

		list_foreach_safe(arg, &p->args.args, entry, tmp)
			s_arg_free(arg);

		if (p->own)
			free(p->own_aux_str);

		break;
	}
	case S_TYPE_changeable: {
		struct s_changeable *p = S_ARG_TO_TYPE(arg, changeable);

		if (p->entering)
			s_arg_free(p->entering);
		if (p->exiting)
			s_arg_free(p->exiting);

		break;
	}
	default:
		break;
	}

	free(s_arg_to_type(arg));
}


struct s_num *
s_num_new(enum s_type type, const char *name, uint64_t value)
{
	struct s_num *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, type, name),
		num);

	res->val = value;

	return res;
}

/* XXX Accommodates parts of logic from fetchstr and printpathn  */
struct s_str *
s_str_new(enum s_type type, const char *name, long addr, long len,
	unsigned flags)
{
	struct s_str *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, type, name),
		str);
	char *buf = NULL;
	size_t size;
	unsigned add_flags = 0;
	int ret;

	if (len >= 0) {
		size = len;
	} else {
		size = max_strlen;
		add_flags = QUOTE_ELLIPSIS;
	}

	if (!addr)
		goto s_str_new_fail;

	buf = xmalloc(size + 1);

	if (flags & QUOTE_0_TERMINATED) {
		ret = umovestr(current_tcp, addr, size + 1, buf);

		if (ret == 0)
			add_flags |= QUOTE_ELLIPSIS;
	} else {
		ret = umoven(current_tcp, addr, size, buf);
	}

	if (ret < 0)
		goto s_str_new_fail;

	res->str = buf;
	res->len = size;
	res->flags = flags | add_flags;

	return res;

s_str_new_fail:
	free(buf);
	s_arg_free(&res->arg);

	return NULL;
}

struct s_str *
s_str_val_new(enum s_type type, const char *name, const char *str, long len,
	unsigned flags)
{
	struct s_str *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, type, name),
		str);

	if (len >= 0) {
		res->str = strndup(str, len + !!(flags & QUOTE_0_TERMINATED));
		res->len = len;
		res->flags = flags;
	} else {
		res->str = strdup(str);
		res->len = strlen(res->str);
		res->flags = flags | QUOTE_0_TERMINATED;
	}

	return res;
}

struct s_addr *
s_addr_new(const char *name, long addr, struct s_arg *arg)
{
	struct s_addr *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, S_TYPE_addr,
		name), addr);

	res->addr = addr;
	res->val = arg;

	return res;
}

struct s_xlat *
s_xlat_new(enum s_type type, const char *name, const struct xlat *x,
	uint64_t val, const char *dflt, bool flags, int8_t scale, bool empty)
{
	struct s_xlat *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, type, name),
		xlat);

	assert((abs(scale) < 64) && !(val & ~(~0 << abs(scale))));

	res->x = x;
	res->val = val;
	res->dflt = dflt;
	res->flags = flags;
	res->empty = empty;
	res->scale = scale;

	list_init(&res->entry);

	return res;
}

struct s_struct *
s_struct_new(enum s_type type, const char *name)
{
	struct s_struct *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, type, name),
		struct);

	list_init(&res->args.args);

	return res;
}

struct s_ellipsis *
s_ellipsis_new(void)
{
	struct s_ellipsis *res = S_ARG_TO_TYPE(s_arg_new(current_tcp,
		S_TYPE_ellipsis, NULL), ellipsis);

	return res;
}

struct s_changeable *
s_changeable_new(const char *name, struct s_arg *entering,
	struct s_arg *exiting)
{
	struct s_changeable *res = S_ARG_TO_TYPE(s_arg_new(current_tcp,
		S_TYPE_changeable, name), changeable);

	res->entering = entering;
	res->exiting = exiting;

	return res;
}

struct s_arg *
s_arg_new_init(struct tcb *tcp, enum s_type type, const char *name)
{
	switch (S_TYPE_KIND(type)) {
	case S_TYPE_KIND_num:
		return &s_num_new(type, name, 0)->arg;
	case S_TYPE_KIND_str:
		/* Since s_str_new fails and returns NULL on empty string */
		return s_arg_new(tcp, type, name);
	case S_TYPE_KIND_addr:
		return &s_addr_new(name, 0, NULL)->arg;
	case S_TYPE_KIND_xlat:
		return &s_xlat_new(type, name, NULL, 0, NULL, false, 0,
			false)->arg;
	case S_TYPE_KIND_sigmask:
		return &s_sigmask_new(name, NULL, 0)->arg;
	case S_TYPE_KIND_struct:
		return &s_struct_new(type, name)->arg;
	case S_TYPE_KIND_changeable:
		return &s_changeable_new(name, NULL, NULL)->arg;

	default:
		return s_arg_new(tcp, type, name);
	}

	return s_arg_new(tcp, type, name);
}

bool
s_arg_equal(struct s_arg *arg1, struct s_arg *arg2)
{
	if (!arg1 || !arg2)
		return false;

	/* This is fine for now */
	if (S_TYPE_KIND(arg1->type) != S_TYPE_KIND(arg2->type))
		return false;

	switch (S_TYPE_KIND(arg1->type)) {
	case S_TYPE_KIND_num: {
		struct s_num *num1 = S_ARG_TO_TYPE(arg1, num);
		struct s_num *num2 = S_ARG_TO_TYPE(arg2, num);

		return num1->val == num2->val;
	}
	case S_TYPE_KIND_str: {
		struct s_str *str1 = S_ARG_TO_TYPE(arg1, str);
		struct s_str *str2 = S_ARG_TO_TYPE(arg2, str);

		if (str1->len != str2->len)
			return false;

		return strncmp(str1->str, str2->str, str1->len);
	}
	case S_TYPE_KIND_addr: {
		struct s_addr *addr1 = S_ARG_TO_TYPE(arg1, addr);
		struct s_addr *addr2 = S_ARG_TO_TYPE(arg2, addr);

		if (addr1->addr != addr2->addr)
			return false;
		if (!addr1->val && !addr2->val)
			return true;
		if (!addr1->val || !addr2->val)
			return false;

		return s_arg_equal(addr1->val, addr2->val);
	}
	case S_TYPE_KIND_xlat: {
		struct s_xlat *xlat1 = S_ARG_TO_TYPE(arg1, xlat);
		struct s_xlat *xlat2 = S_ARG_TO_TYPE(arg2, xlat);
		struct s_xlat *xlat1_cur = xlat1;
		struct s_xlat *xlat2_cur = xlat2;

		do {
			if ((xlat1_cur->x != xlat2_cur->x) ||
			    (xlat1_cur->val != xlat2_cur->val))
				return false;

			xlat1_cur = list_next(xlat1_cur, entry);
			xlat2_cur = list_next(xlat2_cur, entry);
		} while ((xlat1 != xlat1_cur) && (xlat2 != xlat2_cur));

		if ((xlat1 != xlat1_cur) || (xlat2 != xlat2_cur))
			return false;

		return true;
	}
	case S_TYPE_KIND_sigmask: {
		struct s_sigmask *sigmask1 = S_ARG_TO_TYPE(arg1, sigmask);
		struct s_sigmask *sigmask2 = S_ARG_TO_TYPE(arg2, sigmask);
		unsigned i;

		if (sigmask1->bytes != sigmask2->bytes)
			return false;

		for (i = 0; i < sigmask1->bytes; i++)
			if (sigmask1->sigmask[i] != sigmask2->sigmask[i])
				return false;

		return true;
	}
	case S_TYPE_KIND_struct: {
		struct s_struct *struct1 = S_ARG_TO_TYPE(arg1, struct);
		struct s_struct *struct2 = S_ARG_TO_TYPE(arg2, struct);
		struct s_arg *field1 = list_head(&struct1->args.args,
			struct s_arg, entry);
		struct s_arg *field2 = list_head(&struct2->args.args,
			struct s_arg, entry);

		while (field1 && field2) {
			if (!s_arg_equal(field1, field2))
				return false;

			field1 = list_next(field1, entry);
			field2 = list_next(field2, entry);
		}

		if (field1 || field2)
			return false;

		return true;
	}
	case S_TYPE_KIND_changeable: {
		struct s_changeable *chg1 = S_ARG_TO_TYPE(arg1, changeable);
		struct s_changeable *chg2 = S_ARG_TO_TYPE(arg2, changeable);

		if (chg1->entering && chg2->entering) {
			if (!s_arg_equal(chg1->entering, chg2->entering))
				return false;
		} else {
			if (chg1->entering || chg2->entering)
				return false;
		}

		if (chg1->exiting && chg2->exiting) {
			if (!s_arg_equal(chg1->exiting, chg2->exiting))
				return false;
		} else {
			if (chg1->exiting || chg2->exiting)
				return false;
		}

		return true;
	}

	default:
		return false;
	}

	return false;
}

struct s_num *
s_num_new_and_insert(enum s_type type, const char *name, uint64_t value)
{
	struct s_num *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_num_new(type, name, value))->arg, -1);

	return res;
}

struct s_addr *
s_addr_new_and_insert(const char *name, long addr, struct s_arg *arg)
{
	struct s_addr *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_addr_new(name, addr, arg))->arg, -1);

	return res;
}

struct s_xlat *
s_xlat_new_and_insert(enum s_type type, const char *name, const struct xlat *x,
	uint64_t val, const char *dflt, bool flags, int8_t scale, bool empty)
{
	struct s_xlat *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_xlat_new(type, name, x, val, dflt, flags,
			scale, empty))->arg, -1);

	return res;
}

struct s_struct *
s_struct_new_and_insert(enum s_type type, const char *name)
{
	struct s_struct *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_struct_new(type, name))->arg, -1);

	return res;
}

struct s_ellipsis *
s_ellipsis_new_and_insert(void)
{
	struct s_ellipsis *res;

	s_arg_insert(current_tcp->s_syscall, &(res = s_ellipsis_new())->arg,
		-1);

	return res;
}

extern struct s_changeable *s_changeable_new_and_insert(const char *name,
	struct s_arg *entering, struct s_arg *exiting)
{
	struct s_changeable *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_changeable_new(name, entering, exiting))->arg,
		entering ? entering->arg_num : -1);

	return res;
}

struct s_xlat *
s_xlat_append(enum s_type type, const char *name, const struct xlat *x,
	uint64_t val, const char *dflt, bool flags, int8_t scale,
	const char *comment)
{
	struct s_arg *last_arg = s_syscall_get_last_arg(current_tcp->s_syscall);
	struct s_xlat *last_xlat;
	struct s_xlat *res;

	if (last_arg && (last_arg->type == S_TYPE_addr))
		last_arg = S_ARG_TO_TYPE(last_arg, addr)->val;

	if (!last_arg || (S_TYPE_KIND(last_arg->type) != S_TYPE_KIND_xlat))
		return s_xlat_new_and_insert(type, name, x, val, dflt,
			flags, scale, false);

	res = s_xlat_new(type, name, x, val, dflt, flags, scale, false);
	res->arg.comment = comment;

	last_xlat = S_ARG_TO_TYPE(last_arg, xlat);

	list_append(&last_xlat->entry, &res->entry);

	return res;
}

struct s_struct *
s_struct_set_aux_str(struct s_struct *s, const char *aux_str)
{
	if (s->own)
		free(s->own_aux_str);

	s->own = false;
	s->aux_str = aux_str;

	return s;
}

struct s_struct *
s_struct_set_own_aux_str(struct s_struct *s, char *aux_str)
{
	if (s->own)
		free(s->own_aux_str);

	s->own = true;
	s->own_aux_str = aux_str;

	return s;
}

const char *
s_arg_append_comment(struct s_syscall *s, const char *comment)
{
	struct s_arg *arg = s_syscall_get_last_arg(s);
	const char *old_comment;

	assert(arg);

	old_comment = arg->comment;
	arg->comment = comment;

	return old_comment;
}

struct s_args_list *
s_struct_enter(struct s_struct *s)
{
	list_insert(&s->arg.syscall->insertion_stack, &s->args.entry);

	return &s->args;
}

struct s_args_list *
s_syscall_insertion_point(struct s_syscall *s)
{
	if (list_is_empty(&s->insertion_stack))
		return &s->args;

	return list_head(&s->insertion_stack, struct s_args_list,
		entry);
}

struct s_args_list *
s_syscall_pop(struct s_syscall *s)
{
	list_remove_head(&s->insertion_stack);

	return s_syscall_insertion_point(s);
}

struct s_args_list *
s_syscall_pop_all(struct s_syscall *s)
{
	list_init(&s->insertion_stack);

	return &s->args;
}


struct s_syscall *
s_syscall_new(struct tcb *tcp, enum s_syscall_type sc_type)
{
	struct s_syscall *syscall = xmalloc(sizeof(*syscall));

	tcp->s_syscall = syscall;

	syscall->tcp = tcp;
	syscall->type = sc_type;
	syscall->next_get_idx = syscall->next_ins_idx = 0;
	syscall->arg_idx[0] = 0;
	syscall->name_level = S_SAN_DEFAULT;
	syscall->comment_level = S_SAN_DEFAULT;

	list_init(&syscall->args.args);
	list_init(&syscall->changeable_args);
	list_init(&syscall->insertion_stack);

	return syscall;
}

void
s_last_is_changeable(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *last_arg = s_syscall_get_last_arg(syscall);
	struct s_changeable *s_chg;

	assert(last_arg);

	s_chg = s_changeable_new(last_arg->name, last_arg, NULL);
	s_chg->arg.arg_num = last_arg->arg_num;

	list_replace(&last_arg->entry, &s_chg->arg.entry);

	if (entering(syscall->tcp))
		list_append(&syscall->changeable_args, &s_chg->arg.chg_entry);
}

void
s_syscall_free(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *arg;
	struct s_arg *tmp;

	list_foreach_safe(arg, &syscall->args.args, entry, tmp) {
		s_arg_free(arg);
	}

	free(syscall);
	tcp->s_syscall = NULL;
}

struct s_arg *
s_syscall_get_last_arg(struct s_syscall *syscall)
{
	return syscall->last_arg_inserted;
}

static void
s_syscall_pop_fixups(struct s_arg *arg)
{
	/* For addr arg, the following fix ups should be done on addr's val */
	while (S_TYPE_KIND(arg->type) == S_TYPE_KIND_addr) {
		struct s_addr *addr = S_ARG_TO_TYPE(arg, addr);

		if (addr->val)
			arg = addr->val;
	}

	/* Fix up changeable */
	if ((S_TYPE_KIND(arg->type) == S_TYPE_KIND_changeable) &&
	    arg->chg_entry.next)
		list_remove(&arg->chg_entry);

	/* Fix up insertion stack */
	if (S_TYPE_KIND(arg->type) == S_TYPE_KIND_struct) {
		struct s_struct *s = S_ARG_TO_TYPE(arg, struct);
		struct s_arg *field;

		list_foreach(field, &s->args.args, entry) {
			s_syscall_pop_fixups(field);
		}

		if (s->args.entry.next)
			list_remove(&s->args.entry);
	}
}

static void
s_syscall_pop_arg(struct s_arg *arg)
{
	list_remove(&arg->entry);

	s_syscall_pop_fixups(arg);
}

struct s_arg *
s_syscall_pop_last_arg(struct s_syscall *syscall)
{
	struct s_arg *last = syscall->last_arg_inserted;

	if (!last)
		return NULL;

	s_syscall_pop_arg(last);

	return last;
}

struct s_arg *
s_syscall_replace_last_arg(struct s_syscall *syscall, struct s_arg *arg)
{
	struct s_arg *last = s_syscall_pop_last_arg(syscall);
	s_arg_insert(syscall, arg, last->arg_num);

	return last;
}

int
s_syscall_cur_arg_advance(struct s_syscall *syscall, enum s_type type,
	unsigned long long *val)
{
	static unsigned long long dummy_val;
	unsigned long long *local_val = val ? val : &dummy_val;

	if (exiting(syscall->tcp) && syscall->next_get_changeable) {
		int chg_arg = syscall->next_get_changeable->arg_num;

		assert(chg_arg >= 0);

		syscall->arg_idx[syscall->next_get_idx] = chg_arg;

		syscall->next_get_changeable =
			list_next(syscall->next_get_changeable, chg_entry);
		if (&syscall->next_get_changeable->chg_entry ==
		    &syscall->changeable_args)
			syscall->next_get_changeable = NULL;
	}

	assert(syscall->next_get_idx < MAX_ARGS);

	if (S_TYPE_SIZE(type) == S_TYPE_SIZE_ll) {
		syscall->arg_idx[syscall->next_get_idx + 1] =
			getllval(syscall->tcp, local_val,
			syscall->arg_idx[syscall->next_get_idx]);
	} else {
		unsigned long long mask = ~0LLU;

		switch (S_TYPE_SIZE(type)) {
		case S_TYPE_SIZE_c:
			mask = 0xFF;
			break;
		case S_TYPE_SIZE_h:
			mask = 0xFFFF;
			break;
		case S_TYPE_SIZE_i:
			mask = 0xFFFFFFFF;
			break;
		case S_TYPE_SIZE_l:
			if (current_wordsize == sizeof(int))
				mask = 0xFFFFFFFF;
			break;
		case S_TYPE_SIZE_ll:
		default:
			break;
		}

		*local_val = syscall->tcp->u_arg[
			syscall->arg_idx[syscall->next_get_idx]] & mask;
		syscall->arg_idx[syscall->next_get_idx + 1] =
			syscall->arg_idx[syscall->next_get_idx] + 1;
	}

	syscall->next_get_idx++;

	return syscall->arg_idx[syscall->next_get_idx];
}

void
s_syscall_set_name_level(struct s_syscall *s, enum s_syscall_show_arg level)
{
	s->name_level = level;
}

void
s_syscall_set_comment_level(struct s_syscall *s, enum s_syscall_show_arg level)
{
	s->comment_level = level;
}

/* Similar to printxvals() */
static int
s_process_xlat_val(struct s_xlat *arg, s_print_xlat_fn cb, void *cb_data,
	uint32_t flags)
{
	uint64_t lookup_val = (arg->scale > 0) ? (arg->val >> arg->scale) :
		arg->val;
	const char *str = arg->x ? xlookup(arg->x, lookup_val) : NULL;

	cb(arg, arg->val, ~0, str ? str : arg->dflt, flags | SPXF_XLAT_TAIL |
		((str || !arg->x) ? 0 : SPXF_DEFAULT),
		cb_data);

	return 1;
}

/* Similar to printflags64() */
static int
s_process_xlat_flags(struct s_xlat *arg, s_print_xlat_fn cb, void *cb_data,
	uint32_t flags)
{
	int n;
	const struct xlat *xlat;
	uint64_t val;
	uint64_t lookup_val;

	if (arg->val == 0 && arg->x && arg->x->val == 0 && arg->x->str) {
		cb(arg, arg->val, 0, arg->x->str, flags | SPXF_XLAT_TAIL,
			cb_data);
		return 1;
	}

	if (!arg->x) {
		cb(arg, arg->val, ~0, arg->dflt,
			flags | SPXF_DEFAULT | SPXF_XLAT_TAIL, cb_data);
		return 1;
	}

	xlat = arg->x;
	val = arg->val;
	lookup_val = (arg->scale > 0) ? (val >> arg->scale) : val;

	for (n = 0; xlat->str; xlat++) {
		if (xlat->val && (lookup_val & xlat->val) == xlat->val) {
			uint64_t cb_val = (arg->scale > 0) ?
				(xlat->val << arg->scale) : xlat->val;

			cb(arg, cb_val, cb_val, xlat->str, (flags |
				(!(val & ~cb_val) ? SPXF_XLAT_TAIL : 0)) &
				~((val & ~cb_val) ? SPXF_LAST : 0),
				cb_data);
			flags &= ~SPXF_FIRST;
			val &= ~cb_val;
			lookup_val &= ~xlat->val;
			n++;
		}
	}

	if (n) {
		if (val) {
			cb(arg, val, ~0, NULL, flags | SPXF_XLAT_TAIL, cb_data);
			n++;
		}
	} else {
		cb(arg, val, ~0, arg->dflt,
			flags | SPXF_DEFAULT | SPXF_XLAT_TAIL, cb_data);
	}

	return n;
}

void
s_process_xlat(struct s_xlat *arg, s_print_xlat_fn cb, void *cb_data)
{
	struct s_xlat *cur = arg;
	bool first = true;

	do {
		if (!cur->empty)
			first = !(cur->flags ? s_process_xlat_flags :
				s_process_xlat_val)(cur, cb, cb_data,
				/* We provide FIRST, LAST and FIRST_XLAT only */
				(first ? SPXF_FIRST : 0) |
				((cur == arg) ? SPXF_FIRST_XLAT : 0) |
				((list_next(cur, entry) == arg) ?
				SPXF_LAST : 0)) && first;
		cur = list_next(cur, entry);;
	} while (cur != arg);
}

void
s_syscall_print_unfinished(struct tcb *tcp)
{
	s_syscall_new(tcp, S_SCT_SYSCALL);
	if (s_printer_cur->print_unfinished)
		s_printer_cur->print_unfinished(tcp);
}

void
s_syscall_print_leader(struct tcb *tcp, struct timeval *tv, struct timeval *dtv)
{
	s_syscall_new(tcp, S_SCT_SYSCALL);
	if (s_printer_cur->print_leader)
		s_printer_cur->print_leader(tcp, tv, dtv);
}

void
s_syscall_print_before(struct tcb *tcp)
{
	s_syscall_new(tcp, S_SCT_SYSCALL);
	if (s_printer_cur->print_before)
		s_printer_cur->print_before(tcp);
}

void
s_syscall_print_entering(struct tcb *tcp)
{
	tcp->s_syscall->next_get_changeable =
		tcp->s_syscall->next_ins_changeable =
		list_head(&tcp->s_syscall->changeable_args, struct s_arg,
			chg_entry);

	if (s_printer_cur->print_entering)
		s_printer_cur->print_entering(tcp);
}

void
s_syscall_init_exiting(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;

	syscall->next_get_changeable = syscall->next_ins_changeable =
		list_head(&syscall->changeable_args, struct s_arg, chg_entry);

	/* For the case when no changeable arguments have been added */
	syscall->arg_idx[0] = syscall->arg_idx[syscall->next_get_idx];
	syscall->next_get_idx = syscall->next_ins_idx = 0;
}

void
s_syscall_print_exiting(struct tcb *tcp)
{
	if (s_printer_cur->print_exiting)
		s_printer_cur->print_exiting(tcp);
}

void
s_syscall_print_after(struct tcb *tcp)
{
	if (s_printer_cur->print_after)
		s_printer_cur->print_after(tcp);

	s_syscall_free(tcp);
	tcp->s_syscall = NULL;
}

void
s_syscall_print_resumed(struct tcb *tcp)
{
	if (s_printer_cur->print_resumed)
		s_printer_cur->print_resumed(tcp);
}

void
s_syscall_print_tv(struct tcb *tcp, struct timeval *tv)
{
	if (s_printer_cur->print_tv)
		s_printer_cur->print_tv(tcp, tv);
}

void
s_syscall_print_unavailable_entering(struct tcb *tcp, int scno_good)
{
	if (s_printer_cur->print_unavailable_entering)
		s_printer_cur->print_unavailable_entering(tcp, scno_good);
}

void
s_syscall_print_unavailable_exiting(struct tcb *tcp)
{
	if (s_printer_cur->print_unavailable_exiting)
		s_printer_cur->print_unavailable_exiting(tcp);
	s_syscall_free(tcp);
}

void
s_syscall_print_signal(struct tcb *tcp, const void *si_void, unsigned sig)
{
	const siginfo_t *si = si_void;
	struct s_syscall *saved_syscall = tcp->s_syscall;

	s_syscall_new(tcp, S_SCT_SIGNAL);

	s_insert_signo("signal", sig);
	if (si) {
		s_insert_struct("siginfo");
		printsiginfo(si);
		s_struct_finish();
	}

	if (s_printer_cur->print_signal)
		s_printer_cur->print_signal(tcp);

	s_syscall_free(tcp);

	tcp->s_syscall = saved_syscall;
}

void
s_print_message(struct tcb *tcp, enum s_msg_type type, const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	if (s_printer_cur->print_message)
		s_printer_cur->print_message(tcp, type, msg, args);
	va_end(args);
}
