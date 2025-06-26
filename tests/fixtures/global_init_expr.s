.data
y:
    .long 3
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl y, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
