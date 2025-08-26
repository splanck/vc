.bss
.lcomm b, 8
.lcomm c, 8
.text
main:
    pushq %rbp
    movq %rsp, %rbp
    movq $2, %rax
    movl %eax, b
    movq $3, %rax
    movq %rax, c
    movq $1, %rax
    movq $2, %rbx
    movq $3, %rcx
    movq $4, %rdx
    movq %rax, %rdi
    movd %ebx, %xmm0
    movq %rcx, %xmm1
    movq %rdx, %rsi
    call mix
    movq %rax, %rdx
    movq $0, %rcx
    movq %rcx, %rax
    ret
    movq %rbp, %rsp
    popq %rbp
    ret
