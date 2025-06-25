main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movl %eax, b
    movl $1, %eax
    movl %eax, %eax
    ret
