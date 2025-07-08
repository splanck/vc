.data
x:
    .long 0
p:
    .long x
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $3, %eax
    movl %eax, x
    movl p, %eax
    movl (%eax), %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
