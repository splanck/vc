.data
.local g
g:
    .long 1
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl g, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
