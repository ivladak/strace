typedef enum {
S_TYPE_d,
S_TYPE_ld,
S_TYPE_lld,
S_TYPE_u,
S_TYPE_lu,
S_TYPE_llu,

S_TYPE_o,
S_TYPE_lo,
S_TYPE_llo,

S_TYPE_addr,
S_TYPE_path,
S_TYPE_flags,
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


/* complex arguments */

typedef struct s_flags {
	const struct xlat *x;
	uint64_t flags;
	const char *dflt;
} s_flags_t;

/* prototypes */

extern void s_val_print (s_arg_t *arg);
extern void s_push_value_int (s_type_t type, uint64_t value);
extern void s_val_free (s_arg_t *arg);
extern s_arg_t *s_arg_new (s_syscall_t *syscall, s_type_t type);
extern s_syscall_t *s_syscall_new (struct tcb *tcb);
extern void s_syscall_free (s_syscall_t *syscall);
extern void s_syscall_print (s_syscall_t *syscall);

extern s_syscall_t *s_syscall;