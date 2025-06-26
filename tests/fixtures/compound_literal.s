main:
    pushl %ebp
    movl %esp, %ebp
    movl $4, %eax
    subl %eax, %esp
    movl %esp, %ebx
    movl $5, %eax
    movl $0, %ecx
    movl %ecx, %edx
    imull $4, %edx
    addl %ebx, %edx
    movl %eax, (%edx)
    movl (%ebx), %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
