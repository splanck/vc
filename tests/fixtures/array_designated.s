main:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl $0, %ebx
    movl %ebx, a(,%eax,4)
    movl $1, %ebx
    movl $4, %eax
    movl %eax, a(,%ebx,4)
    movl $2, %eax
    movl $7, %ebx
    movl %ebx, a(,%eax,4)
    movl $1, %ebx
    movl a(,%ebx,4), %eax
    movl $2, %ebx
    movl a(,%ebx,4), %ecx
    movl %eax, %ebx
    addl %ecx, %ebx
    movl %ebx, %eax
    ret
