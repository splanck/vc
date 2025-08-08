main:
    pushl %ebp
    movl %esp, %ebp
    subl $16, %esp
    movl $1, %eax
    movl %eax, -4(%ebp)
    movl $1, %eax
    movl -8(%ebp), %ebx
    movl %eax, %ecx
    movl %ebx, %ecx
    sall %cl, %ecx
    movl %ecx, -12(%ebp)
    movl $1, %ecx
    movl -8(%ebp), %ebx
    movl %ecx, %eax
    movl %ebx, %ecx
    sarl %cl, %eax
    movl %eax, -16(%ebp)
    movl -12(%ebp), %eax
    movl -16(%ebp), %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
