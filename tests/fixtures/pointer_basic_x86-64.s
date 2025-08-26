.bss
.lcomm x, 8
.lcomm p, 8
.text
main:
    pushq %rbp
    movq %rsp, %rbp
    movabsq $x, %rax
    movq %rax, p
    movq $42, %rax
    movl %eax, x
    movq p, %rax
    movl (%rax), %ebx
    movq %rbx, %rax
    ret
    movq %rbp, %rsp
    popq %rbp
    ret
