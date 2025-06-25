main:
    pushl %ebp
    movl %esp, %ebp
    movl $5, %eax
    movl %eax, x
    movl x, %eax
    movl %eax, y
    movl y, %eax
    movl %eax, %eax
    ret
