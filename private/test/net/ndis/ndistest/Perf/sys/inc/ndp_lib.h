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
#ifndef __NDP_LIB_H
#define __NDP_LIB_H
#include "ndp_protocol.h"
#ifndef UNDER_CE
#include <strsafe.h>
#endif

//------------------------------------------------------------------------------

#define ETH_MAX_FRAME_SIZE    1514
#define ETH_ADDR_SIZE         6

//------------------------------------------------------------------------------

#ifdef UNDER_CE
DWORD LoadAdapter(LPCTSTR szAdapter,LPCTSTR szDriver);
DWORD UnloadAdapter(LPCTSTR szAdapter);
DWORD QueryAdapters(LPTSTR mszAdapters, DWORD dwSize);
DWORD QueryProtocols(LPTSTR mszProtocols, DWORD dwSize);
DWORD QueryBindings(LPCTSTR szAdapter, LPTSTR mszProtocols, DWORD dwSize);
DWORD BindProtocol(LPCTSTR szAdapterName, LPCTSTR szProtocol);
DWORD UnbindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol);
DWORD ReBindProtocol(LPCTSTR szAdapterName, LPCTSTR szProtocol);

#if 0
	HANDLE EnableWakeUp(LPCTSTR szEventName, DWORD dwSeconds, HANDLE * pWaitEvent);
	void DisableWakeUp(HANDLE hNotify, HANDLE hWaitEvent);
#endif 

HANDLE SetDeviceWakeUp(DWORD dwWakeSrc, DWORD dwSeconds);
BOOL ClearDeviceWakeUp(HANDLE hAdapter, DWORD dwWakeSrc);

#else

typedef struct {
   WCHAR szwDisplayName[256];
   WCHAR szwBindName[256];
   WCHAR szwHelpText[256];
   WCHAR szwId[256];
} tsNtAda, * PtsNtAda;

#define MAX_ENUM_NT_ADAPTERS (25)

typedef struct tsNtEnumAda{
   tsNtAda sNtAda;
   struct tsNtEnumAda *next;
} tsNtEnumAda, * PtsNtEnumAda;

HRESULT NtEnumAdapters(PtsNtEnumAda *psNtEnumAd);

#endif
//------------------------------------------------------------------------------

HANDLE OpenProtocol();
VOID CloseProtocol(HANDLE hAdapter);
VOID CloseBackChannel();

BOOL OpenAdapter(HANDLE hAdapter, LPCTSTR szAdapter);
BOOL CloseAdapter(HANDLE hAdapter);

BOOL StartClient(const TCHAR *szServerAddr, ULONG ulPort, PULONG pErrorCode);

BOOL SendPacket(
   teNDPPacketType packetType, PVOID pParams, PULONG pErrorCode
);

BOOL StartServer(const TCHAR *szServerAddr, ULONG ulPort, PULONG pErrorCode);

BOOL Listen(HANDLE hAdapter, DWORD poolSize, BOOL bDirect, BOOL bBroadcast);
BOOL ReceivePacket(
   teNDPPacketType *pPacketType, PVOID pParams, PULONG pErrorCode
);
BOOL StressSend(
   HANDLE hAdapter, BOOL bSend, UCHAR dstAddr[], teNDPPacketType packetType, 
   DWORD poolSize, DWORD packetSize, DWORD packetsSend,
   DWORD dwFlagStressControlled,DWORD dwDelayInABurst, DWORD dwPacketsInABurst,
   LONGLONG *pTime, LONGLONG *pIdleTime, DWORD *pPacketsSent, DWORD *pBytesSent
);
BOOL StressReceive(
   HANDLE hAdapter, DWORD poolSize, teNDPPacketType packetType, UCHAR srcAddr[], 
   teNDPPacketType *pPacketType, UCHAR *pData, DWORD *pDataSize, LONGLONG* pTime, 
   LONGLONG* pIdleTime, DWORD *pPacketReceived, DWORD *pBytesReceived
);

BOOL Accept(PULONG errorCode);
DWORD WINAPI EndStressReceive(LPVOID param);
BOOL EndStressReceiveAsync(HANDLE hAdapter);

#include <ntddndis.h>

BOOL QueryMPOID(HANDLE hAdapter,NDIS_OID oid, PBYTE puBuff, PDWORD pdwBuffSize, NDIS_STATUS * status);
BOOL SetMPOID(HANDLE hAdapter,NDIS_OID oid, PBYTE puBuff, PDWORD pdwBuffSize, NDIS_STATUS * status);

BOOL GetMACAddress(const TCHAR* tchAdapterInstance, PVOID pvMACAddressBuffer);

//------------------------------------------------------------------------------

#endif
