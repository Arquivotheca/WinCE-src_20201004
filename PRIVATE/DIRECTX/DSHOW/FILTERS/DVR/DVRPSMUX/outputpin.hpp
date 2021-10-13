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

#pragma once

#include <streams.h>

#define OUTPUT_PIN_NAME      L"PS Out"
#define PACKS_PER_SAMPLE 64

//
// ------------- OutputPin_t --------------
//

namespace DvrPsMux
{

class OutputPin_t :
    public CBaseOutputPin
{
    bool m_SinkIsFileWriter;

public:
    OutputPin_t(
        wchar_t*       pClassName,
        CBaseFilter*   pFilter,
        CCritSec*      pLock,
        HRESULT*       pHr,
        const wchar_t* pPinName
        ) :
            CBaseOutputPin(pClassName, pFilter, pLock, pHr, pPinName),
            m_SinkIsFileWriter(false)
    {
    }

    HRESULT
    CheckMediaType(
        const CMediaType* pMediaType
        );

    HRESULT
    GetMediaType(
        int         iPosition,
        CMediaType* pMediaType
        );

    HRESULT
    CompleteConnect(
        IPin *pReceivePin
        );

    HRESULT
    DecideBufferSize(
        IMemAllocator* pAlloc,
        ALLOCATOR_PROPERTIES* pPropInputRequest
        );

    bool
    SinkIsFileWriter()
    {
        return m_SinkIsFileWriter;
    }
};

}

