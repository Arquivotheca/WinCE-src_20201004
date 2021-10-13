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
#define BUILDING_FS_STUBS
#include <windows.h>

//
// OAK exports
//

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Bit-Based Replication

extern "C"
BOOL xxx_CeRegisterReplNotification(CENOTIFYREQUEST *pRequest)
{
    return CeRegisterReplNotification_Trap(pRequest);
}

extern "C"
BOOL xxx_CeGetReplChangeMask(LPDWORD lpmask)
{
    return CeGetReplChangeMask_Trap(lpmask);
}

extern "C"
BOOL xxx_CeSetReplChangeMask(DWORD mask)
{
    return CeSetReplChangeMask_Trap(mask);
}

extern "C"
BOOL xxx_CeGetReplChangeBitsEx(PCEGUID pguid, CEOID oid, LPDWORD lpbits, DWORD dwFlags)
{
    return CeGetReplChangeBitsEx_Trap(pguid, oid,  lpbits,  dwFlags);
}

extern "C"
BOOL xxx_CeSetReplChangeBitsEx(PCEGUID pguid, CEOID oid, DWORD mask)
{
    return CeSetReplChangeBitsEx_Trap(pguid, oid,  mask);
}

extern "C"
BOOL xxx_CeClearReplChangeBitsEx(PCEGUID pguid, CEOID oid, DWORD mask)
{
    return CeClearReplChangeBitsEx_Trap(pguid, oid,  mask);
}

extern "C"
BOOL xxx_CeGetReplOtherBitsEx(PCEGUID pguid, CEOID oid, LPDWORD lpbits)
{
    return CeGetReplOtherBitsEx_Trap(pguid, oid,  lpbits);
}

extern "C"
BOOL xxx_CeSetReplOtherBitsEx(PCEGUID pguid, CEOID oid, DWORD bits)
{
    return CeSetReplOtherBitsEx_Trap(pguid, oid,  bits);
}

