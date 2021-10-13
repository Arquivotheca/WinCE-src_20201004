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
// Definitions and declarations for the WZCIntfEntry_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WZCIntfEntry_t_
#define _DEFINED_WZCIntfEntry_t_
#pragma once

#include "MACAddr_t.hpp"
#include "WZCConfigList_t.hpp"

// ----------------------------------------------------------------------------
//
// WPA2 definitions for temporary backward-compatibility.
//
#ifndef INTF_ENTRY_V1
    #define WZC_INTF_ENTRY INTF_ENTRY
    #undef  WZC_IMPLEMENTS_WPA2
#else
    #define WZC_INTF_ENTRY INTF_ENTRY_EX
    #define WZC_IMPLEMENTS_WPA2
    inline VOID
    WZCDeleteIntfObj(
        PINTF_ENTRY_EX pIntf)
    {
        WZCDeleteIntfObjEx(pIntf);
    }
    inline DWORD
    WZCQueryInterface(
        LPWSTR          pSrvAddr,
        DWORD           dwInFlags,
        PINTF_ENTRY_EX  pIntf,
        LPDWORD         pdwOutFlags)
    {
        return WZCQueryInterfaceEx(pSrvAddr, dwInFlags, pIntf, pdwOutFlags);
    }
    inline DWORD
    WZCSetInterface(
        LPWSTR          pSrvAddr,
        DWORD           dwInFlags,
        PINTF_ENTRY_EX  pIntf,
        LPDWORD         pdwOutFlags)
    {
        return WZCSetInterfaceEx(pSrvAddr, dwInFlags, pIntf, pdwOutFlags);
    }
    inline DWORD
    WZCRefreshInterface(
        LPWSTR          pSrvAddr,
        DWORD           dwInFlags,
        PINTF_ENTRY_EX  pIntf,
        LPDWORD         pdwOutFlags)
    {
        return WZCRefreshInterfaceEx(pSrvAddr, dwInFlags, pIntf, pdwOutFlags);
    }
#endif

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// A class implementation of the INTF_ENTRY structure.
// Contains all the WZC information for a single wireless adapter.
//
class WZCIntfEntry_t
{
private:

    // WZC adapter information:
    WZC_INTF_ENTRY m_Intf;
    DWORD          m_IntfFlags;
    bool           m_IntfPopulated;

    // Copy and assignment are deliberately disabled:
    WZCIntfEntry_t(const WZCIntfEntry_t &src);
    WZCIntfEntry_t &operator = (const WZCIntfEntry_t &src);

public:

    // Constructor and destructor:
    WZCIntfEntry_t(void);
   ~WZCIntfEntry_t(void);

    // Inserts the specified INTF_ENTRY structure:
    void
    Assign(
        const WZC_INTF_ENTRY *pIntf,
        DWORD                  IntfFlags);

    // Retrieves the current INTF_ENTRY structure or flags:
    const WZC_INTF_ENTRY &
    GetIntfEntry(void) const;
    DWORD
    GetIntfFlags(void) const;

    // Determines whether the wireless adapter is connected:
    bool
    IsConnected(void) const;

    // Gets the MAC address or SSID of the connected station:
    // Returns ERROR_NOT_FOUND if adapter is not connected.
    DWORD
    GetConnectedMAC(
        MACAddr_t *pMAC) const;
    DWORD
    GetConnectedSSID(
      __out_ecount(SSIDChars) TCHAR *pSSID,
                              int     SSIDChars) const;

    // Checks whether the adapter supports the specified security modes:
    // Non-WPA2 version just checks whether modes are legal.
    bool
    IsSecurityModeSupported(
        NDIS_802_11_AUTHENTICATION_MODE AuthMode,
        ULONG                           CipherMode,
        DWORD                           EAPType) const;

    // Retreives a copy of the Available or Preferred list:
    DWORD
    GetAvailableNetworks(
        WZCConfigList_t *pConfigList) const;
    DWORD
    GetPreferredNetworks(
        WZCConfigList_t *pConfigList) const;

    // Logs the current adapter information:
    void
    Log(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool Verbose = true) const;  // logs all available information
};

};
};
#endif /* _DEFINED_WZCIntfEntry_t_ */
// ----------------------------------------------------------------------------
