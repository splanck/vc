main:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl %eax, i
Luser0:
    movl $0, %eax
    cmpl $0, %eax
    je L1_end
    jmp Luser2
L1_end:
    movl $1, %eax
    movl %eax, i
    jmp Luser0
Luser2:
    movl i, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
