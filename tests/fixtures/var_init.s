main:
    pushl %ebp
    movl %esp, %ebp
    movl $5, %eax
    movl %eax, x
    movl $5, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
