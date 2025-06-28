.data
LWstr1:
    .long 104
    .long 105
    .long 0
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $LWstr1, %eax
    movl %eax, p
    movl p, %eax
    movl (%eax), %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
