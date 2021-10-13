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

#ifndef _DEVICEID_H_
#define _DEVICEID_H_

#if (_MSC_VER >= 1000)
#pragma once
#endif

//
// The function IsDeviceId checks whether a device ID is indeed a device ID 
// (vs. a handle).  WAVE_MAPPER is considered a device ID.
//
// The following describes the possible device ID and handle 
// formats that can be passed as param to the Wave APIs.  In order to 
// differentiate between a device ID and a handle, first check for 
// WAVE_MAPPER and then check bit 31.  If bit 31 is zero, then it's a device
// ID; otherwise it's a handle.
//
// 1) Normal Device ID
//
//      Bits        Usage
//      -------------------------------------
//      31-16       Zero
//      15-0        Device ID - Limitation imposed by WAVE_MAKE_DEVICEID
//
// 2) WAVE_MAPPER
//
//      Bits        Usage
//      -------------------------------------
//      31-0        WAVE_MAPPER - 0xFFFFFFFF
//
// 3) WAVE_MAKE_DEVICEID
//
//      Bits        Usage
//      -------------------------------------
//      31          Reserved - Set to zero
//      30-28       Flags
//      27-16       Stream Class ID
//      15-0        Device ID
//
// 4) Wave Handle
//
//      Bits        Usage
//      -------------------------------------
//      31          Reserved - Set to one
//      30-24       Type - Must be non-zero
//      23-16       Serial
//      15-0        Index - Max is 0xFFFE <- This prevents a value of 0xFFFFFFFF for a handle
//

BOOL __inline IsDeviceId(UINT uDeviceID)
{
    // First check for WAVE_MAPPER, then use bit 31 to differentiate between
    // a device ID and a handle.  Returns TRUE for cases 1-3 above.
    return ((WAVE_MAPPER == uDeviceID) || (0 == (uDeviceID & (1 << 31))));
}

//
// Helper functions to extract the device ID and stream class ID from the 
// uDeviceID parameter of waveCommonOpen.
//

UINT __inline WAVE_GET_DEVICEID(UINT uDeviceID)
{
    UINT uDeviceIdRaw;

    if (WAVE_MAPPER == uDeviceID)
    {
        uDeviceIdRaw = WAVE_MAPPER;
    }
    else
    {
        uDeviceIdRaw = ((((uDeviceID & 0x8000) != 0x0) ? 0xFFFF : 0x0) << 16) | (uDeviceID & 0xFFFF);
    }

    return uDeviceIdRaw;
}

DWORD __inline WAVE_GET_STREAMCLASSID(UINT uDeviceID)
{
    DWORD dwStreamClassId;

    if (WAVE_MAPPER == uDeviceID)
    {
        dwStreamClassId = 0;
    }
    else
    {
        dwStreamClassId = ((uDeviceID >> 16) & 0xFF);
    }

    return dwStreamClassId;
}

#endif // _DEVICEID_H_
