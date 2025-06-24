main:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl $1, %ebx
    movl %ebx, a(,%eax,4)
    movl $1, %ebx
    movl $2, %eax
    movl %eax, a(,%ebx,4)
    movl $0, %eax
    movl a(,%eax,4), %ebx
    movl $1, %eax
    movl a(,%eax,4), %ecx
    movl %ebx, %eax
    addl %ecx, %eax
    movl %eax, %eax
    ret
