foo:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movd %eax, %xmm0
    movd %ebx, %xmm1
    addss %xmm0, %xmm1
    movd %xmm1, %ecx
    movl 16(%ebp), %ebx
    movd %ecx, %xmm0
    movd %ebx, %xmm1
    addss %xmm0, %xmm1
    movd %xmm1, %eax
    movd %eax, %xmm0
    ret
    movl %ebp, %esp
    popl %ebp
    ret
