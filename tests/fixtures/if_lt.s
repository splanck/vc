main:
    pushl %ebp
    movl %esp, %ebp
    movl $3, %eax
    movl %eax, x
    movl x, %eax
    movl $5, %ebx
    movl %eax, %ecx
    cmpl %ebx, %ecx
    setl %al
    movzbl %al, %ecx
    cmpl $0, %ecx
    je L0_else
    movl $1, %ecx
    movl %ecx, %eax
    ret
    jmp L0_end
L0_else:
    movl $0, %ecx
    movl %ecx, %eax
    ret
L0_end:
