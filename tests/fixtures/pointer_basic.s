main:
    pushl %ebp
    movl %esp, %ebp
    movl $x, %eax
    movl %eax, p
    movl $42, %ebx
    movl %ebx, x
    movl p, %ecx
    movl (%ecx), %edx
    movl %edx, %eax
    ret
