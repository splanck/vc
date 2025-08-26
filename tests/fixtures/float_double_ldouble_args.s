main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movl %eax, -0(%ebp)
    movl $2, %eax
    movl %eax, -0(%ebp)
    movl $3, %eax
    movl %eax, -0(%ebp)
    movl $3, %eax
    sub $4, %esp
    movd %eax, %xmm0
    movss %xmm0, (%esp)
    call sinkf
    addl $4, %esp
    movl %eax, %eax
    movq -0(%ebp), %ebx
    sub $8, %esp
    movq %ebx, %xmm0
    movsd %xmm0, (%esp)
    call sinkd
    addl $8, %esp
    movl %eax, %ebx
    movl -0(%ebp), %ecx
    sub $10, %esp
    fldt %ecx
    fstpt (%esp)
    call sinkld
    addl $10, %esp
    movl %eax, %ecx
    movl $0, %edx
    movl %edx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
