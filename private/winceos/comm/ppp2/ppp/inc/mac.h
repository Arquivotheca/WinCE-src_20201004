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
/*****************************************************************************
* 
*
*   @doc
*   @module mac.h | PPP MAC Layer Header File
*
*/

#ifndef _MAC_H
#define _MAC_H

#include "rasreg.h"
#include "ndis.h"
#include "ndiswan.h"

//
// Device name/type info returned by OID_TAPI_GET_DEV_CAPS.
// This info is cached by PPP so that we don't have to constantly
// reenumerate it.
//
typedef struct _NDISTAPI_DEVICE_INFO
{
    PWSTR       pwszDeviceType;
    PWSTR       pwszDeviceName;
    BOOL        bIsNullModem;
} NDISTAPI_DEVICE_INFO, *PNDISTAPI_DEVICE_INFO;

// Adapter list
typedef struct _tagNDISWAN_ADAPTER
{
    struct _tagNDISWAN_ADAPTER  *pNext;
    DWORD                       dwRefCnt;
    LPTSTR                      szAdapterName;
    NDIS_HANDLE                 hAdapter;           // The adapter we're using
    DWORD                       dwProviderID;
    DWORD                       dwNumDevices;
    PNDISTAPI_DEVICE_INFO       pDeviceInfo;        // Array [0..dwNumDevices-1] of device type/name info
    BOOL                        bEnumeratingDevices;// TRUE if we are in the process of filling the pDeviceInfo array
    BOOL                        bClosingAdapter;    // TRUE if NdisCloseAdapter called
    NDIS_HANDLE                 hUnbindContext;     // non-NULL if UnbindAdapter called
} NDISWAN_ADAPTER, *PNDISWAN_ADAPTER;



// ----------------------------------------------------------------
//                      Function Declarations
// ----------------------------------------------------------------

// mac.c
DWORD   pppMac_Initialize();
DWORD   pppMac_InstanceCreate (void *SessionContext, void **ReturnedContext,
                               LPCTSTR szDeviceName, LPCTSTR szDeviceType);
void    pppMac_InstanceDelete (void *context);
DWORD   pppMac_Dial (void *context, LPRASPENTRY pRasEntry, LPRASDIALPARAMS pRasDialParams);
void    pppMac_Reset(void *context);
void    pppMac_AbortDial (void *context);

void    pppMac_CallClose(
    void        *context,
    void        (*pCallCloseCompleteCallback)(PVOID),
    PVOID       pCallbackData);

void
pppMac_GetCallSpeed(
    void    *context, 
    PDWORD  pSpeed);

void    pppMac_LineClose (
            void        *context,
            void        (*pLineCloseCompleteCallback)(PVOID),
            PVOID       pCallbackData);
DWORD   pppMac_LineOpen(void *context);
DWORD   pppMac_LineListen(void *context);

void
pppMac_GetFramingInfo(
    IN  PVOID   context,
    OUT PDWORD  pFramingBits,
    OUT PDWORD  pDesiredACCM);

// AdptLst.c
DWORD   pppMac_EnumDevices(LPRASDEVINFOW pRasDevInfo, BOOL bOnlyWalkNullModem, LPDWORD lpcb, LPDWORD lpcDevices);
BOOL    FindAdapter (LPCTSTR szDeviceName, LPCTSTR szDeviceType, PNDISWAN_ADAPTER *ppAdapter, LPDWORD pdwDevID);
BOOL    AdapterDelRef (PNDISWAN_ADAPTER pAdapter);

// Memory.c
PNDIS_WAN_PACKET pppMac_GetPacket (void *context);
NDIS_STATUS NdisWanFreePacket (void *pMac, PNDIS_WAN_PACKET pPacket);

// SndData.c
void
pppMac_GetLinkInfo(
    IN  PVOID           pMac,
    IN  void            (*pGetLinkInfoCompleteCallback)(PVOID, NDIS_WAN_GET_LINK_INFO *, NDIS_STATUS),
    IN  PVOID           pCallbackData);

VOID            pppMac_SetLink(void *context, PNDIS_WAN_SET_LINK_INFO pLinkInfo);
PNDIS_WAN_INFO  pppMacGetWanInfo (void *context);
NDIS_STATUS     pppMacGetWanStats (void *context, OUT PNDIS_WAN_GET_STATS_INFO pStats);
NDIS_STATUS     pppMacSndData(void *pMac, USHORT wProtocol, PNDIS_WAN_PACKET pPacket);

// NdisTapi.c
void NdisTapiAdapterInitialize(PNDISWAN_ADAPTER pAdapter);

NDIS_STATUS
NdisTapiIssueLineGetDevCaps(
    PNDISWAN_ADAPTER pAdapter,
    DWORD            dwDeviceID,
    DWORD            dwNeededSize);

NDIS_STATUS
NdisTapiSetNumLineDevs(
    PNDISWAN_ADAPTER        pAdapter,
    DWORD                   dwNumLineDevs);

LPTSTR NdisTapiLineTranslateAddress (PNDISWAN_ADAPTER pAdapter,
                                     DWORD dwDeviceID,
                                     DWORD dwCountryCode,
                                     LPCTSTR szAreaCode,
                                     LPCTSTR szLocalPhoneNumber,
                                     BOOL fDialable, DWORD dwFlags);
DWORD NdisTapiLineConfigDialogEdit (PNDISWAN_ADAPTER pAdapter, DWORD dwDeviceID,
                                    HWND hwndOwner,
                                    LPCTSTR szDeviceClass,
                                    LPVOID pDeviceConfigIn,
                                    DWORD dwSize,
                                    LPVARSTRING pDeviceConfigOut);

NDIS_STATUS NdisTapiGetDevConfig(PNDISWAN_ADAPTER pAdapter, DWORD dwDeviceID,
                                 LPBYTE *plpbDevConfig, DWORD *pdwSize);

NDIS_STATUS NdisTapiLineAnswer(
    void        *pMac,
    PUCHAR      pUserUserInfo,
    ULONG       ulUserUserInfoSize);

#endif
