.bss
.lcomm x, 4
.lcomm y, 4
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $5, %eax
    movl %eax, x
    movl x, %eax
    movl %eax, y
    movl y, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
