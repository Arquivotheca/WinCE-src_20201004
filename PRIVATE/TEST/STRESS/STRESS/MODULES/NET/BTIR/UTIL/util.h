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
#ifndef _UTIL_H_
#define _UTIL_H_

// defines
#define DEFAULT_PORT		11
#define DEFAULT_NAME		"BTIRTEST"
#define DEFAULT_NAME_SVR	"BTIRTESTSVR"
#define DEFAULT_PACKET_SIZE 1024
#define DEFAULT_PACKETS		100

// declarations 
int GetBA (WCHAR *pp, BT_ADDR *pba);

BOOL PrintBuffer(BYTE * pBuffer, DWORD dwSize);
BOOL FormatBuffer(BYTE * pBuffer, DWORD dwSize, DWORD dwIndex);
BOOL VerifyBuffer(BYTE * pBuffer, DWORD dwSize, DWORD dwIndex);


// DoSDP: query for a stress server on a specified address and retrieves  its channel number
// Returns 0 on success, SOCKET_ERROR on error. More info from WASGetLastError()

DWORD DoSDP (BT_ADDR *pb, unsigned char *pcChannel) ;


#include "hash_map.hxx"

// useful containers
typedef ce::hash_map <BT_ADDR, int> ServerHash;
typedef ce::list <BT_ADDR> AddressList;

// FindServers: fills a hash with (key=address, value=port) of discovered servers
// use max_count == 0 to retrieve all servers in range. The function will call
// inquiryMaxLength (in 1.28 sec intervals) and maxCount will be passed to bthPerformInquiry().
// Returns ERROR_SUCCESS on success

DWORD FindServers (ServerHash& servers, int inquiryMaxLength, int maxCount = 0);

#endif // _UTIL_H_
