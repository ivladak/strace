static void
arch_sigreturn(struct tcb *tcp)
{
	long esp = ppc_regs.gpr[1];
	struct sigcontext sc;

	/* Skip dummy stack frame. */
#ifdef POWERPC64
	if (current_personality == 0)
		esp += 128;
	else
#endif
		esp += 64;

	s_insert_struct("sigcontext");
	if (umove(tcp, esp, &sc) < 0) {
		s_insert_addr("mask", esp);
	} else {
		unsigned long mask[NSIG / 8 / sizeof(long)];
#ifdef POWERPC64
		mask[0] = sc.oldmask | (sc._unused[3] << 32);
#else
		mask[0] = sc.oldmask;
		mask[1] = sc._unused[3];
#endif
		s_insert_sigmask("mask", mask, sizeof(mask));
	}
	s_struct_finish();
}
