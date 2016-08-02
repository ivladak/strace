/* Functions and structures to replace strace's traditional
 * output with new structured output subsystem
 */

#include "defs.h"

/* printing and freeing values (args and struct fields) */

void
s_val_print (s_arg_t *arg)
{
	switch (arg->type) {
#define S_MACRO(INT, ENUM, PR) \
case S_TYPE_ ## ENUM : tprintf("%" PR, (INT) arg->value_int); break;
	S_MACRO(char, char, "c")
	S_MACRO(int, int, "d")
	S_MACRO(long, long, "ld")
	S_MACRO(long long, longlong, "lld")
	S_MACRO(unsigned, unsigned, "u")
	S_MACRO(unsigned long, unsigned_long, "lu")
	S_MACRO(unsigned long long, unsigned_longlong, "llu")
#undef S_MACRO

	case S_TYPE_addr:
		printaddr((long) arg->value_int);
		break;
	default:
		tprints(".., ");
		break;
	}
}

void
s_val_free (s_arg_t *arg)
{
	switch (arg->type) {
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
