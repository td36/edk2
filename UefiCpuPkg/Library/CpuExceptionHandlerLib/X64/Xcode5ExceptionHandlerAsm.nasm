;------------------------------------------------------------------------------ ;
; Copyright (c) 2012 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Module Name:
;
;   ExceptionHandlerAsm.Asm
;
; Abstract:
;
;   x64 CPU Exception Handler
;
; Notes:
;
;------------------------------------------------------------------------------
%include "ExceptionHandler.inc"

;
; CommonExceptionHandler()
;

%define VC_EXCEPTION 29

extern ASM_PFX(mErrorCodeFlag)    ; Error code flags for exceptions
extern ASM_PFX(mDoFarReturnFlag)  ; Do far return flag
extern ASM_PFX(CommonExceptionHandler)

SECTION .data

DEFAULT REL
SECTION .text

ALIGN   32
;-------------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  AsmFredEntry (
;    VOID
;    );
;-------------------------------------------------------------------------------------
global ASM_PFX(AsmFredEntry)
ASM_PFX(AsmFredEntry):
AsmFredBegin:
; The trampoline code is 32-byte long. It's a ring 0 FRED event when the entry RIP is
; aligned at the 4K address. Because we only put the trampoline code in a 32-byte
; aligned address, the worst case is the trampoline code is aligned in 32-byte but not
; in 64-byte. To meet the FRED spec requirement that the FRED ring 3 handler should be
; aligned in 4KB, we need (4096/32) trampoline pieces so the last one is just
; aligned in 4KB. Because the ring 0 FRED handler is 256-byte after the ring 3 one,
; we need 256/32 trampoline pieces additionally. In summary, we need
; (4096/32 + 256/32) = (128 + 8) = 136 trampoline pieces.

%assign Vector 0
%rep  136
OneTrampoline %+ Vector:
    ; If current address is 4K align, which means excepiont happens in ring3
    push    rax
    lea     rax, [OneTrampoline %+ Vector]
    and     ax,  ~0xfff
    jz      AsmFredBeginRing3
    jmp     AsmFredBeginRing0
    TIMES   (32 - ($ - (OneTrampoline %+ Vector))) DB 0xCC
%assign Vector Vector+1
%endrep

AsmFredBeginRing3:
; Entry of CPL 3 at offset 0
    jmp $
AsmFredBeginRing0:
; Entry of CPL 0 at offset 256
    pop     rax
    push    rcx
    push    rcx

    ;
    ; Stack:
    ; +---------------------+ <-- 16-byte aligned ensured by processor
    ; +    0                +
    ; +---------------------+
    ; +    Event Data       +
    ; +---------------------+
    ; +    Event Info       + <-- Error Code in low 2 bytes
    ; +---------------------+
    ; +    Old SS           +
    ; +---------------------+
    ; +    Old RSP          +
    ; +---------------------+
    ; +    RFlags           +
    ; +---------------------+
    ; +    CS               +
    ; +---------------------+
    ; +    RIP              +
    ; +---------------------+ <-- FRED Stack Context prepared by processor
    ; +    Old RCX          + <-- will replaced with Event Info (contain Error Code)
    ; +---------------------+
    ; +    Old RCX          +
    ; +---------------------+ <-- RSP, 16-byte aligned
    mov     rcx, qword [rsp + 16 + FRED_STACK_CONTEXT.EventInfo] ; RCX = Event Info
    mov     qword [rsp + 8], rcx
    xor     rcx, rcx
    mov     ch, 1  ; CL = 1, indicating FRED
    mov     cl, byte [rsp + 16 + FRED_STACK_CONTEXT.EventInfo + 4] ; CL = vector number, from 4th byte in Event Info
    jmp     HasErrorCode
ALIGN   8

; Generate 256 IDT vectors.
AsmIdtVectorBegin:
%assign Vector 0
%rep  256
    push    strict dword %[Vector] ; This instruction pushes sign-extended 8-byte value on stack
    push    rax
    mov     rax, strict qword 0    ; mov     rax, ASM_PFX(CommonInterruptEntry)
    jmp     rax
%assign Vector Vector+1
%endrep
AsmIdtVectorEnd:

HookAfterStubHeaderBegin:
    push    strict dword 0      ; 0 will be fixed
VectorNum:
    push    rax
    mov     rax, strict qword 0 ;     mov     rax, HookAfterStubHeaderEnd
JmpAbsoluteAddress:
    jmp     rax
HookAfterStubHeaderEnd:
    mov     rax, rsp
    and     sp,  0xfff0        ; make sure 16-byte aligned for exception context
    sub     rsp, 0x18           ; reserve room for filling exception data later
    push    rcx
    mov     rcx, [rax + 8]
    bt      [ASM_PFX(mErrorCodeFlag)], ecx
    jnc     .0
    push    qword [rsp]             ; push additional rcx to make stack alignment
.0:
    xchg    rcx, [rsp]        ; restore rcx, save Exception Number in stack
    push    qword [rax]             ; push rax into stack to keep code consistence

;---------------------------------------;
; CommonInterruptEntry                  ;
;---------------------------------------;
; The follow algorithm is used for the common interrupt routine for IDT.
; Entry from each interrupt with a push eax and eax=interrupt number
; Stack frame would be as follows as specified in IA32 manuals:
;
; +---------------------+ <-- 16-byte aligned ensured by processor
; +    Old SS           +
; +---------------------+
; +    Old RSP          +
; +---------------------+
; +    RFlags           +
; +---------------------+
; +    CS               +
; +---------------------+
; +    RIP              +
; +---------------------+
; +    Error Code(*)    +
; +---------------------+
; +    Vector Number    +
; +---------------------+
; +    Old RAX          +
; +---------------------+ <-- RSP, 16-byte aligned
; The follow algorithm is used for the common interrupt routine.
global ASM_PFX(CommonInterruptEntry)
ASM_PFX(CommonInterruptEntry):
    cli
    pop     rax
    ;
    ; All interrupt handlers are invoked through interrupt gates, so
    ; IF flag automatically cleared at the entry point
    ;
    xchg    rcx, [rsp]      ; Save rcx into stack and save vector number into rcx
    and     rcx, 0xFF
    cmp     ecx, 32         ; Intel reserved vector for exceptions?
    jae     NoErrorCode
    bt      [ASM_PFX(mErrorCodeFlag)], ecx
    jc      HasErrorCode

NoErrorCode:

    ;
    ; Push a dummy error code on the stack
    ; to maintain coherent stack map
    ;
    push    qword [rsp]
    mov     qword [rsp + 8], 0
HasErrorCode:
; IDT and FRED handler will jump to here with following stack layout

    ; CL = Vector Number
    ; CH = 0: IDT, 1: FRED
    ; Stack:
    ; +---------------------+
    ; +    Old SS           +
    ; +---------------------+
    ; +    Old RSP          +
    ; +---------------------+
    ; +    RFlags           +
    ; +---------------------+
    ; +    CS               +
    ; +---------------------+
    ; +    RIP              +
    ; +---------------------+
    ; +    Error Code       + <-- Every exception stack contains Error Code Now
    ; +---------------------+
    ; +    Old RCX          +
    ; +---------------------+ <-- RSP

    push    rbp
    mov     rbp, rsp
    push    0             ; clear EXCEPTION_HANDLER_CONTEXT.OldIdtHandler
    push    0             ; clear EXCEPTION_HANDLER_CONTEXT.ExceptionDataFlag


    ; FRED pushes 8 QWORDs in stack while IDT pushes 5 QWORDs.
    ; Push another padding QWORD for FRED so that RSP is 16-byte aligned
    ; when "push r15" is executed for both cases.
    test ch, ch
    jz .StackAligned16
    push 0

    ;
    ; Since here the stack pointer is 16-byte aligned, so
    ; EFI_FX_SAVE_STATE_X64 of EFI_SYSTEM_CONTEXT_x64
    ; is 16-byte aligned
    ;
.StackAligned16:
    ;
    ; Stack:
    ; +---------------------+
    ; +    Old SS           +
    ; +---------------------+
    ; +    Old RSP          +
    ; +---------------------+
    ; +    RFlags           +
    ; +---------------------+
    ; +    CS               +
    ; +---------------------+
    ; +    RIP              +
    ; +---------------------+
    ; +    Error Code       +
    ; +---------------------+
    ; + RCX / Vector Number +
    ; +---------------------+
    ; +    Old RBP          +
    ; +---------------------+ <-- RBP, 16-byte aligned
    ; +   0 (OldIdtHandler) +
    ; +---------------------+
    ; +0 (ExceptionDataFlag)+
    ; +---------------------+ <-- RSP, 16-byte aligned, IDT
    ; +  padding for FRED   +
    ; +---------------------+ <-- RSP, 16-byte aligned, FRED

;; UINT64  Rdi, Rsi, Rbp, Rsp, Rbx, Rdx, Rcx, Rax;
;; UINT64  R8, R9, R10, R11, R12, R13, R14, R15;
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rax
    push qword [rbp + 8]   ; RCX
    push rdx
    push rbx
    push qword [rbp + 48]  ; RSP
    push qword [rbp]       ; RBP
    push rsi
    push rdi

;; UINT64  Gs, Fs, Es, Ds, Cs, Ss;  insure high 16 bits of each is zero
    movzx   rax, word [rbp + 56]
    push    rax                      ; for ss
    movzx   rax, word [rbp + 32]
    push    rax                      ; for cs
    mov     rax, ds
    push    rax
    mov     rax, es
    push    rax
    mov     rax, fs
    push    rax
    mov     rax, gs
    push    rax

    mov     [rbp + 8], rcx               ; save vector number

;; UINT64  Rip;
    push    qword [rbp + 24]

;; UINT64  Gdtr[2], Idtr[2];
    xor     rax, rax
    push    rax
    push    rax
    cmp     byte [rbp + 8 + 1], 1  ; check if FRED
    je      SkipSidt
    sidt    [rsp]
    mov     bx, word [rsp]
    mov     rax, qword [rsp + 2]
    mov     qword [rsp], rax
    mov     word [rsp + 8], bx
SkipSidt:
    xor     rax, rax
    push    rax
    push    rax
    sgdt    [rsp]
    mov     bx, word [rsp]
    mov     rax, qword [rsp + 2]
    mov     qword [rsp], rax
    mov     word [rsp + 8], bx

;; UINT64  Ldtr, Tr;
    xor     rax, rax
    str     ax
    push    rax
    sldt    ax
    push    rax

;; UINT64  RFlags;
    push    qword [rbp + 40]

;; UINT64  Cr0, Cr1, Cr2, Cr3, Cr4, Cr8;
    mov     rax, cr8
    push    rax
    mov     rax, cr4
    or      rax, 0x208
    mov     cr4, rax
    push    rax
    mov     rax, cr3
    push    rax
    mov     rax, cr2
    push    rax
    xor     rax, rax
    push    rax
    mov     rax, cr0
    push    rax

;; UINT64  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
    cmp     qword [rbp + 8], VC_EXCEPTION
    je      VcDebugRegs          ; For SEV-ES (#VC) Debug registers ignored

    mov     rax, dr7
    push    rax
    mov     rax, dr6
    push    rax
    mov     rax, dr3
    push    rax
    mov     rax, dr2
    push    rax
    mov     rax, dr1
    push    rax
    mov     rax, dr0
    push    rax
    jmp     DrFinish

VcDebugRegs:
;; UINT64  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7 are skipped for #VC to avoid exception recursion
    xor     rax, rax
    push    rax
    push    rax
    push    rax
    push    rax
    push    rax
    push    rax

DrFinish:
;; FX_SAVE_STATE_X64 FxSaveState;
    sub rsp, 512
    mov rdi, rsp
    fxsave [rdi]

;; UEFI calling convention for x64 requires that Direction flag in EFLAGs is clear
    cld

;; UINT32  ExceptionData;
    push    qword [rbp + 16]

;; Prepare parameter and call
    mov     rcx, [rbp + 8]
    and     rcx, 0xff
    mov     rdx, rsp
    ;
    ; Per X64 calling convention, allocate maximum parameter stack space
    ; and make sure RSP is 16-byte aligned
    ;
    sub     rsp, 4 * 8 + 8
    call    ASM_PFX(CommonExceptionHandler)
    add     rsp, 4 * 8 + 8

    ; The follow algorithm is used for clear shadow stack token busy bit.
    ; The comment is based on the sample shadow stack.
    ; Shadow stack is 32 bytes aligned.
    ; The sample shadow stack layout :
    ; Address | Context
    ;         +-------------------------+
    ;  0xFB8  |   FREE                  | It is 0xFC0|0x02|(LMA & CS.L), after SAVEPREVSSP.
    ;         +-------------------------+
    ;  0xFC0  |  Prev SSP               |
    ;         +-------------------------+
    ;  0xFC8  |   RIP                   |
    ;         +-------------------------+
    ;  0xFD0  |   CS                    |
    ;         +-------------------------+
    ;  0xFD8  |  0xFD8 | BUSY           | BUSY flag cleared after CLRSSBSY
    ;         +-------------------------+
    ;  0xFE0  | 0xFC0|0x02|(LMA & CS.L) |
    ;         +-------------------------+
    ; Instructions for Intel Control Flow Enforcement Technology (CET) are supported since NASM version 2.15.01.
    cmp     qword [ASM_PFX(mDoFarReturnFlag)], 0
    jz      CetDone
    mov     rax, cr4
    and     rax, 0x800000       ; Check if CET is enabled
    jz      CetDone
    sub     rsp, 0x10
    sidt    [rsp]
    mov     rcx, qword [rsp + IA32_DESCRIPTOR.Base]; Get IDT base address
    add     rsp, 0x10
    mov     rax, qword [rbp + 8]; Get exception number
    sal     rax, 0x04           ; Get IDT offset
    add     rax, rcx            ; Get IDT gate descriptor address
    mov     al, byte [rax + IA32_IDT_GATE_DESCRIPTOR.Reserved_0]
    and     rax, 0x01           ; Check IST field
    jz      CetDone
                                ; SSP should be 0xFC0 at this point
    mov     rax, 0x04           ; advance past cs:lip:prevssp;supervisor shadow stack token
    incsspq rax                 ; After this SSP should be 0xFE0
    saveprevssp                 ; now the shadow stack restore token will be created at 0xFB8
    rdsspq  rax                 ; Read new SSP, SSP should be 0xFE8
    sub     rax, 0x10
    clrssbsy [rax]              ; Clear token at 0xFD8, SSP should be 0 after this
    sub     rax, 0x20
    rstorssp [rax]              ; Restore to token at 0xFB8, new SSP will be 0xFB8
    mov     rax, 0x01           ; Pop off the new save token created
    incsspq rax                 ; SSP should be 0xFC0 now
CetDone:

    cli
;; UINT64  ExceptionData;
    add     rsp, 8

;; FX_SAVE_STATE_X64 FxSaveState;

    mov rsi, rsp
    fxrstor [rsi]
    add rsp, 512

;; UINT64  Dr0, Dr1, Dr2, Dr3, Dr6, Dr7;
;; Skip restoration of DRx registers to support in-circuit emualators
;; or debuggers set breakpoint in interrupt/exception context
    add     rsp, 8 * 6

;; UINT64  Cr0, Cr1, Cr2, Cr3, Cr4, Cr8;
    pop     rax
    mov     cr0, rax
    add     rsp, 8   ; not for Cr1
    pop     rax
    mov     cr2, rax
    pop     rax
    mov     cr3, rax
    pop     rax
    mov     cr4, rax
    pop     rax
    mov     cr8, rax

;; UINT64  RFlags;
    pop     qword [rbp + 40]

;; UINT64  Ldtr, Tr;
;; UINT64  Gdtr[2], Idtr[2];
;; Best not let anyone mess with these particular registers...
    add     rsp, 48

;; UINT64  Rip;
    pop     qword [rbp + 24]

;; UINT64  Gs, Fs, Es, Ds, Cs, Ss;
    pop     rax
    ; mov     gs, rax ; not for gs
    pop     rax
    ; mov     fs, rax ; not for fs
    ; (X64 will not use fs and gs, so we do not restore it)
    pop     rax
    mov     es, rax
    pop     rax
    mov     ds, rax
    pop     qword [rbp + 32]  ; for cs
    pop     qword [rbp + 56]  ; for ss

;; UINT64  Rdi, Rsi, Rbp, Rsp, Rbx, Rdx, Rcx, Rax;
;; UINT64  R8, R9, R10, R11, R12, R13, R14, R15;
    pop     rdi
    pop     rsi
    add     rsp, 8               ; not for rbp
    pop     qword [rbp + 48] ; for rsp
    pop     rbx
    pop     rdx
    pop     rcx
    pop     rax
    pop     r8
    pop     r9
    pop     r10
    pop     r11
    pop     r12
    pop     r13
    pop     r14
    pop     r15

    mov     rsp, rbp
    pop     rbp
    add     rsp, 16

    ;
    ; Stack:
    ; +---------------------+
    ; +    Old SS           +
    ; +---------------------+
    ; +    Old RSP          +
    ; +---------------------+
    ; +    RFlags           +
    ; +---------------------+
    ; +    CS               +
    ; +---------------------+
    ; +    RIP              +
    ; +---------------------+ <-- RSP, 16-byte aligned
    ; +    Error Code       +
    ; +---------------------+
    ; + RCX / Vector Number +
    ; +---------------------+ <-- RSP - 16
    ; +    RBP              +
    ; +---------------------+
    ; +   0 (OldIdtHandler) +
    ; +---------------------+ <-- RSP - 32
    ; +0 (ExceptionDataFlag)+
    ; +---------------------+ <-- RSP - 40
    cmp     qword [rsp - 32], 0  ; check EXCEPTION_HANDLER_CONTEXT.OldIdtHandler
    jz      DoReturn
    cmp     qword [rsp - 40], 1  ; check EXCEPTION_HANDLER_CONTEXT.ExceptionDataFlag
    jz      ErrorCode
    jmp     qword [rsp - 32]
ErrorCode:
    sub     rsp, 8
    jmp     qword [rsp - 24]

DoReturn:
    cmp     byte [rsp - 16 + 1], 1  ; check if FRED
    je      DoEret
    cmp     qword [ASM_PFX(mDoFarReturnFlag)], 0   ; Check if need to do far return instead of IRET
    jz      DoIret
    push    rax
    mov     rax, rsp          ; save old RSP to rax
    mov     rsp, [rsp + 0x20]
    push    qword [rax + 0x10]       ; save CS in new location
    push    qword [rax + 0x8]        ; save EIP in new location
    push    qword [rax + 0x18]       ; save EFLAGS in new location
    mov     rax, [rax]        ; restore rax
    popfq                     ; restore EFLAGS
    retfq
DoIret:
    iretq
DoEret:
    ERETS

;-------------------------------------------------------------------------------------
;  GetTemplateAddressMap (&AddressMap);
;-------------------------------------------------------------------------------------
; comments here for definition of address map
global ASM_PFX(AsmGetTemplateAddressMap)
ASM_PFX(AsmGetTemplateAddressMap):
    lea     rax, [AsmIdtVectorBegin]
    mov     qword [rcx], rax
    mov     qword [rcx + 0x8],  (AsmIdtVectorEnd - AsmIdtVectorBegin) / 256
    lea     rax, [HookAfterStubHeaderBegin]
    mov     qword [rcx + 0x10], rax

; Fix up CommonInterruptEntry address
    lea    rax, [ASM_PFX(CommonInterruptEntry)]
    lea    rcx, [AsmIdtVectorBegin]
%rep  256
    mov    qword [rcx + (JmpAbsoluteAddress - 8 - HookAfterStubHeaderBegin)], rax
    add    rcx, (AsmIdtVectorEnd - AsmIdtVectorBegin) / 256
%endrep
; Fix up HookAfterStubHeaderEnd
    lea    rax, [HookAfterStubHeaderEnd]
    lea    rcx, [JmpAbsoluteAddress]
    mov    qword [rcx - 8], rax

    ret

;-------------------------------------------------------------------------------------
;  AsmVectorNumFixup (*NewVectorAddr, VectorNum, *OldVectorAddr);
;-------------------------------------------------------------------------------------
global ASM_PFX(AsmVectorNumFixup)
ASM_PFX(AsmVectorNumFixup):
    mov     rax, rdx
    mov     [rcx + (VectorNum - 4 - HookAfterStubHeaderBegin)], al
    ret

