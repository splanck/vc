main:
    pushl %ebp
    movl %esp, %ebp
    movl $3, %eax
    movl %eax, x
    movl $0, %eax
    cmpl $0, %eax
    je L0_else
    movl $1, %eax
    movl %eax, %eax
    ret
    jmp L0_end
L0_else:
    movl $0, %eax
    movl %eax, %eax
    ret
L0_end:
    movl %ebp, %esp
    popl %ebp
    ret
