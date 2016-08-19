static void
arch_sigreturn(struct tcb *tcp)
{
	const long addr = *s390_frame_ptr + __SIGNAL_FRAMESIZE;

	s_insert_sigcontext(addr);
}
