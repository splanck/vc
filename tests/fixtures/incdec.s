main:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl %eax, i
    movl $1, %eax
    movl %eax, i
    movl i, %eax
    movl $1, %ebx
    movl %eax, %ecx
    subl %ebx, %ecx
    movl %ecx, i
    movl i, %ecx
    movl %ecx, %eax
    ret
