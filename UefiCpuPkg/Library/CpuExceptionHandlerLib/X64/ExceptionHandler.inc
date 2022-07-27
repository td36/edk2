;------------------------------------------------------------------------------ ;
; Copyright (c) 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;------------------------------------------------------------------------------
%include "Nasm.inc"

struc FRED_STACK_CONTEXT
  .ErrorCode:     CTYPE_UINT64 1
  .OldRip:        CTYPE_UINT64 1
  .OldCs:         CTYPE_UINT64 1
  .OldRflags:     CTYPE_UINT64 1
  .OldRsp:        CTYPE_UINT64 1
  .OldSs:         CTYPE_UINT64 1
  .EventData:     CTYPE_UINT64 1
  .MustBeZero:    CTYPE_UINT64 1
endstruc

;
; Equivalent NASM structure of IA32_DESCRIPTOR
;
struc IA32_DESCRIPTOR
  .Limit:                         CTYPE_UINT16 1
  .Base:                          CTYPE_UINTN  1
endstruc

;
; Equivalent NASM structure of IA32_IDT_GATE_DESCRIPTOR
;
struc IA32_IDT_GATE_DESCRIPTOR
  .OffsetLow:                     CTYPE_UINT16 1
  .Selector:                      CTYPE_UINT16 1
  .Reserved_0:                    CTYPE_UINT8 1
  .GateType:                      CTYPE_UINT8 1
  .OffsetHigh:                    CTYPE_UINT16 1
  .OffsetUpper:                   CTYPE_UINT32 1
  .Reserved_1:                    CTYPE_UINT32 1
endstruc

%macro ERETS       0
    DB 0xF2, 0x0F, 0x01, 0xCA
%endmacro
