main:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl $1, %ebx
    movl %ebx, arr(,%eax,4)
    movl $1, %ebx
    movl $2, %eax
    movl %eax, arr(,%ebx,4)
    movl $2, %eax
    movl $3, %ebx
    movl %ebx, arr(,%eax,4)
    movl $2, %ebx
    movl %ebx, i
    movl $arr, %ebx
    movl $2, %eax
    movl %eax, %ecx
    imull $4, %ecx
    addl %ebx, %ecx
    movl %ecx, p
    movl p, %ecx
    movl $-2, %eax
    movl %eax, %ebx
    imull $4, %ebx
    addl %ecx, %ebx
    movl %ebx, p
    movl p, %ebx
    movl (%ebx), %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
