//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    refcnt.h

Abstract:


Revision History:

--*/

#ifndef REFCNT_H
#define REFCNT_H

//
// Reference Count Control Block
//
//  Elements:
//
//  - Count:            number of outstanding references
//  - Instance:      user supplied context 
//  - UserDeleteFunc:   user supplied delete function
//

#define TAG_CNT 8
#define REF_SIG 0x7841eeee

typedef struct
{
    ULONG   Tag;
    ULONG   Count;
} REF_TAG;    

typedef struct  reference_count_control
{
    UINT       	Count;
    PVOID       Instance;
    VOID        (*DeleteHandler)( PVOID );
#if DBG    
    int         Sig;
    REF_TAG     Tags[TAG_CNT];
#endif     
}
REF_CNT, *PREF_CNT;


VOID    
ReferenceInit 
( 
    IN PREF_CNT pRefCnt, 
    PVOID       InstanceHandle, 
    VOID        (*DeleteHandler)( PVOID ) 
);

VOID
ReferenceAdd
(
    IN 	PREF_CNT  pRefCnt
);

VOID
ReferenceAddCount
(
    IN 	PREF_CNT    pRefCnt,
	IN	UINT	    Count
);

PVOID
ReferenceRemove 
(
    IN PREF_CNT  pRefCnt
);

VOID
ReferenceApiTest
( 
	VOID 
);

#if DBG

VOID ReferenceAddDbg(PREF_CNT pRefCnt, ULONG Tag);
VOID ReferenceRemoveDbg(PREF_CNT pRefCnt, ULONG Tag);

#define REFADD(Rc, Tag)     ReferenceAddDbg(Rc, Tag)
#define REFDEL(Rc, Tag)     ReferenceRemoveDbg(Rc, Tag)

#else

#define REFADD(Rc, Tag)  ReferenceAdd(Rc);
#define REFDEL(Rc, Tag)  ReferenceRemove(Rc);

#endif

#endif
