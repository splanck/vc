main:
    pushl %ebp
    movl %esp, %ebp
L0_start:
    movl $1, %eax
    cmpl $0, %eax
    je L0_end
    jmp L0_end
    jmp L0_start
L0_end:
    movl $0, %eax
    movl %eax, %eax
    ret
