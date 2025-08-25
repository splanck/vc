.bss
.lcomm a, 4
.lcomm p, 4
.text
main:
    pushl ebp
    movl ebp, esp
    movl eax, a
    movl p, eax
    movl eax, p
    movl ebx, 1
    mov ecx, ebx
    imull ecx, 4
    add ecx, eax
    movl p, ecx
    movl ecx, 1
    movl ebx, 5
    movl [a+ecx*4], ebx
    movl ebx, p
    movl ecx, [ebx]
    movl eax, ecx
    ret
    movl esp, ebp
    popl ebp
    ret
