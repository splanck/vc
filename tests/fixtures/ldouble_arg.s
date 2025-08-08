main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movl %eax, -0(%ebp)
    movl $1, %eax
    sub $10, %esp
    fldt %eax
    fstpt (%esp)
    call sink
    addl $10, %esp
    movl %eax, %eax
    movl $0, %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
