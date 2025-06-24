foo:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl %eax, %eax
    ret
main:
    pushl %ebp
    movl %esp, %ebp
    call foo
    movl %eax, %eax
    movl $0, %ebx
    movl %ebx, %eax
    ret
