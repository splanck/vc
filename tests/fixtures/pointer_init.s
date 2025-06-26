main:
    pushl %ebp
    movl %esp, %ebp
    movl $x, %eax
    movl %eax, p
    movl $42, %eax
    movl %eax, x
    movl p, %eax
    movl (%eax), %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
