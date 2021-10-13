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

        include ..\..\..\nk\inc\ksarm.h

        TEXTAREA

;++
;
; PSINGLE_LIST_ENTRY
; RtlInterlockedFlushSList (
;    IN PSINGLE_LIST_ENTRY ListHead
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
;    ListHead (r0) - Supplies a pointer to the sequenced listhead from
;         which the list is to be flushed.
;
; Return Value:
;
;    The address of the removed list, or NULL if the list is
;    empty.
;
;--

        LEAF_ENTRY RtlInterlockedFlushSList

        mov     r12, #0                                 ; r12 contains the new pointer (NULL)
SListFlushRetry
        ldrexd  r2, r3, [r0]                            ; fetch link in r2, depth in r3
        strexd  r1, r12, r12, [r0]                      ; attempt to clear both link and depth
        cmp     r1, #1                                  ; did we fail?
        beq     SListFlushRetry                         ; if so, retry
        mov     r0, r2                                  ; return the original head item

        bx      lr
        LEAF_END RtlInterlockedFlushSList


;++
;
; PVOID
; RtlInterlockedPopEntrySList (
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
;    ListHead (r0) - Supplies a pointer to the sequenced listhead from
;         which an entry is to be removed.
;
; Return Value:
;
;    The address of the entry removed from the list, or NULL if the list is
;    empty.
;
;--

;
; The older interfaces just fall into the new code below
;

        LEAF_ENTRY RtlInterlockedPopEntrySList

SListPopRetry

;
; This is also the resume point in case of a fault below
;
        ALTERNATE_ENTRY ExpInterlockedPopEntrySListResume

        ldrexd  r2, r3, [r0]                            ; fetch link in r2, depth in r3
        cbz     r2, SListPopEmpty                       ; did we get a NULL pointer? if so, exit

;
; N.B. It is possible for the following instruction to fault in the rare
;      case where the first entry in the list is allocated on another
;      processor and free between the time the free pointer is read above
;      and the following instruction. When this happens, the access fault
;      code continues execution by returning to the top of the loop and
;      retrying.
;

        ALTERNATE_ENTRY ExpInterlockedPopEntrySListFault

        ldr     r12, [r2]                               ; get address of successor entry

        ALTERNATE_ENTRY ExpInterlockedPopEntrySListEnd

        subs    r3, r3, #1                              ; decrement depth
        strexd  r1, r12, r3, [r0]                       ; attempt the store
        cmp     r1, #1                                  ; did we fail?
        beq     SListPopRetry                           ; if so, retry

SListPopEmpty
        mov     r0, r2                                  ; return the original head item
        bx      lr

        LEAF_END RtlInterlockedPopEntrySList

;++
;
; PVOID
; RtlInterlockedPushEntrySList (
;    IN PSLIST_HEADER ListHead,
;    IN PVOID ListEntry
;    )
;
; Routine Description:
;
;    This function inserts an entry at the head of a sequenced singly linked
;    list so that access to the list is synchronized in an MP system.
;
; Arguments:
;
;    ListHead (r0) - Supplies a pointer to the sequenced listhead into which
;          an entry is to be inserted.
;
;    ListEntry (r1) - Supplies a pointer to the entry to be inserted at the
;          head of the list.
;
; Return Value:
;
;    Previous contents of ListHead.  NULL implies list went from empty
;       to not empty.
;
;--

;
; The old interfaces just fall into the new code below.
;

        NESTED_ENTRY RtlInterlockedPushEntrySList
        PROLOG_PUSH     {r4-r5}

SListPushRetry
;
; N.B. The ARMARM says that a store within up to 2k of a reservation is
;      sufficient to break the reservation. For this reason, the code 
;      below must set the next link in the new entry before taking a
;      reservation, in case the store is within this 2k window. Hence
;      the reason for an extra ldrd at the start.
;
        ldrd    r2, r3, [r0]                            ; next link in r2, depth in r3
        str     r2, [r1]                                ; set next link in new first entry
        dmb     ish                                     ; ensure this is visible to others

        ldrexd  r4, r5, [r0]                            ; fetch link in r4, depth in r5
        cmp     r2, r4                                  ; do links match?
        bne     SListPushRetry                          ; if not, retry
        cmp     r3, r5                                  ; do depths match?
        bne     SListPushRetry                          ; if not, retry
        adds    r3, r3, #1                              ; increment depth
        strexd  r12, r1, r3, [r0]                       ; ...and attempt the store
        cmp     r12, #1                                 ; did we fail?
        beq     SListPushRetry                          ; if so, retry
        mov     r0, r2                                  ; return previous head
        
        EPILOG_POP  {r4-r5}
        EPILOG_RETURN
        NESTED_END RtlInterlockedPushEntrySList

;++
;
; SINGLE_LIST_ENTRY
; InterlockedPushListSList (
;     IN PSLIST_HEADER ListHead,
;     IN PSINGLE_LIST_ENTRY List,
;     IN PSINGLE_LIST_ENTRY ListEnd,
;     IN ULONG Count
;    )
;
; Routine Description:
;
;    This function will push multiple entries onto an SList at once
;
; Arguments:
;
;     ListHead (r0) - List head to push the list to.
;
;     List (r1) - The list to add to the front of the SList
;
;     ListEnd (r2) - The last element in the chain
;
;     Count - (r3) The number of items in the chain
;
; Return Value:
;
;     PSINGLE_LIST_ENTRY - The old header pointer is returned
;
;--

        NESTED_ENTRY InterlockedPushListSList
        PROLOG_PUSH     {r4-r7}

SListPushListRetry
;
; N.B. The ARMARM says that a store within up to 2k of a reservation is
;      sufficient to break the reservation. For this reason, the code 
;      below must set the next link in the new entry before taking a
;      reservation, in case the store is within this 2k window. Hence
;      the reason for an extra ldrd at the start.
;
        ldrd    r4, r5, [r0]                            ; next link in r4, depth in r5
        str     r4, [r2]                                ; set next link in new last entry
        dmb     ish                                     ; ensure this is visible to others

        ldrexd  r6, r7, [r0]                            ; fetch link in r6, depth in r7
        cmp     r4, r6                                  ; do links match?
        bne     SListPushListRetry                      ; if not, retry
        cmp     r5, r7                                  ; do depths match?
        bne     SListPushListRetry                      ; if not, retry
        add     r5, r5, r3                              ; add count to the depth
        strexd  r12, r1, r5, [r0]                       ; ..and attempt the store
        cmp     r12, #1                                 ; did we fail?
        beq     SListPushListRetry                      ; if so, retry

        mov     r0, r4                                  ; return previous head
        
        EPILOG_POP      {r4-r7}
        EPILOG_RETURN
        NESTED_END InterlockedPushListSList


        END
