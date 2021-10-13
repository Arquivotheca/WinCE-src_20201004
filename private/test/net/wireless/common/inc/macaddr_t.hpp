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
// Definitions and declarations for the MACAddr_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_MACAddr_t_
#define _DEFINED_MACAddr_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>

namespace litexml
{
    class XmlBaseElement_t;
    class XmlElement_t;
};

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// MAC (Medium Access Control) address.
//
struct MACAddr_t
{
    BYTE m_Addr[6];

    MACAddr_t(const WCHAR *pString);

    MACAddr_t(void) { Clear(); }

    // Clears the address data:
    void
    Clear(void)
    {
        memset(m_Addr, 0, sizeof(m_Addr));
    }

    // Inserts the specified binary data:
    void
    Assign(
        const BYTE *pData,
        DWORD        DataLen);

    // Compares two MAC addresses:
    int
    Compare(
        const MACAddr_t &rhs) const
    {
        return memcmp(m_Addr, rhs.m_Addr, sizeof(m_Addr));
    }
    
    // Determines whether the address has been set:
    bool
    IsValid(void) const;

    // Translates the value to and from text:
    HRESULT
    ToString(
      __out_ecount(BufferChars) char *pBuffer,
                                int    BufferChars) const;
    HRESULT
    ToString(
      __out_ecount(BufferChars) WCHAR *pBuffer,
                                int     BufferChars) const;
    HRESULT
    FromString(
        const char *pString);
    HRESULT
    FromString(
        const WCHAR *pString);

    // Translates the value to and from XML:
    HRESULT
    AddXmlElement(
        litexml::XmlElement_t  *pRoot,
        const WCHAR            *pName,
        litexml::XmlElement_t **ppElem = NULL) const;   
    HRESULT
    AddXmlAttribute(
        litexml::XmlElement_t *pElem,
        const WCHAR           *pName) const;
    HRESULT
    GetXmlElement(
        const litexml::XmlElement_t  &Root,
        const WCHAR                  *pName,
        const litexml::XmlElement_t **ppElem = NULL);
    HRESULT
    GetXmlAttribute(
        const litexml::XmlBaseElement_t &Elem,
        const WCHAR                     *pName);
    
};

// Comparison operators:
inline bool
operator == (const MACAddr_t &lhs, const MACAddr_t &rhs)
{
    return lhs.Compare(rhs) == 0;
}

inline bool
operator != (const MACAddr_t &lhs, const MACAddr_t &rhs)
{
    return lhs.Compare(rhs) != 0;
}

inline bool
operator < (const MACAddr_t &lhs, const MACAddr_t &rhs)
{
    return lhs.Compare(rhs) < 0;
}

inline bool
operator > (const MACAddr_t &lhs, const MACAddr_t &rhs)
{
    return lhs.Compare(rhs) > 0;
}

inline bool
operator <= (const MACAddr_t &lhs, const MACAddr_t &rhs)
{
    return lhs.Compare(rhs) <= 0;
}

inline bool
operator >= (const MACAddr_t &lhs, const MACAddr_t &rhs)
{
    return lhs.Compare(rhs) >= 0;
}

};
};
#endif /* _DEFINED_MACAddr_t_ */
// ----------------------------------------------------------------------------
