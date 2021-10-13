;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; Use of this source code is subject to the terms of the Microsoft shared
; source or premium shared source license agreement under which you licensed
; this source code. If you did not accept the terms of the license agreement,
; you are not authorized to use this source code. For the terms of the license,
; please see the license agreement between you and Microsoft or, if applicable,
; see the SOURCE.RTF on your install media or the root of your tools installation.
; THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
;
        title  "Interlocked Support"
;++
;
;
; Module Name:
;
;    slist.asm
;
; Abstract:
;
;    This module implements functions to support interlocked S-List
;    operations.
;
; Environment:
;
;    Any mode.
;
; Revision History:
;
;--
.386
        .xlist
include x86callconv.inc                    ; calling convention macros
        .list

    ;EXTRNP  _RtlBackoff,1
    
_TEXT$00   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME  DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

        page , 132
        subttl  "Interlocked Flush Sequenced List"
;++
;
; PSLIST_ENTRY
; FASTCALL
; RtlpInterlockedFlushSList (
;    IN PSLIST_ENTRY ListHead
;    )
;
; Routine Description:
;
;    This function removes the entire list from a sequenced singly
;    linked list so that access to the list is synchronized in an MP system.
;    If there are no entries in the list, then a value of NULL is returned.
;    Otherwise, the address of the entry at the top of the list is removed
;    and returned as the function value and the list header is set to point
;    to NULL.
;
; Arguments:
;
;    (ecx) = ListHead - Supplies a pointer to the sequenced listhead from
;         which the list is to be flushed.
;
; Return Value:
;
;    The address of the entire current list, or NULL if the list is
;    empty.
;
;--

cPublicFastCall RtlpInterlockedFlushSList, 1

cPublicFpo 0,1

;
; Save nonvolatile registers and read the listhead sequence nunmber followed
; by the listhead next link.
;
; N.B. These two dwords MUST be read in exactly this order.
;

        push    ebx                     ; save nonvolatile registers
        push    ebp                     ;
        xor     ebx, ebx                ; zero out new pointer
        mov     ebp, ecx                ; save listhead address
        mov     edx, [ebp] + 4          ; get current sequence number and depth
        mov     ecx, edx                ; copy sequence number and depth
        mov     eax, [ebp] + 0          ; get current next link

;
; N.B. The following code is the retry code should the compare
;      part of the compare exchange operation fail
;
; If the list is empty, then there is nothing that can be removed.
;

        mov     cx, 0                   ; set the depth
        add     ecx, 00010000h          ; increment the sequence number 
Efls10: or      eax, eax                ; check if list is empty
        jz      short Efls20            ; if z set, list is empty

.586
   lock cmpxchg8b qword ptr [ebp]       ; compare and exchange
.386

        jnz     short Efls10            ; if z clear, exchange failed

;
; Restore nonvolatile registers and return result.
;

cPublicFpo 0,0

Efls20: pop     ebp                     ; restore nonvolatile registers
        pop     ebx                     ;

        fstRET    RtlpInterlockedFlushSList

fstENDP RtlpInterlockedFlushSList

        page , 132
        subttl  "Interlocked Pop Entry Sequenced List"

;++
;
; PSLIST_ENTRY
; FASTCALL
; RtlpInterlockedPopEntrySList (
;    IN PSLIST_HEADER ListHead
;    )
;
; Routine Description:
;
;    This function removes an entry from the front of a sequenced singly
;    linked list so that access to the list is synchronized in an MP system.
;    If there are no entries in the list, then a value of NULL is returned.
;    Otherwise, the address of the entry that is removed is returned as the
;    function value.
;
; Arguments:
;
;    (ecx) = ListHead - Supplies a pointer to the sequenced listhead from
;         which an entry is to be removed.
;
; Return Value:
;
;    The address of the entry removed from the list, or NULL if the list is
;    empty.
;
;--

cPublicFastCall RtlpInterlockedPopEntrySList, 1

EPopStackAllocLength equ 4

cPublicFpo 0,2(EPopStackAllocLength)

        push    ebx                     ; save nonvolatile registers
        push    ebp                     ;
        mov     ebp, ecx                ; save listhead address
        sub     esp, EPopStackAllocLength ; allocate
;
; N.B. The following code is the continuation address should a fault
;      occur in the rare case described below.
;
; N.B. An in-flight SLIST pop between here and the end point below is also
;      rolled back in the case of an interrupt to avoid sequence wrapping due
;      to scheduling.
;
; N.B. Instructions after the cmpxchg which decide to loop in the case of a
;      failed cmpxchg may have been delayed for an arbitrary length of time
;      (since such instructions are outside the purview of the automatic SLIST
;      rollback mechanism).
;
;      Therefore the SLIST header must be re-fetched inside the protected region
;      to ensure the sequence numbers used are not stale, so as to avoid SLIST
;      corruption due to sequence number wrapping (the A-B-A problem). This
;      applies equally whether the protected region is entered via a resume from
;      the rollback mechanism or via a retry loop after a failed cmpxchg.
;

        public  ExpInterlockedPopEntrySListResume
ExpInterlockedPopEntrySListResume:      



;
; Read/replace the listhead sequence number followed by reading the head pointer.
;
; N.B. These two dwords MUST be read in exactly this order.
;

Epop10:
        mov     edx, [ebp] + 4          ; get SLIST sequence number and depth
        mov     ecx, edx                ; copy sequence number and depth to ecx

;
; xchg is used to replace the sequence number and to act as a memory barrier
; to ensure read of next pointer happens after the sequence number is
; replaced to avoid the A-B-A issue
;
        xchg    dword ptr [esp], eax    ; memory barrier (exchange random memory)

Epop15: mov     eax, [ebp] + 0          ; get current next link

;
; If the list is empty, then there is nothing that can be removed.
;

        or      eax, eax                ; check if list is empty
        jz      short Epop20            ; if z set, list is empty
        add     ecx, 0000FFFFh          ; increment sequence number (+0x10000), decrement depth (+0xFFFFFFFF)

;
; N.B. It is possible for the following instruction to fault in the rare
;      case where the first entry in the list is allocated on another
;      processor and freed between the time the free pointer is read above
;      and the following instruction. When this happens, the access fault
;      code continues execution at the resume point above. This results in the
;      entire operation being retried without any side-effects (since before
;      ExpInterlockedPopEntrySListEnd no globally observable memory operations
;      are performed).
;

        public  ExpInterlockedPopEntrySListFault
ExpInterlockedPopEntrySListFault:       ;

        mov     ebx, [eax]              ; get address of successor entry

        public  ExpInterlockedPopEntrySListEnd
ExpInterlockedPopEntrySListEnd:         ;

.586
   lock cmpxchg8b qword ptr [ebp]       ; compare and exchange
.386

        jnz     short Epop10            ; if not z, exchange succeeded

;
; Restore nonvolatile registers and return result.
;

cPublicFpo 0,0

Epop20: add     esp, 4                  ; restore stack pointer
        pop     ebp                     ; restore nonvolatile registers
        pop     ebx                     ;

        fstRET    RtlpInterlockedPopEntrySList

fstENDP RtlpInterlockedPopEntrySList

        page , 132
        subttl  "Interlocked Push Entry Sequenced List"
;++
;
; PSLIST_ENTRY
; FASTCALL
; RtlpInterlockedPushEntrySList (
;    IN PSLIST_HEADER ListHead,
;    IN PSLIST_ENTRY ListEntry
;    )
;
; Routine Description:
;
;    This function inserts an entry at the head of a sequenced singly linked
;    list so that access to the list is synchronized in an MP system.
;
; Arguments:
;
;    (ecx) ListHead - Supplies a pointer to the sequenced listhead into which
;          an entry is to be inserted.
;
;    (edx) ListEntry - Supplies a pointer to the entry to be inserted at the
;          head of the list.
;
; Return Value:
;
;    Previous contents of ListHead.  NULL implies list went from empty
;       to not empty.
;
;--

cPublicFastCall RtlpInterlockedPushEntrySList, 2

cPublicFpo 0,2

;
; Save nonvolatile registers and read the listhead sequence number followed
; by the listhead next link.
;
; N.B. These two dwords MUST be read in exactly this order.
;

        push    ebx                     ; save nonvolatile registers
        push    ebp                     ;
        mov     ebp, ecx                ; save listhead address
        mov     ebx, edx                ; save list entry address
        mov     edx, [ebp] + 4          ; get current sequence number and depth
        mov     eax, [ebp] + 0          ; get current next link
Epsh10:

ifdef DBG

        cmp     eax, ebx                ; check if pushing the first element in again
        jne     short Epsh20            ; if ne, pushed entry is not the first element
        int     3                       ; break into debugger
Epsh20:

endif

        mov     [ebx], eax              ; set next link in new first entry
        mov     ecx, edx                ; copy the sequence number and depth
        add     ecx, 00010001h          ; increment the depth and sequence number

.586
   lock cmpxchg8b qword ptr [ebp]       ; compare and exchange
.386

        jnz     short Epsh10            ; if z clear, exchange failed

;
; Restore nonvolatile registers and return result.
;

cPublicFpo 0,0

        pop     ebp                     ; restore nonvolatile registers
        pop     ebx                     ;

        fstRET  RtlpInterlockedPushEntrySList

fstENDP RtlpInterlockedPushEntrySList

_TEXT$00   ends
        end
