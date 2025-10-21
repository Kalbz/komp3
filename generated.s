.section .rodata
.LC_FMT_INT:
    .string "%d\n"
.LC_FMT_BOOL:
    .string "%s\n"
.LC_BOOL_TRUE:
    .string "true"
.LC_BOOL_FALSE:
    .string "false"

.text
.extern printf

.globl E__main
E__main:
    pushq %rbp
    movq %rsp, %rbp
    subq $128, %rsp

.LE__main_entry_0:
    # const0 = iconst 4
    movl $4, %eax
    movl %eax, -16(%rbp)
    # const1 = iconst 2
    movl $2, %eax
    movl %eax, -20(%rbp)
    # tmp2 = mul const0, const1
    movl -16(%rbp), %eax
    imull -20(%rbp), %eax
    movl %eax, -88(%rbp)
    # const3 = iconst 10
    movl $10, %eax
    movl %eax, -52(%rbp)
    # tmp4 = add tmp2, const3
    movl -88(%rbp), %eax
    addl -52(%rbp), %eax
    movl %eax, -116(%rbp)
    # const5 = iconst 2
    movl $2, %eax
    movl %eax, -56(%rbp)
    # const6 = iconst 6
    movl $6, %eax
    movl %eax, -60(%rbp)
    # tmp7 = mul const5, const6
    movl -56(%rbp), %eax
    imull -60(%rbp), %eax
    movl %eax, -120(%rbp)
    # tmp8 = sub tmp4, tmp7
    movl -116(%rbp), %eax
    subl -120(%rbp), %eax
    movl %eax, -124(%rbp)
    # const9 = iconst 4
    movl $4, %eax
    movl %eax, -64(%rbp)
    # const10 = iconst 1
    movl $1, %eax
    movl %eax, -24(%rbp)
    # tmp11 = sub const9, const10
    movl -64(%rbp), %eax
    subl -24(%rbp), %eax
    movl %eax, -68(%rbp)
    # const12 = iconst 2
    movl $2, %eax
    movl %eax, -28(%rbp)
    # tmp13 = mul tmp11, const12
    movl -68(%rbp), %eax
    imull -28(%rbp), %eax
    movl %eax, -72(%rbp)
    # const14 = iconst 2
    movl $2, %eax
    movl %eax, -32(%rbp)
    # tmp15 = div tmp13, const14
    movl -72(%rbp), %eax
    movl -32(%rbp), %ecx
    cltd
    idivl %ecx
    movl %eax, -76(%rbp)
    # tmp16 = add tmp8, tmp15
    movl -124(%rbp), %eax
    addl -76(%rbp), %eax
    movl %eax, -80(%rbp)
    # print tmp16
    movl -80(%rbp), %esi
    movq $.LC_FMT_INT, %rdi
    movl $0, %eax
    call printf
    # bool17 = iconst 1
    movl $1, %eax
    movl %eax, -4(%rbp)
    # bool18 = iconst 1
    movl $1, %eax
    movl %eax, -8(%rbp)
    # tmp19 = not bool18
    movl -8(%rbp), %eax
    cmpl $0, %eax
    setne %al
    movzbl %al, %eax
    xorl $1, %al
    movzbl %al, %eax
    movl %eax, -84(%rbp)
    # bool20 = iconst 0
    movl $0, %eax
    movl %eax, -12(%rbp)
    # tmp21 = and tmp19, bool20
    movl -84(%rbp), %eax
    cmpl $0, %eax
    setne %al
    movzbl %al, %eax
    movl -12(%rbp), %ecx
    cmpl $0, %ecx
    setne %cl
    movzbl %cl, %ecx
    andl %ecx, %eax
    movl %eax, -92(%rbp)
    # tmp22 = eq bool17, tmp21
    movl -4(%rbp), %eax
    cmpl -92(%rbp), %eax
    sete %al
    movzbl %al, %eax
    movl %eax, -96(%rbp)
    # const23 = iconst 10
    movl $10, %eax
    movl %eax, -36(%rbp)
    # const24 = iconst 1
    movl $1, %eax
    movl %eax, -40(%rbp)
    # tmp25 = gt const23, const24
    movl -36(%rbp), %eax
    cmpl -40(%rbp), %eax
    setg %al
    movzbl %al, %eax
    movl %eax, -100(%rbp)
    # const26 = iconst 1
    movl $1, %eax
    movl %eax, -44(%rbp)
    # const27 = iconst 10
    movl $10, %eax
    movl %eax, -48(%rbp)
    # tmp28 = lt const26, const27
    movl -44(%rbp), %eax
    cmpl -48(%rbp), %eax
    setl %al
    movzbl %al, %eax
    movl %eax, -104(%rbp)
    # tmp29 = and tmp25, tmp28
    movl -100(%rbp), %eax
    cmpl $0, %eax
    setne %al
    movzbl %al, %eax
    movl -104(%rbp), %ecx
    cmpl $0, %ecx
    setne %cl
    movzbl %cl, %ecx
    andl %ecx, %eax
    movl %eax, -108(%rbp)
    # tmp30 = or tmp22, tmp29
    movl -96(%rbp), %eax
    cmpl $0, %eax
    setne %al
    movzbl %al, %eax
    movl -108(%rbp), %ecx
    cmpl $0, %ecx
    setne %cl
    movzbl %cl, %ecx
    orl %ecx, %eax
    movl %eax, -112(%rbp)
    # print tmp30
    cmpl $0, -112(%rbp)
    jne .LE__main_bool_true_0
    movq $.LC_BOOL_FALSE, %rsi
    jmp .LE__main_bool_end_0
.LE__main_bool_true_0:
    movq $.LC_BOOL_TRUE, %rsi
.LE__main_bool_end_0:
    movq $.LC_FMT_BOOL, %rdi
    movl $0, %eax
    call printf
.LE__main_epilogue:
    leave
    ret

