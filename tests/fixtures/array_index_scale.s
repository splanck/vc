.data
ca:
    .long 0
    .long 0
    .long 0
    .long 0
da:
    .long 0
    .long 0
    .long 0
    .long 0
.text
get_uc:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl ca(,%eax,1), %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
get_d:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %ebx
    movl da(,%ebx,8), %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
