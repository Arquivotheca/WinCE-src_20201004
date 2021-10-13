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

#define IOCTL_NDP_DEVICE_WAKE \
   CTL_CODE(FILE_DEVICE_NETWORK, 0x0024, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FLAG_NDP_ENABLE_WAKE_UP		(1)
#define FLAG_NDP_DISABLE_WAKE_UP	(2)
#define FLAG_NDP_ENABLE_RTC_TIMER	(4)
#define FLAG_NDP_DISABLE_RTC_TIMER	(8)

typedef struct _tsDevWakeUp {
	DWORD dwActionFlags; //one or more of above flags
	DWORD dwWakeUpSource; //specifies which wake up source to be used.
	DWORD dwRTCAlarmTimeInSec; //specifies wake up time in sec.
} tsDevWakeUp, * PtsDevWakeUp;

//------------------------------------------------------------------------------

#endif
