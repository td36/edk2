;------------------------------------------------------------------------------
; @file
; Sets the CR3 register for 64-bit paging
;
; Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

BITS    64

;
; Modified:  RAX
;
SetCr3ForPageTables64:

    ;
    ; These pages are built into the ROM image in X64/PageTables.asm
    ;
    mov     rax, ADDR_OF(TopLevelPageDirectory)
    mov     cr3, rax

    OneTimeCallRet SetCr3ForPageTables64

