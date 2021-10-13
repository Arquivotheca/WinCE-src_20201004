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
// Implementation of the APController_t class.
//
// ----------------------------------------------------------------------------

#include <MACAddr_t.hpp>

#include <assert.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//  
// Clears the address data.
//
void
MACAddr_t::
Clear(void)
{
    memset(m_Addr, 0, sizeof(m_Addr));
}

// ----------------------------------------------------------------------------
//  
// Inserts the specified binary data.
//
void
MACAddr_t::
Assign(
    const BYTE *pData,
    DWORD        DataLen)
{
    if (NULL == pData)
    {
        assert(NULL != pData);
    }
    else
    if (sizeof(m_Addr) > DataLen)
    {
        memset(m_Addr, 0, sizeof(m_Addr));
        if (0 < DataLen)
        {
            memcpy(m_Addr, pData, DataLen);
        }
    }
    else
    {
        memcpy(m_Addr, pData, sizeof(m_Addr));
    }
}

// ----------------------------------------------------------------------------
//  
// Determines whether the address has been set.
//
bool
MACAddr_t::
IsValid(void) const
{
    static BYTE zeroes[sizeof(m_Addr)] = {0,0,0,0,0,0};
    return memcmp(m_Addr, zeroes, sizeof(m_Addr)) != 0;
}

// ----------------------------------------------------------------------------
//  
// Translates the value to and from text.
//
void
MACAddr_t::
ToString(
  __out_ecount(BufferChars) char *pBuffer,
                            int    BufferChars) const
{
    if (NULL == pBuffer)
    {
        assert(NULL != pBuffer);
    }
    else
    {
        if (3 <= BufferChars)
        {
            static const char *hexmap = "0123456789ABCDEF";
            const char *bufend = &pBuffer[BufferChars - 3];
            if (pBuffer < bufend)
            {
                for (int bx = 0 ; bx < sizeof(m_Addr) && pBuffer < bufend ; ++bx)
                {
                    *(pBuffer++) = hexmap[(m_Addr[bx] >> 4) & 0xf];
                    *(pBuffer++) = hexmap[ m_Addr[bx]       & 0xf];
                    *(pBuffer++) = ':';
                }
                pBuffer--;
            }
        }
        *pBuffer = '\0';
    }
}

void
MACAddr_t::
ToString(
  __out_ecount(BufferChars) WCHAR *pBuffer,
                            int     BufferChars) const
{
    if (NULL == pBuffer)
    {
        assert(NULL != pBuffer);
    }
    else
    {
        if (3 <= BufferChars)
        {
            static const WCHAR *hexmap = L"0123456789ABCDEF";
            const WCHAR *bufend = &pBuffer[BufferChars - 3];
            if (pBuffer < bufend)
            {
                for (int bx = 0 ; bx < sizeof(m_Addr) && pBuffer < bufend ; ++bx)
                {
                    *(pBuffer++) = hexmap[(m_Addr[bx] >> 4) & 0xf];
                    *(pBuffer++) = hexmap[ m_Addr[bx]       & 0xf];
                    *(pBuffer++) = L':';
                }
                pBuffer--;
            }
        }
        *pBuffer = L'\0';
    }
}

HRESULT
MACAddr_t::
FromString(
    const char *pString)
{
    if (NULL == pString)
    {
        assert(NULL != pString);
        return E_INVALIDARG;
    }
    else
    {
        for (int bx = 0 ; bx < sizeof(m_Addr) ; ++bx)
        {
            unsigned int abyte = 0;
            for (int nx = 0 ; nx < 2 ; ++nx)
            {
                unsigned int ch = static_cast<unsigned int>(*(pString++));
                switch (ch)
                {
                    case '0': case '1': case '2': case '3': case '4':
                    case '5': case '6': case '7': case '8': case '9':
                        ch -= '0';
                        break;
                    case 'a': case 'b': case 'c':
                    case 'd': case 'e': case 'f':
                        ch -= 'a' - 10;
                        break;
                    case 'A': case 'B': case 'C':
                    case 'D': case 'E': case 'F':
                        ch -= 'A' - 10;
                        break;
                    default:
                        return E_INVALIDARG;
                }
                abyte <<= 4;
                abyte |= ch;
            }
            m_Addr[bx] = static_cast<BYTE>(abyte);
            if (*pString == '.' || *pString == ':' || *pString == '-')
                 pString++;
        }
        return S_OK;
    }
}

HRESULT
MACAddr_t::
FromString(
    const WCHAR *pString)
{
    if (NULL == pString)
    {
        assert(NULL != pString);
        return E_INVALIDARG;
    }
    else
    {
        for (int bx = 0 ; bx < sizeof(m_Addr) ; ++bx)
        {
            unsigned int abyte = 0;
            for (int nx = 0 ; nx < 2 ; ++nx)
            {
                unsigned int ch = static_cast<unsigned int>(*(pString++));
                switch (ch)
                {
                    case L'0': case L'1': case L'2': case L'3': case L'4':
                    case L'5': case L'6': case L'7': case L'8': case L'9':
                        ch -= L'0';
                        break;
                    case L'a': case L'b': case L'c':
                    case L'd': case L'e': case L'f':
                        ch -= L'a' - 10;
                        break;
                    case L'A': case L'B': case L'C':
                    case L'D': case L'E': case L'F':
                        ch -= L'A' - 10;
                        break;
                    default:
                        return E_INVALIDARG;
                }
                abyte <<= 4;
                abyte |= ch;
            }
            m_Addr[bx] = static_cast<BYTE>(abyte);
            if (*pString == L'.' || *pString == L':' || *pString == L'-')
                 pString++;
        }
        return S_OK;
    }
}

// ----------------------------------------------------------------------------
