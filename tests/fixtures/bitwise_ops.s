main:
    pushl %ebp
    movl %esp, %ebp
    subl $4, %esp
    movl $1, %eax
    movl %eax, -4(%ebp)
    movl $4, %eax
    movl %eax, -4(%ebp)
    movl -4(%ebp), %eax
    movl $3, %ebx
    movl %eax, %ecx
    orl %ebx, %ecx
    movl %ecx, -4(%ebp)
    movl -4(%ebp), %ecx
    movl $1, %ebx
    movl %ecx, %eax
    xorl %ebx, %eax
    movl %eax, -4(%ebp)
    movl -4(%ebp), %eax
    movl $6, %ebx
    movl %eax, %ecx
    andl %ebx, %ecx
    movl %ecx, -4(%ebp)
    movl -4(%ebp), %ecx
    movl $1, %ebx
    movl %ecx, %eax
    movl %ebx, %ecx
    sarl %cl, %eax
    movl %eax, -4(%ebp)
    movl -4(%ebp), %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
