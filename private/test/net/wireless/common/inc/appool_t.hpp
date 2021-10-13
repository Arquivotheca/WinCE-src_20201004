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
// Definitions and declarations for the APPool_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_APPool_t_
#define _DEFINED_APPool_t_
#pragma once

#include <APController_t.hpp>

namespace ce {
namespace qa {

typedef enum APPoolStatus
{
    APPoolStatus_Init = 0,
    APPoolStatus_Fail,
    APPoolStatus_Done,    
};

// ----------------------------------------------------------------------------
//
// Provides a list of APController_t objects.
//
class APControlClient_t;
class APPool_t
{
private:

    // List of APControllers:
    APController_t **m_pAPList;
    int              m_APListSize;
    int              m_APListAlloc;

    // APControl server interface (optional):
    APControlClient_t *m_pClient;

    // Current test status:
    DWORD m_Status;

    // Copy and assignment are deliberately disabled:
    APPool_t(const APPool_t &src);
    APPool_t &operator = (const APPool_t &src);
    
protected:

    // Inserts the specified APController into the list:
    HRESULT
    InsertAPController(
        APConfigurator_t    *pConfig,
        AttenuationDriver_t *pAttenuator);

public:

    // Constructor and destructor:
    APPool_t(void);
    virtual
   ~APPool_t(void);

    // Loads the list from those APControllers managed by the specified
    // APControl server. If the optional server-time argument is supplied,
    // also returns the current system-time on the server:
    virtual HRESULT
    LoadAPControllers(
        IN  const TCHAR *pServerHost,
        IN  const TCHAR *pServerPort,
        OUT SYSTEMTIME  *pServerTime = NULL);

    // Disconnects from the APController server:
    virtual void
    Disconnect(void);

    // Clears all the APControllers from the list:
    virtual void
    Clear(void);

    // Determines how many AP controllers are in the list:
    int
    size(void) const
    {
        return m_APListSize;
    }

    // Retrieves an AP controller by index or case-insensitive name:
          APController_t *GetAP(int APIndex);
    const APController_t *GetAP(int APIndex) const;
          APController_t *FindAP(const TCHAR *pAPName);
    const APController_t *FindAP(const TCHAR *pAPName) const;

    // Gets or sets the current test status:
    HRESULT
    GetTestStatus(DWORD *pStatus);
    HRESULT
    SetTestStatus(DWORD Status);

    // Launch or terminate a server-side process:
    HRESULT
    LaunchServerProcess(
        const TCHAR* pszCommandLine,
        DWORD*       pdwProcessId);
    HRESULT
    TerminateServerProcess(
        const TCHAR* pszCommandLine,
        DWORD        dwProcessId);
};

};
};
#endif /* _DEFINED_APPool_t_ */
// ----------------------------------------------------------------------------
