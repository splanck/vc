main:
    pushl %ebp
    movl %esp, %ebp
Lstr1:
    .asciz "hi"
    movl $Lstr1, %eax
    movl $0, %ebx
    movl %ebx, %eax
    ret
