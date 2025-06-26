.data
Lstr1:
    .asciz "hello"
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $Lstr1, %eax
    movl %eax, s
    movl $0, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
