main:
    pushl %ebp
    movl %esp, %ebp
    movl $5000000000, %eax
    movl %eax, a
    movl $705032709, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
