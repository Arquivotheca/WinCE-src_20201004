//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

