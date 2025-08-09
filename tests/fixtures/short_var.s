.bss
.lcomm s, 4
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movw %eax, s
    movw $1, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
