main:
    pushl %ebp
    movl %esp, %ebp
    subl $8, %esp
    movl $0, %eax
    movl $1, %ebx
    movl %ebx, -8(%ebp)(,%eax,4)
    movl $1, %ebx
    movl $2, %eax
    movl %eax, -8(%ebp)(,%ebx,4)
    movl $0, %eax
    movl -8(%ebp)(,%eax,4), %ebx
    movl $1, %eax
    movl -8(%ebp)(,%eax,4), %ecx
    movl %ebx, %eax
    addl %ecx, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
