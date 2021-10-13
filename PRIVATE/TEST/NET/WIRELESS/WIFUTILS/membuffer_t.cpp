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
// Implementation of the MemBuffer_t class.
//
// ----------------------------------------------------------------------------

#include "MemBuffer_t.hpp"

#include <assert.h>
#include <intsafe.h>

using namespace ce::qa;

/* ============================== MemBufNode =============================== */

// ----------------------------------------------------------------------------
//
// Reference-counted object for use by MemBuffer.
// Since this class is only used by MemBuffer, its methods do not do
// the normal argument validation.
//
namespace ce {
namespace qa {
class MemBufNode_t
{
private:
    
    // Current memory allocated:
    DWORD m_Capacity;

    // Current memory usage:
    DWORD m_Length;
    
    // Reference count:
    LONG m_Refs;

    // Constructor and destructor:
    MemBufNode_t(IN DWORD Capacity)
        : m_Capacity(Capacity),
          m_Length(0),
          m_Refs(1)
    { }
   ~MemBufNode_t(void)
    { }
    
public:
    
    // Allocates a buffer node with the specified capacity:
    static MemBufNode_t *
    CreateNode(IN DWORD Capacity);

    // Deallocates the current buffer node:
    // All deallocation MUST be performed by this virtual method.
    // This guarantees the deallocation will be performed in the same
    // heap as the allocation when buffers are passed between modules.
    virtual void
    DeleteNode(void);
    
    // Accessors:
    void *
    GetData(void)
    {
        return reinterpret_cast<char *>(this) + sizeof(MemBufNode_t);
    }
    const void *
    GetData(void) const
    {
        return reinterpret_cast<const char *>(this) + sizeof(MemBufNode_t);
    }
    DWORD
    GetCapacity(void) const
    {
        return m_Capacity;
    }
    DWORD
    GetLength(void) const
    {
        return m_Length;
    }
    void
    SetLength(IN DWORD newLength)
    {
        m_Length = newLength;
    }
    LONG
    GetReferenceCount(void)
    {
        return InterlockedCompareExchange(&m_Refs, 0, 0);
    }
    
    // Copies data into the buffer:
    bool
    Assign(
        IN const void *pData,
        IN       DWORD  DataBytes,
        IN       DWORD  NullBytes);
    bool
    Append(
        IN const void *pData,
        IN       DWORD  DataBytes,
        IN       DWORD  NullBytes);

  // Buffer-node pointer management:
  
    // Retrieves a pointer to the specified memory node:
    static MemBufNode_t *
    GetBufNode(
        OUT void * volatile *ppNode)
    {
        void *pRawNode = InterlockedCompareExchangePointer(ppNode, NULL, NULL);
        return static_cast<MemBufNode_t *>(pRawNode);
    }

    // Retrieves a duplicate reference to the specified memory-buffer node:
    // Returns a pointer to the object if the ref-count increment succeeds.
    // Returns NULL if the pointer is empty or the object has been deleted.
    static MemBufNode_t *
    AttachBufNode(
        OUT void * volatile *ppNode);
  
    // Detaches and, if necessary, deallocates the specified memory node:
    static void
    DetachBufNode(
        OUT void * volatile *ppNode,
        IN  void            *pNewNode);
};
};
};

// ----------------------------------------------------------------------------
//    
// Allocates a buffer node with the specified capacity.
//
MemBufNode_t *
MemBufNode_t::
CreateNode(IN DWORD Capacity)
{
    DWORD allocSize; void *pAlloc;
    HRESULT hr = DWordAdd(Capacity, sizeof(MemBufNode_t), &allocSize);
    if (FAILED(hr))
         pAlloc = NULL;
    else pAlloc = malloc(allocSize);
    if (NULL == pAlloc)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        return NULL;
    }
    else
    {
        return new (pAlloc) MemBufNode_t(Capacity);
    }
}

// ----------------------------------------------------------------------------
//    
// Deallocates the buffer node.
// All deallocation MUST be performed by this virtual method.
// This guarantees the deallocation will be performed in the same
// heap as the allocation when buffers are passed between modules.
//
void
MemBufNode_t::
DeleteNode(void)
{
    InterlockedExchange(&m_Refs, -999L);
    free(this);
}

// ----------------------------------------------------------------------------
//    
// Copies data into the buffer.
//
bool
MemBufNode_t::
Assign(
    IN const void *pData,
    IN       DWORD  DataBytes,
    IN       DWORD  NullBytes)
{
    unsigned char *thisData = static_cast<unsigned char *>(GetData());
    
    if (0 != DataBytes)
    {
        if (MemBuffer_t::MaximumBufferSize < DataBytes)
        {
            assert(MemBuffer_t::MaximumBufferSize >= DataBytes);
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }
        if (NULL == pData)
        {
            assert(NULL != pData);
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }
        memcpy(thisData, pData, DataBytes);
    }
    
    m_Length = DataBytes;

    if (0 != NullBytes)
    {
        thisData += m_Length;
        do { *(thisData++) = 0; }
        while (--NullBytes != 0);
    }
    
    return true;
}

bool
MemBufNode_t::
Append(
    IN const void *pData,
    IN       DWORD  DataBytes,
    IN       DWORD  NullBytes)
{
    unsigned char *thisData = static_cast<unsigned char *>(GetData());
    
    if (0 != DataBytes)
    {
        if (MemBuffer_t::MaximumBufferSize < DataBytes)
        {
            assert(MemBuffer_t::MaximumBufferSize >= DataBytes);
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }
        if (NULL == pData)
        {
            assert(NULL != pData);
            SetLastError(ERROR_INVALID_PARAMETER);
            return false;
        }
        memcpy(thisData + m_Length, pData, DataBytes);
    }
    
    m_Length += DataBytes;
    
    if (0 != NullBytes)
    {
        thisData += m_Length;
        do { *(thisData++) = 0; }
        while (--NullBytes != 0);
    }
    
    return true;
}

// ----------------------------------------------------------------------------
//    
// Retrieves a duplicate reference to the specified memory-buffer node.
// Returns a pointer to the object if the ref-count increment succeeds.
// Returns NULL if the pointer is empty or the object has been deleted.
//
MemBufNode_t *
MemBufNode_t::
AttachBufNode(
    OUT void * volatile *ppNode)
{
    MemBufNode_t *pNode = GetBufNode(ppNode);
    if (NULL != pNode)
    {
        LONG oldRefs = InterlockedExchangeAdd(&(pNode->m_Refs), 1L);
        if (0 < oldRefs)
            return pNode;
        InterlockedCompareExchange(&(pNode->m_Refs), oldRefs, oldRefs+1);
    }
    return NULL;
}

// ----------------------------------------------------------------------------
//    
// Detaches and, if necessary, deallocates the specified memory node.
//
void
MemBufNode_t::
DetachBufNode(
    OUT void * volatile *ppNode,
    IN  void            *pNewNode)
{
    void *pRawNode = InterlockedExchangePointer(ppNode, pNewNode);
    if (NULL != pRawNode)
    {
        MemBufNode_t *pNode = static_cast<MemBufNode_t *>(pRawNode);
        if (0 == InterlockedDecrement(&(pNode->m_Refs)))
        {
            pNode->DeleteNode();
        }
    }
}

/* =============================== MemBuffer =============================== */

// ----------------------------------------------------------------------------
//    
// Destructor.
//
MemBuffer_t::
~MemBuffer_t(void)
{
    MemBufNode_t::DetachBufNode(&m_pNode, NULL);
}

// ----------------------------------------------------------------------------
//    
// Copy constructor.
//
MemBuffer_t::
MemBuffer_t(const MemBuffer_t &rhs)
    : m_pNode(MemBufNode_t::AttachBufNode(&(rhs.m_pNode)))
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Assignment operator.
//
MemBuffer_t &
MemBuffer_t::
operator = (const MemBuffer_t &rhs)
{
    MemBuffer_t copy(rhs);
    Swap(&copy);
    return *this;
}

// ----------------------------------------------------------------------------
//    
// Allocates at least the specified amount of memory.
// Assumes it's working on a private MemBuffer object.
//
bool
MemBuffer_t::
Allocate(
    IN DWORD DataBytes,  // Number bytes required
    IN DWORD NullBytes,  // Extra null bytes at end
    IN bool  CopyMemory) // true to copy the node if too small, false to just reallocate
{
    // Validate the parameters.
    DWORD capacity = DataBytes + NullBytes;
    if (MaximumBufferSize < DataBytes
     || MaximumBufferSize < NullBytes
     || MaximumBufferSize < capacity)
    {
        assert(MaximumBufferSize >= DataBytes);
        assert(MaximumBufferSize >= NullBytes);
        assert(MaximumBufferSize >= capacity);
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    
    // If both sizes are zero, there's nothing to do.
    if (0 != capacity)
    {
        MemBufNode_t *pOldNode = static_cast<MemBufNode_t *>(m_pNode);
    
        // If the node hasn't been allocated yet, it's too small or
        // it's being used by somebody else, allocate a new node.
        if (NULL == pOldNode
         || pOldNode->GetCapacity() < capacity
         || pOldNode->GetReferenceCount() > 2)
        {
            // Calculate the buffer capacity required to hold the memory 
            // and round to the nearest multiple of MinimumAllocationSize.
            static const DWORD MinimumAllocMask = (MinimumAllocationSize - 1);
            capacity +=  MinimumAllocMask;
            capacity &= ~MinimumAllocMask;
        
            // Allocate the new buffer node.
            MemBufNode_t *pNewNode = MemBufNode_t::CreateNode(capacity);
            if (NULL == pNewNode)
                return false;
            
            // If necessary, copy the old data to the new node.
            if (CopyMemory && NULL != pOldNode)
            {
                memcpy(pNewNode->GetData(),
                       pOldNode->GetData(), pOldNode->GetCapacity());
                pNewNode->SetLength(pOldNode->GetLength());
            }

            // Detach the current node and attach the new one.
            MemBufNode_t::DetachBufNode(&m_pNode, pNewNode);
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
//    
// Accessors.
//
void *
MemBuffer_t::
GetData(void)
{
    MemBufNode_t *pNode = MemBufNode_t::GetBufNode(&m_pNode);
    return (NULL == pNode)? NULL : pNode->GetData();
}

const void *
MemBuffer_t::
GetData(void) const
{
    const MemBufNode_t *pNode = MemBufNode_t::GetBufNode(&m_pNode);
    return (NULL == pNode)? NULL : pNode->GetData();
}

DWORD
MemBuffer_t::
Capacity(void) const
{
    const MemBufNode_t *pNode = MemBufNode_t::GetBufNode(&m_pNode);
    return (NULL == pNode)? 0 : pNode->GetCapacity();
}

DWORD
MemBuffer_t::
Length(void) const
{
    const MemBufNode_t *pNode = MemBufNode_t::GetBufNode(&m_pNode);
    return (NULL == pNode)? 0 : pNode->GetLength();
}

// ----------------------------------------------------------------------------
//    
// Allocates at least the specified amount of memory.
//
bool
MemBuffer_t::
Reserve(
    IN DWORD DataBytes)
{
    MemBuffer_t copy(*this);
    if (copy.Allocate(DataBytes, 0, true))
    {
        Swap(&copy);
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
//    
// Deallocates memory.
//
void
MemBuffer_t::
Free(void)
{
    MemBufNode_t::DetachBufNode(&m_pNode, NULL);
}

// ----------------------------------------------------------------------------
//     
// Clears memory.
//
void
MemBuffer_t::
Clear(void)
{
    MemBuffer_t copy(*this);
    MemBufNode_t *pNode = static_cast<MemBufNode_t *>(copy.m_pNode);

    // If the buffer node exists...
    if (NULL != pNode)
    {
        // If it's being used by somebody else, just detach it.
        if (pNode->GetReferenceCount() > 2)
        {
            MemBufNode_t::DetachBufNode(&copy.m_pNode, NULL);
        }

        // Otherwise, just set its length to zero.
        else
        {
            pNode->SetLength(0);

            // Always add a few nulls in case somebody calls wcslen(buf).
            DWORD *pData = static_cast<DWORD *>(pNode->GetData());
            pData[0] = 0;
        }
        
        Swap(&copy);
    }
}

// ----------------------------------------------------------------------------
//    
// Copies the specified data into the buffer.
//
bool
MemBuffer_t::
Assign(
    IN const BYTE *pData,
    IN       DWORD  DataBytes,
    IN       DWORD  NullBytes)
{
    MemBuffer_t copy(*this);
    if (copy.Allocate(DataBytes, NullBytes, false))
    {
        MemBufNode_t *pNode = static_cast<MemBufNode_t *>(copy.m_pNode);
        if (NULL == pNode || pNode->Assign(pData, DataBytes, NullBytes))
        {
            Swap(&copy);
            return true;
        }
    }
    return false;
}

bool
MemBuffer_t::
Assign(
    IN const ce::string &Message)
{
    return Assign((const BYTE *)(&Message[0]), 
                  Message.length() * sizeof(char), 
                                     sizeof(char)); // terminate with one null
}

bool
MemBuffer_t::
Assign(
    IN const ce::wstring &Message)
{
    return Assign((const BYTE *)(&Message[0]),
                  Message.length() * sizeof(WCHAR),
                                     sizeof(WCHAR)); // terminate with two nulls
}

// ----------------------------------------------------------------------------
//    
// Appends the specified data onto the buffer.
//
bool
MemBuffer_t::
Append(
    IN const BYTE *pData,
    IN       DWORD  DataBytes,
    IN       DWORD  NullBytes)
{
    if (0 == DataBytes && 0 == NullBytes)
    {
        return true;
    }
    else
    {
        MemBuffer_t copy(*this);
        if (copy.Allocate(copy.Length() + DataBytes, NullBytes, true))
        {
            MemBufNode_t *pNode = static_cast<MemBufNode_t *>(copy.m_pNode);
            if (NULL == pNode || pNode->Append(pData, DataBytes, NullBytes))
            {
                Swap(&copy);
                return true;
            }
        }
        return false;
    }
}

bool
MemBuffer_t::
Append(
    IN const ce::string &Message)
{
    return Append((const BYTE *)(&Message[0]),
                  Message.length() * sizeof(char), 
                                     sizeof(char)); // terminate with one null
}

bool
MemBuffer_t::
Append(
    IN const ce::wstring &Message)
{
    return Append((const BYTE *)(&Message[0]),
                  Message.length() * sizeof(WCHAR),
                                     sizeof(WCHAR)); // terminate with two nulls
}

// ----------------------------------------------------------------------------
//    
// Copies the memory onto the local heap.
//
bool
MemBuffer_t::
CopyOut(
    OUT BYTE **ppData,
    OUT DWORD  *pDataBytes) const
{
    if (NULL == ppData || NULL == pDataBytes)
    {
        assert(NULL != ppData && NULL != pDataBytes);
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    else
    {
       *ppData = NULL;
       *pDataBytes = 0;
        MemBuffer_t copy(*this);
        MemBufNode_t *pNode = static_cast<MemBufNode_t *>(copy.m_pNode);
        if (NULL == pNode || sizeof(BYTE) > pNode->GetLength())
        {
            return true;
        }
        else
        {
            void *pData = malloc(pNode->GetLength());
            if (NULL == pData)
            {
                SetLastError(ERROR_OUTOFMEMORY);
            }
            else
            {
                memcpy(pData, pNode->GetData(), pNode->GetLength());
               *ppData = static_cast<BYTE *>(pData);
               *pDataBytes = pNode->GetLength();
                return true;
            }
        }
    }
    return false;
}

bool
MemBuffer_t::
CopyOut(
    OUT ce::string *pMessage) const
{
    if (NULL == pMessage)
    {
        assert(NULL != pMessage);
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    else
    {
        pMessage->clear();
        MemBuffer_t copy(*this);
        MemBufNode_t *pNode = static_cast<MemBufNode_t *>(copy.m_pNode);
        if (NULL == pNode || sizeof(char) > pNode->GetLength())
        {
            return true;
        }
        else
        {
            return pMessage->assign(static_cast<char *>(pNode->GetData()), 
                                                        pNode->GetLength());
        }
    }
    return false;
}

bool
MemBuffer_t::
CopyOut(
    OUT ce::wstring *pMessage) const
{
    if (NULL == pMessage)
    {
        assert(NULL != pMessage);
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    else
    {
        pMessage->clear();
        MemBuffer_t copy(*this);
        MemBufNode_t *pNode = static_cast<MemBufNode_t *>(copy.m_pNode);
        if (NULL == pNode || sizeof(WCHAR) > pNode->GetLength())
        {
            return true;
        }
        else
        {
            return pMessage->assign(static_cast<WCHAR *>(pNode->GetData()),
                                                         pNode->GetLength() / sizeof(WCHAR));
        }
    }
    return false;
}

// -------------------------------------------------------------------------
