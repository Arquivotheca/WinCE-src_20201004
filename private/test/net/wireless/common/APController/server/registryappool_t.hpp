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
// Definitions and declarations for the RegistryAPPool_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_RegistryAPPool_t_
#define _DEFINED_RegistryAPPool_t_
#pragma once

#include <APPool_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Extends APPool to retrieve it's configuration information from the 
// registry.
//
class APConfigDevice_t;
class APConfigNode_t;
class AttenuatorDevice_t;
class AttenuatorNode_t;
class DeviceController_t;
class RegistryAPPool_t : public APPool_t
{
private:

    // List of APConfigDevices:
    APConfigNode_t **m_pConfigList;
    int              m_ConfigListSize;
    int              m_ConfigListAlloc;

    // List of APConfigDevices:
    AttenuatorNode_t **m_pAttenList;
    int                m_AttenListSize;
    int                m_AttenListAlloc;

    // Copy and assignment are deliberately disabled:
    RegistryAPPool_t(const RegistryAPPool_t &src);
    RegistryAPPool_t &operator = (const RegistryAPPool_t &src);

protected:

    // Inserts the specified APController into the list:
    HRESULT
    InsertAPConfigDevice(
        APConfigDevice_t   *pConfig,
        AttenuatorDevice_t *pAttenuator);
    
public:

    // Constructor and destructor:
    RegistryAPPool_t(void);
    virtual
   ~RegistryAPPool_t(void);

    // Initializes the APControllers:
    virtual HRESULT
    LoadAPControllers(const TCHAR *pRootKey);

    // Clears all the APControllers from the list:
    virtual void
    Clear(void);

    // Gets the unique key identifying the specified Access Point or
    // Attenuator:
    const TCHAR *
    GetAPKey(int Index) const;
    const TCHAR *
    GetATKey(int Index) const;

    // Gets the Access Point or Attenuator device-controller identified
    // by the specified unique key:
    DeviceController_t *
    FindAPConfigDevice(const TCHAR *pDeviceKey);
    DeviceController_t *
    FindAttenuatorDevice(const TCHAR *pDeviceKey);
};

};
};
#endif /* _DEFINED_RegistryAPPool_t_ */
// ----------------------------------------------------------------------------
