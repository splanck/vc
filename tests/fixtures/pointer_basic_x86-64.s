.bss
.lcomm x, 8
.lcomm p, 8
.text
main:
    pushq %rbp
    movq %rsp, %rbp
    movq $x, %rax
    movq %rax, p
    movq $42, %rax
    movl %rax, x
    movq p, %rax
    movl (%rax), %rbx
    movq %rbx, %rax
    ret
    movq %rbp, %rsp
    popq %rbp
    ret
