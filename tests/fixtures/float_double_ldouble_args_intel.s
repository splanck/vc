main:
    pushl ebp
    movl ebp, esp
    movl eax, 1
    movl [ebp-0], eax
    movl eax, 2
    movl [ebp-0], eax
    movl eax, 3
    movl [ebp-0], eax
    movl eax, 3
    sub esp, 4
    movd xmm0, eax
    movss [esp], xmm0
    call sinkf
    addl esp, 4
    movl eax, eax
    movl ebx, [ebp-0]
    sub esp, 8
    movq xmm0, ebx
    movsd [esp], xmm0
    call sinkd
    addl esp, 8
    movl ebx, eax
    movl ecx, [ebp-0]
    sub esp, 10
    fld tword ptr ecx
    fstp tword ptr [esp]
    call sinkld
    addl esp, 10
    movl ecx, eax
    movl edx, 0
    movl eax, edx
    ret
    movl esp, ebp
    popl ebp
    ret
