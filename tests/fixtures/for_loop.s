main:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl %eax, i
L0_start:
    movl i, %eax
    movl $3, %ebx
    movl %eax, %ecx
    cmpl %ebx, %ecx
    setl %al
    movzbl %al, %ecx
    cmpl $0, %ecx
    je L0_end
    movl i, %ecx
    movl $1, %ebx
    movl %ecx, %eax
    addl %ebx, %eax
    movl %eax, i
    movl i, %eax
    movl $1, %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl %ecx, i
    jmp L0_start
L0_end:
    movl i, %ecx
    movl %ecx, %eax
    ret
