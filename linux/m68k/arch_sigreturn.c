static void
arch_sigreturn(struct tcb *tcp)
{
	long addr;
	long addr2;

	if (upeek(tcp->pid, 4*PT_USP, &addr) < 0)
		return;
	/* Fetch pointer to struct sigcontext.  */
	if (umove(tcp, addr + 2 * sizeof(int), &addr) < 0)
		return;

	unsigned long mask[NSIG / 8 / sizeof(long)];
	/* Fetch first word of signal mask.  */
	if (umove(tcp, addr, &mask[0]) < 0) {
		s_insert_addr("mask", addr);
		return;
	}

	/* Fetch remaining words of signal mask, located immediately before.  */
	addr2 = addr - (sizeof(mask) - sizeof(long));

	s_insert_struct("sigcontext");
	if (umoven(tcp, addr2, sizeof(mask) - sizeof(long), &mask[1]) < 0)
		s_insert_addr("mask", addr);
	else
		s_insert_sigmask("mask", mask, sizeof(mask));
	s_struct_finish();
}
