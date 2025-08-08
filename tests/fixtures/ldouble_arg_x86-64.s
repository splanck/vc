main:
    pushq %rbp
    movq %rsp, %rbp
    movq $1, %rax
    movq %rax, -0(%rbp)
    movq $1, %rax
    sub $16, %rsp
    fldt %rax
    fstpt (%rsp)
    call sink
    addq $16, %rsp
    movq %rax, %rax
    movq $0, %rbx
    movq %rbx, %rax
    ret
    movq %rbp, %rsp
    popq %rbp
    ret
