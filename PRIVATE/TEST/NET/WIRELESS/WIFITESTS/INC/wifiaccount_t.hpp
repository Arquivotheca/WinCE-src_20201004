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

#include <APController_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides credentials and methods for authenticating applications during
// automated WiFi associations.
//
class WiFiAccountDialog_t;
class WiFiAccount_t
{
private:

    // Account credentials:
    enum { MaximumUserNameChars = 48 };
    enum { MaximumPasswordChars = 48 };
    enum { MaximumDomainChars   = 64 };
    TCHAR m_UserName[MaximumUserNameChars];
    TCHAR m_Password[MaximumPasswordChars];
    TCHAR m_Domain  [MaximumDomainChars];

    // Dialog-box sub-threads:
    enum { MaximumDialogThreads = 10 };
    WiFiAccountDialog_t *m_pDialogThreads[MaximumDialogThreads];
    int                  m_NumberDialogThreads;

public:

    // Certificate enrollment host name:
    // (This is usually the Radius Server IP address.)
    static const TCHAR DefaultEnrollHost[];
    static const TCHAR    *GetEnrollHost(void);
    static void            SetEnrollHost(const TCHAR *pValue);

    // Certificate enrollment command name:
    // (This is usually "enroll".)
    static const TCHAR DefaultEnrollCommand[];
    static const TCHAR    *GetEnrollCommand(void);
    static void            SetEnrollCommand(const TCHAR *pValue);

    // Title of "set root-certificate" dialog-box":
    // (This is usually "Root Certificate Store".)
    static const TCHAR DefaultEnrollRootDialog[];
    static const TCHAR    *GetEnrollRootDialog(void);
    static void            SetEnrollRootDialog(const TCHAR *pValue);

    // Title of "insert new certificate" dialog-box":
    // (This is usually "Enrollment Tool".)
    static const TCHAR DefaultEnrollToolDialog[];
    static const TCHAR    *GetEnrollToolDialog(void);
    static void            SetEnrollToolDialog(const TCHAR *pValue);

    // Milliseconds to wait for enrollment tool to finish:
    static const long MinimumEnrollTime;
    static const long DefaultEnrollTime;
    static const long MaximumEnrollTime;
    static long           GetEnrollTime(void);
    static DWORD          SetEnrollTime(const TCHAR *pValue);

    // Title of the user logon dialog-box":
    // (This is usually "User Logon".)
    static const TCHAR DefaultUserLogonDialog[];
    static const TCHAR    *GetUserLogonDialog(void);
    static void            SetUserLogonDialog(const TCHAR *pValue);

    // Title of the network password dialog-box":
    // (This is usually "Enter Network Password".)
    static const TCHAR DefaultNetPasswordDialog[];
    static const TCHAR    *GetNetPasswordDialog(void);
    static void            SetNetPasswordDialog(const TCHAR *pValue);

    // Milliseconds to wait for user logon dialog to close:
    static const long MinimumLogonCloseTime;
    static const long DefaultLogonCloseTime;
    static const long MaximumLogonCloseTime;
    static long           GetLogonCloseTime(void);
    static DWORD          SetLogonCloseTime(const TCHAR *pValue);

    // Initializes or cleans up static resources:
    static void StartupInitialize(void);
    static void ShutdownCleanup  (void);

    // Parses the class's command-line arguments:
    static void  PrintUsage(void);
    static DWORD ParseCommandLine(int argc, TCHAR *argv[]);

    // Constructor and destructor:
    WiFiAccount_t(
        const TCHAR *pUserName = NULL,
        const TCHAR *pPassword = NULL,
        const TCHAR *pDomain   = NULL);
   ~WiFiAccount_t(void);

    // Gets or sets the account credentials:
    const TCHAR *GetUserName(void) const;
    const TCHAR *GetPassword(void) const;
    const TCHAR *GetDomain  (void) const;
    DWORD SetUserName(const TCHAR *pValue);
    DWORD SetPassword(const TCHAR *pValue);
    DWORD SetDomain  (const TCHAR *pValue);

    // Parses the credentials from the specified string:
    DWORD
    Assign(
        const TCHAR *pParams);

    // Translates the credentials to text form:
    DWORD
    ToString(
      __out_ecount(BufferChars) TCHAR *pBuffer,
                                int     BufferChars) const;

    // Retrieves a certificate from the certificate server:
    DWORD
    Enroll(void);

    // Starts the user logon dialog-box sub-thread to authenticate a
    // connection to the specified SSID. If possible, sets the EAPOL
    // registry so the system doesn't need to launch the dialog.
    DWORD
    StartUserLogon(
        const TCHAR    *pSSIDName,
        APEapAuthMode_e EapAuthMode);

    // Stops the user logon dialog-box sub-thread:
    void
    CloseUserLogon(DWORD CloseWaitTimeMS);

private:

    // Stops the dialog-box sub-threads with or without a WaitFor:
    DWORD
    StopDialogThreads(DWORD CloseWaitTimeMS);
    void
    CloseDialogThreads(void);
};

};
};
#endif /* _DEFINED_WiFiAccount_t_ */
// ----------------------------------------------------------------------------
