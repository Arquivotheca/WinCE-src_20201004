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
#ifndef __BTA2DPPAIR_H__
#define __BTA2DPPAIR_H__

#include <windows.h>




#include <svsutil.hxx>

#include <bt_buffer.h>
#include <bt_ddi.h>
#ifndef BT_ADDR
#include <ws2bth.h>
#endif

#define MAX_NUM_BT_ADRESSES 20

DWORD GetBDAddrList(BD_ADDR* pBdAdresses, const USHORT usNumDevices);


// This method writes the address list back to the registry
// sets the parameter btAddrDefault at the top of the list
// and as the active device.
DWORD SetBDAddrList(BD_ADDR* pBdAdresses, const USHORT cBdAdresses, BD_ADDR bdAddrDefault);
DWORD DeleteBTAddr(BT_ADDR BtAdress);
DWORD AddBTAddr(BT_ADDR BtAdressAdd);

#endif

