main:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl %eax, i
L0_start:
    movl $1, %eax
    movl %eax, i
L0_cond:
    movl i, %eax
    movl $3, %ebx
    movl %eax, %ecx
    cmpl %ebx, %ecx
    setl %al
    movzbl %al, %ecx
    cmpl $0, %ecx
    je L0_end
    jmp L0_start
L0_end:
    movl i, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
