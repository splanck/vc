main:
    pushq %rbp
    movq %rsp, %rbp
    movq $2, %rax
    movl %rax, -0(%rbp)
    movq $3, %rax
    movq %rax, -0(%rbp)
    movq $1, %rax
    movq $3, %rbx
    movq $3, %rcx
    movq $4, %rdx
    movq %rax, %rdi
    movd %rbx, %xmm0
    movq %rcx, %xmm1
    movq %rdx, %rsi
    call mix
    movq %rax, %rdx
    movq $0, %rcx
    movq %rcx, %rax
    ret
    movq %rbp, %rsp
    popq %rbp
    ret
