caller:
    pushq %rbp
    movq %rsp, %rbp
    movq 16(%rbp), %rax
    movq $1, %rbx
    movl %eax, %ecx
    imull %ebx, %ecx
    movq %rcx, %rax
    addq $15, %rax
    andq $-16, %rax
    subq %rax, %rsp
    movq %rsp, %rbx
    call callee
    movq %rax, %rcx
    movq $0, %rax
    movq $0, %rdx
    movq %rax, %rsi
    imulq $1, %rsi
    addq %rbx, %rsi
    movl %edx, (%rsi)
    movq %rbp, %rsp
    popq %rbp
    ret
