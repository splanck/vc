.data
foobar:
    .long 5
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl foobar, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
