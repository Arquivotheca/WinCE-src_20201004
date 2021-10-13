//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __NDP_IOCTL_H
#define __NDP_IOCTL_H

#ifdef UNDER_CE

#define NDP_PROTOCOL_DOS_NAME     _T("NDP1:")

#else

#define NDP_PROTOCOL_DOS_NAME_L   L"\\\\.\\NDP"
#define NDP_PROTOCOL_DOS_NAME     _T("\\\\.\\NDP")

#endif

//------------------------------------------------------------------------------

#define IOCTL_NDP_OPEN_MINIPORT \
   CTL_CODE(FILE_DEVICE_NETWORK, 0x0001, METHOD_BUFFERED, FILE_ANY_ACCESS)

//------------------------------------------------------------------------------

#define IOCTL_NDP_CLOSE_MINIPORT \
   CTL_CODE(FILE_DEVICE_NETWORK, 0x0002, METHOD_BUFFERED, FILE_ANY_ACCESS)

//------------------------------------------------------------------------------

#define IOCTL_NDP_LISTEN \
   CTL_CODE(FILE_DEVICE_NETWORK, 0x0011, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct {
   BOOLEAN fBroadcast;
   BOOLEAN fDirected;
   UINT  poolSize;
} NDP_LISTEN_INP;

//------------------------------------------------------------------------------

#define IOCTL_NDP_SEND_PACKET \
   CTL_CODE(FILE_DEVICE_NETWORK, 0x0012, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct {
   UCHAR destAddr[6];
   UINT  packetType;
   UINT  dataSize;
   UCHAR data[1];
} NDP_SEND_PACKET_INP;

//------------------------------------------------------------------------------

#define IOCTL_NDP_RECV_PACKET \
   CTL_CODE(FILE_DEVICE_NETWORK, 0x0013, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct {
   DWORD timeout;
} NDP_RECV_PACKET_INP;

typedef struct {
   UINT  packetType;
   UCHAR srcAddr[6];
   UINT  dataSize;
   UCHAR data[1];
} NDP_RECV_PACKET_OUT;

//------------------------------------------------------------------------------

#define IOCTL_NDP_STRESS_SEND \
   CTL_CODE(FILE_DEVICE_NETWORK, 0x0021, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct {
   BOOLEAN fSendOnly;
   UCHAR dstAddr[6];
   UINT  poolSize;
   UINT  packetType;
   UINT  packetSize;
   UINT  packetsSend;
   UINT  FlagControl;
   UINT  delayInABurst;
   UINT  packetsInABurst;
} NDP_STRESS_SEND_INP;

typedef struct {
   DWORD time;
   DWORD idleTime;
   UINT  packetsSent;
   UINT  bytesSent;
} NDP_STRESS_SEND_OUT;

//------------------------------------------------------------------------------

#define IOCTL_NDP_STRESS_RECV \
   CTL_CODE(FILE_DEVICE_NETWORK, 0x0022, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct {
   UINT  poolSize;
   UINT  packetType;
} NDP_STRESS_RECV_INP;

typedef struct {
   DWORD time;
   DWORD idleTime;
   UINT  packetsReceived;
   UINT  bytesReceived;
   UINT  packetType;
   UCHAR srcAddr[6];
   UINT  dataSize;
   UCHAR data[1];
} NDP_STRESS_RECV_OUT;

//------------------------------------------------------------------------------

#define IOCTL_NDP_MP_OID \
   CTL_CODE(FILE_DEVICE_NETWORK, 0x0023, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef enum { QUERY=0, SET} NDPOIDTYPE;
typedef struct _NDP_OID_MP {
	NDPOIDTYPE eType;
	NDIS_OID oid;
	LPVOID lpInOutBuffer;
	PDWORD pnInOutBufferSize;
	NDIS_STATUS status;
} NDP_OID_MP, * PNDP_OID_MP;

//------------------------------------------------------------------------------

#endif
