.bss
.lcomm x, 8
.lcomm y, 8
.text
main:
    pushq %rbp
    movq %rsp, %rbp
    movq $5, %rax
    movl %eax, x
    movq $5, %rax
    movl %eax, y
    movq $5, %rax
    movq %rax, %rax
    ret
    movq %rbp, %rsp
    popq %rbp
    ret
