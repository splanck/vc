multi:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl %eax, %ecx
    imull %ebx, %ecx
    movl 16(%ebp), %ebx
    movl %ecx, %eax
    addl %ebx, %eax
    movl 8(%ebp), %ebx
    movl %eax, %ecx
    subl %ebx, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
main:
    pushl %ebp
    movl %esp, %ebp
    movl $10, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
