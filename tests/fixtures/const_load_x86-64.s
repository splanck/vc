main:
    pushq %rbp
    movq %rsp, %rbp
    movq $5, %rax
    movq %rax, x
    movq $5, %rax
    movq %rax, y
    movq $5, %rax
    movq %rax, %rax
    ret
    movq %rbp, %rsp
    popq %rbp
    ret
