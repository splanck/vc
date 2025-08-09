main:
    pushl %ebp
    movl %esp, %ebp
    subl $4, %esp
    movl $97, %eax
    movb %eax, -4(%ebp)
    movb $97, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
