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
// Implentation of the CoreDllLoader_t class.
//
// ----------------------------------------------------------------------------

#include <CoreDllLoader_t.hpp>

using namespace CENetworkQA;

// ----------------------------------------------------------------------------
// 
// Constructs an object by retrieving the core DLL.
//
CoreDllLoader_t::
CoreDllLoader_t(void)
    : m_LoadErrorCode(0)
{
    m_DllHandle = LoadLibrary(TEXT("coredll.dll"));
    if (m_DllHandle == NULL) 
    {
        m_LoadErrorCode = GetLastError();
    }
}

// ----------------------------------------------------------------------------
//
// Destructor: unloads the DLL.
//
CoreDllLoader_t:: 
~CoreDllLoader_t(void)
{
    if (m_DllHandle != NULL)
    {
        FreeLibrary(m_DllHandle);
        m_DllHandle = NULL;
    }
}

// ----------------------------------------------------------------------------
// 
// Retrieves a pointer to the specified procedure.
//
FARPROC 
CoreDllLoader_t::
GetProcedureAddress(LPCTSTR pProcedureName) const
{
    if (!IsLoaded())
    {
        return NULL;
    }
    else
    {
#if defined(UNICODE) && defined(UNDER_NT)
        size_t bufferSize = wcslen(pProcedureName) * 4;
        LPSTR pBuffer = (LPSTR) LocalAlloc(0, (bufferSize + 1) * sizeof(char));
        if (NULL == pBuffer)
        {
            SetLastError(ERROR_OUTOFMEMORY);
            return NULL;
        }
        else
        {
            FARPROC ret;
            size_t convertSize = wcstombs(pBuffer, pProcedureName, bufferSize);            
            if ((size_t)-1 == convertSize)
            {
                SetLastError(ERROR_NO_UNICODE_TRANSLATION);
                ret = NULL;
            }
            else
            {
                pBuffer[bufferSize] = '\0';
                ret = GetProcAddress(m_DllHandle, pBuffer);
            }
            LocalFree(pBuffer);
            return ret;
        }
#else
        return GetProcAddress(m_DllHandle, pProcedureName);
#endif
    }
}

// ----------------------------------------------------------------------------
