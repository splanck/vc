main:
    pushl %ebp
    movl %esp, %ebp
    movl $4, %eax
    pushl %eax
    call vcadd
    addl $4, %esp
    movl %eax, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
vcadd:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl $2, %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
