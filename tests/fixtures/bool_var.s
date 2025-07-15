main:
    pushl %ebp
    movl %esp, %ebp
    subl $4, %esp
    movl $1, %eax
    movl %eax, -4(%ebp)
    movl $1, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
