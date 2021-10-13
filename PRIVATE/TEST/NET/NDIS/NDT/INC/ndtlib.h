//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __NDT_LIB_H
#define __NDT_LIB_H

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------

#define NDT_COUNTER_UNEXPECTED_EVENTS        1

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

HRESULT NDTSend(
   HANDLE hAdapter, ULONG cbAddr, UCHAR* pucSrcAddr, ULONG nDestAddr, 
   UCHAR* apucDestAddrs[], UCHAR ucResponseMode, UCHAR ucPacketSizeMode,
   ULONG ulPacketSize, ULONG ulPacketCount, UINT uiBeatDelay, UINT uiBeatGroup, 
   HANDLE* phRequest
);

HRESULT NDTSendWait(
   HANDLE hAdapter, HANDLE hRequest, DWORD dwTimeout, ULONG* pulPacketsSent,
   ULONG* pulPacketsCompleted, ULONG* pulPacketsCanceled, 
   ULONG* pulPacketsUncanceled, ULONG* pulPacketsReplied, ULONG* pulTime, 
   ULONG* pulBytesSent, ULONG* pulBytesReceived
);

HRESULT NDTReceive(HANDLE hAdapter, HANDLE *phRequest);

HRESULT NDTReceiveStop(
   HANDLE hAdapter, HANDLE hRequest, ULONG* pulPacketsReceived, 
   ULONG* pulPacketsReplied, ULONG* pulPacketsCompleted, ULONG* pulTime,
   ULONG* pulBytesSent, ULONG* pulBytesReceived
);

//------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

//------------------------------------------------------------------------------

#endif
