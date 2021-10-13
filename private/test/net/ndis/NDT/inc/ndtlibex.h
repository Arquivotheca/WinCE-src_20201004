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
#ifndef __NDT_LIB_EX_H
#define __NDT_LIB_EX_H

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------

void   NDTGetMediumInfo(NDIS_MEDIUM ndisMedium, UINT* pcbAddr, UINT* pcbHeader);
UCHAR* NDTGetRandomAddr(NDIS_MEDIUM ndisMedium);
UCHAR* NDTGetRandomAddrEx(NDIS_MEDIUM ndisMedium, unsigned short int uiVar, PUCHAR pucAddr);
UCHAR* NDTGetBroadcastAddr(NDIS_MEDIUM ndisMedium);
UCHAR* NDTGetMulticastAddr(NDIS_MEDIUM ndisMedium);
UCHAR* NDTGetMulticastAddrEx(NDIS_MEDIUM ndisMedium, UINT uiVar);
UCHAR* NDTGetBadMulticastAddr(NDIS_MEDIUM ndisMedium);
UCHAR* NDTGetGroupAddr(NDIS_MEDIUM ndisMedium);
DWORD  NDTGetReceiveDelay(NDIS_MEDIUM ndisMedium);

HRESULT NDTGetPermanentAddr(
   HANDLE hAdapter, NDIS_MEDIUM ndisMedium, UCHAR** ppucAddress
);
HRESULT NDTGetMaximumFrameSize(HANDLE hAdapter, UINT *pulMaximumFrameSize);
HRESULT NDTGetMaximumTotalSize(HANDLE hAdapter, UINT *pulMaximumTotalSize);
HRESULT NDTSetPacketFilter(HANDLE hAdapter, UINT uiFilter);
HRESULT NDTGetPacketFilter(HANDLE hAdapter, UINT *puiFilter);
HRESULT NDTGetPhysicalMedium(HANDLE hAdapter, UINT* puiMedium);
HRESULT NDTGetLinkSpeed(HANDLE hAdapter, UINT* puiLinkSpeed);

HRESULT NDTGetFilters(
   HANDLE hAdapter, NDIS_MEDIUM ndisMedium, UINT* puiFilters
);
HRESULT NDTAddMulticastAddr(
   HANDLE hAdapter, NDIS_MEDIUM ndisMedium, UCHAR* pbAddress
);
HRESULT NDTDeleteMulticastAddr(
   HANDLE hAdapter, NDIS_MEDIUM ndisMedium, UCHAR* pbAddress
);

ULONG NDTGetRandom(ULONG ulLow, ULONG ulHigh);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif
