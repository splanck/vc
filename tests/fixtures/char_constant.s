main:
    pushl %ebp
    movl %esp, %ebp
    movl $65, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
