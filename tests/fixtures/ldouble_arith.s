main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movl %eax, -0(%ebp)
    movl $2, %eax
    movl %eax, -0(%ebp)
    movl $2, %eax
    movl $2, %ebx
    fldt %eax
    fldt %ebx
    faddp
    fstpt %ecx
    fldt %ecx
    ret
    movl %ebp, %esp
    popl %ebp
    ret
