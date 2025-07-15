main:
    pushl %ebp
    movl %esp, %ebp
    subl $4, %esp
    movl $1, %eax
    movl %eax, -4(%ebp)
    movl $1, %eax
    cmpl $0, %eax
    je L0_false
    movl $5, %eax
    movl %eax, tmp0
    jmp L0_end
L0_false:
    movl $2, %eax
    movl %eax, tmp0
L0_end:
    movl $2, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
