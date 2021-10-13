//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

#include "FnFakeIpAddress.h"
#include <auto_xxx.hxx>
#include <winsock2.h>

class FnAddressGenerator
{
private:
    SOCKADDR_STORAGE  m_CurrentAddress;
    int m_iCurrentSubnetLength;
    int m_iCurrentSubnet;
public:
    FnAddressGenerator( );

    // Generate a new IPv4 address - This function only supports 254 addresses per subnet
    ce::smart_ptr<FnFakeIpAddress> GenerateIPv4Address();

    // Get the gateway for the current subnet, always X.X.X.1
    ce::smart_ptr<FnFakeIpAddress> GenerateIPv4GatewayAddress();

    //move to the next subnet
    void NextSubnet();

	// Set the current address
	void SetCurrentAddress( SOCKADDR_STORAGE newCurrentAddress );
};


