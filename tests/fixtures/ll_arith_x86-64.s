.bss
.lcomm a, 8
.lcomm b, 8
.lcomm r, 8
.text
main:
    pushq %rbp
    movq %rsp, %rbp
    movq $5000000000, %rax
    movq %rax, a
    movq $7, %rax
    movq %rax, b
    movq $705032711, %rax
    movl %eax, r
    movl r, %eax
    movq %rax, %rax
    ret
    movq %rbp, %rsp
    popq %rbp
    ret
