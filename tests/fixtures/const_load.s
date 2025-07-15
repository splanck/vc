main:
    pushl %ebp
    movl %esp, %ebp
    subl $8, %esp
    movl $5, %eax
    movl %eax, -4(%ebp)
    movl $5, %eax
    movl %eax, -8(%ebp)
    movl $5, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
