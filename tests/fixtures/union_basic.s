.data
u:
    .zero 4
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $4, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
