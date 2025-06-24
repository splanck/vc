main:
    pushl %ebp
    movl %esp, %ebp
    movl $5, %eax
    movl %eax, x
    movl x, %ebx
    movl %ebx, %eax
    ret
