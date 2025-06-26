main:
    pushl %ebp
    movl %esp, %ebp
    movl $42, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
