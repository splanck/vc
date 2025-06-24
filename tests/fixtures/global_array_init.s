.data
nums:
    .long 1
    .long 2
    .long 3
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $0, %eax
    movl nums(,%eax,4), %ebx
    movl $2, %eax
    movl nums(,%eax,4), %ecx
    movl %ebx, %eax
    addl %ecx, %eax
    movl %eax, %eax
    ret
