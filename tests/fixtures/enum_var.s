.data
c:
    .long 0
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movl %eax, c
    movl $1, %eax
    movl %eax, %eax
    ret
