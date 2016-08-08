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
		case S_TYPE_KIND_addr: \
		case S_TYPE_KIND_fd: \
		case S_TYPE_KIND_path: \
		__ARG_TYPE_CASE(num); \
		\
		__ARG_TYPE_CASE(str); \
		__ARG_TYPE_CASE(xlat); \
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
s_arg_new(struct tcb *tcp, enum s_type type)
{
	struct s_syscall *syscall = tcp->s_syscall;
	void *p = calloc(1, s_type_size(type));
	struct s_arg *arg = s_type_to_arg(p, type);

	arg->syscall = syscall;
	arg->type = type;

	return arg;
}

void
s_arg_push(struct s_syscall *syscall, struct s_arg *arg)
{
	STAILQ_INSERT_TAIL(&syscall->args, arg, entry);
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

struct s_arg *
s_arg_next(struct tcb *tcp, enum s_type type)
{
	if (entering(tcp)) {
		struct s_arg *arg = s_arg_new(current_tcp, type);

		s_arg_push(current_tcp->s_syscall, arg);

		return arg;
	} else {
		struct s_syscall *syscall = tcp->s_syscall;
		struct s_arg *arg = s_arg_new(tcp, type);

		struct s_arg *chg = syscall->last_changeable;
		syscall->last_changeable = STAILQ_NEXT(chg, chg_entry);

		struct s_changeable *s_ch = S_ARG_TO_TYPE(chg, changeable);
		s_ch->exiting = arg;

		return arg;
	}
}


struct s_num *
s_num_new(enum s_type type, uint64_t value)
{
	struct s_num *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, type), num);

	res->val = value;

	return res;
}

struct s_str *
s_str_new(long addr, long len)
{
	struct s_str *res = S_ARG_TO_TYPE(s_arg_new(current_tcp, S_TYPE_str),
		str);

	res->addr = addr;
	res->str = NULL;

	if (!addr)
		return res;

	char *outstr;
	outstr = alloc_outstr();

	if (!fetchstr(current_tcp, addr, len, outstr)) {
		free(outstr);
		outstr = NULL;
	} else {
		res->str = outstr;
	}

	return res;
}

struct s_xlat *
s_xlat_new(const struct xlat *x, uint64_t val, const char *dflt, bool flags)
{
	struct s_xlat *res = S_ARG_TO_TYPE(s_arg_new(current_tcp,
		S_TYPE_xlat), xlat);

	res->x = x;
	res->val = val;
	res->dflt = dflt;
	res->flags = flags;

	return res;
}

struct s_changeable *
s_changeable_new(struct s_arg *entering, struct s_arg *exiting)
{
	struct s_changeable *res = S_ARG_TO_TYPE(s_arg_new(current_tcp,
		S_TYPE_changeable), changeable);

	res->entering = entering;
	res->exiting = exiting;

	return res;
}

struct s_num *
s_num_new_and_push(enum s_type type, uint64_t value)
{
	struct s_num *res;

	s_arg_push(current_tcp->s_syscall,
		&(res = s_num_new(type, value))->arg);

	return res;
}

struct s_str *
s_str_new_and_push(long addr, long len)
{
	struct s_str *res;

	s_arg_push(current_tcp->s_syscall,
		&(res = s_str_new(addr, len))->arg);

	return res;
}

struct s_xlat *
s_xlat_new_and_push(const struct xlat *x, uint64_t val, const char *dflt,
	bool flags)
{
	struct s_xlat *res;

	s_arg_push(current_tcp->s_syscall,
		&(res = s_xlat_new(x, val, dflt, flags))->arg);

	return res;
}

extern struct s_changeable *s_changeable_new_and_push(struct s_arg *entering,
	struct s_arg *exiting)
{
	struct s_changeable *res;

	s_arg_push(current_tcp->s_syscall,
		&(res = s_changeable_new(entering, exiting))->arg);

	return res;
}

struct s_xlat *
s_xlat_append(const struct xlat *x, uint64_t val, const char *dflt,
	bool flags)
{
	struct s_arg *last_arg = STAILQ_LAST(&current_tcp->s_syscall->args,
		s_arg, entry);
	struct s_xlat *last_xlat;
	struct s_xlat *res;

	if (!last_arg || (S_TYPE_KIND(last_arg->type) != S_TYPE_KIND_xlat))
		return s_xlat_new_and_push(x, val, dflt, flags);

	res = s_xlat_new(x, val, dflt, flags);

	last_xlat = S_ARG_TO_TYPE(last_arg, xlat);

	if (last_xlat->last)
		last_xlat->last->next = res;
	else
		last_xlat->next = res;

	last_xlat->last = res;
}



struct s_syscall *
s_syscall_new(struct tcb *tcp)
{
	struct s_syscall *syscall = malloc(sizeof(*syscall));

	tcp->s_syscall = syscall;

	syscall->tcp = tcp;
	syscall->cur_arg = 0;

	STAILQ_INIT(&syscall->args);
	STAILQ_INIT(&syscall->changeable_args);

	return syscall;
}

void
s_last_is_changeable(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *last_arg = STAILQ_LAST(&syscall->args, s_arg, entry);
	STAILQ_REMOVE(&syscall->args, last_arg, s_arg, entry);

	s_changeable_new_and_push(last_arg, NULL);
}

void
s_syscall_free(struct tcb *tcp)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *arg;
	struct s_arg *tmp;

	STAILQ_FOREACH_SAFE(arg, &syscall->args, entry, tmp) {
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

void
s_syscall_print_entering(struct tcb *tcp)
{
	struct s_printer **cur = s_printers;

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

/* struct representation */

/*
void
s_field_push(s_struct_t *s_struct, const char *name,
	enum s_type type, void *value)
{
	struct s_arg *field = malloc(sizeof(struct s_arg));
	field->name = name;
	field->type = type;
	s_val_push(field, type, value);
	field->next = NULL;
	s_struct->tail->next = field;
	s_struct->tail = field;
	return;
}

s_struct_t *
s_struct_new (void)
{
	s_struct_t *s_struct = malloc(sizeof(s_struct_t));
	struct s_arg *dummy = malloc(sizeof(struct s_arg));
	dummy->value_int = 0;
	dummy->name = NULL;
	dummy->next = NULL;
	s_struct->head = dummy;
	s_struct->tail = dummy;
	return s_struct;
}

void
s_struct_free (s_struct_t *s_struct)
{
	struct s_arg *next = s_struct->head;
	struct s_arg *head;
	while (next) {
		head = next;
		next = head->next;
		s_arg_free(head);
	}
	free(s_struct);
}

void
s_struct_print (s_struct_t *s_struct)
{
	struct s_arg *head = s_struct->head;
	struct s_arg *next = head->next;
	tprints("{");
	while (next) {
		head = next;
		next = head->next;
		tprintf("%s=", head->name);
		s_val_print(head);
		if (next)
			tprints(", ");
	}
	tprints("}");
}
*/
