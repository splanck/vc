main:
    pushq %rbp
    movq %rsp, %rbp
    movq $7, %rax
    movq %rax, %rax
    ret
    movq %rbp, %rsp
    popq %rbp
    ret
