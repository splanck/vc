many_params:
    pushq %rbp
    movq %rsp, %rbp
    movq 64(%rbp), %rax
    movq %rax, %rax
    ret
    movq %rbp, %rsp
    popq %rbp
    ret
