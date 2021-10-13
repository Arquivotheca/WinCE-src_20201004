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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
