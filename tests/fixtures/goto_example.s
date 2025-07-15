main:
    pushl %ebp
    movl %esp, %ebp
    subl $4, %esp
    movl $0, %eax
    movl %eax, -4(%ebp)
Luser0:
    movl $0, %eax
    cmpl $0, %eax
    je L1_end
    jmp Luser2
L1_end:
    movl $1, %eax
    movl %eax, -4(%ebp)
    jmp Luser0
Luser2:
    movl -4(%ebp), %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
