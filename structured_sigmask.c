#include "defs.h"

#include "structured_sigmask.h"

struct s_sigmask *
s_sigmask_new(const char *name, const void *sig_mask, unsigned int bytes)
{
	struct s_sigmask *res = S_ARG_TO_TYPE(s_arg_new(current_tcp,
		S_TYPE_sigmask, name), sigmask);

	res->bytes = bytes;
	memcpy(&res->sigmask, sig_mask, bytes);

	return res;
}

struct s_sigmask *
s_sigmask_new_and_insert(const char *name, const void *sig_mask,
	unsigned int bytes)
{
	struct s_sigmask *res;

	s_arg_insert(current_tcp->s_syscall,
		&(res = s_sigmask_new(name, sig_mask, bytes))->arg, -1);

	return res;
}

void
s_insert_sigmask(const char *name, const void *sig_mask, unsigned int bytes)
{
	s_sigmask_new_and_insert(name, sig_mask, bytes);
}

void
s_insert_sigmask_addr(const char *name, unsigned long addr, unsigned int bytes)
{
	uint32_t sigmask[NSIG / 32];
	struct s_arg *arg = NULL;

	if (bytes > sizeof(sigmask))
		bytes = sizeof(sigmask);

	if (!s_umoven_verbose(current_tcp, addr, bytes, sigmask))
		arg = S_TYPE_TO_ARG(s_sigmask_new(name, sigmask, bytes));

	s_insert_addr_arg(name, addr, arg);
}

/* Equivalent to s_push_sigmask_addr */
void
s_push_sigmask(const char *name, unsigned int bytes)
{
	unsigned long long addr;

	s_syscall_pop_all(current_tcp->s_syscall);
	s_syscall_cur_arg_advance(current_tcp->s_syscall, S_TYPE_addr, &addr);

	s_insert_sigmask_addr(name, addr, bytes);
}


/** Helper for processing sigmask during output, similar to s_process_xlat */
void
s_process_sigmask(struct s_sigmask *arg, s_print_sigmask_fn cb, void *cb_data)
{
	const unsigned endian = 1;
	int little_endian = * (char *) (void *) &endian;

	static const size_t MAX_SIG_NAME_SIZE = 16;
	/* Buffer for storing signal name */
	char buf[MAX_SIG_NAME_SIZE];
	unsigned pos_xor_mask = little_endian ? 0 : current_wordsize - 1;
	unsigned i;
	size_t cur_byte;
	const char *str;

	for (i = 1; i < arg->bytes * 8; i++) {
		cur_byte = (i >> 3) ^ pos_xor_mask;

		if (i < nsignals) {
			str = signalent[i] + 3;
		}
#ifdef ASM_SIGRTMAX
		else if (i >= ASM_SIGRTMIN && i <= ASM_SIGRTMAX) {
			snprintf(buf, sizeof(buf), "RT_%u", i - ASM_SIGRTMIN);
			str = buf;
		}
#endif
		else {
			snprintf(buf, sizeof(buf), "%u", i);
			str = buf;
		}

		cb(i, str, !!((arg->sigmask[cur_byte] >> ((i - 1) & 7)) & 1),
			cb_data);
	}
}
