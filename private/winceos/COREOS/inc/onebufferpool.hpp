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

#pragma once

#include <auto_xxx.hxx>

//
// Maintains a single buffer instance of a fixed size. Optimized allocation for
// scenarios where there is typically only one outstanding allocation at any
// point in time. Outstanding allocations beyond the first will be allocated
// using the global new[] operator.
//
template <typename _TYPE, size_t _ELEMENTS>
class OneBufferPool
{
public:

    inline OneBufferPool() :
                m_pBuffer(NULL)
    {
        ;
    }

    inline ~OneBufferPool()
    {
        if(m_pBuffer)
        {
            delete[] m_pBuffer;
            m_pBuffer = NULL;
        }
    }

    inline _TYPE* AllocBuffer()
    {
        // Try to use the existing buffer, if any exists.
        _TYPE* pBuffer = (_TYPE*)InterlockedExchangePointer(&m_pBuffer, NULL);

        if(!pBuffer)
        {
            // No existing buffer, allocate one.
            pBuffer = new _TYPE[_ELEMENTS];
        }

        return pBuffer;
    }

    inline void FreeBuffer(_TYPE* pBuffer)
    {
        ASSERT(pBuffer);

        // Try to return the buffer to the tracked instance.
        if(pBuffer)
        {
            pBuffer = (_TYPE*)InterlockedExchangePointer(&m_pBuffer, pBuffer);

            if(pBuffer)
            {
                // Already tracking a buffer for re-allocation, so free this one.
                delete[] pBuffer;
            }
        }
    }

    static inline size_t BufferElementCount()
    {
        return _ELEMENTS;
    }

protected:

    volatile BYTE* m_pBuffer;
};