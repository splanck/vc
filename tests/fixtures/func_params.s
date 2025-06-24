id:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl %eax, %eax
    ret
main:
    pushl %ebp
    movl %esp, %ebp
    movl $5, %eax
    pushl %eax
    call id
    addl $4, %esp
    movl %eax, %eax
    movl %eax, %eax
    ret
