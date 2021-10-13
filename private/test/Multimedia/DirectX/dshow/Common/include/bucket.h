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
#ifndef _BUCKET_H_
#define _BUCKET_H_

#include "windows.h"

// Encapsulates a bucket object, used to fetch from a colleciton of items
// Class must be inherited from
class CBucket
{
public:
    CBucket();
    virtual ~CBucket();

    // Add items to the bucket from pSource
    virtual HRESULT Add(void* pSource);

    // Perform any configuration steps for the bucket
    virtual HRESULT Initialize(void* pConfig);

    // Get the current item in the bucket without advancing
    virtual HRESULT Peek(void* pItem);

    // Get the current item and advance to the next one
    virtual HRESULT Next(void* pItem);

    // Fetch the number of items left in the bucket
    virtual HRESULT Count(DWORD* pdwCount);    
};

#endif
