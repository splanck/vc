sum:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl (%eax), %ebx
    movl 12(%ebp), %eax
    movl (%eax), %ecx
    movl %ebx, %eax
    addl %ecx, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
