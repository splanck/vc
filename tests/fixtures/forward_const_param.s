main:
    pushl %ebp
    movl %esp, %ebp
    movl $5, %eax
    pushl %eax
    call inc
    addl $4, %esp
    movl %eax, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
inc:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl $1, %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
