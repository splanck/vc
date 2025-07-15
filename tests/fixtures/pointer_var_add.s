.bss
.lcomm i, 4
.lcomm arr, 4
.lcomm p, 4
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $1, %eax
    movl %eax, i
    movl $arr, %eax
    movl %eax, p
    movl p, %eax
    movl $1, %ebx
    movl %ebx, %ecx
    imull $4, %ecx
    addl %eax, %ecx
    movl %ecx, p
    movl $1, %ecx
    movl $4, %ebx
    movl %ebx, arr(,%ecx,4)
    movl p, %ebx
    movl (%ebx), %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
