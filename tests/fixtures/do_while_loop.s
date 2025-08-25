main:
    pushl %ebp
    movl %esp, %ebp
    subl $4, %esp
    movl $0, %eax
    movl %eax, -4(%ebp)
L0_start:
    movl $1, %eax
    movl %eax, -4(%ebp)
L0_cond:
    movl -4(%ebp), %eax
    movl $3, %ebx
    cmpl %ebx, %eax
    setl %al
    movzbl %al, %ecx
    cmpl $0, %ecx
    je L0_end
    jmp L0_start
L0_end:
    movl -4(%ebp), %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
