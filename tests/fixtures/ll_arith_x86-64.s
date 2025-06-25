main:
    pushq %rbp
    movq %rsp, %rbp
    movq $5000000000, %rax
    movq %rax, a
    movq $7, %rax
    movq %rax, b
    movq $705032711, %rax
    movq %rax, r
    movq r, %rax
    movq %rax, %rax
    ret
