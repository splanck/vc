main:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl $1, %ebx
    movl %ebx, nums(,%eax,4)
    movl $1, %ebx
    movl $2, %eax
    movl %eax, nums(,%ebx,4)
    movl $2, %eax
    movl $3, %ebx
    movl %ebx, nums(,%eax,4)
    movl $1, %ebx
    movl nums(,%ebx,4), %eax
    movl %eax, %eax
    ret
