.data
s:
    .zero 4
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl s, %eax
    andl $-8, %eax
    orl $5, %eax
    movl %eax, s
    movl s, %eax
    andl $0xffffff07, %eax
    orl $136, %eax
    movl %eax, s
    movl s, %eax
    movl %eax, %ecx
    andl $7, %ecx
    shrl $3, %eax
    andl $31, %eax
    movl %ecx, %ebx
    addl %eax, %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
