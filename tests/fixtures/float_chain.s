foo:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movd %eax, %xmm0
    movd %ebx, %xmm1
    addss %xmm1, %xmm0
    movd %xmm0, %ecx
    movl 16(%ebp), %ebx
    movd %ecx, %xmm0
    movd %ebx, %xmm1
    addss %xmm1, %xmm0
    movd %xmm0, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
