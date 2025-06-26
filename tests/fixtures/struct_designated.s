main:
    pushl %ebp
    movl %esp, %ebp
    movl $s, %eax
    movl $4, %ebx
    movl %ebx, %ecx
    imull $1, %ecx
    addl %eax, %ecx
    movl $5, %ebx
    movl %ebx, (%ecx)
    movl $0, %ebx
    movl %ebx, %ecx
    imull $1, %ecx
    addl %eax, %ecx
    movl $3, %ebx
    movl %ebx, (%ecx)
    movl $s, %ebx
    movl $0, %ecx
    movl %ecx, %eax
    imull $1, %eax
    addl %ebx, %eax
    movl (%eax), %ecx
    movl $s, %eax
    movl $4, %ebx
    movl %ebx, %edx
    imull $1, %edx
    addl %eax, %edx
    movl (%edx), %ebx
    movl %ecx, %edx
    addl %ebx, %edx
    movl %edx, %eax
    ret
