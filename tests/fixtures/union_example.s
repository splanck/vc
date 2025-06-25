.data
u:
    .zero 4
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $u, %eax
    movl $65, %ebx
    movl $0, %ecx
    movl %ecx, %edx
    imull $1, %edx
    addl %eax, %edx
    movl %ebx, (%edx)
    movl $u, %ebx
    movl $4, %edx
    movl %edx, %ecx
    imull $1, %ecx
    addl %ebx, %ecx
    movl (%ecx), %edx
    movl %edx, %eax
    ret
