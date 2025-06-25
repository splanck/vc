main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movl %eax, a
    movl $2, %eax
    movl %eax, b
    movl $1, %eax
    movl $2, %ebx
    movd %eax, %xmm0
    movd %ebx, %xmm1
    addss %xmm1, %xmm0
    movd %xmm0, %ecx
    movl %ecx, %eax
    ret
