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
// Definitions and declarations for the WZCConfigList_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WZCConfigList_t_
#define _DEFINED_WZCConfigList_t_
#pragma once

#include "WZCConfigItem_t.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// A list of WZCNetConfig objects.
// This is used for both the Available and Preferred Networks lists in
// the WZCIntfEntry class.
//
class WZCConfigList_t
{
public:

    // Copies or deallocates the specified WZC network-configuration
    // list structure:
    static DWORD
    CopyConfigList(RAW_DATA *pDest, const RAW_DATA &Source);
    static void
    FreeConfigList(RAW_DATA *pData);

private:

    // Reference-counted RAW_DATA pointer:
    typedef WZCStructPtr_t<RAW_DATA,
                           DWORD (*)(RAW_DATA *,
                               const RAW_DATA &),
                           WZCConfigList_t::CopyConfigList,
                           void (*)(RAW_DATA *),
                           WZCConfigList_t::FreeConfigList> WZCConfigListPtr_t;


    // WZC list data;
    WZCConfigListPtr_t m_pData;

    // List name:
    TCHAR m_ListName[32];

public:

    // Constructor and destructor:
    WZCConfigList_t(void);
   ~WZCConfigList_t(void);

    // Copy and assignment:
    explicit WZCConfigList_t(const WZCConfigList_t &src);
    WZCConfigList_t &operator = (const WZCConfigList_t &src);

    // Constructs or assigns this object from a RAW_DATA structure:
    explicit WZCConfigList_t(const RAW_DATA &src);
    WZCConfigList_t &operator = (const RAW_DATA &src);

    // Retrieves the underlying RAW_DATA structure:
    operator const RAW_DATA & (void) const { return *m_pData; }

    // Makes sure this object contains a private copy of the WZC data:
    DWORD
    Privatize(void);

    // Gets the count of network configurations in the list:
    int
    Size(void) const;

    // Gets or sets the name of the list:
    const TCHAR *
    GetListName(void) const;
    void
    SetListName(const TCHAR *pListName);

    // Retrieves a copy of the network configuration element describing
    // the specified SSID and/or MAC address.
    // Returns ERROR_NOT_FOUND if there is no match.
    // Note that when only selecting based on the SSID, more than one
    // list element may contain a matching SSID. In that case, the method
    // will retrieve the first match.
    DWORD
    Find(
        const TCHAR     *pSSIDName,     // optional
        const MACAddr_t *pMACAddress,   // optional
        WZCConfigItem_t *pConfig) const;

    // Inserts the specified element at the head of the list:
    DWORD
    AddHead(
        const WZCConfigItem_t &Config);

    // Inserts the specified element at the tail of the list:
    DWORD
    AddTail(
        const WZCConfigItem_t &Config);

    // Removes the specfied element from the list:
    // Returns ERROR_NOT_FOUND if there is no match.
    DWORD
    Remove(
        const WZCConfigItem_t &Config);

    // Logs the list preceded by the specified header:
    void
    Log(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true) const; // display all available info
};

};
};
#endif /* _DEFINED_WZCConfigList_t_ */
// ----------------------------------------------------------------------------
