;------------------------------------------------------------------------------
; @file
; 64-bit initialization code
;
; Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------


BITS    64

;
; @param[out] DI    'BP' to indicate boot-strap processor
;
EarlyBspInit64:
    mov     di, 'BP'
    jmp     short Main64

;
; Modified:  EAX
;
; @param[in]  EAX   Initial value of the EAX register (BIST: Built-in Self Test)
; @param[out] ESP   Initial value of the EAX register (BIST: Built-in Self Test)
;
EarlyInit64:
    ;
    ; ESP -  Initial value of the EAX register (BIST: Built-in Self Test)
    ;
    mov     esp, eax

    debugInitialize

    OneTimeCallRet EarlyInit64

