.data
Lstr1:
    .asciz "foo"
.text
foo:
    pushl %ebp
    movl %esp, %ebp
    movl $Lstr1, %eax
    movl %eax, p
    movl p, %eax
    movl $1, %ebx
    movl %ebx, %ecx
    imull $1, %ecx
    addl %eax, %ecx
    movl (%ecx), %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
