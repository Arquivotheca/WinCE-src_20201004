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
// Definitions and declarations for the WiFiService_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiService_t_
#define _DEFINED_WiFiService_t_
#pragma once

#include "WiFiTypes.hpp"
#include "WiFiConn_t.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// A simplified interface for the WiFi configuration APIs.
//
// Each of these objects provides mechanisms for controlling one WiFi
// adapter. Since there is usually only one adapter present, this class
// is generally used as a Singleton: GetInstance() retrieves a reference
// to the Singleton object and DeleteInstance() deletes it and cleans up
// allocated resources.
//
class CmdArgList_t;
struct MACAddr_t;
class MemBuffer_t;
class WiFiAccount_t;
class WiFiConfig_t;
class WiFiHandle_t;
class WiFiListener_t;
class WiFiService_t
{
public:

    // Constructor / destructor:
    // The constructor does not attach to any WiFi adapters.
    // That will be done by the first call to SetAdapterName().
    //
    WiFiService_t(
        WiFiServiceType_e ServiceType = WiFiServiceAny);
    virtual
   ~WiFiService_t(void);

    // Copy and assignment:
    //
    WiFiService_t(const WiFiService_t &rhs);
    WiFiService_t &operator = (const WiFiService_t &rhs);

    // Retrieves or deletes the singleton instance:
    // Note that this is not a traditional singleton pattern since
    // additional instances can be created to control additional adapters.
    //
    static WiFiService_t *
    GetInstance(void);
    static void
    DeleteInstance(void);

 // Test Configuration:

    // Note that the command-argument list must be generated and used to
    // determine the run-time configuration before calling GetInstance the
    // first time. Otherwise, none of the configuration will be used to 
    // generate the initial instance.

    // Gets the CmdArg object(s) required to configure the singleton:
    //
    static CmdArgList_t *
    GetCmdArgList(void);

    // Gets the configured wireless service to be tested:
    //
    static WiFiServiceType_e
    GetDefaultServiceType(void);

    // Gets the configured name of the wireless adapter to be tested:
    //
    static const TCHAR *
    GetDefaultAdapterName(void);

    // Gets the configured wireless connection time-limit:
    //
    static const long MinimumConnectTimeMS =     5*1000; //   5 seconds
    static const long DefaultConnectTimeMS =   140*1000; // 140 seconds
    static const long MaximumConnectTimeMS = 15*60*1000; //  15 minutes
    static long
    GetDefaultConnectTimeMS(void);

    // Gets the configured wireless disconnection time-limit:
    //
    static const long MinimumDisconnectTimeMS =     5*1000; //  5 seconds
    static const long DefaultDisconnectTimeMS =    80*1000; // 80 seconds
    static const long MaximumDisconnectTimeMS = 15*60*1000; // 15 minutes
    static long
    GetDefaultDisconnectTimeMS(void);

    // Gets the configured stabilization time after a wireless connection
    // is esablished before the new connection should be utilized:
    //
    static const long MinimumStableTimeMS =    1*1000; // min = 1 second
    static const long DefaultStableTimeMS =    6*1000; // def = 6 seconds
    static const long MaximumStableTimeMS = 5*60*1000; // max = 5 minutes
    static long
    GetDefaultStableTimeMS(void);

 // Wireless Service Configuration:

    // Retrieves the wireless configuration service type:
    //
    WiFiServiceType_e
    GetServiceType(void) const;

    // Selects the wireless configuration service type:
    // Returns ERROR_INVALID_NETNAME if the selected service is not available.
    //
    DWORD
    SetServiceType(WiFiServiceType_e ServiceType);

    // Enables or disables the wireless service:
    // Returns ERROR_CALL_NOT_IMPLEMENTED if this wireless service
    // cannot be enabled or disabled.
    //
    DWORD
    Enable(void);
    DWORD
    Disable(void);

    // Gets or sets the interval between scans for Available Networks
    // when the adapter is disconnected.
    // Default: 60 sec
    //
    DWORD
    GetDisconnectedScanInterval(
        DWORD *pIntervalMsec); // in millisec
    DWORD
    SetDisconnectedScanInterval(
        const DWORD IntervalMsec); // in millisec

    // Gets or sets the interval between scans for Available Networks
    // when the adapter is associated to a network.
    // Default: 60 sec
    //
    DWORD
    GetAssociatedScanInterval(
        DWORD *pIntervalMsec); // in millisec
    DWORD
    SetAssociatedScanInterval(
        const DWORD IntervalMsec); // in millisec

  // Adapter Management:

    // Retrieves the list of WiFi adapters on the system:
    // Returns ERROR_NOT_SUPPORTED if the service cannot be loaded.
    // The optional in/out service-type parameter can be used to specify
    // and/or retrieve the type of service. (Set the initial value to
    // WiFiServiceAny to just retrieve the service type.)
    //
    static DWORD
    GetAdapterNames(
        WiFiAdapterNamesList **ppNamesList,
        MemBuffer_t           *pNamesBuffer,
        WiFiServiceType_e     *pServiceType = NULL);

    // Connects or disconnects the current wireless adapter:
    // Connect attaches to the adapter named by the last SetAdapterName().
    // Use SetAdapterName(NULL) to connect the default adapter.
    //
    DWORD
    ConnectAdapter(void);
    void
    DisconnectAdapter(void);

    // Determines whether the service is controlling a wireless adapter:
    //
    bool
    HasAdapter(void) const;

    // Specifies the wireless adapter to be controlled:
    // A null or empty name selects the first adapter on the system.
    // Returns ERROR_INVALID_NETNAME if the selected adapter is not available.
    //
    DWORD
    SetAdapterName(
        const TCHAR *pAdapterName);

    // Gets the name of the wireless adapter being controlled:
    // Returns an empty string if the system has no wireless adapters.
    // Otherwise, returns either the adapter selected by SetAdapterName
    // or the first adapter on the system.
    //
    const WCHAR *
    GetAdapterGuid(void) const;   // wide characters
    const TCHAR *
    GetAdapterName(void) const;  // flexibible

    // Determines whether the adapter supports Native WiFi:
    //
    bool
    IsNativeWiFi(void) const;

    // Checks whether the adapter supports the specified modes:
    //
    bool
    IsSecurityModeSupported(
        const WiFiConfig_t &NetworkConfig);

  // Notifications:

    // Generates a notification-listener object for the wireless adapter:
    //
    DWORD
    CreateListener(
        WiFiListener_t **ppListener);

  // Preferred-Networks Managment:

    // Clears the current Preferred Networks List:
    // This will disconnect everything attached to the wireless adapter.
    // If the optional wait-time is specified the function waits that 
    // long for the disconnection to finish.
    //
    DWORD
    Clear(
        long DisconnectTimeMS = 0);

    // Refreshes the wireless service to force a reconnection:
    //
    DWORD
    Refresh(void);

    // Inserts the specified element at the head of the Preferred
    // Networks list:
    // As a side-effect, the added network is marked "current" so
    // the wireless service will immediately attempt a connection.
    //
    DWORD
    AddToHeadOfPreferredList(
        const WiFiConfig_t &Network);

    // Inserts the specified element at the tail of the Preferred
    // Networks list:
    //
    DWORD
    AddToTailOfPreferredList(
        const WiFiConfig_t &Network);

    // Removes the specfied SSID from the Preferred Networks list:
    // By default, removes the first instance of the specified SSID.
    // If the RemoveAllMatches flag is true, all the matching Preferred
    // Networks entries will be removed.
    // Returns ERROR_NOT_FOUND if there was no match.
    //
    DWORD
    RemoveFromPreferredList(
        const TCHAR *pSsidName,
        bool         RemoveAllMatches = false);

    // Checks if a particular SSID is within the Available Networks Lists:
    // Returns ERROR_TIMEOUT if the SSID is not scanned within the specified
    // time limit.
    //
    static const long MinimumAvailableTimeMS =   60*1000; // min = 1 minute
    static const long DefaultAvailableTimeMS = 2*60*1000; // def = 2 minutes
    static const long MaximumAvailableTimeMS = 5*60*1000; // max = 5 minutes
    DWORD
    CheckAvailableNetwork(
        const TCHAR *pSsidName,
        const long   TimeLimitMS);

    // Configures the WiFi service to connect the specified Access Point
    // or Ad Hoc network:
    // If the optional EapCredentials object is supplied, uses it to
    // supply the user's security credientials for EAP authentication.
    // If the network is not auto-connect, calls ManualConnect, too.
    //
    DWORD
    ConnectNetwork(
        const WiFiConfig_t &Network,
        WiFiAccount_t      *pEapCredentials = NULL,
        WiFiAccount_t      *pEapPreCredentials = NULL);

    // Immediately connects the specified network:
    // This is particularly required for non-auto-connect networks (such
    // as all ad hoc networks) which will never be connected otherwise.
    // Returns ERROR_NOT_FOUND if SSID is not found in the preferred list.
    //
    DWORD
    ManualConnect(
        const TCHAR *pSsidName);

  // Connection Status:

    // Gets or sets the connection mode:
    //
    WiFiConnectMode_e
    GetConnectMode(void);
    DWORD
    SetConnectMode(
        WiFiConnectMode_e ConnMode);

    // Gets or sets the packet-latency mode:
    // Returns ERROR_CALL_NOT_IMPLEMENTED if the system does not support
    // VoIP packet latency.
    //
    DWORD
    GetLatencyMode(
        BOOL  *pbEnable,
        ULONG *pPacketLatencyMS);
    DWORD
    SetLatencyMode(
        BOOL  bEnable,
        ULONG PacketLatencyMS);

    // Gets the IP address assigned to the wireless adapter:
    // Returns ERROR_NOT_CONNECTED if the adapter hasn't been connected
    // and assigned a valid IP address yet.
    // If the "fAcceptTempIPAddr" flag is set, a tempoary IP "169.x.x.x"
    // IP address will qualify as a valid address. Otherwise, the method
    // assumes such an address was assigned because DHCP failed.
    //
    DWORD
    GetIPAddresses(
      __out_ecount(AddressChars) TCHAR *pIPAddress,
      __out_ecount(AddressChars) TCHAR *pGatewayAddress, // optional
                                 int    AddressChars,
                                 bool   fAcceptTempIPAddr = false);

    // Monitors the wireless adapter's connection status until either the
    // specified time runs out or it associates with the specified SSID and,
    // optionally, BSSID and is assigned a valid IP address:
    // Uses the configured connect and stabilization timeouts unless the
    // second form of the command is used.
    // Returns ERROR_TIMEOUT if the connection isn't essablished within
    // the time-limit.
    //
    DWORD
    MonitorConnection(
        const TCHAR     *pExpectedSsid,
        const MACAddr_t *pExpectedBSsid    = NULL,
        bool             fAcceptTempIPAddr = false);
    DWORD
    MonitorConnection(
        const TCHAR     *pExpectedSsid,
        const MACAddr_t *pExpectedBSsid,
        bool             fAcceptTempIPAddr,
        long             ConnectTimeMS,  // see GetDefaultConnectTimeMS()
        long             StableTimeMS);  // see GetDefaultStableTimeMS()

    // Monitors the wireless adapter until it disconnects:
    // If the optional SSID is specified, considers a re-connection to
    // a different SSID as the same thing.
    // Uses the configured disconnection timeout unless the second form
    // of the command is used.
    // Returns ERROR_TIMEOUT if the adapter doesn't disconnect within
    // the time-limit.
    //
    DWORD
    MonitorDisconnect(
        const TCHAR *pExpectedSsid = NULL);
    DWORD
    MonitorDisconnect(
        const TCHAR *pExpectedSsid,
        long         DisconnectTimeMS); // see GetDefaultDisconnectTimeMS()


    // Query for interface information, which include adapter Mac Address, 
    // AP SSID and BSSID if associated
    DWORD
    QueryIntState(WiFiConn_t* pInfConn);

  // Logging:

    // Queries and logs the current adapter information:
    //
    DWORD
    Log(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true);  // true == log all available information

    // Queries and logs the current adapter configuration:
    //
    DWORD
    LogConfiguration(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true);  // true == log all available information

    // Queries and logs the Available or Preferred Networks list:
    //
    DWORD
    LogAvailableNetworks(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true);  // true == log all available information
    DWORD
    LogPreferredNetworks(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true);  // true == log all available information

    // Queries and logs driver information:
    //
    DWORD
    LogDriverInfo(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true);  // true == log all available information

    // Formats the specified supported-rates into the output buffer:
    //
    static const TCHAR *
    FormatSupportedRates(
        const NDIS_802_11_RATES       &Rates,
      __out_ecount(BufferChars) TCHAR *pBuffer,
                                size_t  BufferChars);
    static const TCHAR *
    FormatSupportedRates(
        const NDIS_802_11_RATES_EX    &Rates,
      __out_ecount(BufferChars) TCHAR *pBuffer,
                                size_t  BufferChars);

private:

    // Wireless configuration service type:
    //
    WiFiServiceType_e m_ServiceType;

    // Wireless adapter name:
    //
    TCHAR m_AdapterName[MaxWiFiAdapterNameChars];

    // Current wireless adapter handle:
    //
    WiFiHandle_t *m_pHandle;

};

};
};

#endif /* _DEFINED_WiFiService_t_ */
// ----------------------------------------------------------------------------
