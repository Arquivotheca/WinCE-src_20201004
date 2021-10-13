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

#include <WEPKeys_t.hpp>

#include <assert.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//  
// Clears the key data.
//
void
WEPKeys_t::
Clear(void)
{
    memset(this, 0, sizeof(*this));
}

// ----------------------------------------------------------------------------
//  
// Determines whether the keys have been set.
//
bool
WEPKeys_t::
IsValid(void) const
{
    return (0 <= m_KeyIndex && m_KeyIndex < 4
            && m_Size[m_KeyIndex-1] != BYTE(0));
}

// ----------------------------------------------------------------------------
//  
// Determines whether the specified key size is valid.
//
bool
WEPKeys_t::
ValidKeySize(int KeySize)
{
    return (KeySize == 5 || KeySize == 13 || KeySize == 16 || KeySize == 32);
}

// ----------------------------------------------------------------------------
//  
// Translates the specified key value to text.
//
void
WEPKeys_t::
ToString(
                            int    KeyIndex,
  __out_ecount(BufferChars) char *pBuffer,
                            int    BufferChars) const
{
    if (NULL == pBuffer)
    {
        assert(NULL != pBuffer);
    }
    else
    {
        if (0 > KeyIndex || KeyIndex >= 4 || 3 > BufferChars)
        {
            assert(0 <= KeyIndex && KeyIndex < 4);
        }
        else
        {
            static const char *hexmap = "0123456789ABCDEF";
            int keySize = static_cast<int>(m_Size[KeyIndex]);
            if (keySize > 0)
            {
                const char *bufend = &pBuffer[BufferChars - 3];
                if (pBuffer < bufend)
                {
                    if (keySize > sizeof(m_Material[0]))
                        keySize = sizeof(m_Material[0]);
                    const BYTE *keymat = &m_Material[KeyIndex][0];
                    const BYTE *keyend = &m_Material[KeyIndex][keySize];
                    for (; keymat < keyend && pBuffer < bufend ; ++keymat)
                    {
                        *(pBuffer++) = hexmap[(*keymat >> 4) & 0xf];
                        *(pBuffer++) = hexmap[ *keymat       & 0xf];
                        *(pBuffer++) = '.';
                    }
                    pBuffer--;
                }
            }
        }
        *pBuffer = '\0';
    }
}

void
WEPKeys_t::
ToString(
                            int     KeyIndex,
  __out_ecount(BufferChars) WCHAR *pBuffer,
                            int     BufferChars) const
{
    if (NULL == pBuffer)
    {
        assert(NULL != pBuffer);
    }
    else
    {
        if (0 > KeyIndex || KeyIndex >= 4 || 3 > BufferChars)
        {
            assert(0 <= KeyIndex && KeyIndex < 4);
        }
        else
        {
            static const WCHAR *hexmap = L"0123456789ABCDEF";
            int keySize = static_cast<int>(m_Size[KeyIndex]);
            if (keySize > 0)
            {
                const WCHAR *bufend = &pBuffer[BufferChars - 3];
                if (pBuffer < bufend)
                {
                    if (keySize > sizeof(m_Material[0]))
                        keySize = sizeof(m_Material[0]);
                    const BYTE *keymat = &m_Material[KeyIndex][0];
                    const BYTE *keyend = &m_Material[KeyIndex][keySize];
                    for (; keymat < keyend && pBuffer < bufend ; ++keymat)
                    {
                        *(pBuffer++) = hexmap[(*keymat >> 4) & 0xf];
                        *(pBuffer++) = hexmap[ *keymat       & 0xf];
                        *(pBuffer++) = L'.';
                    }
                    pBuffer--;
                }
            }
        }
        *pBuffer = L'\0';
    }
}

// ----------------------------------------------------------------------------
//  
// Translates the specified text to key value.
// Also sets the current key-index.
//
HRESULT
WEPKeys_t::
FromString(
    int         KeyIndex,
    const char *pString)
{
    if (0 > KeyIndex || KeyIndex >= 4 || NULL == pString)
    {
        assert(0 <= KeyIndex && KeyIndex < 4);
        assert(NULL != pString);
    }
    else
    {
              BYTE *keymat = &m_Material[KeyIndex][0];
        const BYTE *keyend = &m_Material[KeyIndex][sizeof(m_Material[0])];

        int mx = 0;
        for (; keymat < keyend && *pString ; ++mx, ++keymat)
        {
            unsigned int kbyte = 0;
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
                kbyte <<= 4;
                kbyte |= ch;
            }
           *keymat = static_cast<BYTE>(kbyte);

            if (*pString == '.' || *pString == ':' || *pString == '-')
                 pString++;
            else
            if (*pString == '\0')
            {
                mx++;
                break;
            }
        }

        if (ValidKeySize(mx))
        {
            m_KeyIndex = static_cast<BYTE>(KeyIndex);
            m_Size[KeyIndex] = static_cast<BYTE>(mx);
            return S_OK;
        }
    }

    return E_INVALIDARG;
}

HRESULT
WEPKeys_t::
FromString(
    int          KeyIndex,
    const WCHAR *pString)
{
    if (0 > KeyIndex || KeyIndex >= 4 || NULL == pString)
    {
        assert(0 <= KeyIndex && KeyIndex < 4);
        assert(NULL != pString);
    }
    else
    {
              BYTE *keymat = &m_Material[KeyIndex][0];
        const BYTE *keyend = &m_Material[KeyIndex][sizeof(m_Material[0])];

        int mx = 0;
        for (; keymat < keyend && *pString ; ++mx, ++keymat)
        {
            unsigned int kbyte = 0;
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
                kbyte <<= 4;
                kbyte |= ch;
            }
           *keymat = static_cast<BYTE>(kbyte);

            if (*pString == L'.' || *pString == L':' || *pString == L'-')
                 pString++;
            else
            if (*pString == L'\0')
            {
                mx++;
                break;
            }
        }

        if (ValidKeySize(mx))
        {
            m_KeyIndex = static_cast<BYTE>(KeyIndex);
            m_Size[KeyIndex] = static_cast<BYTE>(mx);
            return S_OK;
        }
    }

    return E_INVALIDARG;
}

// ----------------------------------------------------------------------------
