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
// Definitions and declarations of the basic WLAN types and translators.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_NdisTypes_hpp_
#define _DEFINED_NdisTypes_hpp_
#pragma once

#include <WiFiTypes.hpp>
#include <ntddndis.h>
#include <nuiouser.h>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Ndis Medium Types :
//

const TCHAR *
NdisMediumType2Name(
    NDIS_MEDIUM medium);

// ----------------------------------------------------------------------------
//
// Ndis Network Types :
//

const TCHAR *
NdisNetworkType2Name(
    NDIS_802_11_NETWORK_TYPE network);

// ----------------------------------------------------------------------------
//
// Ndis Network Operation Mode :
//

const TCHAR *
NdisNetworkMode2Name(
    NDIS_802_11_NETWORK_INFRASTRUCTURE  mode);

// ----------------------------------------------------------------------------
//
// Ndis Dot11 Power Mode :
//
const TCHAR *
NdisDot11PowerMode2Name(
    DOT11_POWER_MODE mode);

// ----------------------------------------------------------------------------
//
// Ndis Dot11 Operation Mode :
//
const TCHAR *
NdisDot11OperationMode2Name(
    ULONG mode);


// ----------------------------------------------------------------------------
//
// Ndis Dot11 Power Level :
//
const TCHAR *
NdisDot11PowerLevel2Name(
    ULONG level);


// ----------------------------------------------------------------------------
//
// Ndis Dot11 Association States :
//
const TCHAR *
NdisDot11AssociationState2Name(
    DOT11_ASSOCIATION_STATE state);

// ----------------------------------------------------------------------------
//
// Ndis Power Mode :
//

const TCHAR *
NdisPowerMode2Name(
    NDIS_802_11_POWER_MODE   mode);

// ----------------------------------------------------------------------------
//
// Ndis Physical Medium Types :
//

const TCHAR *
NdisPhysicalMediumType2Name(
    NDIS_PHYSICAL_MEDIUM medium);

};
};

#endif /* _DEFINED_NdisTypes_hpp_ */
// ----------------------------------------------------------------------------
