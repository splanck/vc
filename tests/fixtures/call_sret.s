foo:
    pushl %ebp
    movl %esp, %ebp
    leal -0(%ebp), %ebx
    movl $0, %ecx
    movl %ecx, %edx
    imull $1, %edx
    addl %ebx, %edx
    movl $5, %ecx
    movl %ecx, (%edx)
    movl -0(%ebp), %ecx
    movl 8(%ebp), %eax
    movl %ecx, (%eax)
    movl %eax, (%eax)
    ret
    movl %ebp, %esp
    popl %ebp
    ret
main:
    pushl %ebp
    movl %esp, %ebp
    movl $4, %eax
    subl %eax, %esp
    movl %esp, %ecx
    pushl %ecx
    call foo
    addl $4, %esp
    movl %eax, %eax
    movl %ecx, -0(%ebp)
    leal -0(%ebp), %ecx
    movl $0, %edx
    movl %edx, %ebx
    imull $1, %ebx
    addl %ecx, %ebx
    movl (%ebx), %edx
    movl %edx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
