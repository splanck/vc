main:
    pushl %ebp
    movl %esp, %ebp
    subl $12, %esp
    movl $0, %eax
    movl $1, %ebx
    movl %ebx, -12(%ebp)(,%eax,4)
    movl $1, %ebx
    movl $2, %eax
    movl %eax, -12(%ebp)(,%ebx,4)
    movl $2, %eax
    movl $3, %ebx
    movl %ebx, -12(%ebp)(,%eax,4)
    movl $1, %ebx
    movl -12(%ebp)(,%ebx,4), %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
