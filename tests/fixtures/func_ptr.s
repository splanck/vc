.data
fp:
    .long 0
.text
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
    movl $add, %ecx
    movl %ecx, fp
    movl fp, %ecx
    movl $2, %ebx
    movl $3, %eax
    pushl %eax
    pushl %ebx
    call *%ecx
    addl $8, %esp
    movl %eax, %ebx
    movl %ebx, %eax
    ret
