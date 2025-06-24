main:
    pushl %ebp
    movl %esp, %ebp
    movl $a, %eax
    movl %eax, p1
    movl $a, %eax
    movl $2, %ebx
    movl %ebx, %ecx
    imull $4, %ecx
    addl %eax, %ecx
    movl %ecx, p2
    movl p2, %ecx
    movl p1, %ebx
    movl %ecx, %eax
    subl %ebx, %eax
    sarl $2, %eax
    movl %eax, %eax
    ret
