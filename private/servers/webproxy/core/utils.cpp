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

#include "utils.h"

CBuffer::CBuffer(void)
{
    m_pBuffer = m_Buffer;
    m_iSize = DEFAULT_BUFFER_SIZE;
}

CBuffer::~CBuffer(void)
{
    if (m_pBuffer != m_Buffer) {
        delete[] m_pBuffer;
    }
}

PBYTE CBuffer::GetBuffer(int iSize, BOOL fPreserveData)
{    
    //
    // If buffer is not big enough, we have to allocate new buffer
    // which is big enough.
    //
    if (iSize < 0) {
        ASSERT(0);
        return NULL;
    }
    
    if (iSize > m_iSize) {
        PBYTE pTmp = new BYTE[iSize];
        if (! pTmp) {
            return NULL;
        }

        if (fPreserveData) {
            memcpy(pTmp, m_pBuffer, m_iSize);
        }
    
        // Delete current buffer if it was allocated on the heap
        if (m_pBuffer != m_Buffer) {
            delete[] m_pBuffer;
        }

        // If alloc succeeded then update buffer and size
        m_pBuffer = pTmp;
        m_iSize = iSize;
    }

    return m_pBuffer;
}

int CBuffer::GetSize(void)
{
    return m_iSize;
}

