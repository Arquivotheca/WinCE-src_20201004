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
// Definitions and declarations for the WiFiBase_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiBase_t_
#define _DEFINED_WiFiBase_t_
#pragma once

#include "Utils.hpp"
#include "WiFiConfig_t.hpp"
#include "WZCService_t.hpp"

#include <APController_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides the basic implementation for all the tests to be run by the
// WiFiTests Tux test-suites.
//
class APPool_t;
class WiFiBase_t
{
public:

    // Initializes or cleans up static resources:
    static void StartupInitialize(void);
    static void ShutdownCleanup  (void);

  // Run-time configuration:

    // Selected wireless adapter name (default is first available):
    static const TCHAR *GetSelectedAdapter(void);
    static void         SetSelectedAdapter(const TCHAR *pValue);

    // LAN APControl server host-address and port-number:
    static const TCHAR DefaultLANServerHost[];
    static const TCHAR    *GetLANServerHost(void);
    static void            SetLANServerHost(const TCHAR *pValue);

    static const TCHAR DefaultLANServerPort[];
    static const TCHAR    *GetLANServerPort(void);
    static void            SetLANServerPort(const TCHAR *pValue);

    // WiFi APControl server host-address, port-number, and wireless
    // connection parameters:
    static const TCHAR DefaultWiFiServerHost[];
    static const TCHAR    *GetWiFiServerHost(void);
    static void            SetWiFiServerHost(const TCHAR *pValue);

    static const TCHAR DefaultWiFiServerPort[];
    static const TCHAR    *GetWiFiServerPort(void);
    static void            SetWiFiServerPort(const TCHAR *pValue);

    static const WiFiConfig_t DefaultWiFiServerAPConfig;
    static const WiFiConfig_t    &GetWiFiServerAPConfig(void);
    static bool                    IsWiFiServerAPConfigured(void);

    // Parses the DLL's command-line arguments:
    static void  PrintUsage(void);
    static DWORD ParseCommandLine(int argc, TCHAR *argv[]);

  // Test methods:

    // Constructor and destructor:
    WiFiBase_t(void);
    virtual
   ~WiFiBase_t(void);

    // Initializes this concrete test class:
    // Returns ERROR_CALL_NOT_IMPLEMENTED if the test is to be skipped.
    virtual DWORD
    Init(void) = 0;

    // Runs the test:
    // Returns ERROR_CALL_NOT_IMPLEMENTED if the test is to be skipped.
    virtual DWORD
    Run(void) = 0;

    // Cleans up after the test finishes:
    // Note that this could be called while the test is still processing
    // the Run method. In that case, the test is expected to stop the
    // test as quickly as possible.
    virtual DWORD
    Cleanup(void) = 0;

  // Utility methods:

    // Initializes and retrieves the APPool object used for controlling
    // the test's Access Points and RF Attenuators:
    static DWORD
    InitAPPool(void);
    static APPool_t *
    GetAPPool(void);

    // Dissociates the WiFi APControl server:
    // If the server was connected using a LAN connection, this does nothing.
    // Otherwise, this disconnects the WiFi server and dissociates from the
    // WiFi-server Access Point.
    static DWORD
    DissociateAPPool(void);

    // Initializes and retrieves the WZCService object used for accessing
    // the WZC (Wireless Zero Config) facilities:
    static DWORD
    InitWZCService(void);
    static WZCService_t *
    GetWZCService(void);

    // Gets the APController with the current security mode configuration
    // most closely matching that specified. If the optional list of AP
    // names is supplied, limits the search to those Access Points:
    // Returns ERROR_CALL_NOT_IMPLEMENTED if there are no Access Points
    // which can handle the specified (valid) authentication modes.
    static DWORD
    GetAPController(
        const WiFiConfig_t &Config,
        APController_t    **ppControl,
        const TCHAR * const pAPNames[] = NULL,
        int                 NumberAPNames = 0);
};

};
};
#endif /* _DEFINED_WiFiBase_t_ */
// ----------------------------------------------------------------------------
