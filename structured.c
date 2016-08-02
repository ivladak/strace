/* Functions and structures to replace strace's traditional
 * output with new structured output subsystem
 */

#include "defs.h"

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

	PRINT_ALT_INT(int, o, "o");
	PRINT_ALT_INT(long, lo, "lo");
	PRINT_ALT_INT(long long, llo, "llo");

#undef PRINT_ALT_INT

	case S_TYPE_addr:
		printaddr((long) arg->value_int);
		break;
	case S_TYPE_path:
		printpathcur((long) arg->value_int);
		break;
	case S_TYPE_flags: ;
		s_flags_t *p = arg->value_p;
		printflags64(p->x, p->flags, p->dflt);
		break;

	default:
		tprints(">:[ , ");
		break;
	}
}

void
s_val_free (s_arg_t *arg)
{
	switch (arg->type) {
	case S_TYPE_flags:
		free(arg->value_p);
	default:
		break;
	}
}

/* syscall representation */

s_arg_t *
s_arg_new (s_syscall_t *syscall, s_type_t type)
{
	s_arg_t *arg = malloc(sizeof(s_arg_t));
	arg->type = type;
	arg->next = NULL;
	syscall->tail->next = arg;
	syscall->tail = arg;
	return arg;
}

s_syscall_t *
s_syscall_new (struct tcb *tcb)
{
	s_syscall_t *syscall = malloc(sizeof(s_syscall_t));
	syscall->tcb = tcb;
	s_arg_t *dummy = malloc(sizeof(s_arg_t));
	dummy->value_int = 0;
	dummy->name = NULL;
	dummy->next = NULL;
	syscall->head = dummy;
	syscall->tail = dummy;
	return syscall;
}

void
s_syscall_free (s_syscall_t *syscall)
{
	s_arg_t *next = syscall->head;
	s_arg_t *head;
	while (next) {
		head = next;
		next = head->next;
		s_val_free(head);
		free(head);
	}
	free(syscall);
}

void
s_syscall_print (s_syscall_t *syscall)
{
	s_arg_t *head = syscall->head;
	s_arg_t *next = head->next;
	while (next) {
		head = next;
		next = head->next;
		s_val_print(head);
		if (next)
			tprints(", ");
	}
}

/* struct representation */

/*
void
s_field_push (s_struct_t *s_struct, const char *name,
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
