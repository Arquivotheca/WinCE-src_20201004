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
// Header file for FakeWiFiUtil functions
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_FAKEFACTORY_
#define _DEFINED_FAKEFACTORY_
#pragma once

#include <auto_xxx.hxx>

namespace ce {
namespace qa {
class FakeWifiUtilIntf_t;
class FakeFatWifiUtilIntf_t;
class FakeNativeWifiUtilIntf_t;

// ----------------------------------------------------------------------------
//
// Types of fakewifi drivers
//
enum FakeWifi_e
{
    Legacy				= 1 // XWifi driver
   ,NWifi               = 2 // Fake Native Wifi driver
};

	
class FakeFactory_t
{
public:
	// Creates an implementation of the IFakeWifiUtil interface
	// depending on the driver type specified:
	ce::smart_ptr<FakeWifiUtilIntf_t> CreateFakeWifiUtil(
		FakeWifi_e DriverType);
};
};
};
#endif /* _DEFINED_FAKEFACTORY_ */