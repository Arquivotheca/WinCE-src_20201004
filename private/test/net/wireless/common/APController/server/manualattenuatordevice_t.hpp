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
// Definitions and declarations for the ManualAttenuatorDevice_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_ManualAttenuatorDevice_t_
#define _DEFINED_ManualAttenuatorDevice_t_
#pragma once

#include "AttenuatorDevice_t.hpp"

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Extends AttenuationDevice_t to provide an implementation which
// "configures" the RF attenuator by popping up a MessageBox telling
// the operator how the attenuator should be manually configured.
//
class ManualAttenuatorDevice_t : public AttenuatorDevice_t
{
protected:

    // Generates an attenuator-type parameter for use by CreateAttenuator:
    virtual HRESULT
    CreateAttenuatorType(ce::tstring *pConfigParam) const;

public:
   
    // Constructor and destructor:
    ManualAttenuatorDevice_t(
        const TCHAR *pAPName,
        const TCHAR *pConfigKey,
        const TCHAR *pDeviceType,
        const TCHAR *pDeviceName);
    __override virtual
   ~ManualAttenuatorDevice_t(void);
    
    // Gets the current attenuation settings from the registry:
    __override virtual DWORD
    GetAttenuator(
        RFAttenuatorState_t *pResponse);

    // Tells the operator how the sttenuator hould be configured and
    // updates the registry:
    __override virtual DWORD
    SetAttenuator(
        const RFAttenuatorState_t &NewState,
              RFAttenuatorState_t *pResponse);
 
    // Tells the operator how the attenuator should be configured:
    __override virtual HRESULT
    SaveAttenuation(void);
};

};
};
#endif /* _DEFINED_ManualAttenuatorDevice_t_ */
// ----------------------------------------------------------------------------
