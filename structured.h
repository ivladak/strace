typedef enum {
S_TYPE_char,
S_TYPE_int,
S_TYPE_long,
S_TYPE_longlong,
S_TYPE_unsigned,
S_TYPE_unsigned_long,
S_TYPE_unsigned_longlong,
} s_type_t;

/* syscall */

typedef struct s_arg {
	const char *name; /* if is a field of a struct */
	s_type_t type;
	union {
		void *value_p;
		uint64_t value_int;
	};
	struct s_arg *next;
} s_arg_t;

typedef struct s_syscall {
	struct tcb *tcb;
	s_arg_t *head;
	s_arg_t *tail;
	s_type_t ret_type;
} s_syscall_t;

/* struct */

typedef struct s_struct {
	s_arg_t *head;
	s_arg_t *tail;
} s_struct_t;


/* prototypes */

extern void s_val_print (s_arg_t *arg);
extern void s_push_value_int (s_type_t type, uint64_t value);
extern void s_val_free (s_arg_t *arg);
extern s_arg_t *s_arg_new (s_syscall_t *syscall, s_type_t type);
extern s_syscall_t *s_syscall_new (struct tcb *tcb);
extern void s_syscall_free (s_syscall_t *syscall);
extern void s_syscall_print (s_syscall_t *syscall);

extern s_syscall_t *s_syscall;
