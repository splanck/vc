add:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
main:
    pushl %ebp
    movl %esp, %ebp
    movl $5, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
