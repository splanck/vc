main:
    pushl %ebp
    movl %esp, %ebp
    movl $5000000000, %eax
    movl %eax, a
    movl $7, %eax
    movl %eax, b
    movl $705032711, %eax
    movl %eax, r
    movl r, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
