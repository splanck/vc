main:
    pushl %ebp
    movl %esp, %ebp
    subl $4, %esp
    movl $0, %eax
    movl %eax, -4(%ebp)
L0_start:
    movl $1, %eax
    cmpl $0, %eax
    je L0_end
    movl $1, %eax
    movl %eax, -4(%ebp)
L0_cont:
    movl -4(%ebp), %eax
    movl $1, %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl %ecx, -4(%ebp)
    jmp L0_start
L0_end:
    movl -4(%ebp), %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
