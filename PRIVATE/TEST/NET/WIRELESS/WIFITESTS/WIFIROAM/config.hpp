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
// Definitions and declarations for the Config class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiRoamConfig_
#define _DEFINED_WiFiRoamConfig_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides a mechanism for accessing the program's configuration info.
//
class APPool_t;
class Config
{
private:

    // Construction and assignment are deliberately disabled:
    Config(void);
   ~Config(void);
    Config(const Config &src);
    Config &operator = (const Config &src);

public:

    // Names of the program's config-registry keys:
    static const TCHAR RegistryKey[];
    static const TCHAR DTPTEnabledKey[];
    static const TCHAR DTPTServerHostKey[];
    static const TCHAR DTPTServerPortKey[];
    static const TCHAR WiFiServerHostKey[];
    static const TCHAR WiFiServerPortKey[];

    // Initializes or cleans up static resources:
    static void StartupInitialize(void);
    static void ShutdownCleanup  (void);

    // Determines whether the mobile device is connected to the DTPT/USB
    // ActiveSync port:
    static bool IsDTPTEnabled(void);

    // Generates or retrieves an APPool object attached using the 
    // specified type of connection:
    static HRESULT GetAPPool(DWORD ConnType, APPool_t **ppAPPool);

    // Gets the host- or port-address of the APControl server for
    // connecting over the DTPT or WiFi LAN:
    static const TCHAR *GetDTPTServerHost(void);
    static const TCHAR *GetDTPTServerPort(void);
    static const TCHAR *GetWiFiServerHost(void);
    static const TCHAR *GetWiFiServerPort(void);

    // Gets the SSID of one of the test's Access Points:
    static const TCHAR *GetHomeSSID(void);
    static const TCHAR *GetOfficeSSID(void);
    static const TCHAR *GetHotspotSSID(void);
};

};
};
#endif /* _DEFINED_WiFiRoamConfig_ */
// ----------------------------------------------------------------------------
