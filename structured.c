/* Functions and structures to replace strace's traditional
 * output with new structured output subsystem
 */

#include "defs.h"
#include "structured.h"

#include "structured_fmt_text.h"

/** List of printers used. */
static struct s_printer *s_printers[] = {
	&s_printer_text,
	NULL
};


/* syscall representation */

struct s_arg *
s_arg_new(struct tcb *tcp, enum s_type type)
{
	struct s_syscall *syscall = tcp->s_syscall;
	struct s_arg *arg = malloc(sizeof(*arg));

	arg->syscall = syscall;
	arg->type = type;

	STAILQ_INSERT_TAIL(&syscall->args, arg, entry);
	if (type == S_TYPE_changeable) {
		STAILQ_INSERT_TAIL(&syscall->changeable_args, arg, chg_entry);
	}

	return arg;
}

void
s_arg_free(struct s_arg *arg)
{
	switch (arg->type) {
	case S_TYPE_flags:
		free(arg->value_p);
		break;
	case S_TYPE_str: {
		struct s_str *s_p = arg->value_p;

		if (s_p->str)
			free(s_p->str);

		free(s_p);
		break;
	}
	default:
		break;
	}

	free(arg);
}

struct s_arg *
s_arg_next(struct tcb *tcp, enum s_type type)
{
	if (entering(tcp)) {
		return s_arg_new(current_tcp, type);
	} else {
		struct s_syscall *syscall = tcp->s_syscall;
		struct s_arg *arg = malloc(sizeof(*arg));

		arg->syscall = syscall;
		arg->type = type;

		struct s_arg *chg = syscall->last_changeable;
		syscall->last_changeable = STAILQ_NEXT(chg, chg_entry);
		struct s_changeable *s_ch = chg->value_p;
		s_ch->exiting = arg;

		return arg;
	}
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

	struct s_arg *chg = s_arg_new(tcp, S_TYPE_changeable);
	struct s_changeable *p = malloc(sizeof(*p));
	chg->value_p = p;
	p->entering = last_arg;
	p->exiting = NULL;
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
