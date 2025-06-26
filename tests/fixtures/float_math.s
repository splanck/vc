main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movl %eax, a
    movl $2, %eax
    movl %eax, b
    movl $3, %eax
    movl %eax, %eax
    ret
