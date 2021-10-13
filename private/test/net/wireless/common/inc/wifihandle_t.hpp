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
// Definitions and declarations for the WiFiHandle_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiHandle_t_
#define _DEFINED_WiFiHandle_t_
#pragma once

#include "WiFiTypes.hpp"
#include "NdisTypes.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// This class serves two purposes; 1) it is a virtual base for a series of
// classes customized for manipulating different types of wireless adapters
// and or wireless APIs and 2) it provides a shared "handle" for use by the
// WiFiService Facade class.
//
struct MACAddr_t;
class MemBuffer_t;
class WiFiAccount_t;
class WiFiConfig_t;
class WiFiListener_t;
class WiFiConn_t;
class WiFiHandle_t
{
private:
    // Wireless configuration service type:
    //
    WiFiServiceType_e m_ServiceType;

    // Wireless adapter name:
    //
    WCHAR m_AdapterGuid[MaxWiFiAdapterNameChars];
    TCHAR m_AdapterName[MaxWiFiAdapterNameChars];

    // Number WiFiService's attached:
    //
    int m_NumberAttached;

    // Pointer to next Handle in list:
    //
    WiFiHandle_t *m_pNext;

    // Copy and assignment is deliberately disabled:
    //
    WiFiHandle_t(const WiFiHandle_t &rhs);
    WiFiHandle_t &operator = (const WiFiHandle_t &rhs);

protected:

    // Constructor / destructor:
    //
    WiFiHandle_t(
        WiFiServiceType_e ServiceType);
    virtual
   ~WiFiHandle_t(void);

    // Initializes the wireless service:
    // Returns ERROR_NOT_SUPPORTED if the service cannot be loaded.
    // Uses the optional adapter-names object, if supplied, to initialize
    // the adapter's name and/or GUID.
    //
    virtual DWORD
    InitService(
        const WiFiAdapterName *pAdapterName = NULL) = 0;
    
    // Sets the adapter name or GUID:
    //
    void SetAdapterGuid(const WCHAR *pAdapterGuid);
    void SetAdapterName(const WCHAR *pAdapterName);

public:

    // Creates and/or attaches the appropriate type of handle:
    // If the apapter name is null or empty, attaches to the first
    // wireless adapter controlled by the specified config service.
    //
    static WiFiHandle_t *
    AttachHandle(
        WiFiServiceType_e ServiceType,
        const TCHAR      *pAdapterName,
        const WCHAR      *pAdapterGuid = NULL);

    // Detaches the specified handle and, if this was the last reference
    // deletes it:
    //
    static void
    DetachHandle(
        const WiFiHandle_t *pHandle);

 // Wireless Service Configuration:

    // Retrieves the wireless configuration service type:
    //
    WiFiServiceType_e
    GetServiceType(void) const { return m_ServiceType; }

    // Enables or disables the wireless service:
    // Returns ERROR_CALL_NOT_IMPLEMENTED if this wireless service
    // cannot be enabled or disabled.
    //
    virtual DWORD
    Enable(void) = 0;
    virtual DWORD
    Disable(void) = 0;

    // Gets or sets the interval between scans for Available Networks
    // when the adapter is disconnected.
    // Default: 60 sec
    //
    virtual DWORD
    GetDisconnectedScanInterval(
        DWORD *pIntervalMsec) = 0; // in millisec
    virtual DWORD
    SetDisconnectedScanInterval(
        const DWORD IntervalMsec) = 0; // in millisec

    // Gets or sets the interval between scans for Available Networks
    // when the adapter is associated to a network.
    // Default: 60 sec
    //
    virtual DWORD
    GetAssociatedScanInterval(
        DWORD *pIntervalMsec) = 0; // in millisec
    virtual DWORD
    SetAssociatedScanInterval(
        const DWORD IntervalMsec) = 0; // in millisec

  // Adapter Management:

    // Retrieves the list of WiFi adapters on the system:
    // Returns ERROR_NOT_SUPPORTED if the service cannot be loaded or there
    // are no wireless adapters on the system.
    // The optional in/out service-type parameter can be used to specify
    // and/or retrieve the type of service. (Set the initial value to
    // WiFiServiceAny to just retrieve the service type.)
    //
    static DWORD
    GetAdapterNames(
        WiFiAdapterNamesList **ppNamesList,
        MemBuffer_t           *pNamesBuffer,
        WiFiServiceType_e     *pServiceType = NULL);
  
    // Gets the name of the wireless adapter being controlled:
    //
    const WCHAR *
    GetAdapterGuid(void) const { return m_AdapterGuid; }
    const TCHAR *
    GetAdapterName(void) const { return m_AdapterName; }

    // Determines whether the adapter supports Native WiFi:
    //
    bool
    IsNativeWiFi(void) const;

    // Checks whether the adapter supports the specified modes:
    //
    virtual bool
    IsSecurityModeSupported(
        const WiFiConfig_t &NetworkConfig) = 0;

  // Notifications:

    // Generates a notification-listener object for the wireless adapter:
    // The default implementation just returns ERROR_CALL_NOT_IMPLEMENTED.
    //
    virtual DWORD
    CreateListener(
        WiFiListener_t **ppListener);

  // Preferred-Networks Managment:

    // Clears the current Preferred Networks List:
    // This will disconnect everything attached to the wireless adapter.
    // If the optional wait-time is specified the function waits that 
    // long for the disconnection to finish.
    //
    virtual DWORD
    Clear(
        long DisconnectTimeMS = 0) = 0;

    // Refreshes WZC to force a reconnection:
    //
    virtual DWORD
    Refresh(void) = 0;

    // Inserts the specified element at the head of the Preferred
    // Networks list:
    // As a side-effect, the added network is marked "current" so
    // WZC will immediately attempt a connection.
    //
    virtual DWORD
    AddToHeadOfPreferredList(
        const WiFiConfig_t &Network) = 0;

    // Inserts the specified element at the tail of the Preferred
    // Networks list:
    //
    virtual DWORD
    AddToTailOfPreferredList(
        const WiFiConfig_t &Network) = 0;

    // Removes the specfied SSID from the Preferred Networks list:
    // By default, removes the first instance of the specified SSID.
    // If the RemoveAllMatches flag is true, all the matching Preferred
    // Networks entries will be removed.
    // Returns ERROR_NOT_FOUND if there was no match.
    //
    virtual DWORD
    RemoveFromPreferredList(
        const TCHAR *pSsidName,
        bool         RemoveAllMatches = false) = 0;

    // Checks if a particular SSID is within the Available Networks Lists:
    // Returns ERROR_TIMEOUT if the SSID is not scanned within the specified
    // time limit.
    //
    virtual DWORD
    CheckAvailableNetwork(
        const TCHAR *pSsidName,
        long         TimeLimitMS) = 0;

    // Configures the WiFi service to connect the specified Access Point
    // or Ad Hoc network:
    // If the optional EapCredentials object is supplied, uses it to
    // supply the user's security credientials for EAP authentication.
    //
    virtual DWORD
    ConnectNetwork(
        const WiFiConfig_t &Network,
        WiFiAccount_t      *pEapCredentials = NULL,
        WiFiAccount_t      *pEapPreCredentials = NULL) = 0;

    // Immediately connects the specified network:
    // This is particularly required for non-auto-connect networks (such
    // as all ad hoc networks) which will never be connected otherwise.
    // Returns ERROR_NOT_FOUND if SSID is not found in the preferred list.
    //
    virtual DWORD
    ManualConnect(
        const TCHAR *pSsidName) = 0;

  // Connection Status:

    // Gets or sets the connection mode:
    //
    virtual WiFiConnectMode_e
    GetConnectMode(void) = 0;
    virtual DWORD
    SetConnectMode(
        WiFiConnectMode_e ConnMode) = 0;

    // Gets or sets the packet-latency mode:
    // Returns ERROR_CALL_NOT_IMPLEMENTED if the system does not support
    // VoIP packet latency.
    //
    virtual DWORD
    GetLatencyMode(
        BOOL  *pbEnable,
        ULONG *pPacketLatencyMS) = 0;
    virtual DWORD
    SetLatencyMode(
        BOOL  bEnable,
        ULONG PacketLatencyMS) = 0;

    // Gets the IP address assigned to the wireless adapter:
    // Returns ERROR_NOT_CONNECTED if the adapter hasn't been connected
    // and assigned a valid IP address yet.
    // If the "fAcceptTempIPAddr" flag is set, a tempoary IP "169.x.x.x"
    // IP address will qualify as a valid address. Otherwise, the method
    // assumes such an address was assigned because DHCP failed.
    //
    virtual DWORD
    GetIPAddresses(
      __out_ecount(AddressChars) TCHAR *pIPAddress,
      __out_ecount(AddressChars) TCHAR *pGatewayAddress, // optional
                                 int    AddressChars,
                                 bool   fAcceptTempIPAddr = false);

    // Monitors the wireless adapter's connection status until either the
    // specified time runs out or it associates with the specified SSID and,
    // optionally, BSSID and is assigned a valid IP address:
    // Returns ERROR_TIMEOUT if the connection isn't essablished within
    // the time-limit.
    //
    virtual DWORD
    MonitorConnection(
        const TCHAR     *pExpectedSsid,
        const MACAddr_t *pExpectedBSsid,
        bool             fAcceptTempIPAddr,
        long             ConnectTimeMS,
        long             StableTimeMS) = 0;

    // Monitors the wireless adapter until it disconnects:
    // If the optional SSID is specified, considers a re-connection to
    // a different SSID as the same thing.
    // Returns ERROR_TIMEOUT if the adapter doesn't disconnect within
    // the time-limit.
    //
    virtual DWORD
    MonitorDisconnect(
        const TCHAR *pExpectedSsid,
        long         DisconnectTimeMS) = 0;

   // Query the state of the a particular interface:
   // If the interface is at connected state, will retrieve the 
   // associated SSID and BSSID and fill up the infConn parameter.
   //
    virtual DWORD
    QueryIntState(
        WiFiConn_t* pInfConn) { return NO_ERROR;}

  // Logging:

    // Queries and logs the current adapter information:
    //
    virtual DWORD
    Log(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true) = 0;  // true == log all available information

    // Queries and logs the current adapter configuration:
    //
    virtual DWORD
    LogConfiguration(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true) = 0;  // true == log all available information

    // Queries and logs the Available or Preferred Networks list:
    //
    virtual DWORD
    LogAvailableNetworks(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true) = 0;  // true == log all available information

    virtual DWORD
    LogPreferredNetworks(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true) = 0;  // true == log all available information

    // Queries and logs driver information:
    //
    virtual DWORD
    LogDriverInfo(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose = true);  // true == log all available information

protected:

    // Queries and logs Legacy WiFi driver information:
    // 
    virtual DWORD
    LogLegacyWiFiDriverInfo(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose,          // true == log all available information
        HANDLE hNdisUio);        // Handle to NdisUio

    // Queries and logs Native WiFi driver information:
    // The default implementation of this method does nothing.
    // 
    virtual DWORD
    LogNativeWiFiDriverInfo(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        bool   Verbose,          // true == log all available information
        HANDLE hNdisUio);        // Handle to NdisUio

    // Queries and logs a Bool value using the specified format:
    //
    VOID
    QueryOidBoolValue(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        HANDLE       hNdisUio,
        DWORD        Oid,
        const TCHAR *pFormat);

    // Queries and logs a MAC Address using the specified format:
    //
    VOID
    QueryOidMacValue(
        void (*LogFunc)(const TCHAR *pFormat, ...),
        HANDLE       hNdisUio,
        DWORD        Oid,
        const TCHAR *pFormat,
        const void  *pMac = NULL);

    // Queries and logs the specified ULong value:
    //
    BOOL
    QueryOidUlongValue(
        HANDLE hNdisUio,
        DWORD  Oid,
        ULONG &ulValue);

    // Queries an OID value or, if Native WiFi, an IOCTL:
    //
    DWORD
    QueryOidValue(
        HANDLE hNdisUio,
        PVOID *ppBuf,
        DWORD  Oid);
    
  // Utility methods:

    // Retrieves the list of WiFi adapters supported by the derived
    // class's service:
    // Returns ERROR_NOT_SUPPORTED if the service cannot be loaded.
    //
    virtual DWORD
    DoGetAdapterNames(
        MemBuffer_t *pNamesBuffer) = 0;

    // Gets the IP addresses for GetIPAddresses:
    // The fShowError and pLastError arguments provide a mechanism for
    // indicating how the method should handle errors. These flags allow
    // the method to be used during connection setup to check for address
    // assignment without producing excess error messages.
    //
    DWORD
    GetAdapterAddresses(
      __out_ecount(AddressChars) TCHAR *pIPAddress,
      __out_ecount(AddressChars) TCHAR *pGatewayAddress, // optional
                                 int    AddressChars,
                                 bool   fAcceptTempIPAddr,
                                 bool   fShowError,      // else warn
                                 DWORD *pLastError);        

};

};
};

#endif /* _DEFINED_WiFiHandle_t_ */
// ----------------------------------------------------------------------------
