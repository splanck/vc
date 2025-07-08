main:
    pushl %ebp
    movl %esp, %ebp
    movl $2, %eax
    movl %eax, f
    movl $2, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
