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
// Definitions and declarations for the APService_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_APService_t_
#define _DEFINED_APService_t_
#pragma once

#include <APController_t.hpp>
#include <APPool_t.hpp>
#include <CmdArgList_t.hpp>

#ifdef UNDER_CE
#include <WiFiConfig_t.hpp>
#endif

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// A singleton service providing access to and control of a remote Access
// Point control server.
//
class APService_t
{
private:

    // AP-Controller list:
    //
    APPool_t m_APPool;

    // Connection status:
    //
    bool m_bLANConnected;
    bool m_bWiFiConnected;

    // Copy and assignment are deliberately disabled:
    //
    APService_t(const APService_t &rhs);
    APService_t &operator = (const APService_t &rhs);

    // Construction is provided by GetInstance:
    //
    APService_t(void);
   
public:

    // Destructor:
    //
   ~APService_t(void);

    // Retrieves or deletes the singleton service object:
    //
    static APService_t *
    GetInstance(void);
    static void
    DeleteInstance(void);

  // Run-time configuration:

    // Note that the command-argument list must be generated and used to
    // determine the run-time configuration before calling GetInstance the
    // first time. Otherwise, none of the configuration will be used to 
    // generate the initial instance.
    
    // Creates and/or retrieves the list of CmdArg objects used to
    // configure the singleton's access to the APControl server:
    //
    static CmdArgList_t *
    GetCmdArgList(void);

    // LAN APControl server host-address and port-number:
    //
    static const TCHAR *
    GetLANServerHost(void);
    static const TCHAR *
    GetLANServerPort(void);

#ifdef UNDER_CE
    // WiFi APControl server host-address, port-number, and wireless
    // connection parameters:
    //
    static const TCHAR *
    GetWiFiServerHost(void);
    static const TCHAR *
    GetWiFiServerPort(void);

    static const WiFiConfig_t &
    GetWiFiServerAPConfig(void);
    static bool 
    IsWiFiServerAPConfigured(void);
#endif

    // List of Access Points to be used for this test:
    //
    static const CmdArgMultiString_t *
    GetAPNamesList(void);
    static const TCHAR *
    GetAPNamesListEntry(int Index);
    static int
    GetAPNamesListSize(void);

  // APControl server connection:

    // (Re)connects to the APControl server:
    // If the object is connecting using a WiFi Access Point, and the specified
    // bWiFiIsConnected flag is true, the object does not associate with the AP.
    // This is designed for the situation where we Disconnect'ed the normal AP
    // then manually reassociated using an alternate AP.
    //
    DWORD
    Connect(
        IN bool bWiFiIsConnected = false);
  
    // Disconnects from the APControl server:
    // If the object is connected using a LAN link and the bLeaveLANConnected
    // flag is true, this method does nothing.
    //
    DWORD
    Disconnect(
        IN bool bLeaveLANConnected = false);

  // APControllers list:
    
    // Determines how many AP controllers are in the server's list:
    int size(void) const { return m_APPool.size(); }

    // Retrieves an AP controller by index or case-insensitive name:
          APController_t *GetAP(IN int APIndex)       { return m_APPool.GetAP(APIndex); }
    const APController_t *GetAP(IN int APIndex) const { return m_APPool.GetAP(APIndex); }
          APController_t *FindAP(IN const TCHAR *pAPName)       { return m_APPool.FindAP(pAPName); }
    const APController_t *FindAP(IN const TCHAR *pAPName) const { return m_APPool.FindAP(pAPName); }

    APPool_t*             GetAPPool(void)  { return &m_APPool;}

#ifdef UNDER_CE
    // Selects the most suitable Access Points from the list of those
    // cotrolled by the APPool using the command-line AP-names argument
    // and (if necessary) the specified WiFi security modes:
    DWORD
    SelectAccessPoints(
        OUT APController_t **ppAPList,
        OUT int              *pAPListSize); // in/out: size of AP list
    DWORD
    SelectAccessPoints(
        OUT APController_t   **ppAPList,
        IN  const WiFiConfig_t &WiFiConfig,    // requested security modes
        OUT int                *pAPListSize,   // in/out: size of AP list
        OUT int                *pFilteredAPs); // out: APs which can handle
                                               // specified security modes                                               
#endif
};

};
};
#endif /* _DEFINED_APService_t_ */
// ----------------------------------------------------------------------------
