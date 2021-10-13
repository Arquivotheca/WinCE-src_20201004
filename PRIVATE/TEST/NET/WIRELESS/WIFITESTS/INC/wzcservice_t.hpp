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
// Definitions and declarations for the WZCService_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WZCService_t_
#define _DEFINED_WZCService_t_
#pragma once

#include "WZCIntfEntry_t.hpp"
#include <eapol.h>
#include <wzcsapi.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// A simplified interface for the WZC (Wireless Zero Config) libraries.
//
class WZCService_t
{
private:

    // Current wireless adapter:
    enum { MaxAdapterNameChars = 64 };
    WCHAR m_AdapterGuid[MaxAdapterNameChars];
    TCHAR m_AdapterName[MaxAdapterNameChars];
    bool  m_AdapterPopulated;

    // Copy and assignment are deliberately disabled:
    WZCService_t(const WZCService_t &src);
    WZCService_t &operator = (const WZCService_t &src);

public:

    // Constructor and destructor:
    WZCService_t(void);
   ~WZCService_t(void);

    // Determines whether the system has any wireless adapters:
    bool
    HasAdapter(void);

    // Specifies the wireless adapter to be controlled:
    // A null or empty name selects the first adapter on the system.
    // Returns an error if the specified adapter doesn't exist.
    DWORD
    SetAdapterName(
        const TCHAR *pAdapterName);

    // Gets the name of the wireless adapter being controlled:
    // Returns an empty string if the system has no wireless adapters.
    // Otherwise, returns either the adapter selected by SetAdapterName
    // or the first adapter on the system.
    const WCHAR *
    GetAdapterGuid(void);   // wide characters
    const TCHAR *
    GetAdapterName(void);   // flexibible

    // Queries the current wireless adapter information:
    DWORD
    QueryAdapterInfo(
        WZCIntfEntry_t *pIntf);

    // Enables or disables the WZC service:
    DWORD
    Enable(void);
    DWORD
    Disable(void);

    // Clears the current Preferred Networks List:
    // This will disconnect everything attached to the wireless adapter.
    DWORD
    Clear(void);

    // Refreshes WZC to force a reconnection:
    DWORD
    Refresh(void);

    // Replaces the Preferred Networks list:
    // This will schedule an immediate reconnection.
    DWORD
    SetPreferredList(
        const WZCConfigList_t &ConfigList);

    // Inserts the specified element at the head of the Preferred
    // Networks list:
    DWORD
    AddToHeadOfPreferredList(
        const WZCConfigItem_t &Config);

    // Inserts the specified element at the tail of the Preferred
    // Networks list:
    DWORD
    AddToTailOfPreferredList(
        const WZCConfigItem_t &Config);

    // Removes the specfied Access Point from the Preferred Networks list:
    // If the optional MAC address is not supplied, the method will remove
    // the first Access Point with the specified SSID.
    // If, in addition, the RemoveAllMatches flag is true, all the matching
    // Access Point configuration entries will be removed.
    // Returns ERROR_NOT_FOUND if there was no match.
    DWORD
    RemoveFromPreferredList(
        const TCHAR     *pSSIDName,
        const MACAddr_t *pMACAddress = NULL,
        bool             RemoveAllMatches = false);

    // Set the interval at which WZCSVC tries to determine available BSSIDs
    // when disconnected
    // Default 60 sec
    static DWORD
    SetFailedScanInterval(
        const DWORD IntervalMsec); // in millisec

    // Get the interval at which WZCSVC tries to determine available BSSIDs
    // when disconnected
    static DWORD
    GetFailedScanInterval(
        DWORD *pIntervalMsec); // in millisec
    
    // Set the interval at which WZCSVC tries to determine available BSSIDs
    // when associated to a network
    // Default 60 sec
    static DWORD
    SetAssociatedScanInterval(
        const DWORD IntervalMsec); // in millisec

    // Get the interval at which WZCSVC tries to determine available BSSIDs
    // when associated to a network
    static DWORD
    GetAssociatedScanInterval(
        DWORD *pIntervalMsec); // in millisec
    

  // Utility methods:

    // Logs the current WZC context information:
    static DWORD
    LogWZCContext(
        void (*LogFunc)(const TCHAR *pFormat, ...));

    // Monitors the adapter's status until the specified time runs out 
    // or it associates with the specified Access Point:
    // Returns ERROR_TIMEOUT if the association isn't established within
    // the time-limit.
    DWORD
    MonitorAssociation(
        const TCHAR *pExpectedSSID,
        long         TimeLimit); // milliseconds
    
    // Translates the specified authentication-mode into text form:
    static const TCHAR *
    AuthenticationMode2Text(
        int AuthMode);
    
    // Translates the specified infrastructure-mode into text form:
    static const TCHAR *
    InfrastructureMode2Text(
        int InfraMode);
    
    // Translates the specified network-type into text form:
    static const TCHAR *
    NetworkType2Text(
        int NetworkType);
    
    // Translates the specified encryption-mode into text form:
    static const TCHAR *
    PrivacyMode2Text(
        int PrivacyMode);
    
    // Translates the specified WEP-status code into text form:
    static const TCHAR *
    WEPStatus2Text(
        int WEPStatus);

    // Formats the specified SSID into text form:
    static HRESULT
    SSID2Text(
                                const BYTE *pData,
                                DWORD        DataLen,
      __out_ecount(BufferChars) TCHAR      *pBuffer,
                                int          BufferChars);
    
    // Calculates the 802.11b channel number for given frequency:
    // Returns 1-14 based on the given ulFrequency_kHz.
    // Returns 0 for invalid frequency range.
    static UINT
    ChannelNumber2Int(
        ULONG ulFrequency_kHz); // frequency in kHz
};

};
};
#endif /* _DEFINED_WZCService_t_ */
// ----------------------------------------------------------------------------
