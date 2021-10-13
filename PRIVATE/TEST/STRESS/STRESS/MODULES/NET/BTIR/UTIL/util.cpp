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

#include <windows.h>
#include <tchar.h>
#include <winsock2.h>
#include <ws2bth.h>
#include <bthapi.h>
#include <bt_api.h>
#include <bt_sdp.h>
#include <stressutils.h>
#include "util.h"


int GetBA (WCHAR *pp, BT_ADDR *pba) {
	while (*pp == ' ')
		++pp;

	for (int i = 0 ; i < 4 ; ++i, ++pp) {
		if (! iswxdigit (*pp))
			return FALSE;

		int c = *pp;
		if (c >= 'a')
			c = c - 'a' + 0xa;
		else if (c >= 'A')
			c = c - 'A' + 0xa;
		else c = c - '0';

		if ((c < 0) || (c > 16))
			return FALSE;

		*pba = *pba * 16 + c;
	}

	for (i = 0 ; i < 8 ; ++i, ++pp) {
		if (! iswxdigit (*pp))
			return FALSE;

		int c = *pp;
		if (c >= 'a')
			c = c - 'a' + 0xa;
		else if (c >= 'A')
			c = c - 'A' + 0xa;
		else c = c - '0';

		if ((c < 0) || (c > 16))
			return FALSE;

		*pba = *pba * 16 + c;
	}

	if ((*pp != ' ') && (*pp != '\0'))
		return FALSE;

	return TRUE;
}

BOOL PrintBuffer(BYTE * pBuffer, DWORD dwSize)
{
	TCHAR szLine1[32];
	TCHAR szLine2[32];
	TCHAR szLine[64];

	for (DWORD i = 0; i < dwSize; i ++)
	{
		if (i % 8 == 0)
		// New line
		{
			_tcscpy(szLine1, TEXT(""));
			_tcscpy(szLine2, TEXT(""));
			_tcscpy(szLine, TEXT(""));
		}
		_stprintf (szLine1, TEXT("%s %02x"), szLine1, pBuffer[i]);
		if ((pBuffer[i] > '!') && (pBuffer[i] < '~'))
			_stprintf(szLine2, TEXT("%s %c"), szLine2, pBuffer[i]);
		else
			_stprintf(szLine2, TEXT("%s %c"), szLine2, TEXT('.'));
		if (i % 8 == 7)
		// End of line
		{
			_stprintf (szLine, TEXT("      %03x %s | %s"), i - 7, szLine1, szLine2);
			LogFail(TEXT("%s"), szLine);
		}
	}
	if ((i % 8 != 7) && (i % 8 != 0))
	// End of line
	{
		for (DWORD j = 0; j < 8 - i % 8; j ++)
		{
			_stprintf (szLine1, TEXT("%s   "), szLine1);
		}
		_stprintf (szLine, TEXT("      %03x %s | %s"), i - 7, szLine1, szLine2);
		LogFail(TEXT("%s"), szLine);
	}

	return TRUE;
}

BOOL FormatBuffer(BYTE * pBuffer, DWORD dwSize, DWORD dwIndex)
{
	for (DWORD i = 0; i < dwSize; i ++)
	{
		pBuffer[i] = (BYTE)((dwIndex + i) & 0xFF);
	}

	return TRUE;
}

BOOL VerifyBuffer(BYTE * pBuffer, DWORD dwSize, DWORD dwIndex)
{
	for (DWORD i = 0; i < dwSize; i ++)
	{
		if (pBuffer[i] != ((dwIndex + i) & 0xFF))
		{
			LogFail(TEXT("Invalid data received"));
			LogFail(TEXT("Expecting buffer contains bytes sequencially increasing from 0x%02x"), dwIndex & 0xFF);
			LogFail(TEXT("Got:"));
			PrintBuffer(pBuffer, dwSize);
			return FALSE;
		}
	}

	return TRUE;
}



DWORD DoSDP (BT_ADDR *pb, unsigned char *pcChannel) 
{	
	int iResult = 0;
	BTHNS_RESTRICTIONBLOB RBlob;

	memset (&RBlob, 0, sizeof(RBlob));

	RBlob.type = SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST;
	RBlob.numRange = 1;
	RBlob.pRange[0].minAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
	RBlob.pRange[0].maxAttribute = SDP_ATTRIB_PROTOCOL_DESCRIPTOR_LIST;
	RBlob.uuids[0].uuidType = SDP_ST_UUID16;
	RBlob.uuids[0].u.uuid16 = 0x11ff;

	BLOB blob;
	blob.cbSize = sizeof(RBlob);
	blob.pBlobData = (BYTE *)&RBlob;

	SOCKADDR_BTH sa;

	memset (&sa, 0, sizeof(sa));

	*(BT_ADDR *)(&sa.btAddr) = *pb;
	sa.addressFamily = AF_BT;

	CSADDR_INFO		csai;

	*pcChannel = 0;

	memset (&csai, 0, sizeof(csai));
	csai.RemoteAddr.lpSockaddr = (sockaddr *)&sa;
	csai.RemoteAddr.iSockaddrLength = sizeof(sa);

	WSAQUERYSET		wsaq;

	memset (&wsaq, 0, sizeof(wsaq));
	wsaq.dwSize      = sizeof(wsaq);
	wsaq.dwNameSpace = NS_BTH;
	wsaq.lpBlob      = &blob;
	wsaq.lpcsaBuffer = &csai;

	HANDLE hLookup;

	int iRet = WSALookupServiceBegin (&wsaq, 0, &hLookup);

	if (ERROR_SUCCESS == iRet) {
		union {
			CHAR buf[5000];
			double __unused;
		};

		LPWSAQUERYSET pwsaResults = (LPWSAQUERYSET) buf;
		DWORD dwSize  = sizeof(buf);

		memset(pwsaResults, 0, sizeof(WSAQUERYSET));
		pwsaResults->dwSize      = sizeof(WSAQUERYSET);
		pwsaResults->dwNameSpace = NS_BTH;
		pwsaResults->lpBlob      = NULL;

		iRet = WSALookupServiceNext (hLookup, 0, &dwSize, pwsaResults);

		if (iRet == SOCKET_ERROR)
			LogWarn2(TEXT("WSALookupServiceNext failed: %d"), WSAGetLastError());
			
		if ((iRet == ERROR_SUCCESS) && (pwsaResults->lpBlob->cbSize >= 20)) {	// Success - got the stream
			*pcChannel = pwsaResults->lpBlob->pBlobData[20];
		}

		WSALookupServiceEnd(hLookup);
	}

	return (DWORD)iRet;
}


DWORD FindServers (ServerHash& servers, int inquiryMaxLength, int maxCount)
{
	DWORD dwResult = 0;

	ASSERT(maxCount >= 0);
	ASSERT(inquiryMaxLength > 0);

	// clear the servers hash
	servers.clear();

	// perform an inquiry on the air

	unsigned int cDiscoveredDevices = 0;
	BthInquiryResult inquiryResultList[128];

	dwResult = (DWORD) BthPerformInquiry(	BT_ADDR_GIAC, 
											inquiryMaxLength,
											maxCount, 
											sizeof(inquiryResultList)/sizeof(BthInquiryResult),
											&cDiscoveredDevices,
											(BthInquiryResult *) inquiryResultList);

	if (dwResult != ERROR_SUCCESS)
	{
		LogWarn2(TEXT("BthPerformInquiry failed with error %d"), dwResult);
		return dwResult;
	}
	
	LogComment(TEXT("%u devices discovered"), cDiscoveredDevices);

	for (int i = 0; i < cDiscoveredDevices; i++)
	{
		BT_ADDR addr = inquiryResultList[i].ba;
		
		LogComment(TEXT("Device[%u] addr = %04x%08x cod = 0x%08x clock = 0x%04x page = %d period = %d repetition = %d"), 
					i,
					GET_NAP(addr), 
					GET_SAP(addr), 
					inquiryResultList[i].cod, 
					inquiryResultList[i].clock_offset, 
					inquiryResultList[i].page_scan_mode, 
					inquiryResultList[i].page_scan_period_mode, 
					inquiryResultList[i].page_scan_repetition_mode);

		unsigned char port;

		if (DoSDP(&addr, &port) == ERROR_SUCCESS && port != 0)
		{
			servers.insert (addr, port); // is a server, add it to hash
			LogComment(TEXT("Found server on port %d"), port);
		}						
	}


	// get the list of connected devices

	unsigned short handles[20];
	int cHandlesReturned = 0;
	
	dwResult = BthGetBasebandHandles(sizeof(handles)/sizeof(handles[0]), handles, &cHandlesReturned);

	if (dwResult != ERROR_SUCCESS)
	{
		LogWarn2(TEXT("BthGetBasebandHandles failed with error %d"), dwResult);
		return dwResult;
	}
	
	LogComment(TEXT("%u connected devices"), cHandlesReturned);
	
	for (int i = 0; i < (unsigned int) cHandlesReturned; i++)
	{
		BT_ADDR addr;

		// get the address from the handle
		if (BthGetAddress(handles[i], &addr) != ERROR_SUCCESS )
		{
			LogWarn2(TEXT("Could not get address for handle 0x%08x"), handles[i]);
			continue;
		}

		LogComment(TEXT("Connected Device = %04x%08x"), GET_NAP(addr), GET_SAP(addr));

		unsigned char port;

		if (DoSDP(&addr, &port) == ERROR_SUCCESS && port != 0)
		{
			servers.insert (addr, port); // is a server, add it to hash
			LogComment(TEXT("Found server on port %d"), port);
		}
	}

	return ERROR_SUCCESS;
}

