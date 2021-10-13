//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
