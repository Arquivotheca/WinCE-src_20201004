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
#include "stdafx.h"

CParameter::CParameter()
{
    wzName = NULL;
    wzRecipient = NULL;
    dwCount = 0;
    pValues = NULL;
}


CParameter::~CParameter()
{
    if( wzName )
    {
        LocalFree( wzName );
        wzName = NULL;
    }

    if( wzRecipient )
    {
        LocalFree( wzRecipient );
        wzRecipient = NULL;
    }

    if( pValues )
    {
        LocalFree( pValues );
        pValues = NULL;
    }
}


CParameterList::CParameterList()
{
    dwParameterCount = 0;
    pParameters = NULL;
}


CParameterList::~CParameterList()
{
    delete[] pParameters;
}


HRESULT
CParameterList::CreateList( DWORD dwCount )
{
    if( pParameters || dwCount == 0 || dwCount >= 64)
    {
        // Already initialized
        return E_FAIL;
    }

    pParameters = new CParameter[ dwCount ];
    if( pParameters == NULL )
    {
        return E_OUTOFMEMORY;
    }

    dwParameterCount = dwCount;

    return S_OK;
}

