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
// Definitions and declarations for the WiFiAccount_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_WiFiAccount_t_
#define _DEFINED_WiFiAccount_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>
#include <APCTypes.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides credentials and methods for authenticating applications during
// automated WiFi associations.
//
class CmdArgList_t;
class WiFiAccount_t
{
private:

    // Current EAP authentication mode:
    //
    APEapAuthMode_e m_EapAuthMode;

    // Account credentials:
    //
    static const int MaximumUserNameChars = 48;
    static const int MaximumPasswordChars = 48;
    static const int MaximumDomainChars   = 64;
    TCHAR m_UserName[MaximumUserNameChars];
    TCHAR m_Password[MaximumPasswordChars];
    TCHAR m_Domain  [MaximumDomainChars];
    
public:

    // Initializes or cleans up static resources:
    //
    static void StartupInitialize(void);
    static void ShutdownCleanup  (void);

    // Constructor / destructor:
    //
    WiFiAccount_t(
        const TCHAR *pUserName = NULL,
        const TCHAR *pPassword = NULL,
        const TCHAR *pDomain   = NULL);
   ~WiFiAccount_t(void);

    // Copy and assignment:
    //
    WiFiAccount_t(const WiFiAccount_t &rhs);
    WiFiAccount_t &operator = (const WiFiAccount_t &rhs);

    // Certificate enrollment host name:
    //
    static const TCHAR DefaultEnrollHost[]; // "10.10.0.1"
    static const TCHAR *
    GetEnrollHost(void);
    static void
    SetEnrollHost(
        const TCHAR *pValue);

    // Certificate enrollment command name:
    //
    static const TCHAR DefaultEnrollCommand[]; // "enroll"
    static const TCHAR *
    GetEnrollCommand(void);
    static void
    SetEnrollCommand(
        const TCHAR *pValue);

    // Milliseconds to wait for enrollment tool to finish:
    //
    static const long MinimumEnrollTime =      15*1000; // 15 seconds
    static const long DefaultEnrollTime =    3*60*1000; // 3 minutes
    static const long MaximumEnrollTime = 2*60*60*1000; // 2 hours
    static long
    GetEnrollTime(void);
    static DWORD
    SetEnrollTime(
        const TCHAR *pValue);

    // Creates and/or retrieves the list of CmdArg objects used to
    // configure the static variables:
    //
    static CmdArgList_t *
    GetCmdArgList(void);

    // Retrieves a copy of the default credentials for the specified EAP
    // authentication mode:
    //
    static WiFiAccount_t *
    CloneEapCredentials(
        APEapAuthMode_e EapType); 

    // Gets or sets the account credentials:
    //
    const TCHAR *GetUserName(void) const;
    const TCHAR *GetPassword(void) const;
    const TCHAR *GetDomain(void) const;
    DWORD SetUserName(const TCHAR *pValue);
    DWORD SetPassword(const TCHAR *pValue);
    DWORD SetDomain(const TCHAR *pValue);

    // Parses the credentials from the specified string:
    //
    DWORD
    Assign(
        const TCHAR *pParams);

    // Translates the credentials to text form:
    //
    DWORD
    ToString(
      __out_ecount(BufferChars) TCHAR *pBuffer,
                                int     BufferChars) const;

    // Retrieves a certificate from the certificate server:
    //
    DWORD
    Enroll(void);

    // Installs the pseudo-NetUI credential-dialog plugin to authenticate
    // a connection to the specified SSID:
    //
    DWORD
    StartUserLogon(
        const TCHAR    *pSSIDName,
        APEapAuthMode_e EapAuthMode);

    // Uninstalls the pseudo-NetUI credential-dialog plugin:
    //
    void
    CloseUserLogon(void);
    
};

};
};
#endif /* _DEFINED_WiFiAccount_t_ */
// ----------------------------------------------------------------------------
