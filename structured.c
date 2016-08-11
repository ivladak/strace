/* Functions and structures to replace strace's traditional
 * output with new structured output subsystem
 */

#include <assert.h>

#include "defs.h"
#include "structured.h"

#include "structured_fmt_text.h"


/** List of printers used. */
static struct s_printer *s_printers[] = {
	&s_printer_text,
	NULL
};


/* syscall representation */

#define S_TYPE_FUNC(S_TYPE_FUNC_NAME, S_TYPE_FUNC_ARG, S_TYPE_FUNC_RET, \
    S_TYPE_SWITCH) \
	S_TYPE_FUNC_RET \
	S_TYPE_FUNC_NAME(S_TYPE_FUNC_ARG arg) \
	{ \
		switch (S_TYPE_KIND(S_TYPE_SWITCH)) { \
		case S_TYPE_KIND_fd: \
		__ARG_TYPE_CASE(num); \
		\
		case S_TYPE_KIND_path: \
		__ARG_TYPE_CASE(str); \
		\
		__ARG_TYPE_CASE(addr); \
		__ARG_TYPE_CASE(xlat); \
		__ARG_TYPE_CASE(struct); \
		\
		case S_TYPE_KIND_changeable_void: \
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

	return arg;
}

struct s_arg *
s_arg_next(struct tcb *tcp, enum s_type type, const char *name)
{
	if (entering(tcp)) {
		struct s_arg *arg = s_arg_new(current_tcp, type, name);

		s_arg_insert(current_tcp->s_syscall, arg);

		return arg;
	} else {
		struct s_syscall *syscall = tcp->s_syscall;
		struct s_arg *arg = s_arg_new(tcp, type, name);

		struct s_arg *chg = syscall->last_changeable;
		syscall->last_changeable = STAILQ_NEXT(chg, chg_entry);

		struct s_changeable *s_ch = S_ARG_TO_TYPE(chg, changeable);
		s_ch->exiting = arg;

		return arg;
	}
}

void
s_arg_insert(struct s_syscall *syscall, struct s_arg *arg)
{
	STAILQ_INSERT_TAIL(s_syscall_insertion_point(syscall), arg, entry);
	if (arg->type == S_TYPE_changeable) {
		STAILQ_INSERT_TAIL(&syscall->changeable_args, arg, chg_entry);
	}
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

		STAILQ_FOREACH_SAFE(arg, &p->args.args, entry, tmp)
			s_arg_free(arg);

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
s_str_new(enum s_type type, const char *name, long addr, long len, bool has_nul)
{
	struct s_str *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, type, name),
		str);
	char *buf = NULL;
	size_t size = max_strlen;
	int ret;

	if (len == -1)
		size = max_strlen;
	if (len >= 0)
		size = len;

	if (!addr)
		goto s_str_new_fail;

	buf = xmalloc(size + 1);

	if (has_nul)
		ret = umovestr(current_tcp, addr, size + 1, buf);
	else
		ret = umoven(current_tcp, addr, size, buf);

	if (ret < 0)
		goto s_str_new_fail;

	res->str = buf;
	res->addr = addr;
	res->len = size;
	res->has_nul = has_nul;

	return res;

s_str_new_fail:
	free(buf);
	s_arg_free(&res->arg);

	return NULL;
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
	uint64_t val, const char *dflt, bool flags)
{
	struct s_xlat *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, type, name),
		xlat);

	res->x = x;
	res->val = val;
	res->dflt = dflt;
	res->flags = flags;

	return res;
}

struct s_struct *
s_struct_new(const char *name)
{
	struct s_struct *res = S_ARG_TO_TYPE(s_arg_new(current_tcp,
		S_TYPE_struct, name), struct);

	STAILQ_INIT(&res->args.args);

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

struct s_num *
s_num_new_and_insert(enum s_type type, const char *name, uint64_t value)
{
	struct s_num *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_num_new(type, name, value))->arg);

	return res;
}

struct s_addr *
s_addr_new_and_insert(const char *name, long addr, struct s_arg *arg)
{
	struct s_addr *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_addr_new(name, addr, arg))->arg);

	return res;
}

struct s_xlat *
s_xlat_new_and_insert(enum s_type type, const char *name, const struct xlat *x,
	uint64_t val, const char *dflt, bool flags)
{
	struct s_xlat *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_xlat_new(type, name, x, val, dflt, flags))->arg);

	return res;
}

extern struct s_changeable *s_changeable_new_and_insert(const char *name,
	struct s_arg *entering, struct s_arg *exiting)
{
	struct s_changeable *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_changeable_new(name, entering, exiting))->arg);

	return res;
}

struct s_xlat *
s_xlat_append(enum s_type type, const char *name, const struct xlat *x,
	uint64_t val, const char *dflt, bool flags)
{
	struct s_arg *last_arg = STAILQ_LAST(&current_tcp->s_syscall->args.args,
		s_arg, entry);
	struct s_xlat *last_xlat;
	struct s_xlat *res;

	if (!last_arg && (last_arg->type == S_TYPE_addr))
		last_arg = S_ARG_TO_TYPE(last_arg, addr)->val;

	if (!last_arg || (S_TYPE_KIND(last_arg->type) != S_TYPE_KIND_xlat))
		return s_xlat_new_and_insert(type, last_arg->name, x, val, dflt,
			flags);

	res = s_xlat_new(type, name, x, val, dflt, flags);

	last_xlat = S_ARG_TO_TYPE(last_arg, xlat);

	if (last_xlat->last)
		last_xlat->last->next = res;
	else
		last_xlat->next = res;

	last_xlat->last = res;

	return res;
}

struct args_queue *
s_struct_enter(struct s_struct *s)
{
	SLIST_INSERT_HEAD(&s->arg.syscall->insertion_stack, &s->args, entry);

	return &s->args.args;
}

struct args_queue *
s_syscall_insertion_point(struct s_syscall *s)
{
	if (SLIST_EMPTY(&s->insertion_stack))
		return &s->args.args;

	return &(SLIST_FIRST(&s->insertion_stack)->args);
}

struct args_queue *
s_syscall_pop(struct s_syscall *s)
{
	SLIST_REMOVE_HEAD(&s->insertion_stack, entry);

	return s_syscall_insertion_point(s);
}

struct args_queue *
s_syscall_pop_all(struct s_syscall *s)
{
	SLIST_INIT(&s->insertion_stack);

	return &s->args.args;
}


struct s_syscall *
s_syscall_new(struct tcb *tcp)
{
	struct s_syscall *syscall = xmalloc(sizeof(*syscall));

	tcp->s_syscall = syscall;

	syscall->tcp = tcp;
	syscall->cur_arg = 0;

	STAILQ_INIT(&syscall->args.args);
	STAILQ_INIT(&syscall->changeable_args);
	SLIST_INIT(&syscall->insertion_stack);

	return syscall;
}

void
s_last_is_changeable(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *last_arg = STAILQ_LAST(&syscall->args.args, s_arg, entry);
	STAILQ_REMOVE(&syscall->args.args, last_arg, s_arg, entry);

	s_changeable_new_and_insert(last_arg->name, last_arg, NULL);
}

void
s_syscall_free(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *arg;
	struct s_arg *tmp;

	STAILQ_FOREACH_SAFE(arg, &syscall->args.args, entry, tmp) {
		s_arg_free(arg);
	}

	free(syscall);
}

int
s_syscall_cur_arg_advance(struct s_syscall *syscall, enum s_type type,
	unsigned long long *val)
{
	if (S_TYPE_SIZE(type) == S_TYPE_SIZE_ll) {
		syscall->cur_arg = getllval(current_tcp, val,
			syscall->cur_arg);
	} else {
		*val = current_tcp->u_arg[syscall->cur_arg];
		syscall->cur_arg++;
	}

	return syscall->cur_arg;
}

/* Similar to printxvals() */
static int
s_process_xlat_val(struct s_xlat *arg, s_print_xlat_fn cb, bool first)
{
	const char *str = arg->x ? xlookup(arg->x, arg->val) : NULL;

	cb(arg->val, ~0, str ? str : arg->dflt,
		(first ? SPXF_FIRST : 0) | ((str || !arg->x) ? 0 : SPXF_DEFAULT));

	return 1;
}

/* Similar to printflags64() */
static int
s_process_xlat_flags(struct s_xlat *arg, s_print_xlat_fn cb, bool first)
{
	int n;
	const struct xlat *xlat;
	uint64_t flags;

	if (arg->val == 0 && arg->x && arg->x->val == 0 && arg->x->str) {
		cb(arg->val, 0, arg->x->str, first ? SPXF_FIRST : 0);
		return 1;
	}

	if (!arg->x) {
		cb(arg->val, ~0, arg->dflt,
			(first ? SPXF_FIRST : 0) | SPXF_DEFAULT);
		return 1;
	}

	xlat = arg->x;
	flags = arg->val;

	for (n = 0; xlat->str; xlat++) {
		if (xlat->val && (flags & xlat->val) == xlat->val) {
			cb(xlat->val, xlat->val, xlat->str,
				first ? SPXF_FIRST : 0);
			first = false;
			flags &= ~xlat->val;
			n++;
		}
	}

	if (n) {
		if (flags) {
			cb(flags, ~0, NULL, 0);
			n++;
		}
	} else {
		cb(flags, ~0, arg->dflt,
			(first ? SPXF_FIRST : 0) | SPXF_DEFAULT);
	}

	return n;
}

void
s_process_xlat(struct s_xlat *arg, s_print_xlat_fn cb)
{
	bool first = true;

	while (arg) {
		first = !(arg->flags ? s_process_xlat_flags :
			s_process_xlat_val)(arg, cb, first) && first;
		arg = arg->next;
	}
}

void
s_syscall_print_before(struct tcb *tcp)
{
	struct s_printer **cur = s_printers;

	//for (; *cur; cur++)
		(*cur)->print_before(tcp);
	s_syscall_new(tcp);
}

void
s_syscall_print_entering(struct tcb *tcp)
{
	struct s_printer **cur = s_printers;

	tcp->s_syscall->last_changeable =
		STAILQ_FIRST(&tcp->s_syscall->changeable_args);

	for (; *cur; cur++)
		(*cur)->print_entering(tcp);
}

void
s_syscall_print_exiting(struct tcb *tcp)
{
	struct s_printer **cur = s_printers;

	for (; *cur; cur++)
		(*cur)->print_exiting(tcp);
}

void
s_syscall_print_after(struct tcb *tcp)
{
	s_syscall_free(tcp);
	struct s_printer **cur = s_printers;

	//for (; *cur; cur++)
		(*cur)->print_after(tcp);
}

void
s_syscall_print_unavailable(struct tcb *tcp)
{
	struct s_printer **cur = s_printers;

	//for (; *cur; cur++)
		(*cur)->print_unavailable(tcp);
	s_syscall_free(tcp);
}
