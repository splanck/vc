main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movl %eax, a
    movl $2, %eax
    movl %eax, b
    movl $1, %eax
    movl $2, %ebx
    fldt %eax
    fldt %ebx
    faddp
    fstpt %ecx
    movl %ecx, %eax
    ret
