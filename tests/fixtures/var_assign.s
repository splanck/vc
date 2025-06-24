main:
    pushl %ebp
    movl %esp, %ebp
    movl $5, %eax
    movl %eax, x
    movl x, %eax
    movl %eax, %eax
    ret
