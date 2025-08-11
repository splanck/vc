sub:
    pushl ebp
    movl ebp, esp
    movl eax, [ebp+8]
    movl ebx, [ebp+12]
    movd xmm0, eax
    movd xmm1, ebx
    subss xmm0, xmm1
    movd ecx, xmm0
    movd xmm0, ecx
    ret
    movl esp, ebp
    popl ebp
    ret
