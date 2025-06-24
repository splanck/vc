.data
x:
    .long 0
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $3, %eax
    movl %eax, x
    movl x, %eax
    movl %eax, %eax
    ret
