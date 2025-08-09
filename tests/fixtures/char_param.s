id:
    pushl %ebp
    movl %esp, %ebp
    movb 8(%ebp), %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
main:
    pushl %ebp
    movl %esp, %ebp
    movl $90, %eax
    pushl %eax
    call id
    addl $4, %esp
    movl %eax, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
