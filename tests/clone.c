#include "tests.h"

#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <asm/unistd.h>
#include <unistd.h>

#define STACK_SIZE 1024 * 1024

int
do_clone(unsigned long flags, void *child_stack, int *ptid, int *ctid,
	void *tls)
{
	long rc;
#if defined(ARC) || defined(ARM) || defined(ARM64) || defined(MIPS) || \
	defined(HPPA) || defined(POWERPC) || defined(POWERPC64) || \
	defined(I386) || defined(XTENSA)
	/* CLONE_BACKWARDS */
	rc = syscall(__NR_clone, flags, child_stack, ptid, tls, ctid);
#elif defined(S390) || defined(S390X)
	/* CLONE_BACKWARDS2 */
	rc = syscall(__NR_clone, child_stack, flags, ptid, ctid, tls);
#elif defined(MICROBLAZE)
	/* CLONE_BACKWARDS3 */
	rc = syscall(__NR_clone, flags, child_stack, STACK_SIZE, ptid, ctid, tls);
#else
	/* normal args order */
	rc = syscall(__NR_clone, flags, child_stack, ptid, ctid, tls);
#endif

	if (!rc)
		_exit(0);

	return (int) rc;
}

void
fn (void *parm)
{
	_exit(0);
}

int
main(void)
{

	const struct sigaction sa = {
		.sa_handler = SIG_IGN
	};
	sigaction(SIGCHLD, &sa, NULL);

	void **stack_top;
	void *stack = malloc(STACK_SIZE);
	if (!stack)
		perror_msg_and_fail("malloc(%d)", STACK_SIZE);
#ifdef HPPA
	stack_top = stack;
	*++stack_top = fn;
#else
	stack_top = stack + STACK_SIZE;
	*--stack_top = fn;
#endif

	int wstatus, pid;
	pid = do_clone(0, NULL, NULL, NULL, NULL);
	wait(&wstatus);
	printf("clone(child_stack=NULL, flags=) = %d\n", pid);

	pid = do_clone(SIGCHLD, NULL, NULL, NULL, NULL);
	wait(&wstatus);
	printf("clone(child_stack=NULL, flags=SIGCHLD) = %d\n", pid);

	pid = do_clone(CLONE_VM, stack_top, NULL, NULL, NULL);
	wait(&wstatus);
	printf("clone(child_stack=%#lx, flags=CLONE_VM) = %d\n",
		(unsigned long) stack_top, pid);

	pid = do_clone(CLONE_VM | SIGCHLD,
		stack_top, NULL, NULL, NULL);
	wait(&wstatus);
	printf("clone(child_stack=%#lx, flags=CLONE_VM|SIGCHLD) = %d\n",
		(unsigned long) stack_top, pid);

	pid = do_clone(CLONE_FS | CLONE_NEWNS, NULL, NULL, NULL, NULL);
	wait(&wstatus);
	puts("clone(child_stack=NULL, flags=CLONE_FS|CLONE_NEWNS) = -1 EINVAL "
		"(Invalid argument)\n+++ exited with 0 +++");

	free(stack);

	return 0;
}
