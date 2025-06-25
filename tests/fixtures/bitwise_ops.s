main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movl %eax, x
    movl $4, %eax
    movl %eax, x
    movl x, %eax
    movl $3, %ebx
    movl %eax, %ecx
    orl %ebx, %ecx
    movl %ecx, x
    movl x, %ecx
    movl $1, %ebx
    movl %ecx, %eax
    xorl %ebx, %eax
    movl %eax, x
    movl x, %eax
    movl $6, %ebx
    movl %eax, %ecx
    andl %ebx, %ecx
    movl %ecx, x
    movl x, %ecx
    movl $1, %ebx
    movl %ecx, %eax
    movl %ebx, %ecx
    sarl %cl, %eax
    movl %eax, x
    movl x, %eax
    movl %eax, %eax
    ret
