main:
    pushl %ebp
    movl %esp, %ebp
    movl $a, %eax
    movl %eax, p
    movl p, %eax
    movl $1, %ebx
    movl %ebx, %ecx
    imull $4, %ecx
    addl %eax, %ecx
    movl %ecx, p
    movl $1, %ecx
    movl $5, %ebx
    movl %ebx, a(,%ecx,4)
    movl p, %ebx
    movl (%ebx), %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
