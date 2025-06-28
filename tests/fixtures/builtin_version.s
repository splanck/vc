foo:
    pushl %ebp
    movl %esp, %ebp
    movl $199901, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
