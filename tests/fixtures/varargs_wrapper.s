.bss
.lcomm n, 4
.lcomm ap, 4
.lcomm t, 4
.lcomm i, 4
.text
sum:
    pushl %ebp
    movl %esp, %ebp
    movl $n, %eax
    movl $1, %ebx
    movl %ebx, %ecx
    imull $4, %ecx
    addl %eax, %ecx
    movl %ecx, ap
    movl $0, %ecx
    movl %ecx, t
    movl $0, %ecx
    movl %ecx, i
L0_start:
    movl $0, %ecx
    movl 8(%ebp), %ebx
    movl %ecx, %eax
    cmpl %ebx, %eax
    setl %al
    movzbl %al, %eax
    cmpl $0, %eax
    je L0_end
    movl $0, %eax
    movl ap, %ebx
    movl $1, %ecx
    movl %ecx, %edx
    imull $4, %edx
    addl %ebx, %edx
    movl %edx, ap
    movl (%ebx), %edx
    movl %eax, %ebx
    addl %edx, %ebx
    movl %ebx, t
L0_cont:
    movl $1, %ebx
    movl %ebx, i
    jmp L0_start
L0_end:
    movl ap, %ebx
    movl %ebx, ap
    movl t, %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
main:
    pushl %ebp
    movl %esp, %ebp
    movl $3, %ebx
    movl $1, %edx
    movl $2, %eax
    movl $3, %ecx
    pushl %ecx
    pushl %eax
    pushl %edx
    pushl %ebx
    call sum
    addl $16, %esp
    movl %eax, %ebx
    movl %ebx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
