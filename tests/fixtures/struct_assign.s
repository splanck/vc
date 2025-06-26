.data
p:
    .zero 8
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $p, %eax
    movl $5, %ebx
    movl $0, %ecx
    movl %ecx, %edx
    imull $1, %edx
    addl %eax, %edx
    movl %ebx, (%edx)
    movl $p, %ebx
    movl $10, %edx
    movl $4, %ecx
    movl %ecx, %eax
    imull $1, %eax
    addl %ebx, %eax
    movl %edx, (%eax)
    movl $p, %edx
    movl $0, %eax
    movl %eax, %ecx
    imull $1, %ecx
    addl %edx, %ecx
    movl (%ecx), %eax
    movl $p, %ecx
    movl $4, %edx
    movl %edx, %ebx
    imull $1, %ebx
    addl %ecx, %ebx
    movl (%ebx), %edx
    movl %eax, %ebx
    addl %edx, %ebx
    movl %ebx, %eax
    ret
