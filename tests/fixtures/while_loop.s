main:
    pushl %ebp
    movl %esp, %ebp
    movl $3, %eax
    movl %eax, i
L0_start:
    movl i, %ebx
    cmpl $0, %ebx
    je L0_end
    movl i, %ecx
    movl $1, %edx
    movl %ecx, %esi
    subl %edx, %esi
    movl %esi, i
    jmp L0_start
L0_end:
    movl i, %edi
    movl %edi, %eax
    ret
