static void
arch_sigreturn(struct tcb *tcp)
{
	long regs[PT_MAX + 1];

	if (ptrace(PTRACE_GETREGS, tcp->pid, NULL, (long)regs) < 0) {
		perror_msg("sigreturn: PTRACE_GETREGS");
		return;
	}
	const long addr = regs[PT_USP] + offsetof(struct sigcontext, oldmask);

	s_insert_sigcontext(addr);
}
