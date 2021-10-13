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
// Definitions and declarations for the ManualAPConfigDevice_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_ManualAPConfigDevice_t_
#define _DEFINED_ManualAPConfigDevice_t_
#pragma once

#include "APConfigDevice_t.hpp"


namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Extends APConfigiDevice_t to provide an implementation which "configures"
// the access point by popping up a MessageBox telling the operator how the
// AP should be manually configured.
//
class ManualAPConfigDevice_t : public APConfigDevice_t
{
protected:

    // Generates a configurator-type parameter for use by CreateConfigurator:
    __override virtual HRESULT
    CreateConfiguratorType(ce::tstring *pConfigParam) const;

public:

    // Constructor and destructor:
    ManualAPConfigDevice_t(
        const TCHAR *pAPName,
        const TCHAR *pConfigKey,
        const TCHAR *pDeviceType,
        const TCHAR *pDeviceName);
    __override virtual
   ~ManualAPConfigDevice_t(void);

    // Gets the current Access Point configuration from the registry:
    __override virtual DWORD
    GetAccessPoint(
        AccessPointState_t *pResponse);

    // Tells the operator how the Access Point should be configured and
    // updates the registry:
    __override virtual DWORD
    SetAccessPoint(
        const AccessPointState_t &NewState,
              AccessPointState_t *pResponse);

    // Tells the operator how the Access Point should be configured:
    __override virtual HRESULT
    SaveConfiguration(void);
};

};
};
#endif /* _DEFINED_ManualAPConfigDevice_t_ */
// ----------------------------------------------------------------------------
