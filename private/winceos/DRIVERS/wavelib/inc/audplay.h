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

#include <unknwn.h>

class IAudPlay;

class IAudNotify : public IUnknown
{
public:
    virtual HRESULT Done() = 0;
};

class IAudPlay : public IUnknown
{
public:
    virtual HRESULT Open(IStream *pIStream, IAudNotify *pIAudNotify) = 0;
    virtual HRESULT Close() = 0;

    virtual HRESULT Length(ULONG *pcbLength, ULONG *pcbBlockAlign) = 0;
    virtual HRESULT Duration(ULONG *pTimeMs) = 0;
    virtual HRESULT Start() = 0;
    virtual HRESULT Stop() = 0;
    virtual HRESULT Seek(ULONG Pos) = 0;
    virtual HRESULT SetVolume(DWORD Volume) = 0;
    virtual HRESULT Preroll() = 0;
    virtual HRESULT Reset() = 0;
};

HRESULT CreateAudPlay(DWORD numBuffers, DWORD msPerBuffer, IAudPlay **ppIAudPlay);

