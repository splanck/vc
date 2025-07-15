main:
    pushl %ebp
    movl %esp, %ebp
    movl %eax, -0(%ebp)
    movl %eax, -0(%ebp)
    movl -0(%ebp), %eax
    movl -0(%ebp), %ebx
    movsd %ebx, %xmm2
    movsd %ebx, %xmm3
    movsd %xmm2, %xmm4
    mulsd %xmm2, %xmm2
    mulsd %xmm3, %xmm3
    addsd %xmm3, %xmm2
    movsd %eax, %xmm0
    mulsd %xmm4, %xmm0
    movsd %eax, %xmm1
    movsd %ebx, %xmm3
    mulsd %xmm3, %xmm1
    addsd %xmm1, %xmm0
    divsd %xmm2, %xmm0
    movsd %xmm0, %ecx
    movsd %eax, %xmm0
    mulsd %xmm4, %xmm0
    movsd %eax, %xmm1
    movsd %ebx, %xmm3
    mulsd %xmm3, %xmm1
    subsd %xmm1, %xmm0
    divsd %xmm2, %xmm0
    movsd %xmm0, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
