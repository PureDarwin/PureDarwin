/* Hyper-V x64 hypercall. SysV entry is rdi = control, rsi = input GPA, rdx = output GPA, status in rax.
 * Data pages are not executable, so issue the hypercall page's instruction here */
	.text
	.globl	_pd_hv_vmcall
	.p2align 4
_pd_hv_vmcall:
	movq	%rdx, %r8
	movq	%rsi, %rdx
	movq	%rdi, %rcx
	vmcall
	ret

	.globl	_pd_hv_vmmcall
	.p2align 4
_pd_hv_vmmcall:
	movq	%rdx, %r8
	movq	%rsi, %rdx
	movq	%rdi, %rcx
	vmmcall
	ret
