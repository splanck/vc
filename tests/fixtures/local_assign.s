main:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp
    movq $0, %rax
    movq %rax, -4(%rbp)
L0_start:
    movq $1, %rax
    cmpq $0, %rax
    je L0_end
    movq $1, %rax
    movq %rax, -4(%rbp)
    jmp L0_start
L0_end:
    movq -4(%rbp), %rax
    movq %rax, %rax
    ret
    movq %rbp, %rsp
    popq %rbp
    ret
