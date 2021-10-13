//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Copyright 2001, Cisco Systems, Inc.  All rights reserved.
// No part of this source, or the resulting binary files, may be reproduced,
// transmitted or redistributed in any form or by any means, electronic or
// mechanical, for any purpose, without the express written permission of Cisco.
//
//---------------------------------------------------------------------------
// alloc.cpp
//---------------------------------------------------------------------------
// Description: Memory allocation operators.
//
// Revision History:
//
// Date        
//---------------------------------------------------------------------------
// 10/25/00    jbeaujon       -Initial Revision
//
//
//---------------------------------------------------------------------------

extern "C" {
#include <ndis.h>
}
#include "support.h"

//---------------------------------------------------------------------------
// Tag that appears in any crash dump of the system.
//---------------------------------------------------------------------------
#define MEM_ALLOC_TAG 'oriA'

//===========================================================================
    void* __cdecl operator new (size_t size)
//===========================================================================
// 
// Description: Allocate a block of memory size bytes long.
//    
//      Inputs: size - size of block to allocate
//    
//     Returns: pointer to memory block if successful, NULL otherwise.
// 
//       Notes: Caller must be running at IRQL <= DISPATCH_LEVEL.
//    
//      (10/25/00)
//---------------------------------------------------------------------------
{
    // 
    // Allocate a little extra to store size of block.  We'll need the size
    // when we go to delete this memory.
    // 
    size_t totalSize = size + sizeof(size);

    char *p = NULL;

#if (NDISVER < 5) || (NDISVER == 41)
    NDIS_PHYSICAL_ADDRESS HighestAcceptableMax = NDIS_PHYSICAL_ADDRESS_CONST(-1,-1);
	NdisAllocateMemory((void**)&p, totalSize, 0, HighestAcceptableMax);
#else
	NdisAllocateMemoryWithTag((void**)&p, totalSize, MEM_ALLOC_TAG);
#endif
    // 
    // Store size at beginning of buffer and bump pointer before returning it.
    // 
    if (p != NULL) {
        *((size_t*)p) = totalSize;
        
        #if DEBUG_ALLOC
		DbgPrint("   new: 0x%08X", (ULONG)p);
		DbgPrint(", size = %d\n", (ULONG)totalSize);
        #endif

        p += sizeof(totalSize);
        }
    return(p);
}

//===========================================================================
    void __cdecl operator delete (void *p)
//===========================================================================
// 
// Description: Deallocate the block of memory pointed to by p.
//    
//      Inputs: p - pointer to block of memory to deallocate.
//    
//     Returns: nothing.
//    
//       Notes: Caller must be running at IRQL <= DISPATCH_LEVEL.
//              p MUST have been allocated with the new operator above.
//    
//      (10/25/00)
//---------------------------------------------------------------------------
{
    // 
    // Allow a NULL pointer to be deleted.
    // 
    if (p != NULL) {
        // 
        // Point to actual beginning of allocated buffer (remember that the size 
        // is stored immediately preceeding the memory pointed to by p).
        // 
        char *ptr = (char*)p - sizeof(size_t);
        // 
        // Grab the size
        // 
        size_t size = *((size_t*)ptr);

        #if DEBUG_ALLOC
		DbgPrint("   delete: 0x%08X", (ULONG)ptr);
		DbgPrint(", size = %d\n", (ULONG)size);
        #endif

        // 
        // Deallocate.
        // 
        NdisFreeMemory(ptr, size, 0);
        }
}

/*
//===========================================================================
    size_t getSize (void *p)
//===========================================================================
// 
// Description: Return the size of the buffer pointed to by p.  
//    
//      Inputs: p - pointer to buffer allocated with the new operator above.
//    
//     Returns: the size of the buffer, excluding the storage required to 
//              store the size of the buffer.
//    
//       Notes: p MUST have been allocated with the new operator above.
//
//      (10/26/00)
//---------------------------------------------------------------------------
{
    char *ptr = (char*)p - sizeof(size_t);
    return *((size_t*)ptr) - sizeof(size_t);
}

//===========================================================================
    void *reNew (void *p, size_t size)
//===========================================================================
// 
// Description: Reallocate (re-new) the pointer p with the specified size.  The 
//              contents of the buffer pointed to by p are copied to the new 
//              buffer.  If the new size is smaller the the old size then data 
//              may be lost.
//    
//      Inputs: p       - pointer to block of memory.
//              size    - new size of buffer.
//    
//     Returns: the new buffer.
// 
//       Notes: Caller must be running at IRQL <= DISPATCH_LEVEL.
//              p MUST have been allocated with the new operator above.
//    
//      (10/26/00)
//---------------------------------------------------------------------------
{
    size_t pSize = 0;

    if (p != NULL) {
        pSize = getSize(p);
        }
    size_t  bytesToCopy = (pSize > size) ? size : pSize;
    char    *p1         = new char[size];
    if ((p1 != NULL) && (p != NULL)) {
        memcpy(p1, p, bytesToCopy);
        delete p;
        }
    return(p1);
}
*/
