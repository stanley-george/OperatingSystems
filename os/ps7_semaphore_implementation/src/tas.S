	.text
	.globl tas
	.type	tas,@function
tas:
	pushl	%ebp									#save ebp value
	movl	%esp, %ebp						#start stack frame for this function
	movl	8(%ebp), %edx					#access function arg and save to edx
	movl	$1, %eax							#move 1 into eax
#APP
	lock;xchgb	%al,(%edx)
#NO_APP
	movsbl	%al,%eax
	popl	%ebp
	ret
.Lfe1:
	.size	tas,.Lfe1-tas
