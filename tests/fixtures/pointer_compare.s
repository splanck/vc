.bss
.lcomm arr, 4
.lcomm p1, 4
.lcomm p2, 4
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $arr, %eax
    movl %eax, p1
    movl $arr, %eax
    movl $1, %ebx
    movl %ebx, %ecx
    imull $4, %ecx
    addl %eax, %ecx
    movl %ecx, p2
    movl p1, %ecx
    movl p2, %ebx
    movl %ecx, %eax
    cmpl %ebx, %eax
    setl %al
    movzbl %al, %eax
    movl %eax, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
