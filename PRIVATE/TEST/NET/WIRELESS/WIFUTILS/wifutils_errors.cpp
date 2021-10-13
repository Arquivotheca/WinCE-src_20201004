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
#include <netcmn.h>
#include <strsafe.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Converts the specified Win32 or HRESULT error-code to text form.
//
const TCHAR *
__Win32ErrorText::
operator() (IN DWORD ErrorCode)
{
    HRESULT hr = StringCchPrintf(m_ErrorText, COUNTOF(m_ErrorText),
                                 TEXT("error=%u (0x%X:%s)"),
                                 ErrorCode, ErrorCode,
                                 ErrorToText(ErrorCode));
    if (ERROR_SUCCESS == ErrorCode || FAILED(hr))
    {
        m_ErrorText[0] = TEXT('n');
        m_ErrorText[1] = TEXT('o');
        m_ErrorText[2] = TEXT(' ');
        m_ErrorText[3] = TEXT('e');
        m_ErrorText[4] = TEXT('r');
        m_ErrorText[5] = TEXT('r');
        m_ErrorText[6] = TEXT('o');
        m_ErrorText[7] = TEXT('r');
        m_ErrorText[8] = TEXT('\0');
    }
    return m_ErrorText;
}

const TCHAR *
__HRESULTErrorText::
operator() (IN HRESULT ErrorCode)
{
    DWORD result = HRESULT_CODE(ErrorCode);
    HRESULT hr = StringCchPrintf(m_ErrorText, COUNTOF(m_ErrorText),
                                 TEXT("error=0x%X (%u:%s)"),
                                 ErrorCode, result,
                                 ErrorToText(result));
    if (SUCCEEDED(ErrorCode) || FAILED(hr))
    {
        m_ErrorText[0] = TEXT('n');
        m_ErrorText[1] = TEXT('o');
        m_ErrorText[2] = TEXT(' ');
        m_ErrorText[3] = TEXT('e');
        m_ErrorText[4] = TEXT('r');
        m_ErrorText[5] = TEXT('r');
        m_ErrorText[6] = TEXT('o');
        m_ErrorText[7] = TEXT('r');
        m_ErrorText[8] = TEXT('\0');
    }
    return m_ErrorText;
}

// ----------------------------------------------------------------------------
