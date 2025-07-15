main:
    pushl %ebp
    movl %esp, %ebp
    subl $20, %esp
    movl $0, %eax
    movl $0, %ebx
    movl %ebx, -20(%ebp)(,%eax,4)
    movl $1, %ebx
    movl $0, %eax
    movl %eax, -20(%ebp)(,%ebx,4)
    movl $2, %eax
    movl $4, %ebx
    movl %ebx, -20(%ebp)(,%eax,4)
    movl $3, %ebx
    movl $0, %eax
    movl %eax, -20(%ebp)(,%ebx,4)
    movl $4, %eax
    movl $9, %ebx
    movl %ebx, -20(%ebp)(,%eax,4)
    movl $2, %ebx
    movl -20(%ebp)(,%ebx,4), %eax
    movl $4, %ebx
    movl -20(%ebp)(,%ebx,4), %ecx
    movl %eax, %ebx
    addl %ecx, %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
