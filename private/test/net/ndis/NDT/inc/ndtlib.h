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
#ifndef __NDT_LIB_H
#define __NDT_LIB_H

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------

#define NDT_COUNTER_UNEXPECTED_EVENTS        (1)
#define NDT_COUNTER_STATUS_RESET_START       (2)
#define NDT_COUNTER_STATUS_RESET_END		 (3)
#define NDT_COUNTER_STATUS_MEDIA_CONNECT	 (4)
#define NDT_COUNTER_STATUS_MEDIA_DISCONNECT	 (5)

//------------------------------------------------------------------------------

#define NDT_RESPONSE_NONE                    0
#define NDT_RESPONSE_FULL                    1
#define NDT_RESPONSE_ACK                     2
#define NDT_RESPONSE_ACK10                   3
#define NDT_RESPONSE_MASK                    0x0F

#define NDT_RESPONSE_FLAG_WINDOW             0x40
#define NDT_RESPONSE_FLAG_RESPONSE           0x80
#define NDT_RESPONSE_FLAG_MASK               0xF0

#define NDT_PACKET_TYPE_FIXED                0x00
#define NDT_PACKET_TYPE_RANDOM               0x01
#define NDT_PACKET_TYPE_CYCLICAL             0x02
#define NDT_PACKET_TYPE_RAND_SMALL           0x03
#define NDT_PACKET_TYPE_MASK                 0x03

#define NDT_PACKET_BUFFERS_NORMAL            0x00
#define NDT_PACKET_BUFFERS_RANDOM            0x04
#define NDT_PACKET_BUFFERS_SMALL             0x08
#define NDT_PACKET_BUFFERS_ZEROS             0x0C
#define NDT_PACKET_BUFFERS_ONES              0x10
#define NDT_PACKET_BUFFERS_MASK              0x1C

#define NDT_PACKET_FLAG_CANCEL               0x40
#define NDT_PACKET_FLAG_GROUP                0x80
#define NDT_PACKET_FLAG_MASK                 0xC0

#define FLAG_NDT_ENABLE_WAKE_UP		(1)
#define FLAG_NDT_DISABLE_WAKE_UP	(2)
#define FLAG_NDT_ENABLE_RTC_TIMER	(4)
#define FLAG_NDT_DISABLE_RTC_TIMER	(8)

typedef enum {NDT_PPC=1, //Pocket PC
                NDT_SP,  //Smart phone
                NDT_CE,  //CE
                NDT_MAX  //Error.
} tePlatType;
BOOL GetPlatformType(tePlatType * pPlatform);
//------------------------------------------------------------------------------

LPTSTR NDTSystem(LPCTSTR szAdapter);

HRESULT NDTStartup(LPCTSTR szSystem);
HRESULT NDTCleanup(LPCTSTR szSystem);

HRESULT NDTQueryAdapters(LPCTSTR szSystem, LPTSTR mszAdapters, DWORD dwSize);
HRESULT NDTQueryProtocols(LPCTSTR szSystem, LPTSTR mszProtocols, DWORD dwSize);
HRESULT NDTQueryBindings(LPCTSTR szAdapter, LPTSTR mszProtocols, DWORD dwSize);

HRESULT NDTBindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol);
HRESULT NDTUnbindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol);

HRESULT NDTOpen(LPTSTR szAdapterName, HANDLE* phAdapter);
HRESULT NDTClose(HANDLE* phAdapter);

HRESULT NDTLoadMiniport(HANDLE hAdapter);
HRESULT NDTUnloadMiniport(HANDLE hAdapter);

HRESULT NDTWriteVerifyFlag(HANDLE hAdapter, DWORD dwFlag);
HRESULT NDTDeleteVerifyFlag(HANDLE hAdapter);

HRESULT NDTBind(
   HANDLE hAdapter, BOOL bForce30, NDIS_MEDIUM ndisMedium, 
   NDIS_MEDIUM* pNdisMedium = NULL
);
HRESULT NDTUnbind(HANDLE hAdapter);

HRESULT NDTSetOptions(HANDLE hAdapter, DWORD dwZoneMask, DWORD dwBeatDelay = 0);
HRESULT NDTGetCounter(HANDLE hAdapter, ULONG nIndex, ULONG* pnValue);
HRESULT NDTClearCounters(HANDLE hAdapter);
    
HRESULT NDTReset(HANDLE hAdapter);

HRESULT NDTSetId(HANDLE hAdapter, USHORT usLocalId, USHORT usRemoteId);

HRESULT NDTQueryInfo(
   HANDLE hAdapter, NDIS_OID ndisOid, PVOID pvBuffer, UINT cbBuffer, 
   UINT* pcbUsed, UINT* pcbRequired
);

HRESULT NDTSetInfo(
   HANDLE hAdapter, NDIS_OID ndisOid, PVOID pvBuffer, UINT cbBuffer, 
   UINT* pcbUsed, UINT* pcbRequired
);

HRESULT NDTMethodnfo(
   HANDLE hAdapter, NDIS_OID ndisOid, PVOID pvBuffer, ULONG ulMethodId, ULONG cbInpBuffer, 
   ULONG cbOutBuffer, UINT* pcbUsed, UINT* pcbRequired
);

HRESULT NDTSend(
   HANDLE hAdapter, ULONG cbAddr, UCHAR* pucSrcAddr, ULONG nDestAddr, 
   UCHAR* apucDestAddrs[], UCHAR ucResponseMode, UCHAR ucPacketSizeMode,
   ULONG ulPacketSize, ULONG ulPacketCount, ULONG ulConversationId, UINT uiBeatDelay, UINT uiBeatGroup, 
   HANDLE* phRequest
);

#ifndef NDT6
HRESULT NDTSendWait(
   HANDLE hAdapter, HANDLE hRequest, DWORD dwTimeout, ULONG* pulPacketsSent,
   ULONG* pulPacketsCompleted, ULONG* pulPacketsCanceled, 
   ULONG* pulPacketsUncanceled, ULONG* pulPacketsReplied, ULONG* pulTime, 
   ULONG* pulBytesSent, ULONG* pulBytesReceived
);
#else
HRESULT NDTSendWait(
   HANDLE hAdapter, HANDLE hRequest, DWORD dwTimeout, ULONG* pulPacketsSent,
   ULONG* pulPacketsCompleted, ULONG* pulPacketsCanceled, 
   ULONG* pulPacketsUncanceled, ULONG* pulPacketsReplied, ULONGLONG* pulTime, 
   ULONG* pulBytesSent, ULONG* pulBytesReceived
);
#endif

HRESULT NDTReceive(HANDLE hAdapter, ULONG ulConversationId, HANDLE *phRequest);

HRESULT NDTReceiveEx(HANDLE hAdapter, ULONG ulConversationId, 
    ULONG cbAddr, ULONG nDestAddr, BYTE* apucDestAddrs[], HANDLE *phRequest);

HRESULT NDTReceiveExStop(
   HANDLE hAdapter, HANDLE hRequest, ULONG* pulPacketsReceived
);

#ifndef NDT6
HRESULT NDTReceiveStop(
   HANDLE hAdapter, HANDLE hRequest, ULONG* pulPacketsReceived, 
   ULONG* pulPacketsReplied, ULONG* pulPacketsCompleted, ULONG* pulTime,
   ULONG* pulBytesSent, ULONG* pulBytesReceived
);
#else
HRESULT NDTReceiveStop(
   HANDLE hAdapter, HANDLE hRequest, ULONG* pulPacketsReceived, 
   ULONG* pulPacketsReplied, ULONG* pulPacketsCompleted, ULONGLONG* pulTime,
   ULONG* pulBytesSent, ULONG* pulBytesReceived
);
#endif

HRESULT NDTStatus(HANDLE hAdapter, ULONG ulStatus, HANDLE *phStatus);
#ifdef NDT6
HRESULT NDTStatusWait(HANDLE hAdapter, HANDLE hRequest, DWORD dwTimeout, UINT *pcbStatusBuffer, PVOID pStatusBuffer);
#else
HRESULT NDTStatusWait(HANDLE hAdapter, HANDLE hRequest, DWORD dwTimeout);
#endif
DWORD NDTGetWakeUpResolution();
HRESULT NDTHalSetupWake(HANDLE hAdapter, DWORD dwWakeSrc, DWORD dwFlags, DWORD dwTimeOut);
HANDLE NDTEnableWakeUp(LPCTSTR szEventName, DWORD dwSeconds, HANDLE * pWaitEvent);
void NDTDisableWakeUp(HANDLE hNotify, HANDLE hWaitEvent);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif
