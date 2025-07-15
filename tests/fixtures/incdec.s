main:
    pushl %ebp
    movl %esp, %ebp
    subl $4, %esp
    movl $0, %eax
    movl %eax, -4(%ebp)
    movl $1, %eax
    movl %eax, -4(%ebp)
    movl -4(%ebp), %eax
    movl $1, %ebx
    movl %eax, %ecx
    subl %ebx, %ecx
    movl %ecx, -4(%ebp)
    movl -4(%ebp), %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
