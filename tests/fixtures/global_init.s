.data
x:
    .long 5
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl x, %eax
    movl %eax, %eax
    ret
