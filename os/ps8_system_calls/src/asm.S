#Stanley George
#ECE 460: Computer Operating Systems
#Professor Jeff Hakner
#PS 8: System Calls

# ------------------------------------------------------------------------
#|Syscall # | Param 1	| Param 2 | Param 3 | Param 4 | Param 5 | Param 6 |
# ------------------------------------------------------------------------
#|  eax	    |   ebx	|   ecx	  |   edx   |   esi   |   edi	|  ebp    |
# ------------------------------------------------------------------------
#-------------
#Return value |
#-------------
#    eax      |
#-------------

.text
.globl _start

_start:
	movl	$4, %eax 	#store write syscall # (4) in eax
	movl	$1, %ebx 	#put fd=stdout into ebx
	movl	$msg, %ecx 	#put string into ecx
	movl	$len, %edx 	#put string length into edx
	int	$0x80		#call kernel
	
	movl 	$1, %eax 	#store exit syscall # (1) in eax
	movl 	$0, %ebx 	#put exit code into ebx
	int	$0x80		#call kernel

.data
msg:
	.ascii "Hello, world!\n"
	len =	. - msg
