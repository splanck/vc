main:
    pushl %ebp
    movl %esp, %ebp
    subl $4, %esp
    movl $97, %eax
    movl %eax, %eax
    movb %al, -4(%ebp)
    movl $97, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
