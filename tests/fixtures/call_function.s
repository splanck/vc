foo:
    pushl %ebp
    movl %esp, %ebp
    movl $3, %eax
    movl %eax, %eax
    ret
main:
    pushl %ebp
    movl %esp, %ebp
    call foo
    movl %eax, %ebx
    movl %ebx, %eax
    ret
