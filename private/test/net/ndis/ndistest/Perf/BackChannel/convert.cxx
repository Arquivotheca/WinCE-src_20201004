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
#include "Convert.h"
#include <Strsafe.h>

#define CTL_ABORTIF(condition,err_code) \
{                                       \
    if ((condition))                    \
    {                                   \
        hr=err_code;                    \
        goto ErrReturn;                 \
    }                                   \
}

#define CTL_ABORT_ON_ERROR(hr)         \
{                                      \
    if (FAILED(hr))                    \
    {                                  \
        goto ErrReturn;                \
    }                                  \
}

//+--------------------------------------------------------------------------
//
//  Function:   CopyString
//
//  Synopsis:   Convert a wide (Unicode) string to a multibyte string
//
//  Parameters: [pszSource]     -- The wide string
//              [ppszDest]      -- Where to put the multibyte string
//
//  Returns:    S_OK if all went well
//
//  History:    31-Oct-96   MikeW   Created
//
//---------------------------------------------------------------------------

HRESULT CopyString(__in LPCWSTR pszSource, __out LPSTR *ppszDest)
{
    int     bufferSize;
    HRESULT hr = S_OK;

    *ppszDest = NULL;

    CTL_ABORTIF
            (
            NULL == pszSource,
            S_FALSE
            );

    //
    // Find the length of the buffer needed for the multibyte string
    //

    bufferSize = WideCharToMultiByte(
                        CP_ACP,
                        0,
                        pszSource,
                        -1,
                        *ppszDest,
                        0,
                        NULL,
                        NULL);

    if (0 == bufferSize)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    //
    // Allocate the buffer
    //

    if(S_OK == hr)
    {
        *ppszDest = new char[bufferSize];

        if (NULL == *ppszDest)
        {
            hr = E_OUTOFMEMORY;
        }
    }

    //
    // Do the conversion
    //

    if (S_OK == hr)
    {
        bufferSize = WideCharToMultiByte(
                                CP_ACP,
                                0,
                                pszSource,
                                -1,
                                *ppszDest,
                                bufferSize,
                                NULL,
                                NULL);

        if (0 == bufferSize)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }

    //
    // Clean up if there's an error
    //

    if (S_OK != hr && NULL != *ppszDest)
    {
        delete [] *ppszDest;
        *ppszDest = NULL;
    }

ErrReturn:

    return hr;
}



//+--------------------------------------------------------------------------
//
//  Function:   CopyString
//
//  Synopsis:   Copy a multibyte string
//
//  Parameters: [pszSource]             -- The original string
//              [ppszDest]              -- The copy
//
//  Returns:    S_OK if all went well
//
//  History:    31-Oct-96   MikeW   Created
//
//---------------------------------------------------------------------------

HRESULT CopyString(__in LPCSTR pszSource, __out LPSTR *ppszDest)
{
    size_t     bufferSize;
    HRESULT hr = S_OK;

    *ppszDest = NULL;
    CTL_ABORTIF
            (
            NULL == pszSource,
            S_FALSE
            );

    //
    // Find the length of the original string (in bytes for DBCS)
    //

    bufferSize = strlen(pszSource) + 1;

    //
    // Allocate the buffer
    //

    *ppszDest = new char[bufferSize];

    if (NULL == *ppszDest)
    {
        hr = E_OUTOFMEMORY;
    }

    //
    // Copy the string
    //

    if (S_OK == hr)
    {
        hr = StringCchCopyA
                (
                *ppszDest,
                strlen(pszSource) + 1,
                pszSource
                );
        CTL_ABORT_ON_ERROR(hr);
    }

ErrReturn:

    return hr;
}
