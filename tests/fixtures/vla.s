main:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl $2, %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl $4, %ebx
    movl %ecx, %eax
    imull %ebx, %eax
    subl %eax, %esp
    movl %esp, %ebx
    movl $0, %eax
    movl $1, %ecx
    movl %eax, %edx
    imull $4, %edx
    addl %ebx, %edx
    movl %ecx, (%edx)
    movl 8(%ebp), %ecx
    movl %ecx, %edx
    imull $4, %edx
    addl %ebx, %edx
    movl (%edx), %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
