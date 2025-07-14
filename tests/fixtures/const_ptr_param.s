load:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl (%eax), %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
