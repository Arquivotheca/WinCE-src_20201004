//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once
#include <windows.h>

class DxSharedBuffer
{
public:
    DxSharedBuffer();
    virtual ~DxSharedBuffer();

    HRESULT PrepareBuffer(size_t cbBufferSize, const TCHAR * tszBufferName);
    HRESULT FillBuffer(size_t cbBytesToWrite, const void * pData);
    HRESULT ReadBuffer(size_t cbBytesToRead, void * pData);
    HRESULT CleanupBuffer();

private:
    HANDLE m_hFileMap;
    size_t m_cbBufferSize;
};
