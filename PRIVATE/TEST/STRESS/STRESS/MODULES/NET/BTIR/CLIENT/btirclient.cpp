//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <tchar.h>
#include <winsock2.h>
#include <ws2bth.h>
#include <bthapi.h>
#include <bt_api.h>
#include <bt_sdp.h>
#include <af_irda.h>
#include <stressutils.h>
#include "util.h"

// Global variables
#define MAX_BTSERVER 5

BT_ADDR	g_BTServerAddr[MAX_BTSERVER];
SOCKADDR_IRDA g_IRServerAddr;

HANDLE g_hEvent = NULL;
BOOL g_bRunBluetooth = FALSE;
BOOL g_bRunIrda = FALSE;

DWORD g_cTotalSend_bth = 0;
DWORD g_cTotalRecv_bth = 0;
DWORD g_cTotalSend_irda = 0;
DWORD g_cTotalRecv_irda = 0;

DWORD PerformInquiry(unsigned int LAP, unsigned char length, BT_ADDR *addresses, unsigned char num_responses)
{
	DWORD dwResult = 0;
	unsigned int cDiscoveredDevices = 0;
	BthInquiryResult inquiryResultList[256];
	unsigned int i = 0;
	DWORD dwStartTime = 0, dwEndTime = 0;

	// perform inquiry
	dwResult = (DWORD) BthPerformInquiry(	LAP, 
											length,
											num_responses, 
											sizeof(inquiryResultList)/sizeof(BthInquiryResult),
											&cDiscoveredDevices,
											(BthInquiryResult *) inquiryResultList);
	if (dwResult != ERROR_SUCCESS)
	{
		LogWarn2(TEXT("BthPerformInquiry failed with error %d"), dwResult);
	}
	else
	{
		LogComment(TEXT("%u devices discovered"), cDiscoveredDevices);
		for (i = 0; i < cDiscoveredDevices; i++)
		{
			LogComment(TEXT("Device[%u] addr = %04x%08x cod = 0x%08x clock = 0x%04x page = %d period = %d repetition = %d"), 
						i,
						GET_NAP(inquiryResultList[i].ba), 
						GET_SAP(inquiryResultList[i].ba), 
						inquiryResultList[i].cod, 
						inquiryResultList[i].clock_offset, 
						inquiryResultList[i].page_scan_mode, 
						inquiryResultList[i].page_scan_period_mode, 
						inquiryResultList[i].page_scan_repetition_mode);
			if (i < num_responses)
			{
				addresses[i] = inquiryResultList[i].ba;
			}
		}
		if (cDiscoveredDevices > num_responses)
		{
			LogWarn2(TEXT("More than %d devices available"), num_responses);
		}
	}

	return dwResult;
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

		memset(pwsaResults,0,sizeof(WSAQUERYSET));
		pwsaResults->dwSize      = sizeof(WSAQUERYSET);
		pwsaResults->dwNameSpace = NS_BTH;
		pwsaResults->lpBlob      = NULL;

		iRet = WSALookupServiceNext (hLookup, 0, &dwSize, pwsaResults);
		if ((iRet == ERROR_SUCCESS) && (pwsaResults->lpBlob->cbSize >= 20)) {	// Success - got the stream
			*pcChannel = pwsaResults->lpBlob->pBlobData[20];
		}

		WSALookupServiceEnd(hLookup);
	}

	return (DWORD)iRet;
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL InitializeStressModule (
							/*[in]*/ MODULE_PARAMS* pmp, 
							/*[out]*/ UINT* pnThreads
							)
{
	LogVerbose(TEXT("+InitializeStressModule"));

	// Set this value to indicate the number of threads
	// that you want your module to run.  The module container
	// that loads this DLL will manage the life-time of these
	// threads.  Each thread will call your DoStressIteration()
	// function in a loop.

	*pnThreads = 1;
	
	// !! You must call this before using any stress utils !!

	InitializeStressUtils (
							_T("BTIRCLIENT"),	// Module name to be used in logging
							LOGZONE(SLOG_SPACE_NET, SLOG_BLUETOOTH | SLOG_OBEX),	// Logging zones used by default
							pmp			    // Forward the Module params passed on the cmd line
							);


	// TODO:  Any module-specific initialization here
	
	WSADATA			wsd;
	int				iResult, iLen;
	DEVICELIST		deviceList;
	SOCKADDR_BTH	addr;
	SOCKET			sock = INVALID_SOCKET;
	DWORD			dwResult;

	// Can't run when another instance of btir client/server is running
	g_hEvent = CreateEvent (NULL, FALSE, FALSE, TEXT(DEFAULT_NAME));
	if ((dwResult = GetLastError ()) == ERROR_ALREADY_EXISTS)
	{
		LogWarn2(TEXT("Another BTIRCLIENT/BTIRSERVER instance is already running.  Bailing ..."));
		goto FAIL_EXIT;
	}
	if (g_hEvent == NULL)
	{
		LogFail(TEXT("CreateEvent failed, error = %d"), dwResult);
		goto FAIL_EXIT;
	}

	memset(&g_BTServerAddr[0], 0, sizeof(g_BTServerAddr));
	memset(&g_IRServerAddr, 0, sizeof(g_IRServerAddr));

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (iResult != 0)
	{
		LogFail(TEXT("WSAStartup failed, error = %d"), iResult);

		goto FAIL_EXIT;
	}

	// Detect Bluetooth
	g_bRunBluetooth = FALSE;
	sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	if (sock == INVALID_SOCKET)
	{
		LogWarn2(TEXT("No Bluetooth support, error = %d"), WSAGetLastError());
		LogWarn2(TEXT("Please check if Bluetooth is included in image"));
	}
	else
	{	
		// Read local Bluetooth device address
		memset(&addr, 0, sizeof(addr));
		addr.addressFamily = AF_BTH;

		if (SOCKET_ERROR == bind(sock, (const struct sockaddr *)&addr,  sizeof(addr)))
		{
			LogWarn2(TEXT("bind() failed, error = %d"), WSAGetLastError());
			LogWarn2(TEXT("Bluetooth transport is DISABLED"));
		}
		else
		{
			iLen = sizeof(addr);
			if (SOCKET_ERROR == getsockname(sock, (struct sockaddr*)&addr, &iLen))
			{
				LogWarn2(TEXT("getsockname() failed, error = %d"), WSAGetLastError());
				LogWarn2(TEXT("Bluetooth transport is DISABLED"));
			}
			else
			{
				LogComment(TEXT("Local Bluetooth address [%04x%08x]"), GET_NAP(addr.btAddr), GET_SAP(addr.btAddr));

				if ((GET_NAP(addr.btAddr) == 0) && (GET_SAP(addr.btAddr) == 0))
				{
					LogWarn2(TEXT("Failed to read Bluetooth address"));
					LogWarn2(TEXT("Please check if hardware is working properly"));
					LogWarn2(TEXT("Bluetooth transport is DISABLED"));
				}
				else
				{
					g_bRunBluetooth = TRUE;
				}
			}
		}

		shutdown(sock, 0);
		closesocket(sock);

		// Find available devices
		if (g_bRunBluetooth)
		{
			if (PerformInquiry (BT_ADDR_GIAC, 8, &g_BTServerAddr[0], MAX_BTSERVER) != 0)
			{
				LogWarn2(TEXT("PerformInquiry failed"));
			}
			if ((GET_NAP(g_BTServerAddr[0]) == 0) && (GET_SAP(g_BTServerAddr[0]) == 0))
			{
				LogWarn2(TEXT("No available Bluetooth device nearby"));
				LogWarn2(TEXT("Bluetooth transport is DISABLED"));
				g_bRunBluetooth = FALSE;
			}
			else
			{
				LogComment(TEXT("Bluetooth transport is ENABLED"));
			}
		}
	}

	// Detect irda
	g_bRunIrda = FALSE;
	sock = socket(AF_IRDA, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET)
	{
		LogWarn2(TEXT("No Irda support, error = %d"), WSAGetLastError());
		LogWarn2(TEXT("Please check if Irda is included in image"));
	}
	else
	{
		iLen = sizeof(deviceList);
		if (SOCKET_ERROR == getsockopt(sock, SOL_IRLMP, IRLMP_ENUMDEVICES, (char *)&deviceList, &iLen))
		{
			LogFail(TEXT("getsockopt failed, error = %d"), WSAGetLastError());
		}
		else
		{
			if (deviceList.numDevice == 0)
			{
				LogWarn2(TEXT("No Irda server found"));
			}
			else
			{
				memcpy(&g_IRServerAddr.irdaDeviceID, &deviceList.Device[0].irdaDeviceID[0], sizeof(g_IRServerAddr.irdaDeviceID));
				g_bRunIrda = TRUE;
			}
		}
		shutdown(sock, 0);
		closesocket(sock);
	}
	if (g_bRunIrda)
	{
		LogComment(TEXT("Irda transport is ENABLED"));
	}
	else
	{
		LogWarn2(TEXT("Irda transport is DISABLED"));
	}

	if (!g_bRunBluetooth && !g_bRunIrda)
	{
		LogFail(TEXT("Neither Bluetooth nor Irda is enabled"));

		goto FAIL_EXIT;
	}


	LogVerbose(TEXT("-InitializeStressModule"));

	return TRUE;

FAIL_EXIT:
	if (sock != INVALID_SOCKET)
	{
		shutdown(sock, 0);
		closesocket(sock);
	}
	WSACleanup();

	if ((g_hEvent != NULL) && (g_hEvent != INVALID_HANDLE_VALUE))
	{
		CloseHandle(g_hEvent);
	}
	LogVerbose(TEXT("-InitializeStressModule"));
	return FALSE;
}



/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIteration (
						/*[in]*/ HANDLE hThread, 
						/*[in]*/ DWORD dwThreadId, 
						/*[in,out]*/ LPVOID pv /*unused*/)
{
	LogVerbose(TEXT("+DoStressIteration"));

	// TODO:  Do your actual testing here.
	//
	//        You can do whatever you want here, including calling other functions.
	//        Just keep in mind that this is supposed to represent one iteration
	//        of a stress test (with a single result), so try to do one discrete 
	//        operation each time this function is called. 

	int				i, j, iLen, iBytes, iTotalBytes;
	SOCKADDR_BTH	remote_bth;
	SOCKADDR_IRDA	remote_irda;
	SOCKET			sock = INVALID_SOCKET;
	BYTE *			pBuffer = NULL;
	TCHAR			tszName[40];
	BYTE			queryBuffer[sizeof(IAS_QUERY) + 1024];
	IAS_QUERY *		pQuery = (IAS_QUERY *) &queryBuffer[0];

	pBuffer = (BYTE *) malloc (DEFAULT_PACKET_SIZE);
	if (!pBuffer)
	{
		LogFail(TEXT("malloc() failed while trying to allocate %d bytes"), DEFAULT_PACKET_SIZE);

		goto FAIL_EXIT;
	}

	for (i = 0; i < 2; i ++)
	{
		if (((i % 2 == 0) && !g_bRunBluetooth) || ((i % 2 == 1) && (!g_bRunIrda)))
		{
			continue;
		}

		if (i % 2 == 0)
		{
			sock = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
			if (sock == INVALID_SOCKET)
			{
				LogFail(TEXT("socket() failed, error = %d"), WSAGetLastError());
				continue;
			}


			memset(&remote_bth, 0, sizeof(remote_bth));
			remote_bth.addressFamily = AF_BTH;
			remote_bth.port = DEFAULT_PORT;
			iLen = sizeof(remote_bth);

			// connect to server
			for (j = 0; (j < MAX_BTSERVER) && (g_BTServerAddr[j] != 0); j ++)
			{
				remote_bth.btAddr = g_BTServerAddr[j];
				DoSDP(&remote_bth.btAddr, (unsigned char *)&remote_bth.port);
				if (remote_bth.port != 0)
					break;
			}
			if (remote_bth.port == 0)
			{
				LogComment(TEXT("No available Bluetooth enabled btir server in range"));
				shutdown(sock, 0);
				closesocket(sock);
				continue;
			}

			LogComment(TEXT("Connect to Bluetooth server [%04x%08x] on channel %d"), 
				GET_NAP(remote_bth.btAddr), GET_SAP(remote_bth.btAddr), remote_bth.port);
			if (SOCKET_ERROR == connect(sock, (struct sockaddr *)&remote_bth, iLen))
			{
				LogFail(TEXT("connect() failed, error = %d"), WSAGetLastError());
				shutdown(sock, 0);
				closesocket(sock);
				continue;
			}
		}
		else
		{
			sock = socket(AF_IRDA, SOCK_STREAM, 0);
			if (sock == INVALID_SOCKET)
			{
				LogFail(TEXT("socket() failed, error = %d"), WSAGetLastError());
				continue;
			}

			memset(&remote_irda, 0, sizeof(remote_irda));
			remote_irda.irdaAddressFamily = AF_IRDA;
			memcpy(&remote_irda.irdaDeviceID[0], &g_IRServerAddr.irdaDeviceID[0], sizeof(remote_irda.irdaDeviceID));
			strncpy(&remote_irda.irdaServiceName[0], DEFAULT_NAME, sizeof(remote_irda.irdaServiceName));
			iLen = sizeof(remote_irda);

			// connect to server
			MultiByteToWideChar(CP_ACP, 0, remote_irda.irdaServiceName, sizeof(remote_irda.irdaServiceName), 
				tszName, sizeof(tszName)/sizeof(tszName[0]));

			iLen = sizeof (queryBuffer);
			memset (pQuery, 0, iLen);
			memcpy (&pQuery->irdaDeviceID[0], &g_IRServerAddr.irdaDeviceID[0], sizeof (pQuery->irdaDeviceID));
			memcpy (&pQuery->irdaClassName[0], remote_irda.irdaServiceName, sizeof (pQuery->irdaClassName));
			pQuery->irdaAttribType = IAS_ATTRIB_INT;
			memcpy (&pQuery->irdaAttribName[0], remote_irda.irdaServiceName, sizeof (pQuery->irdaAttribName));
			if (SOCKET_ERROR == getsockopt(sock, SOL_IRLMP, IRLMP_IAS_QUERY, (char *)pQuery, &iLen))
			{
				LogFail(TEXT("getsockopt() failed, error = %d"), WSAGetLastError());
				shutdown(sock, 0);
				closesocket(sock);
				continue;
			}
			if ((pQuery->irdaAttribType == IAS_ATTRIB_NO_ATTRIB) || (pQuery->irdaAttribType == IAS_ATTRIB_NO_CLASS))
			{
				LogComment(TEXT("No available Irda enabled btir server in range"));
				shutdown(sock, 0);
				closesocket(sock);
				continue;
			}

			LogComment(TEXT("Connect to Irda server [%08x] on name %s"), *(int *)&g_IRServerAddr.irdaDeviceID[0], tszName);
			if (SOCKET_ERROR == connect(sock, (struct sockaddr *)&remote_irda, iLen))
			{
				LogFail(TEXT("connect() failed, error = %d"), WSAGetLastError());
				shutdown(sock, 0);
				closesocket(sock);
				continue;
			}
		}

		// send to server
		if (i % 2 == 0)
		{
			LogComment(TEXT("Send %d packets of size %d to Bluetooth server [%04x%08x]"), DEFAULT_PACKETS, DEFAULT_PACKET_SIZE,
				GET_NAP(remote_bth.btAddr), GET_SAP(remote_bth.btAddr));
		}
		else
		{
			LogComment(TEXT("Send %d packets of size %d to Irda server [%08x]"), DEFAULT_PACKETS, DEFAULT_PACKET_SIZE,
				*(int *)&g_IRServerAddr.irdaDeviceID[0]);
		}
		iTotalBytes = 0;
		while (iTotalBytes < DEFAULT_PACKET_SIZE * DEFAULT_PACKETS)
		{
			iBytes = 0;
			FormatBuffer(pBuffer, DEFAULT_PACKET_SIZE, iTotalBytes);
			iBytes = send(sock, (const char *)pBuffer, DEFAULT_PACKET_SIZE, 0);
			if (iBytes == SOCKET_ERROR)
			{
				LogFail(TEXT("send() failed, error = %d"), WSAGetLastError());
				goto FAIL_EXIT;
			}
			iTotalBytes += iBytes;
		}
		if (i % 2 == 0)
		{
			g_cTotalSend_bth += iTotalBytes;
		}
		else
		{
			g_cTotalSend_irda += iTotalBytes;
		}
		// recv from server
		if (i % 2 == 0)
		{
			LogComment(TEXT("Recv %d packets of size %d from Bluetooth server [%04x%08x]"), DEFAULT_PACKETS, DEFAULT_PACKET_SIZE,
				GET_NAP(remote_bth.btAddr), GET_SAP(remote_bth.btAddr));
		}
		else
		{
			LogComment(TEXT("Recv %d packets of size %d from Irda server [%08x]"), DEFAULT_PACKETS, DEFAULT_PACKET_SIZE,
				*(int *)&g_IRServerAddr.irdaDeviceID[0]);
		}
		iTotalBytes = 0;
		while (iTotalBytes < DEFAULT_PACKET_SIZE * DEFAULT_PACKETS)
		{
			iBytes = 0;
			iBytes = recv(sock, (char *)pBuffer, DEFAULT_PACKET_SIZE, 0);
			if (iBytes == SOCKET_ERROR)
			{
				LogFail(TEXT("recv() failed, error = %d"), WSAGetLastError());
				goto FAIL_EXIT;
			}
			if (!VerifyBuffer(pBuffer, iBytes, iTotalBytes))
			{
				goto FAIL_EXIT;
			}
			iTotalBytes += iBytes;
		}
		if (i % 2 == 0)
		{
			g_cTotalRecv_bth += iTotalBytes;
		}
		else
		{
			g_cTotalRecv_irda += iTotalBytes;
		}

		// disconnect from server
		if (i % 2 == 0)
		{
			LogComment(TEXT("Disconnect from Bluetooth server [%04x%08x]"), GET_NAP(remote_bth.btAddr), GET_SAP(remote_bth.btAddr));
		}
		else
		{
			LogComment(TEXT("Disconnect from Irda server [%08x]"), *(int *)&g_IRServerAddr.irdaDeviceID[0]);
		}

		if (SOCKET_ERROR == shutdown(sock, 0))
		{
			LogWarn1(TEXT("shutdown() failed, error = %d"), WSAGetLastError());
		}
		if (SOCKET_ERROR == closesocket(sock))
		{
			LogWarn1(TEXT("closesocket() failed, error = %d"), WSAGetLastError());
		}

		// Wait for disconnect to finish
		if (g_bRunBluetooth && !g_bRunIrda)
		{
			Sleep(500);
		}
	}
	
	free(pBuffer);

	LogVerbose(TEXT("-DoStressIteration"));

	// You must return one of these values:

	return CESTRESS_PASS;
	//return CESTRESS_FAIL;
	//return CESTRESS_WARN1;
	//return CESTRESS_WARN2;
	//return CESTRESS_ABORT;  // Use carefully.  This will cause your module to be terminated immediately.

FAIL_EXIT:
	if (sock != SOCKET_ERROR)
	{
		shutdown(sock, 0);
		closesocket(sock);
	}
	if (pBuffer)
	{
		free(pBuffer);
	}

	LogVerbose(TEXT("-DoStressIteration"));
	
	return CESTRESS_FAIL;
}



/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
	LogVerbose(TEXT("+TerminateStressModule"));

	// TODO:  Do test-specific clean-up here.

	WSACleanup();

	// Output statitics info
	if (g_bRunBluetooth)
	{
		LogComment(TEXT("Sent %d bytes and received %d bytes over Bluetooth"), g_cTotalSend_bth, g_cTotalRecv_bth);
	}
	if (g_bRunIrda)
	{
		LogComment(TEXT("Sent %d bytes and received %d bytes over Irda"), g_cTotalSend_irda, g_cTotalRecv_irda);
	}

	if ((g_hEvent != NULL) && (g_hEvent != INVALID_HANDLE_VALUE))
	{
		CloseHandle(g_hEvent);
	}

	// This will be used as the process exit code.
	// It is not currently used by the harness.

	LogVerbose(TEXT("-TerminateStressModule"));

	return ((DWORD) 0);
}



///////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
					HANDLE hInstance, 
					ULONG dwReason, 
					LPVOID lpReserved
					)
{
    return TRUE;
}
