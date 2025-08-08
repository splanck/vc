main:
    pushl ebp
    movl ebp, esp
    subl esp, 16
    movl eax, 1
    movl [ebp-4], eax
    movl eax, 1
    movl ebx, [ebp-8]
    movl eax, eax
    movl ecx, ebx
    sal eax, cl
    movl ecx, eax
    movl [ebp-12], ecx
    movl ecx, 1
    movl ebx, [ebp-8]
    movl eax, ecx
    movl ecx, ebx
    sar eax, cl
    movl [ebp-16], eax
    movl eax, [ebp-12]
    movl ebx, [ebp-16]
    movl ecx, eax
    addl ecx, ebx
    movl eax, ecx
    ret
    movl esp, ebp
    popl ebp
    ret
