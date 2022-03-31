;------------------------------------------------------------------------------
; @file
; Emits Page Tables for 1:1 mapping of the addresses 4GB-8KB ~ 4GB-4KB and 3GB ~ 4GB
;
; Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
;------------------------------------------------------------------------------

BITS    64

%define INIT_PAGE_TABLE_PDP(offset) (ADDR_OF(RecursiveInitPageTable) + (offset) + \
                                    PAGE_PDP_ATTR)

ALIGN 16

;
; Recursive Initial Page Table
;
; This is the fixed location(0xffffe000) where Royal processor will fetch
; the initial page table when power on.
;
RecursiveInitPageTable:
    ;
    ; PML4 entry 0 (4GB-8KB ~ 4GB-4KB) - Valid
    ; PDPT entry 0 (0 ~ 1GB) - Invalid
    ;
    DQ    INIT_PAGE_TABLE_PDP(0)
    ;
    ; PDPT entry 1 (1GB ~ 2GB) - Invalid
    ;
    DQ    0
    ;
    ; PDPT entry 2 (2GB ~ 3GB) - Invalid
    ;
    DQ    0
    ;
    ; PDPT entry 3 (3GB ~ 4GB) - Valid
    ; "ResetVector"(0xfffffff0) is located within this address range
    ;
    DQ    PDP_1G(3)

    TIMES 0x1000-($ - RecursiveInitPageTable) DB 0

EndOfInitialPageTables:
