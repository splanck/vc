.data
Lstr1:
    .asciz "foobar"
.bss
.lcomm p, 4
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $Lstr1, %eax
    movl %eax, p
    movl p, %eax
    movl (%eax), %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
