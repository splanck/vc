.bss
.lcomm i, 4
.text
main:
    pushl ebp
    movl ebp, esp
    movl eax, 3
    movl i, eax
L0_start:
    movl eax, 3
    cmpl eax, 0
    je L0_end
    movl eax, 2
    movl i, eax
    jmp L0_start
L0_end:
    movl eax, i
    movl eax, eax
    ret
    movl esp, ebp
    popl ebp
    ret
