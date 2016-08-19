static void
arch_sigreturn(struct tcb *tcp)
{
	long addr;

	if (upeek(tcp->pid, REG_FP, &addr) < 0)
		return;
	addr += offsetof(struct sigcontext, sc_mask);

	s_insert_sigcontext(addr);
}
