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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Definitions and declarations for the MemBuffer_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_MemBuffer_t_
#define _DEFINED_MemBuffer_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>
#include <string.hxx>

#ifdef _PREFAST_
#pragma warning(push)
#pragma warning(disable:26020)
#else
#pragma warning(disable:4068)
#endif

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides a reference-counted object for simplifying memory management.
// Because the allocated memory is reference-counted, copying these objects
// cost almost nothing; The reference count is incremented and both copies
// refer to the same memory.
//
// This implies, however, that a portion of an application may inadvertantly
// modify memory being used elsewhere without notifying the other users. To
// avoid this, these objects also implement a "copy on write" algorithm.
// Using this mechanism, any changes affecting a shared object will copy
// the memory so the the modifying party has a private instance.
//
// This explains the naming of the two data-pointer access methods:
//   GetShared retrieves a const pointer to the possibly-shared data.
//   GetPrivate retrieves a mutable pointer to the data. If another user
//      has a reference to the data, this method copies the data to create
//      a private copy.
//
class MemBufNode_t;
class MemBuffer_t
{
private:

    // Allocated memory node (if any):
    mutable void * volatile m_pNode;

    // Swaps this object's memory buffer for the peer's:
    // Assumes the peer is a private MemBuffer object.
    void
    Swap(
        OUT MemBuffer_t *peer)
    {
        peer->m_pNode = InterlockedExchangePointer(&m_pNode, peer->m_pNode);
    }

    // Allocates at least the specified amount of memory:
    // Assumes it's working on a private MemBuffer object.
    bool
    Allocate(
        IN DWORD DataBytes,   // Number bytes required
        IN DWORD NullBytes,   // Extra null bytes at end
        IN bool  CopyMemory); // true to copy the node if too small, false to just reallocate

public:

    // All memory allocations are rounded up to a multiple of 16 bytes:
    static const DWORD MinimumAllocationBits = 4;
    static const DWORD MinimumAllocationSize = 1<<MinimumAllocationBits;
    
    // Maximum allocation size:
    static const DWORD MaximumBufferSize = 16*1024*1024;

    // Constructor and destructor:
    MemBuffer_t(void) : m_pNode(NULL) { }
   ~MemBuffer_t(void);
    
    // Copy and assignment:
    MemBuffer_t(const MemBuffer_t &rhs);
    MemBuffer_t &operator = (const MemBuffer_t &rhs);
    
    // Accessors:
    DWORD
    Capacity(void) const;
    DWORD
    Length(void) const; 
    DWORD
    SetLength(
        IN DWORD NewLength);

    // Gets a pointer to the stored data:
    // DO NOT MODIFY the data. Use GetPrivate to make sure no others
    // are accessing the memory at the same time.
    const void *
    GetShared(void) const;

    // Gets a pointer to the "privatized" data:
    // If two MemBuffers point to the same memory, this will replicate
    // the data to create a private copy before returning the pointer.
    // Returns NULL if nothing has been allocated or the copy fails.
    void *
    GetPrivate(void);

    // Allocates at least the specified amount of memory:
    DWORD
    Reserve(
        IN DWORD DataBytes);

    // Deallocates memory:
    void
    Free(void);

    // Zeroes the length of the current memory:
    void
    Clear(void);

    // Copies the specified data into the buffer:
    DWORD
    Assign(
        IN const BYTE *pData,
        IN       DWORD DataBytes,
        IN       DWORD NullBytes = 0); // Extra null bytes at end
    DWORD
    Assign(
        IN const ce::string &Message);
    DWORD
    Assign(
        IN const ce::wstring &Message);

    // Expands the buffer at the specified location and inserts data:
    // Note that this will only insert the specified nulls if the new
    // data is at the end of the buffer. In other words, this will
    // never insert nulls inside the buffer.
    DWORD
    Insert(
        IN       DWORD InsertAt,
        IN const BYTE *pData,
        IN       DWORD DataBytes,
        IN       DWORD NullBytes = 0); // Extra null bytes at end
    DWORD
    Insert(
        IN       DWORD       InsertAt,
        IN const ce::string &Message);
    DWORD
    Insert(
        IN       DWORD        InsertAt,
        IN const ce::wstring &Message);
    
    // Adds the specified data to the end of the buffer:
    DWORD
    Append(
        IN const BYTE *pData,
        IN       DWORD DataBytes,
        IN       DWORD NullBytes = 0); // Extra null bytes at end
    DWORD
    Append(
        IN const ce::string &Message);
    DWORD
    Append(
        IN const ce::wstring &Message);
      
    // Removes the specified data from the buffer:
    DWORD
    Remove(
        IN DWORD RemoveAt,   // Remove bytes from this offset
        IN DWORD DataBytes); // Remove this many bytes
    DWORD
    Remove(
        IN       DWORD       RemoveAt,
        IN const ce::string &Message);
    DWORD
    Remove(
        IN       DWORD        RemoveAt,
        IN const ce::wstring &Message); 
    
    // Copies the buffer onto the local heap:
    DWORD
    CopyOut(
      __out_ecount(pDataBytes) OUT BYTE **ppData,
                               OUT DWORD  *pDataBytes) const;
    DWORD
    CopyOut(
        OUT ce::string *pMessage) const;
    DWORD
    CopyOut(
        OUT ce::wstring *pMessage) const;
};

};
};

#ifdef _PREFAST_
#pragma warning(pop)
#endif

#endif /* _DEFINED_MemBuffer_t_ */
// ----------------------------------------------------------------------------
