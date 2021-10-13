//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:

linklist.h

Abstract:

Macros for linked-list manipulation.

Notes:


--*/


#ifndef _LINKLIST_H_
#define _LINKLIST_H_

#ifdef __cplusplus
extern "C" {
#endif


/*NOINC*/

#if !defined(WIN32)

typedef struct _LIST_ENTRY {
    struct _LIST_ENTRY FAR * Flink;
    struct _LIST_ENTRY FAR * Blink;
} LIST_ENTRY;
typedef LIST_ENTRY FAR * PLIST_ENTRY;

#endif  // !WIN32

//
// Linked List Manipulation Functions - from NDIS.H
//

//
// Calculate the address of the base of the structure given its type, and an
// address of a field within the structure. - from NDIS.H
//
#ifndef CONTAINING_RECORD
#define CONTAINING_RECORD(address, type, field) ((type *)( \
                          (LPBYTE)(address) - \
                          (LPBYTE)(&((type *)0)->field)))
#endif  // CONTAINING_RECORD

//
//  Doubly-linked list manipulation routines.  Implemented as macros
//
#define InitializeListHead(ListHead) \
    ((ListHead)->Flink = (ListHead)->Blink = (ListHead) )

#define IsListEmpty(ListHead) \
    (( ((ListHead)->Flink == (ListHead)) ? TRUE : FALSE ) )


#ifdef DEBUG
_inline
void
CheckListEntry(PLIST_ENTRY Entry)
{
    //
    //  check the PLIST_ENTRY pointer
    //  if the pointer is valid then
    //      check Flink
    //      check Blink
    //
    ASSERT( (LIST_ENTRY*) Entry        != (LIST_ENTRY*) 0xcccccccc );
    ASSERT( (LIST_ENTRY*) Entry->Flink != (LIST_ENTRY*) 0xcccccccc );
    ASSERT( (LIST_ENTRY*) Entry->Blink != (LIST_ENTRY*) 0xcccccccc );
}
#define ASSERT_LIST_ENTRY CheckListEntry
#else
#define ASSERT_LIST_ENTRY
#endif


#ifdef DEBUG
__inline
void
NullListEntry(PLIST_ENTRY Entry)
{
    //
    //  NULL the Flink/Blink values
    //
    Entry->Flink = NULL;
    Entry->Blink = NULL;
}
#define NULL_LIST_ENTRY NullListEntry
#else
#define NULL_LIST_ENTRY
#endif


__inline
PLIST_ENTRY
RemoveHeadList(PLIST_ENTRY ListHead)
{
    //
    //  returns a pointer to the Flink element
    //  ListHead->Flink is the element being removed
    //  ListHead is supposed to be a sentinel
    //  removes the ListHead->Flink element from the doubly linked list
    //
    PLIST_ENTRY FirstEntry = ListHead->Flink;
    PLIST_ENTRY SecondEntry = FirstEntry->Flink;

    ASSERT_LIST_ENTRY( ListHead );
    ASSERT_LIST_ENTRY( FirstEntry );
    ASSERT_LIST_ENTRY( SecondEntry );

    SecondEntry->Blink = ListHead;
    ListHead->Flink = SecondEntry;

    ASSERT_LIST_ENTRY( ListHead );
    ASSERT_LIST_ENTRY( FirstEntry );
    ASSERT_LIST_ENTRY( SecondEntry );

    //
    //  FirstEntry pointers should be NULL
    //
    NULL_LIST_ENTRY( FirstEntry );
    return FirstEntry;
}


//
//  RemoveEntryList:
//
//      - removes Entry from the doubly linked list
//      - Entry Flink/Blink values are NULL in a debug image
//
__inline
void
RemoveEntryList(PLIST_ENTRY Entry)
{
    PLIST_ENTRY _EX_Entry = Entry;

    ASSERT_LIST_ENTRY( Entry );

    _EX_Entry->Blink->Flink = _EX_Entry->Flink;
    _EX_Entry->Flink->Blink = _EX_Entry->Blink;

    ASSERT_LIST_ENTRY( Entry );

    //
    //  Entry pointers should be NULL
    //
    NULL_LIST_ENTRY( Entry );
}



//
//  RemoveTailList:
//
//      - remove the Entry at the end of the doubly linked list
//      - return the Entry being removed
//      - ListHead pointer stays the same
//      - ListHead Blink pointer changes
//
_inline
PLIST_ENTRY
RemoveTailList(PLIST_ENTRY ListHead)
{
    PLIST_ENTRY _Tail_Entry = ListHead->Blink;
    RemoveEntryList(_Tail_Entry);
    return _Tail_Entry;
}


//
//  InsertTailList:
//
//      - insert Entry at the end of the doubly linked list
//      - Entry/ListHead pointer values do not change
//      - ListHead Blink values do change
//      - original ListHead->Blink->Flink value changes
//      - Entry Flink/Blink values change
//
__inline
void
InsertTailList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
{
    PLIST_ENTRY _EX_ListHead = ListHead;
    PLIST_ENTRY _EX_Blink = _EX_ListHead->Blink;

    ASSERT_LIST_ENTRY( Entry );
    ASSERT_LIST_ENTRY( _EX_ListHead );
    ASSERT_LIST_ENTRY( _EX_Blink );

    Entry->Flink = _EX_ListHead;
    Entry->Blink = _EX_Blink;
    _EX_Blink->Flink = Entry;
    _EX_ListHead->Blink = Entry;

    ASSERT_LIST_ENTRY( Entry );
    ASSERT_LIST_ENTRY( _EX_Blink );
    ASSERT_LIST_ENTRY( _EX_ListHead );
}


//
//  InsertHeadList:
//
//      - insert Entry at the head of the doubly linked list
//      - Entry/ListHead pointer values do not change
//      - Entry/ListHead Flink/Blink values do change
//
__inline
void
InsertHeadList(PLIST_ENTRY ListHead, PLIST_ENTRY Entry)
{
    PLIST_ENTRY _EX_ListHead = ListHead;
    PLIST_ENTRY _EX_Flink = _EX_ListHead->Flink;

    ASSERT_LIST_ENTRY( Entry );
    ASSERT_LIST_ENTRY( _EX_Flink );
    ASSERT_LIST_ENTRY( _EX_ListHead );

    Entry->Flink = _EX_Flink;
    Entry->Blink = _EX_ListHead;
    _EX_Flink->Blink = Entry;
    _EX_ListHead->Flink = Entry;

    ASSERT_LIST_ENTRY( Entry );
    ASSERT_LIST_ENTRY( _EX_Flink );
    ASSERT_LIST_ENTRY( _EX_ListHead );
}

/*INC*/

#ifdef __cplusplus
}
#endif


#endif  // _LINKLIST_H_
