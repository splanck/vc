id:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl %eax, %eax
    ret
main:
    pushl %ebp
    movl %esp, %ebp
    call id
    movl %eax, %eax
    movl %eax, %eax
    ret
