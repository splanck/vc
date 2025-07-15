.bss
.lcomm p, 4
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $p, %eax
    movl $0, %ebx
    movl %ebx, %ecx
    imull $1, %ecx
    addl %eax, %ecx
    movl $1, %ebx
    movl %ebx, (%ecx)
    movl $4, %ebx
    movl %ebx, %ecx
    imull $1, %ecx
    addl %eax, %ecx
    movl $5, %ebx
    movl %ebx, (%ecx)
    movl $p, %ebx
    movl $4, %ecx
    movl %ecx, %eax
    imull $1, %eax
    addl %ebx, %eax
    movl (%eax), %ecx
    movl $p, %eax
    movl $0, %ebx
    movl %ebx, %edx
    imull $1, %edx
    addl %eax, %edx
    movl (%edx), %ebx
    movl %ecx, %edx
    subl %ebx, %edx
    movl %edx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
