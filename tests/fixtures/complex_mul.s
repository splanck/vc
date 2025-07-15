main:
    pushl %ebp
    movl %esp, %ebp
    movl %eax, -0(%ebp)
    movl %eax, -0(%ebp)
    movl -0(%ebp), %eax
    movl -0(%ebp), %ebx
    movsd %eax, %xmm0
    movsd %eax, %xmm1
    movsd %ebx, %xmm2
    movsd %ebx, %xmm3
    mulsd %xmm2, %xmm0
    mulsd %xmm3, %xmm1
    subsd %xmm1, %xmm0
    movsd %xmm0, %ecx
    movsd %eax, %xmm0
    mulsd %xmm3, %xmm0
    movsd %eax, %xmm1
    mulsd %xmm2, %xmm1
    addsd %xmm1, %xmm0
    movsd %xmm0, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
