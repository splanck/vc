.bss
.lcomm a, 4
.lcomm b, 4
.lcomm c, 4
.lcomm d, 4
.lcomm e, 4
.lcomm f, 4
.text
main:
    pushl %ebp
    movl %esp, %ebp
    movl $3, %eax
    movl %eax, a
    movl $7, %eax
    movl %eax, b
    movl $11, %eax
    movl %eax, c
    movl $15, %eax
    movl %eax, d
    movl $19, %eax
    movl %eax, e
    movl $23, %eax
    movl %eax, f
    movl a, %eax
    movl b, %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl c, %ebx
    movl %ecx, %eax
    addl %ebx, %eax
    movl d, %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl e, %ebx
    movl %ecx, %eax
    addl %ebx, %eax
    movl f, %ebx
    movl %eax, %ecx
    addl %ebx, %ecx
    movl %ecx, %eax
    ret
    movl %ebp, %esp
    popl %ebp
    ret
