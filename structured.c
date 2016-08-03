/* Functions and structures to replace strace's traditional
 * output with new structured output subsystem
 */

#include "defs.h"
#include "structured.h"

/* printing and freeing values (args and struct fields) */

void
s_val_print(s_arg_t *arg)
{
	switch (arg->type) {
#define PRINT_INT(TYPE, ENUM, PR) \
	case S_TYPE_ ## ENUM : tprintf("%" PR, (TYPE) arg->value_int); break

	PRINT_INT(int, d, "d");
	PRINT_INT(long, ld, "ld");
	PRINT_INT(long long, lld, "lld");

	PRINT_INT(unsigned, u, "u");
	PRINT_INT(unsigned long, lu, "lu");
	PRINT_INT(unsigned long long, llu, "llu");

#undef PRINT_INT

#define PRINT_ALT_INT(TYPE, ENUM, PR) \
	case S_TYPE_ ## ENUM : tprintf("%#" PR, (TYPE) arg->value_int); break

	PRINT_ALT_INT(unsigned, x, "x");
	PRINT_ALT_INT(unsigned long, lx, "lx");
	PRINT_ALT_INT(unsigned long long, llx, "llx");

	PRINT_ALT_INT(int, o, "o");
	PRINT_ALT_INT(long, lo, "lo");
	PRINT_ALT_INT(long long, llo, "llo");

#undef PRINT_ALT_INT

	case S_TYPE_str: ;
		s_str_t *s_p = arg->value_p;
		if (!s_p->str) {
			if (s_p->addr) {
				printaddr(s_p->addr);
			} else {
				tprints("NULL");
			}
		} else {
			tprints(s_p->str);
		}
		break;
	case S_TYPE_addr:
		printaddr((long) arg->value_int);
		break;
	case S_TYPE_path:
		printpathcur((long) arg->value_int);
		break;
	case S_TYPE_flags: ;
		s_flags_t *f_p = arg->value_p;
		printflags64(f_p->x, f_p->flags, f_p->dflt);
		break;

	default:
		tprints("[!!! unknown value type]");
		break;
	}
}

void
s_val_free(s_arg_t *arg)
{
	switch (arg->type) {
	case S_TYPE_flags:
		free(arg->value_p);
		break;
	case S_TYPE_str: ;
		s_str_t *s_p = arg->value_p;
		if (s_p->str)
			free(s_p->str);
		free(s_p);
		break;
	default:
		break;
	}
}

/* syscall representation */

s_arg_t *
s_arg_new(struct tcb *tcp, s_type_t type)
{
	s_syscall_t *syscall = tcp->s_syscall;
	s_arg_t *arg = malloc(sizeof(s_arg_t));

	arg->type = type;

	STAILQ_INSERT_TAIL(&syscall->args, arg, entry);

	return arg;
}

s_syscall_t *
s_syscall_new(struct tcb *tcp)
{
	s_syscall_t *syscall = malloc(sizeof(s_syscall_t));

	syscall->tcp = tcp;
	tcp->s_syscall = syscall;

	STAILQ_INIT(&syscall->args);

	return syscall;
}

void
s_syscall_free(struct tcb *tcp)
{
	s_syscall_t *syscall = tcp->s_syscall;
	struct s_arg *arg;
	struct s_arg *tmp;

	STAILQ_FOREACH_SAFE(arg, &syscall->args, entry, tmp) {
		s_val_free(arg);
		free(arg);
	}

	free(syscall);
}

void
s_syscall_print(struct tcb *tcp)
{
	s_syscall_t *syscall = tcp->s_syscall;
	struct s_arg *arg;
	struct s_arg *tmp;

	STAILQ_FOREACH_SAFE(arg, &syscall->args, entry, tmp) {
		s_val_print(arg);

		if (tmp)
			tprints(", ");
	}
}

/* struct representation */

/*
void
s_field_push(s_struct_t *s_struct, const char *name,
	s_type_t type, void *value)
{
	s_arg_t *field = malloc(sizeof(s_arg_t));
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
	s_arg_t *dummy = malloc(sizeof(s_arg_t));
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
	s_arg_t *next = s_struct->head;
	s_arg_t *head;
	while (next) {
		head = next;
		next = head->next;
		s_val_free(head);
		free(head);
	}
	free(s_struct);
}

void
s_struct_print (s_struct_t *s_struct)
{
	s_arg_t *head = s_struct->head;
	s_arg_t *next = head->next;
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
