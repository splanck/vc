.data
Lstr13:
    .asciz "factorial(%d) = %d\n"
.text
main:
    pushl %ebp
    movl %esp, %ebp
    subl $12, %esp
    movl $5, %eax
    movl %eax, -4(%ebp)
    movl $1, %eax
    movl %eax, -8(%ebp)
    movl $1, %eax
    movl %eax, -12(%ebp)
L0_start:
    movl -12(%ebp), %eax
    movl -4(%ebp), %ebx
    movl %eax, %ecx
    cmpl %ebx, %ecx
    setle %al
    movzbl %al, %ecx
    cmpl $0, %ecx
    je L0_end
    movl -8(%ebp), %ecx
    movl -12(%ebp), %ebx
    movl %ecx, %eax
    imull %ebx, %eax
    movl %eax, -8(%ebp)
    movl -12(%ebp), %eax
    movl $1, %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl %ecx, -12(%ebp)
    jmp L0_start
L0_end:
    movl $Lstr13, %ecx
    movl -4(%ebp), %ebx
    movl -8(%ebp), %eax
    pushl %eax
    pushl %ebx
    pushl %ecx
    call printf
    addl $12, %esp
    movl %eax, %ecx
    movl $0, %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
