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
// Definitions and declarations for the ConfigSPService_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_ConfigSPWiFiService_t_
#define _DEFINED_ConfigSPWiFiService_t_
#pragma once

#include "WLANTypes.hpp"
#if (CMWIFI_VERSION > 0)

#include <WiFUtils.hpp>
#include "WiFiHandle_t.hpp"
#include "CMWiFiService_t.hpp"
#include "cfgmgr.h"


#include <string.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Extends WiFiHandle to provide a simplified interface for the
// Configuration Service Provider.
//

class ConfigSPWiFiService_t : public CMWiFiService_t
{
public:

    // Constructor / destructor:
    //
    ConfigSPWiFiService_t(
        WiFiServiceType_e ServiceType);
  __override virtual
   ~ConfigSPWiFiService_t(void);

    // Initializes the wireless service:
    // Returns ERROR_NOT_SUPPORTED if the service cannot be loaded.
    // Uses the optional adapter-names object, if supplied, to initialize
    // the adapter's name and/or GUID.
    //
  __override virtual DWORD
    InitService(
        const WiFiAdapterName *pAdapterName = NULL);

  // Preferred-Networks Managment:

    // Clears the current Preferred Networks List:
    // This will disconnect everything attached to the wireless adapter.
    // If the optional wait-time is specified the function waits that 
    // long for the disconnection to finish.
    //
  __override virtual DWORD
    Clear(
        long DisconnectTimeMS = 0);

    // Inserts the specified element at the head of the Preferred
    // Networks list:
    // As a side-effect, the added network is marked "current" so
    // CM will immediately attempt a connection.
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

    // Removes the specfied Access Point from the Preferred Networks list:
    // By default, removes the first instance of the specified SSID.
    // If the RemoveAllMatches flag is true, all the matching Preferred
    // Networks entries will be removed.
    // Returns ERROR_NOT_FOUND if there was no match.
    //
  __override virtual DWORD
    RemoveFromPreferredList(
        const TCHAR *pSsidName,
        bool         RemoveAllMatches = false);

    // Configures the WiFi service to connect the specified Access Point
    // or Ad Hoc network:
    // If the optional EapCredentials object is supplied, uses it to
    // supply the user's security credientials for EAP authentication.
    // If the optional TemporaryNetwork flag is set, adds the new network
    // for the duration of the connection. Otherwise, adds it to the
    // preferred-networks list.
    //
  __override virtual DWORD
    ConnectNetwork(
        const WiFiConfig_t &Network,
        WiFiAccount_t      *pEapCredentials = NULL);

private:

    // Process Provisioned Xml
    //
    DWORD
    ProcessProvisionXml(
        litexml::XmlElement_t* pRoot);

    // Adds the specified network to the preferred networks list.
    //
    DWORD
    AddToConnectionList(
        const WiFiConfig_t &Network,
        WiFiAccount_t      *pEapCredentials = NULL);

    // Config Manager
    //
    IConfigManager *m_pConfigMgr;

};

};
};

#endif /* if (CMWIFI_VERSION > 0) */
#endif /* _DEFINED_ConfigSPWiFiService_t_ */
// ----------------------------------------------------------------------------
