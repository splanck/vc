main:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl %eax, i
L0_start:
    movl $1, %eax
    cmpl $0, %eax
    je L0_end
    movl $1, %eax
    movl %eax, i
    movl i, %eax
    movl $1, %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl %ecx, i
    jmp L0_start
L0_end:
    movl i, %ecx
    movl %ecx, %eax
    ret
