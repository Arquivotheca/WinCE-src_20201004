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
// Definitions and declarations for the WLANService_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WLANService_t_
#define _DEFINED_WLANService_t_
#pragma once

#include "WLANTypes.hpp"
#include "Eapcfg.h"

#if (WLAN_VERSION > 0)

#include "WiFiHandle_t.hpp"
#include "WLANAPIIntf_t.hpp"

#include <string.hxx>
#include <vector.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Extends WiFiHandle to provide a simplified interface for the WLAN
// (Native WiFi / Auto Configuration) libraries.
//
class WiFiConn_t;
class WLANProfile_t;
class WLANService_t : public WiFiHandle_t
{
public:

    // Constructor / destructor:
    //
    WLANService_t(
        WiFiServiceType_e ServiceType);
  __override virtual
   ~WLANService_t(void);

    // Initializes the wireless service:
    // Returns ERROR_NOT_SUPPORTED if the service cannot be loaded.
    // Uses the optional adapter-names object, if supplied, to initialize
    // the adapter's name and/or GUID.
    //
  __override virtual DWORD
    InitService(
        const WiFiAdapterName *pAdapterName = NULL);

 // WLAN Context:

    // Gets the WLAN service handle:
    //
    HANDLE
    GetWlanHandle(void) const { return m_hWLAN; }

    // Enables or disables the WLAN service:
    //
  __override virtual DWORD
    Enable(void);
  __override virtual DWORD
    Disable(void);

    // Gets or sets the interval between scans for Available Networks
    // when the adapter is disconnected.
    // Default: 60 sec
    //
  __override virtual DWORD
    GetDisconnectedScanInterval(
        DWORD *pIntervalMsec); // in millisec
  __override virtual DWORD
    SetDisconnectedScanInterval(
        const DWORD IntervalMsec); // in millisec

    // Gets or sets the interval between scans for Available Networks
    // when the adapter is associated to a network.
    // Default: 60 sec
    //
  __override virtual DWORD
    GetAssociatedScanInterval(
        DWORD *pIntervalMsec); // in millisec
  __override virtual DWORD
    SetAssociatedScanInterval(
        const DWORD IntervalMsec); // in millisec

  // Adapter Management:

    // Checks whether the adapter supports the specified modes:
    //
  __override virtual bool
    IsSecurityModeSupported(
        const WiFiConfig_t &NetworkConfig);

  // Notifications:

    // Generates a notification-listener object for the wireless adapter:
    //
  __override virtual DWORD
    CreateListener(
        WiFiListener_t **ppListener);

  // Preferred-Networks Managment:

    // Clears the current Preferred Networks List:
    // This will disconnect everything attached to the wireless adapter.
    // If the optional wait-time is specified the function waits that 
    // long for the disconnection to finish.
    //
  __override virtual DWORD
    Clear(
        long DisconnectTimeMS = 0);

    // Refreshes WLAN to force a reconnection:
    //
  __override virtual DWORD
    Refresh(void);

    // Inserts the specified element at the head of the Preferred
    // Networks list:
    // As a side-effect, the added network is marked "current" so
    // WLAN will immediately attempt a connection.
    //
  __override virtual DWORD
    AddToHeadOfPreferredList(
        const WiFiConfig_t &Network);

    // Inserts the specified element at the tail of the Preferred
    // Networks list:
    //
  __override virtual DWORD
    AddToTailOfPreferredList(
        const WiFiConfig_t &Network);

    // Removes the specfied SSID from the Preferred Networks list:
    // By default, removes the first instance of the specified SSID.
    // If the RemoveAllMatches flag is true, all the matching Access
    // Point configuration entries will be removed.
    // Returns ERROR_NOT_FOUND if there was no match.
    //
  __override virtual DWORD
    RemoveFromPreferredList(
        const TCHAR *pSsidName,
        bool         RemoveAllMatches = false);

    // Checks if a particular SSID is within the Available Networks Lists:
    // Returns ERROR_TIMEOUT if the SSID is not scanned within the specified
    // time limit.
    //
  __override virtual DWORD
    CheckAvailableNetwork(
        const TCHAR *pSsidName,
        long         TimeLimitMS);

    // Configures the WiFi service to connect the specified Access Point
    // or Ad Hoc network:
    // If the optional EapCredentials object is supplied, uses it to
    // supply the user's security credientials for EAP authentication.
    //
  __override virtual DWORD
    ConnectNetwork(
        const WiFiConfig_t &Network,
        WiFiAccount_t      *pEapCredentials = NULL,
        WiFiAccount_t      *pEapPreCredentials = NULL);

    // Immediately connects the specified network:
    // This is particularly required for non-auto-connect networks (such
    // as all ad hoc networks) which will never be connected otherwise.
    // Returns ERROR_NOT_FOUND if SSID is not found in the preferred list.
    //
  __override DWORD
    ManualConnect(
        const TCHAR *pSsidName);

  // Connection Status:

    // Gets or sets the connection mode:
    //
  __override virtual WiFiConnectMode_e
    GetConnectMode(void);
  __override virtual DWORD
    SetConnectMode(
        WiFiConnectMode_e ConnMode);

    // Gets or sets the packet-latency mode:
    // Returns ERROR_CALL_NOT_IMPLEMENTED if the system does not support
    // VoIP packet latency.
    //
  __override virtual DWORD
    GetLatencyMode(
        BOOL  *pbEnable,
        ULONG *pPacketLatencyMS);
  __override virtual DWORD
    SetLatencyMode(
        BOOL  bEnable,
        ULONG PacketLatencyMS);

    // Monitors the wireless adapter's connection status until either the
    // specified time runs out or it associates with the specified SSID and,
    // optionally, BSSID and is assigned a valid IP address:
    // Returns ERROR_TIMEOUT if the connection isn't essablished within
    // the time-limit.
    //
  __override virtual DWORD
    MonitorConnection(
        const TCHAR     *pExpectedSsid,
        const MACAddr_t *pExpectedBSsid,
        bool             fAcceptTempIPAddr,
        long             ConnectTimeMS,
        long             StableTimeMS);

    // Monitors the wireless adapter until it disconnects:
    // If the optional SSID is specified, considers a re-connection to
    // a different SSID as the same thing.
    // Returns ERROR_TIMEOUT if the adapter doesn't disconnect within
    // the time-limit.
    //
  __override virtual DWORD
    MonitorDisconnect(
        const TCHAR *pExpectedSsid,
        long         DisconnectTimeMS); // see GetDefaultDisconnectTimeMS()


   // Query the stae of the a particular interface
   // If the interface is at connected state, will retrieve the 
   // associated SSID and BSSID and fill up the infConn parameter
  __override virtual DWORD
    QueryIntState(
        WiFiConn_t* pInfConn);         

  // Logging:

    // Queries and logs the current adapter information:
    //
  __override virtual DWORD
    Log(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool Verbose = true);  // logs all available information

    // Queries and logs the current WLAN context information:
    //
  __override virtual DWORD
    LogConfiguration(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool Verbose = true);  // logs all available information

    // Queries and logs the Available or Preferred Networks list:
    //
  __override virtual DWORD
    LogAvailableNetworks(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true);  // logs all available info
  __override virtual DWORD
    LogPreferredNetworks(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true);  // logs all available info

  // Utility methods:        

    // Generates a registry key for accessing WLAN prameters:
    //
    static DWORD
    CreateWlanRegKey(
        const GUID *pInterfaceGuid,  // adapter GUID (optional)
        bool        bGroupPolicy,    // group-policy information?
        const GUID *pProfileGuid,    // profile GUID (optional)
        bool        bUserData,       // user-data information?
        bool        bNamedMetaData,  // meta-data information?
      __out_ecount(BufferChars) LPWSTR pBuffer,
                                DWORD   BufferChars);
    
    // Generates a registry key for accessing the interface's parameters:
    //
    DWORD
    CreateAdapterRegKey(
      __out_ecount(BufferChars) LPWSTR pBuffer,
                                DWORD   BufferChars) const;

    // Convert the specified GUID to a string:
    //
    static HRESULT
    Guid2Text(
        const GUID &Guid,
      __out_ecount(BufferChars) LPWSTR pBuffer,
                                DWORD   BufferChars);

    // Converts the specified profile to XML text:
    //
    static DWORD
    Profile2XmlText(
        const WLANProfile_t &Profile,
        ce::wstring         *pXmlText,
        const WCHAR         *pLinePrefix = NULL);
    
protected:

    // Retrieves the list of WiFi adapters supported by the WLAN service:
    // Returns ERROR_NOT_SUPPORTED if the service cannot be loaded.
    //
  __override virtual DWORD
    DoGetAdapterNames(
        MemBuffer_t *pNamesBuffer);

    // Adds the specified network to the preferred networks list.
    //
    DWORD
    AddToPreferredList(
        const WLANProfile_t &Profile,
        bool                 bInsertAtHead);

    // Initializes a WLAN profile element with a unique name from
    // the specified network and, optionally, credential data:
    //
    DWORD
    CreateUniqueProfile(
        const WiFiConfig_t  &Network,
        const WiFiAccount_t *pEapCredentials,
        WLANProfile_t       *pProfile) ;

    // Retrieves the current interface-status:
    //
    DWORD
    GetInterfaceStatus(
        WLAN_CONNECTION_ATTRIBUTES *pStatus) const;
    
    // Gets the list of profile names in preference order:
    // Caller is responsible for deleting allocated memory.
    //
    DWORD
    GetProfileNames(
        WLAN_PROFILE_INFO_LIST **ppInfos) const;

    // Retrieves the current profile list in preference order:
    // 
    typedef vector<WLANProfile_t, ce::allocator, ce::incremental_growth<0>> WLANProfileList_t;
    DWORD
    GetProfileList(
        WLANProfileList_t *pProfiles,
        DWORD             *pCurrentIndex = NULL);

    // Definition of a WlanQueryInterface or WlanQueryAutoConfigParameter
    // query operation:
    //
    struct QueryOpCode_t
    {
        DWORD       OpCode;
        const char *pOpCodeName;
        bool        bVerbose;   // only display if Verbose is set
        bool        bInterface; // read using WlanQueryInterface
        DWORD (*OpLogFunc)(
            const char            *pOpCodeName,
            const TCHAR           *pAdapterName,
            const void            *pData,
            DWORD                  DataSize,
            WLAN_OPCODE_VALUE_TYPE ValueType,
            void                 (*LogFunc)(const TCHAR *, ...),
            const char            *pPrefix,
            int                    NameWidth);
    };

    // Performs the specified query operation and logs the results:
    //
    DWORD
    LogQueryResults(
        const QueryOpCode_t &OpCode,
        void               (*LogFunc)(const TCHAR *, ...),
        bool                 Verbose,
        const char          *pPrefix,
        int                  NameWidth);

    // Logs the specified boolian value:
    //
    static DWORD
    LogBoolValue(
        const char            *pOpCodeName,
        const TCHAR           *pAdapterName,
        const void            *pData,
        DWORD                  DataSize,
        WLAN_OPCODE_VALUE_TYPE ValueType,
        void                 (*LogFunc)(const TCHAR *, ...),
        const char            *pPrefix,
        int                    NameWidth);
    
    // Queries and logs Native WiFi driver information:
    // 
  __override virtual DWORD
    LogNativeWiFiDriverInfo(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose,          // true == log all available information
        HANDLE hNdisUio);        // Handle to NdisUio
    

    // Adapter GUID:
    //
    GUID m_GUID;

private:

    // WLAN API interface:
    //
    WLANAPIIntf_t m_API;

    // WLAN service handle:
    //
    HANDLE m_hWLAN;

    // WLAN service version number:
    //
    WORD m_WLANMajor;
    WORD m_WLANMinor;

};

};
};

#endif /* if (WLAN_VERSION > 0) */
#endif /* _DEFINED_WLANService_t_ */
// ----------------------------------------------------------------------------
