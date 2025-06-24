main:
    pushl %ebp
    movl %esp, %ebp
    movl $a, %eax
    movl $1, %ebx
    movl %ebx, %ecx
    imull $4, %ecx
    addl %eax, %ecx
    movl %ecx, p
    movl p, %ecx
    movl $-1, %ebx
    movl %ebx, %eax
    imull $4, %eax
    addl %ecx, %eax
    movl %eax, p
    movl $0, %eax
    movl $7, %ebx
    movl %ebx, a(,%eax,4)
    movl p, %ebx
    movl (%ebx), %eax
    movl %eax, %eax
    ret
