//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
