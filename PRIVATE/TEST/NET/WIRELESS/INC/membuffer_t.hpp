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
#include <inc/string.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides a reference-counted object for simplifying memory management.
// Also guarantees that memory will be deallocated in the module where it
// was allocated. This allows these objects to be used for safely passing
// raw data between modules.
//
// Because the allocated memory is reference-counted, copying these objects
// cost almost nothing; The reference count is increment and both copies
// refer to the same memory. This implies, however, that two portions of
// the application can inadvertantly modify the same memory without meaning
// to do so. To avoid this, these objects also implement a "copy on write"
// algorithm. In other words, any changes affecting a copied object will
// replicate the shared object so both parties have a private copy.
//
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
        peer->m_pNode = InterlockedExchangePointer(m_pNode, peer->m_pNode);
    }

    // Allocates at least the specified amount of memory:
    // Assumes it's working on a private MemBuffer object.
    bool
    Allocate(
        IN DWORD DataBytes,   // Number bytes required
        IN DWORD NullBytes,   // Extra null bytes at end
        IN bool  CopyMemory); // true to copy the node if too small, false to just reallocate

public:

    // Maximum allocation size:
    static const DWORD MaximumBufferSize = 16*1024*1024;

    // All memory allocations are rounded up to a multiple of this size:
    static const DWORD MinimumAllocationSize = 0x10; // 16 bytes = 128 bits

    // Constructor and destructor:
    MemBuffer_t(void) : m_pNode(NULL) { }
   ~MemBuffer_t(void);
    
    // Copy and assignment:
    MemBuffer_t(const MemBuffer_t &rhs);
    MemBuffer_t &operator = (const MemBuffer_t &rhs);
    
    // Accessors:
    void *
    GetData(void);
    const void *
    GetData(void) const;
    DWORD
    Capacity(void) const;
    DWORD
    Length(void) const;

    // Allocates at least the specified amount of memory:
    bool
    Reserve(
        IN DWORD DataBytes);

    // Deallocates memory:
    void
    Free(void);

    // Zeroes the length of the current memory:
    void
    Clear(void);

    // Copies the specified data into the buffer:
    bool
    Assign(
        IN const BYTE *pData,
        IN       DWORD  DataBytes,
        IN       DWORD  NullBytes = sizeof(WCHAR)); // Extra null bytes at end
    bool
    Assign(
        IN const ce::string &Message);
    bool
    Assign(
        IN const ce::wstring &Message);

    // Appends the specified data onto the buffer:
    bool
    Append(
        IN const BYTE *pData,
        IN       DWORD  DataBytes,
        IN       DWORD  NullBytes = sizeof(WCHAR)); // Extra null bytes at end
    bool
    Append(
        IN const ce::string &Message);
    bool
    Append(
        IN const ce::wstring &Message);
    
    // Copies the buffer onto the local heap:
    bool
    CopyOut(
        OUT BYTE **ppData,
        OUT DWORD  *pDataBytes) const;
    bool
    CopyOut(
        OUT ce::string *pMessage) const;
    bool
    CopyOut(
        OUT ce::wstring *pMessage) const;
};

};
};
#endif /* _DEFINED_MemBuffer_t_ */
// ----------------------------------------------------------------------------
