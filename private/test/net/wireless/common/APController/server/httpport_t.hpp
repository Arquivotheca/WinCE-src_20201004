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
// Definitions and declarations for the HttpPort_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_HttpPort_t_
#define _DEFINED_HttpPort_t_
#pragma once

#include "HtmlForms_t.hpp"
#include <sync.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides a contention-controlled interface to an HTTP web-server.
// Unless external synchronization is provided, each of these objects
// should be used by a single thread at a time.
//
class HttpHandle_t;
class HttpPort_t
{
private:

    // Current web-server connection:
    HttpHandle_t *m_pHandle;

    // Copy and assignment are deliberately disabled:
    HttpPort_t(const HttpPort_t &src);
    HttpPort_t &operator = (const HttpPort_t &src);

public:
    
    // Contructor and destructor:
    HttpPort_t(const TCHAR *pServerHost);
    virtual
   ~HttpPort_t(void);

    // Returns true if the port handle is valid:
    bool IsValid(void) const;

    // Gets the web-server name or address:
    const TCHAR *
    GetServerHost(void) const;

    // Gets or sets the user-name:
    const TCHAR *
    GetUserName(void) const;
    void
    SetUserName(const TCHAR *Value);

    // Gets or sets the admin password:
    const TCHAR *
    GetPassword(void) const;
    void
    SetPassword(const TCHAR *Value);

    // Retrieves an object which can be locked to prevent other threads
    // from accessing this web-server:
    // Callers should lock this object before performing any I/O operations.
    ce::critical_section &
    GetLocker(void) const;

    // Closes the existing connection to force a reconnection next time:
    void
    Disconnect(void);

    // Loads the specified web-page:
    DWORD
    SendPageRequest(
        const WCHAR *pPageName,   // page to be loaded
        long         SleepMillis, // wait time before reading response
        ce::string  *pResponse);  // response from device

    // Sends a configuration-update command for the specified web-page:
    DWORD
    SendUpdateRequest(
        const WCHAR    *pPageName,   // page being updated
        const WCHAR    *pMethod,     // update method (GET or POST)
        const WCHAR    *pAction,     // action to be performed
        const ValueMap &Parameters,  // modified form parameters
        long            SleepMillis, // wait time before reading response
        ce::string     *pResponse);  // response from device

    // Sends a configuration-update command for the specified web-page
    // and checks the response:
    DWORD
    SendUpdateRequest(
        const WCHAR    *pPageName,   // page being updated
        const WCHAR    *pMethod,     // update method (GET or POST)
        const WCHAR    *pAction,     // action to be performed
        const ValueMap &Parameters,  // modified form parameters
        long            SleepMillis, // wait time before reading response
        ce::string     *pResponse,   // response from device
        const char     *pExpected);  // expected response

    // Gets or sets the specified cookie:
    const WCHAR *
    GetCookie(const char *pName) const;
    DWORD
    SetCookie(const char *pName, const char *pValue);
    DWORD
    SetCookie(const char *pName, const WCHAR *pValue);
};

};
};
#endif /* _DEFINED_HttpPort_t_ */
// ----------------------------------------------------------------------------
