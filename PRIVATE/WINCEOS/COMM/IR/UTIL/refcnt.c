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

    refcnt.c

Abstract:

    This module exports Reference Counting support functions. By 
    including a Reference Count Control Block (REF_CNT) in a
    dynamic type, and using this API, a Reference scheme can be
    implemented for that type.


Revision History:

--*/

//
// Include Files
//

#include "irda.h"

VOID
ReferenceInit 
(
    IN PREF_CNT pRefCnt,
    PVOID       InstanceHandle,
    VOID        (*DeleteHandler)( PVOID )
)

/*++

Routine Description:

    ReferenceInit initializes and adds one reference to the
    supplied Reference Control Block. If provided, an instance
    handle and delete handler are saved for use by the ReferenceRemove 
    function when all references to the instance are removed.

Arguments:

    pRefCnt - pointer to uninitialized Reference Control Block
    InstanceHandle - handle to the managed instance.
    DeleteHandler - pointer to delete function, NULL is OK.

Return Value:

    The function's value is VOID.

--*/

{
    DEBUGMSG( DBG_REF,(TEXT("ReferenceInit( 0x%x, 0x%x, 0x%x )\n"), 
        pRefCnt, InstanceHandle, DeleteHandler ));

    if (pRefCnt) {
        // Set the reference to 1 and save the instance 
        // handle and the delete handler.

        pRefCnt->Count         = 0;
        pRefCnt->Instance      = InstanceHandle;
        pRefCnt->DeleteHandler = DeleteHandler;

#if DBG
        pRefCnt->Sig = REF_SIG;
        RtlZeroMemory(pRefCnt->Tags, sizeof(REF_TAG) * TAG_CNT);
        pRefCnt->Tags[0].Tag = 'LTOT';
#endif
    } else {
        ASSERT(0);
    }

}

VOID
ReferenceAdd
(
    IN  PREF_CNT pRefCnt
)

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    if (pRefCnt) {
        InterlockedIncrement(&pRefCnt->Count);
    } else {
        ASSERT(0);
    }

//    DEBUGMSG( DBG_REF,( "R+%d\n", pRefCnt->Count ));    
}

#ifndef UNDER_CE
// CE doesn't support this since we don't have InterlockedExchangeAdd.
VOID
ReferenceAddCount
(
    IN  PREF_CNT    pRefCnt,
    IN  UINT        Count
)

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    ASSERT( pRefCnt->Count > 0 );

    pRefCnt->Count += Count;
}
#endif // !UNDER_CE

PVOID
ReferenceRemove 
(
    IN PREF_CNT  pRefCnt
)

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
// UINT    Count;
// UINT    NoReference;

    if (pRefCnt) {
        if (pRefCnt->Count) {
            // If the decremented count is non zero return the instance handle

            if( InterlockedDecrement(&pRefCnt->Count) > 0 )
            {
        //        DEBUGMSG( DBG_REF,( "R-%d\n", pRefCnt->Count ));        
        //        DEBUGMSG( DBG_REF,( "ReferenceRemove:remaining: %d\n", pRefCnt->Count ));
                return( pRefCnt->Instance );
            }

            // Delete this instance if a delete handler is available

            if( pRefCnt->DeleteHandler )
            {
                DEBUGMSG( DBG_REF,(TEXT("Executing DeleteHandler\n")));

                (pRefCnt->DeleteHandler)( pRefCnt->Instance );
            }

        } else {
            // Trap remove reference on a zero count
            ASSERT(0);
        }
    } else {
        // Trap NULL pRefCnt
        ASSERT(0);
    }

    // Indicate no active references to this instance

    return( NULL );
}

//
// API Test Support
//

#if DBG

VOID
ReferenceApiTest( VOID )
{
REF_CNT  RefCnt;

    DEBUGMSG( 1,( TEXT("\nReferenceApiTest\n")));
    DEBUGMSG( 1,( TEXT("\nTest #1: NULL delete handler\n")));

    ReferenceInit( &RefCnt, &RefCnt, NULL );

    ReferenceAdd( &RefCnt );
    ReferenceAdd( &RefCnt );
    ReferenceAdd( &RefCnt );

    while( ReferenceRemove( &RefCnt ) )
    {
        ;
    }

    DEBUGMSG( 1,( TEXT("\nTest #2: Delete Handler - TBD\n")));
}

VOID
ReferenceAddDbg(PREF_CNT pRefCnt, ULONG Tag)
{
    int     i;

    ASSERT(pRefCnt->Sig == REF_SIG);
    
    for (i = 1; i < TAG_CNT; i++)
    {
        if (pRefCnt->Tags[i].Tag == 0 || pRefCnt->Tags[i].Tag == Tag)
        {
            pRefCnt->Tags[i].Tag = Tag;
            pRefCnt->Tags[i].Count++;
            break;
        }
    }
    
    ASSERT(i < TAG_CNT);
    
    ReferenceAdd(pRefCnt);           
    
    pRefCnt->Tags[0].Count = pRefCnt->Count;
}

VOID
ReferenceRemoveDbg(PREF_CNT pRefCnt, ULONG Tag)
{
    int     i;
    
    ASSERT(pRefCnt->Sig == REF_SIG);
    
    for (i = 1; i < TAG_CNT; i++)
    {
        if (pRefCnt->Tags[i].Tag == Tag)
        {
            pRefCnt->Tags[i].Count -= 1;
            if (pRefCnt->Tags[i].Count == 0)
                pRefCnt->Tags[i].Tag = 0;
            break;
        }
    }
    
    pRefCnt->Tags[0].Count = pRefCnt->Count - 1;    
    
    ReferenceRemove(pRefCnt);
}    

#endif
