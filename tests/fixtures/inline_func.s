add:
    pushl %ebp
    movl %esp, %ebp
    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl %ecx, %eax
    ret
main:
    pushl %ebp
    movl %esp, %ebp
    movl $2, %ecx
    movl $3, %ebx
    pushl %ebx
    pushl %ecx
    call add
    addl $8, %esp
    movl %eax, %ecx
    movl %ecx, %eax
    ret
