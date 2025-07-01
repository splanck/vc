main:
    pushl ebp
    movl ebp, esp
    movl eax, 5
    movl x, eax
    movl eax, 5
    movl y, eax
    movl eax, 5
    movl eax, eax
    ret
    movl esp, ebp
    popl ebp
    ret
