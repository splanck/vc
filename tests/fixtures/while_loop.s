main:
    pushl %ebp
    movl %esp, %ebp
    movl $3, %eax
    movl %eax, i
L0_start:
    movl i, %eax
    cmpl $0, %eax
    je L0_end
    movl i, %eax
    movl $1, %ebx
    movl %eax, %ecx
    subl %ebx, %ecx
    movl %ecx, i
    jmp L0_start
L0_end:
    movl i, %ecx
    movl %ecx, %eax
    ret
