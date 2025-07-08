main:
    pushl %ebp
    movl %esp, %ebp
    movl %eax, a
    movl %eax, b
    movl a, %eax
    movl b, %ebx
    movsd %eax, %xmm0
    movsd %ebx, %xmm1
    addsd %xmm1, %xmm0
    movsd %xmm0, %ecx
    movsd %eax, %xmm0
    movsd %ebx, %xmm1
    addsd %xmm1, %xmm0
    movsd %xmm0, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
