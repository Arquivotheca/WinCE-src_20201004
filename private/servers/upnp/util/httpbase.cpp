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
#include <windows.h>

#include "HttpBase.h"


// shared constants
const CHAR c_szHttpVersion[] = {"HTTP/1.1"};
const CHAR c_szHttpTimeout[] = {"Second-1800"};

// static members
ce::string* HttpBase::m_pstrAgentName = NULL;


HttpBase::HttpBase()
{
}

// Initialize
bool HttpBase::Initialize(LPCSTR pszAgent, ...)
{
    if (m_pstrAgentName != NULL || pszAgent == NULL)
    {
        assert(m_pstrAgentName == NULL);
        assert(pszAgent != NULL);
        return false;
    }

    va_list arguments;
    va_start(arguments, pszAgent);

    if(m_pstrAgentName = new ce::string)
    {
#ifdef DEBUG
        m_pstrAgentName->reserve(1);
#else        
        m_pstrAgentName->reserve(strlen(pszAgent));
#endif        
        
        while (FAILED(StringCchVPrintfA(m_pstrAgentName->get_buffer(), m_pstrAgentName->capacity(), pszAgent, arguments)))
        {
            ce::string::size_type n = m_pstrAgentName->capacity();
            
            if(n > INTERNET_MAX_URL_LENGTH || !m_pstrAgentName->reserve(2 * n))
            {
                break;
            }
        }
        
        return true;
    }
    else
        return false;

    va_end(arguments);
}


ce::string *HttpBase::Identify(void)
{
    return m_pstrAgentName;
}

// Uninitialize
void HttpBase::Uninitialize(void)
{
    if (m_pstrAgentName != NULL)
    {
        delete m_pstrAgentName;
    }
    m_pstrAgentName = NULL;
}

