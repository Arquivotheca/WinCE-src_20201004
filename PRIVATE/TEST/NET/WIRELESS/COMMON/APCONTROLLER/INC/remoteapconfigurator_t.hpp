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
// Definitions and declarations for the RemoteAPConfigurator_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_RemoteAPConfigurator_t_
#define _DEFINED_RemoteAPConfigurator_t_
#pragma once

#include "APConfigurator_t.hpp"
#include "APControlClient_t.hpp"
#include <inc/sync.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Extends APConfigurator_t to specialize it to provide an implementation
// which configures Access Points using a remote APControl server.
//
class RemoteAPConfigurator_t : public APConfigurator_t
{   
private:

    // APControl connection controller:
    APControlClient_t m_Client;
    
    // Synchronization object:
    ce::critical_section m_Locker;

protected:

    // (Re)connects to the APControl control server and gets the current
    // access point configuration values:
    virtual HRESULT
    Reconnect(void);

public:
   
    // Constructor and destructor:
    RemoteAPConfigurator_t(
        const TCHAR *pServerHost,
        const TCHAR *pServerPort,
        const TCHAR *pDeviceType,
        const TCHAR *pDeviceName);
    virtual
   ~RemoteAPConfigurator_t(void);

    // Gets the access point device type:
    const TCHAR *
    GetDeviceType(void) const
    {
        return m_Client.GetDeviceType();
    }
    
    // Sends the updated configuration values (if any) to the Access
    // Point device and stores them in the configuration store:
    virtual HRESULT
    SaveConfiguration(void);
};

};
};
#endif /* _DEFINED_RemoteAPConfigurator_t_ */
// ----------------------------------------------------------------------------
