.bss
.lcomm a, 8
.text
main:
    pushq %rbp
    movq %rsp, %rbp
    movq $5000000000, %rax
    movq %rax, a
    movq $705032709, %rax
    movq %rax, %rax
    ret
    movq %rbp, %rsp
    popq %rbp
    ret
