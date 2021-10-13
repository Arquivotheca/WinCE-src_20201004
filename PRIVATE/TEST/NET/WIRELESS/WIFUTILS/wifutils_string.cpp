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
// Implementation of the WiFUtils class.
//
// ----------------------------------------------------------------------------

#include "WiFUtils.hpp"

#include <assert.h>
#include <strsafe.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// If required, logs an error message for one of the ConvertString overloads.
//
static HRESULT
ShowConvertResult(
    IN HRESULT      hr,
    IN const char  *pIn,
    IN const char  *pStringName,
    IN const TCHAR *pErrorFormat)
{
    if (NULL != pStringName && '\0' != pStringName[0])
    {
        char inBuff[MAX_PATH+1];
        if (FAILED(WiFUtils::ConvertString(inBuff, pIn, MAX_PATH)))
             pIn = "!invalid!";
        else pIn = inBuff;
        LogError(pErrorFormat, pStringName, pIn, HRESULTErrorText(hr));
    }
    return hr;
}

static HRESULT
ShowConvertResult(
    IN HRESULT      hr,
    IN const WCHAR *pIn,
    IN const char  *pStringName,
    IN const TCHAR *pErrorFormat)
{
    if (NULL != pStringName && '\0' != pStringName[0])
    {
        WCHAR inBuff[MAX_PATH+1];
        if (FAILED(WiFUtils::ConvertString(inBuff, pIn, MAX_PATH)))
             pIn = L"!invalid!";
        else pIn = inBuff;
        LogError(pErrorFormat, pStringName, pIn, HRESULTErrorText(hr));
    }
    return hr;
}

// ----------------------------------------------------------------------------
//
// Fill the extra space at the end of the specified output buffer with nulls.
// Returns the size of the padded area (if any).
//
template <class T>
inline int
InsertNullPadding(
  __out_ecount(*pOutChars) OUT T   *pOut,
                           OUT int *pOutChars,
                           IN  int  InChars)
{
    int excessChars = 0;
    if (0 <= InChars && *pOutChars > InChars)
    {
        excessChars = *pOutChars - InChars;
       *pOutChars = InChars+1;
        memset(&pOut[InChars], 0, excessChars * sizeof(T));
    }
    return excessChars;
}

// ----------------------------------------------------------------------------
//
// Translates from Unicode to ASCII or vice-versa.
// Generates an error message if the conversion fails and the
// optional string-name is specified.
// The input may be NULL or empty and, if the optional InChars
// argument is supplied, needs no null-termination.
// The output will ALWAYS be null-terminated and -padded.
//
HRESULT
WiFUtils::
ConvertString(
  __out_ecount(OutChars) OUT char       *pOut,
                         IN  const char *pIn,
                         IN  int         OutChars,
                         IN  const char *pStringName,
                         IN  int         InChars,
                         IN  UINT        CodePage)
{
    HRESULT hr = S_OK;

    if (NULL == pOut)
    {
        assert(NULL != pOut);
        hr = E_INVALIDARG;
    }
    else
    if (0 < OutChars)
    {
        int excessChars = InsertNullPadding(pOut, &OutChars, InChars);
        
        hr = StringCchCopyExA(pOut, OutChars, pIn, NULL, NULL,
                              STRSAFE_FILL_BEHIND_NULL
                            | STRSAFE_IGNORE_NULLS);
        
        if (excessChars && STRSAFE_E_INSUFFICIENT_BUFFER == hr)
        {
            hr = S_OK;
        }
    }
            
    return SUCCEEDED(hr)? hr
         : ShowConvertResult(hr, pIn, pStringName,
                             TEXT("Error copying %hs \"%.128hs\": %s"));
}

HRESULT
WiFUtils::
ConvertString(
  __out_ecount(OutChars) OUT WCHAR      *pOut,
                         IN  const char *pIn,
                         IN  int         OutChars,
                         IN  const char *pStringName,
                         IN  int         InChars,
                         IN  UINT        CodePage)
{
    HRESULT hr = S_OK;

    if (NULL == pOut)
    {
        assert(NULL != pOut);
        hr = E_INVALIDARG;
    }
    else
    if (0 < OutChars)
    {
        if (0 == InChars || NULL == pIn || '\0' == pIn[0])
        {
            memset(pOut, 0, OutChars * sizeof(WCHAR));
        }
        else
        {
            if (InChars <  0)
                InChars = -1;
            else
            {
                size_t inSize;
                if (SUCCEEDED(StringCchLengthA(pIn, InChars, &inSize)))
                    InChars = static_cast<int>(inSize);
            }

            for (;;)
            {
                int xlated = ::MultiByteToWideChar(CodePage, 0, pIn,  InChars,
                                                                pOut, OutChars);
                if (0 < xlated)
                {
                    InsertNullPadding(pOut, &OutChars, xlated);
                    break;
                }
                // If necessary, try the ANSI codepage, too.
                if (CP_ACP == CodePage)
                {
                    hr = HRESULT_FROM_WIN32(GetLastError());
                    break;
                }
                CodePage = CP_ACP;
            }
        }
    }

    return SUCCEEDED(hr)? hr
         : ShowConvertResult(hr, pIn, pStringName,
                             TEXT("Error converting %hs")
                             TEXT(" \"%.128hs\" to Unicode: %s"));
}

HRESULT
WiFUtils::
ConvertString(
  __out_ecount(OutChars) OUT char        *pOut,
                         IN  const WCHAR *pIn,
                         IN  int          OutChars,
                         IN  const char  *pStringName,
                         IN  int          InChars,
                         IN  UINT         CodePage)
{
    HRESULT hr = S_OK;

    if (NULL == pOut)
    {
        assert(NULL != pOut);
        hr = E_INVALIDARG;
    }
    else
    if (0 < OutChars)
    {
        if (0 == InChars || NULL == pIn || L'\0' == pIn[0])
        {
            memset(pOut, 0, OutChars);
        }
        else
        {
            if (InChars <  0)
                InChars = -1;
            else
            {
                size_t inSize;
                if (SUCCEEDED(StringCchLengthW(pIn, InChars, &inSize)))
                    InChars = static_cast<int>(inSize);
            }

            // If necessary, try the conversion again with the ANSI codepage.
            for (;;)
            {
                int xlated = ::WideCharToMultiByte(CodePage, 0, pIn,  InChars,
                                                                pOut, OutChars,
                                                                NULL, NULL);
                if (0 < xlated)
                {
                    InsertNullPadding(pOut, &OutChars, xlated);
                    break;
                }
                // If necessary, try the ANSI codepage, too.
                if (CP_ACP == CodePage)
                {
                    hr = HRESULT_FROM_WIN32(GetLastError());
                    break;
                }
                CodePage = CP_ACP;
            }
        }
    }

    return SUCCEEDED(hr)? hr
         : ShowConvertResult(hr, pIn, pStringName,
                             TEXT("Error converting %hs")
                             TEXT(" \"%.128ls\" to ASCII: %s"));
}

HRESULT
WiFUtils::
ConvertString(
  __out_ecount(OutChars) OUT WCHAR       *pOut,
                         IN  const WCHAR *pIn,
                         IN  int          OutChars,
                         IN  const char  *pStringName,
                         IN  int          InChars,
                         IN  UINT         CodePage)
{
    HRESULT hr = S_OK;

    if (NULL == pOut)
    {
        assert(NULL != pOut);
        hr = E_INVALIDARG;
    }
    else
    if (0 < OutChars)
    {
        int excessChars = InsertNullPadding(pOut, &OutChars, InChars);

        hr = StringCchCopyExW(pOut, OutChars, pIn, NULL, NULL,
                              STRSAFE_FILL_BEHIND_NULL
                            | STRSAFE_IGNORE_NULLS);
        
        if (excessChars && STRSAFE_E_INSUFFICIENT_BUFFER == hr)
        {
            hr = S_OK;
        }
    }

    return SUCCEEDED(hr)? hr
         : ShowConvertResult(hr, pIn, pStringName,
                             TEXT("Error copying %hs \"%.128ls\": %s"));
}

HRESULT
WiFUtils::
ConvertString(
    OUT ce::string  *pOut,
    IN  const char  *pIn,
    IN  const char  *pStringName,
    IN  int          InChars,
    IN  UINT         CodePage)
{
    HRESULT hr = S_OK;

    if (NULL == pOut)
    {
        assert(NULL != pOut);
        hr = E_INVALIDARG;
    }
    else
    if (0 == InChars || NULL == pIn || '\0' == pIn[0])
    {
        pOut->clear();
    }
    else
    if (0 > InChars)
    {
        if (!pOut->assign(pIn))
            hr = E_OUTOFMEMORY;
    }
    else
    {
        size_t inSize;
        if (FAILED(StringCchLengthA(pIn, InChars, &inSize)))
            inSize = static_cast<size_t>(InChars);

        if (!pOut->assign(pIn, inSize))
            hr = E_OUTOFMEMORY;
    }

    return SUCCEEDED(hr)? hr
         : ShowConvertResult(hr, pIn, pStringName,
                             TEXT("Error copying %hs \"%.128hs\": %s"));
}

HRESULT
WiFUtils::
ConvertString(
    OUT ce::wstring *pOut,
    IN  const char  *pIn,
    IN  const char  *pStringName,
    IN  int          InChars,
    IN  UINT         CodePage)
{
    HRESULT hr = S_OK;

    if (NULL == pOut)
    {
        assert(NULL != pOut);
        hr = E_INVALIDARG;
    }
    else
    if (0 == InChars || NULL == pIn || '\0' == pIn[0])
    {
        pOut->clear();
    }
    else
    {
        if (InChars <  0)
            InChars = -1;
        else
        {
            size_t inSize;
            if (SUCCEEDED(StringCchLengthA(pIn, InChars, &inSize)))
                InChars = static_cast<int>(inSize);
        }
        
        int xlated = ::MultiByteToWideChar(CodePage, 0, pIn,  InChars,
                                                        NULL, 0);

        // If necessary, try the ANSI codepage, too.
        if (0 >= xlated && CP_ACP != CodePage)
        {
            CodePage = CP_ACP;
            xlated = ::MultiByteToWideChar(CodePage, 0, pIn,  InChars,
                                                        NULL, 0);
        }
                
        if (0 >= xlated)
            hr = HRESULT_FROM_WIN32(GetLastError());
        else
        if (!pOut->reserve(xlated))
            hr = E_OUTOFMEMORY;
        else
        {
            xlated = ::MultiByteToWideChar(CodePage, 0, pIn,  InChars,
                                                        pOut->get_buffer(),
                                       static_cast<int>(pOut->capacity()));
            if (0 >= xlated)
                 hr = HRESULT_FROM_WIN32(GetLastError());
            else pOut->resize(xlated);
        }
    }
    
    return SUCCEEDED(hr)? hr
         : ShowConvertResult(hr, pIn, pStringName,
                             TEXT("Error converting %hs")
                             TEXT(" \"%.128hs\" to Unicode: %s"));
}

HRESULT
WiFUtils::
ConvertString(
    OUT ce::string  *pOut,
    IN  const WCHAR *pIn,
    IN  const char  *pStringName,
    IN  int          InChars,
    IN  UINT         CodePage)
{
    HRESULT hr = S_OK;

    if (NULL == pOut)
    {
        assert(NULL != pOut);
        hr = E_INVALIDARG;
    }
    else
    if (0 == InChars || NULL == pIn || L'\0' == pIn[0])
    {
        pOut->clear();
    }
    else
    {
        if (InChars <  0)
            InChars = -1;
        else
        {
            size_t inSize;
            if (SUCCEEDED(StringCchLengthW(pIn, InChars, &inSize)))
                InChars = static_cast<int>(inSize);
        }

        int xlated = ::WideCharToMultiByte(CodePage, 0, pIn,  InChars,
                                                        NULL, 0,
                                                        NULL, NULL);

        // If necessary, try the ANSI codepage, too.
        if (0 >= xlated && CP_ACP != CodePage)
        {
            CodePage = CP_ACP;
            xlated = ::WideCharToMultiByte(CodePage, 0, pIn,  InChars,
                                                        NULL, 0,
                                                        NULL, NULL);
        }
                
        if (0 >= xlated)
            hr = HRESULT_FROM_WIN32(GetLastError());
        else
        if (!pOut->reserve(xlated))
            hr = E_OUTOFMEMORY;
        else
        {
            xlated = ::WideCharToMultiByte(CodePage, 0, pIn,  InChars,
                                                        pOut->get_buffer(),
                                       static_cast<int>(pOut->capacity()),
                                                        NULL, NULL);
            if (0 >= xlated)
                 hr = HRESULT_FROM_WIN32(GetLastError());
            else pOut->resize(xlated);
        }
    }

    return SUCCEEDED(hr)? hr
         : ShowConvertResult(hr, pIn, pStringName,
                             TEXT("Error converting %hs")
                             TEXT(" \"%.128ls\" to ASCII: %s"));
}

HRESULT
WiFUtils::
ConvertString(
    OUT ce::wstring *pOut,
    IN  const WCHAR *pIn,
    IN  const char  *pStringName,
    IN  int          InChars,
    IN  UINT         CodePage)
{
    HRESULT hr = S_OK;

    if (NULL == pOut)
    {
        assert(NULL != pOut);
        hr = E_INVALIDARG;
    }
    else
    if (0 == InChars || NULL == pIn || L'\0' == pIn[0])
    {
        pOut->clear();
    }
    else
    if (0 > InChars)
    {
        if (!pOut->assign(pIn))
            hr = E_OUTOFMEMORY;
    }
    else
    {
        size_t inSize;
        if (FAILED(StringCchLengthW(pIn, InChars, &inSize)))
            inSize = static_cast<size_t>(InChars);

        if (!pOut->assign(pIn, inSize))
            hr = E_OUTOFMEMORY;
    }

    return SUCCEEDED(hr)? hr
         : ShowConvertResult(hr, pIn, pStringName,
                             TEXT("Error copying %hs \"%.128ls\": %s"));
}

// ----------------------------------------------------------------------------
//
// Safely copies a string into a fixed-length buffer.
// (Limits output size to MAX_PATH*100 or fewer characters.)
//
void
ce::qa::
SafeCopy(
  __out_ecount(OutChars) OUT char        *pOut,
                         IN  const char  *pIn,
                         IN  int          OutChars)
{
    if (NULL == pOut)
    {
        assert(NULL != pOut);
    }
    else
    if (0 < OutChars)
    {
        if (OutChars > MAX_PATH*100)
            OutChars = MAX_PATH*100;
        HRESULT hr = StringCchCopyExA(pOut, OutChars, pIn, 
                                      NULL, NULL, STRSAFE_IGNORE_NULLS);
        if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr)
             pOut[0]          = '\0';
        else pOut[OutChars-1] = '\0';
    }
}

void
ce::qa::
SafeCopy(
  __out_ecount(OutChars) OUT WCHAR       *pOut,
                         IN  const WCHAR *pIn,
                         IN  int          OutChars)
{
    if (NULL == pOut)
    {
        assert(NULL != pOut);
    }
    else
    if (0 < OutChars)
    {
        if (OutChars > MAX_PATH*100)
            OutChars = MAX_PATH*100;
        HRESULT hr = StringCchCopyExW(pOut, OutChars, pIn, 
                                      NULL, NULL, STRSAFE_IGNORE_NULLS);
        if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr)
             pOut[0]          = L'\0';
        else pOut[OutChars-1] = L'\0';
    }
}

// ----------------------------------------------------------------------------
//
// Safely appends a string to the end of a fixed-length buffer.
// (Limits output size to MAX_PATH*100 or fewer characters.)
//
void
ce::qa::
SafeAppend(
  __out_ecount(OutChars) OUT char        *pOut,
                         IN  const char  *pIn,
                         IN  int          OutChars)
{
    if (NULL == pOut)
    {
        assert(NULL != pOut);
    }
    else
    if (0 < OutChars)
    {
        if (OutChars > MAX_PATH*100)
            OutChars = MAX_PATH*100;
        HRESULT hr = StringCchCatExA(pOut, OutChars, pIn, 
                                     NULL, NULL, STRSAFE_IGNORE_NULLS);
        if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr)
             pOut[0]          = '\0';
        else pOut[OutChars-1] = '\0';
    }
}

void
ce::qa::
SafeAppend(
  __out_ecount(OutChars) OUT WCHAR       *pOut,
                         IN  const WCHAR *pIn,
                         IN  int          OutChars)
{
    if (NULL == pOut)
    {
        assert(NULL != pOut);
    }
    else
    if (0 < OutChars)
    {
        if (OutChars > MAX_PATH*100)
            OutChars = MAX_PATH*100;
        HRESULT hr = StringCchCatExW(pOut, OutChars, pIn, 
                                     NULL, NULL, STRSAFE_IGNORE_NULLS);
        if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr)
             pOut[0]          = L'\0';
        else pOut[OutChars-1] = L'\0';
    }
}

// ----------------------------------------------------------------------------
