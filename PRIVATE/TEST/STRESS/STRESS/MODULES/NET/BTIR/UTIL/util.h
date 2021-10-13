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
#define DEFAULT_PACKET_SIZE 1024
#define DEFAULT_PACKETS		10

// declarations 
int GetBA (WCHAR *pp, BT_ADDR *pba);

BOOL PrintBuffer(BYTE * pBuffer, DWORD dwSize);
BOOL FormatBuffer(BYTE * pBuffer, DWORD dwSize, DWORD dwIndex);
BOOL VerifyBuffer(BYTE * pBuffer, DWORD dwSize, DWORD dwIndex);

#endif // _UTIL_H_
