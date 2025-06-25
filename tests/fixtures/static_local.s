.data
.local __static0
__static0:
    .long 3
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl __static0, %eax
    movl %eax, %eax
    ret
