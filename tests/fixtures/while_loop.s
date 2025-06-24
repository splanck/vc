main:
    pushl %ebp
    movl %esp, %ebp
    movl $3, %eax
    movl %eax, i
L0_start:
    movl $3, %eax
    cmpl $0, %eax
    je L0_end
    movl $2, %eax
    movl %eax, i
    jmp L0_start
L0_end:
    movl i, %eax
    movl %eax, %eax
    ret
