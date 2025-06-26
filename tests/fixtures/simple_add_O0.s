main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movl $2, %ebx
    movl $3, %ecx
    movl %ebx, %edx
    imull %ecx, %edx
    movl %eax, %ecx
    addl %edx, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
