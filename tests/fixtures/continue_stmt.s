main:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl %eax, i
L0_start:
    movl $1, %eax
    cmpl $0, %eax
    je L0_end
    jmp L0_cont
L0_cont:
    movl $1, %eax
    movl %eax, i
    jmp L0_start
L0_end:
    movl i, %eax
    movl %eax, %eax
    ret
