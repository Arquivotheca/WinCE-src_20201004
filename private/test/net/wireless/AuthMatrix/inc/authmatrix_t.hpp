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
// Definitions and declarations for the AuthMatrix_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_AuthMatrix_t_
#define _DEFINED_AuthMatrix_t_
#pragma once

#include <WiFiBase_t.hpp>
#include <WiFiConfig_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Extends WiFiBase_t to add the Authentication-Matrix tests.
//
class AMFactory_t;
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
    bool m_IsStressTest;

    // EAP authentication credentials:
    WiFiAccount_t *m_pEapCredentials;

    // This is the credential used to prefill the UI
    WiFiAccount_t *m_pEapPreFilledCredentials;

    // Start-time:
    long m_StartTime;

    // Make the Factory a friend:
    friend class AMFactory_t;
    
public:

    // Initializes or cleans up static resources:
    static void StartupInitialize(void);
    static void ShutdownCleanup(void);

    // Creates and/or retrieves the list of CmdArg objects used to
    // configure the static variables:
    static CmdArgList_t *
    GetCmdArgList(void);

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
        bool                IsBadModes = false,
        bool                IsBadKey = false,
        bool                IsStressTest = false);
    virtual
   ~AuthMatrix_t(void);

    // Initializes, runs or cleans up the test:
    // Returns ERROR_CALL_NOT_IMPLEMENTED if the test is to be skipped.
  __override virtual DWORD Init(void);
  __override virtual DWORD Run(void);
  __override virtual DWORD Cleanup(void);

private:

   typedef enum 
   {
       AP_ConfigType_SetUp,
       AP_ConfigType_PowerDown,
       
       AP_ConfigType_Invalid,
   }AP_ConfigType;

    // Selects and configures the appropriate Access Point.
    DWORD
    ConfigureAccessPoint(AP_ConfigType configType);

    // Makes sure the wireless adapter is properly connected:
    DWORD
    CheckConnected(void);

    DWORD
    CheckDisconnected(void);

    DWORD
    CheckConnectedStress(void);

    DWORD
    DoStressTest(void);
    
};

};
};
#endif /* _DEFINED_AuthMatrix_t_ */
// ----------------------------------------------------------------------------
