main:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl $0, %ebx
    movl %ebx, nums(,%eax,4)
    movl $1, %ebx
    movl $0, %eax
    movl %eax, nums(,%ebx,4)
    movl $2, %eax
    movl $4, %ebx
    movl %ebx, nums(,%eax,4)
    movl $3, %ebx
    movl $0, %eax
    movl %eax, nums(,%ebx,4)
    movl $4, %eax
    movl $9, %ebx
    movl %ebx, nums(,%eax,4)
    movl $2, %ebx
    movl nums(,%ebx,4), %eax
    movl $4, %ebx
    movl nums(,%ebx,4), %ecx
    movl %eax, %ebx
    addl %ecx, %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
