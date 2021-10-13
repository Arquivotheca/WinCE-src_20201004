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
// Definitions and declarations for the AuthMatrix_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_AuthMatrix_t_
#define _DEFINED_AuthMatrix_t_
#pragma once

#include "WiFiBase_t.hpp"
#include "WiFiConfig_t.hpp"

#include <tux.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Extends WiFiBase_t to add the Authentication-Matrix tests.
//
class Factory_t;
class WiFiAccount_t;
class AuthMatrix_t : public WiFiBase_t
{
private:

    // Test ID and name:
    DWORD        m_TestId;
    const TCHAR *m_pTestName;

    // WiFi configuration under test:
    WiFiConfig_t m_Config;

    // Access Point's WiFi configuration:
    WiFiConfig_t m_APConfig;

    // Negative test indicators:
    bool m_IsBadModes;      // security-modes are invalid
    bool m_IsBadKey;        // key-value is invalid

    // EAP authentication credentials:
    WiFiAccount_t *m_pEapCredentials;

    // Start-time:
    long m_StartTime;

public:

    // Beginning and end of class's ID range in Tux function-table:
    enum { TestIdStart =  2000 };
    enum { TestIdEnd   = 21999 };

    // Initializes or cleans up static resources:
    static void StartupInitialize(void);
    static void ShutdownCleanup(void);

    // Parses the test class's command-line arguments:
    static void  PrintUsage(void);
    static DWORD ParseCommandLine(int argc, TCHAR *argv[]);

    // Adds all the tests to the specified factory's function-table:
    static DWORD
    AddFunctionTable(Factory_t *pFactory);

    // Generates an object for processing the specified test:
    static DWORD
    CreateTest(
        const FUNCTION_TABLE_ENTRY *pFTE,
        WiFiBase_t                **ppTest);

    // Gets the current WiFi configuration under test:
    // Returns false if there is no test running.
    static bool
    GetTestConfig(
        WiFiConfig_t *pAPConfig,
        WiFiConfig_t *pConfig,
        long         *pStartTime);
    
    // Constructors and destructor:
    AuthMatrix_t(
        DWORD               TestId,
        const TCHAR        *pTestName,
        const WiFiConfig_t &Config);
    AuthMatrix_t(
        DWORD               TestId,
        const TCHAR        *pTestName,
        const WiFiConfig_t &APConfig,
        const WiFiConfig_t &Config,
        bool                IsBadModes,
        bool                IsBadKey);
    virtual
   ~AuthMatrix_t(void);

    // Initializes, runs or cleans up the test:
    // Returns ERROR_CALL_NOT_IMPLEMENTED if the test is to be skipped.
  __override virtual DWORD Init(void);
  __override virtual DWORD Run(void);
  __override virtual DWORD Cleanup(void);

private:

    // Selects and configures the appropriate Access Point.
    DWORD
    ConfigureAccessPoint(void);

    // Makes sure the wireless adapter is properly connected:
    DWORD
    CheckConnected(void);
};

};
};
#endif /* _DEFINED_AuthMatrix_t_ */
// ----------------------------------------------------------------------------
