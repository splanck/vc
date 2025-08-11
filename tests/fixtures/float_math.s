main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movl %eax, -0(%ebp)
    movl $2, %eax
    movl %eax, -0(%ebp)
    movl $4, %eax
    movd %eax, %xmm0
    ret
    movl %ebp, %esp
    popl %ebp
    ret
