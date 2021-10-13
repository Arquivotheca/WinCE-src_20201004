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
#include <af_irda.h>
#include <stressutils.h>
#include "util.h"

// Global variables

DWORD g_cTotalSend_bth = 0;
DWORD g_cTotalRecv_bth = 0;
DWORD g_cTotalSend_irda = 0;
DWORD g_cTotalRecv_irda = 0;

HANDLE g_hEvent = NULL;
BOOL g_bRunBluetooth = FALSE;
BOOL g_bRunIrda = FALSE;
SOCKET g_listenSock_bth = INVALID_SOCKET;
SOCKET g_listenSock_irda = INVALID_SOCKET;

#define SDP_RECORD_SIZE 0x00000043
#define SDP_CHANNEL_OFFSET 26

static const BYTE rgbSdpRecordDUN[] = {
    0x35, 0x41, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19,
    0x11, 0xff,												// service class is 0x11ff
				0x09, 0x00, 0x04, 0x35, 0x0c, 0x35,			
    0x03, 0x19, 0x01, 0x00, 0x35, 0x05, 0x19, 0x00,
    0x03, 0x08,
				DEFAULT_PORT,								// server channel goes here (+26)
					  0x09, 0x00, 0x06, 0x35, 0x09,
    0x09, 0x65, 0x6e, 0x09, 0x00, 0x6a, 0x09, 0x01,
    0x00, 0x09, 0x00, 0x09, 0x35, 0x08, 0x35, 0x06,
    0x19, 
		  0x11, 0xff, 0x09, 0x01, 0x00, 					// profile is 0x11f, version 0x0100
										0x09, 0x01,									
    0x00, 0x25, 0x08, 'B',  'T',  'I',  'R',  'T',
    'E',  'S',  'T'
};

BOOL RegisterSDP (ULONG *pulRecordHandle) 
{
	DWORD port = DEFAULT_PORT;
	DWORD dwSizeOut = 0;

	struct {
		BTHNS_SETBLOB	b;
		unsigned char   uca[SDP_RECORD_SIZE];
	} bigBlob;

	ULONG ulSdpVersion = BTH_SDP_VERSION;

	bigBlob.b.pRecordHandle   = pulRecordHandle;
    bigBlob.b.pSdpVersion     = &ulSdpVersion;
	bigBlob.b.fSecurity       = 0;
	bigBlob.b.fOptions        = 0;
	bigBlob.b.ulRecordLength  = SDP_RECORD_SIZE;

	memcpy (bigBlob.b.pRecord, rgbSdpRecordDUN, SDP_RECORD_SIZE);

	bigBlob.b.pRecord[SDP_CHANNEL_OFFSET] = (unsigned char)port;

	BLOB blob;
	blob.cbSize    = sizeof(BTHNS_SETBLOB) + SDP_RECORD_SIZE - 1;
	blob.pBlobData = (PBYTE) &bigBlob;

	WSAQUERYSET Service;
	memset (&Service, 0, sizeof(Service));
	Service.dwSize = sizeof(Service);
	Service.lpBlob = &blob;
	Service.dwNameSpace = NS_BTH;

	int iRet = WSASetService (&Service, RNRSERVICE_REGISTER, 0);
	if (iRet != ERROR_SUCCESS) 
	{
		LogFail(TEXT("WSASetService failed, error = %d"), WSAGetLastError());
		return FALSE;
	}

	LogVerbose(TEXT("BTIRSERVER: Created Bluetooth SDP record 0x%08x, channel %d"), *pulRecordHandle, port);
	return TRUE;
}

BOOL RemoveSDP (ULONG ulRecordHandle) 
{
	ULONG ulSdpVersion = BTH_SDP_VERSION;

	BTHNS_SETBLOB delBlob;
	memset (&delBlob, 0, sizeof(delBlob));
	delBlob.pRecordHandle = &ulRecordHandle;
    delBlob.pSdpVersion = &ulSdpVersion;

	BLOB blob;
	blob.cbSize    = sizeof(BTHNS_SETBLOB);
	blob.pBlobData = (PBYTE) &delBlob;

	WSAQUERYSET Service;

	memset (&Service, 0, sizeof(Service));
	Service.dwSize = sizeof(Service);
	Service.lpBlob = &blob;
	Service.dwNameSpace = NS_BTH;

	int iErr = WSASetService (&Service, RNRSERVICE_DELETE, 0);
	if (iErr != ERROR_SUCCESS) {
		LogFail(TEXT("WSASetService failed, error = %d"), WSAGetLastError());
		return FALSE;
	}

	LogVerbose(TEXT("BTIRSERVER : Removed SDP record 0x%08x"), ulRecordHandle);
	return TRUE;
}

DWORD RecvSend(SOCKET sock)
{
	BYTE bRemoteBuffer[100];
	SOCKADDR *pRemote = (SOCKADDR *)&bRemoteBuffer[0];
	SOCKADDR_BTH *pRemote_bth;
	SOCKADDR_IRDA *pRemote_irda;
	BYTE * pBuffer = NULL;
	int	iBytes, iTotalBytes, iLen;

	pBuffer = (BYTE *) malloc (DEFAULT_PACKET_SIZE);
	if (!pBuffer)
	{
		LogFail(TEXT("malloc() failed while trying to allocate %d bytes"), DEFAULT_PACKET_SIZE);
		goto FAIL_EXIT;
	}

	// recv from client
	memset(pRemote, 0, sizeof(bRemoteBuffer));
	iLen = sizeof(bRemoteBuffer);
	if (SOCKET_ERROR == getpeername(sock, (struct sockaddr *)pRemote, &iLen))
	{
		LogFail(TEXT("getpeername() failed, error = %d"), WSAGetLastError());

		goto FAIL_EXIT;
	}
	
	pRemote_bth = (SOCKADDR_BTH *)pRemote;
	pRemote_irda = (SOCKADDR_IRDA *)pRemote;

	if (pRemote->sa_family == AF_BTH)
	{
		LogComment(TEXT("Recv %d packets of size %d from Bluetooth client [%04x%08x]"), DEFAULT_PACKETS, DEFAULT_PACKET_SIZE,
			GET_NAP(pRemote_bth->btAddr), GET_SAP(pRemote_bth->btAddr));
	}
	else
	{
		LogComment(TEXT("Recv %d packets of size %d from Irda client [%08x]"), DEFAULT_PACKETS, DEFAULT_PACKET_SIZE,
			*(int *)&pRemote_irda->irdaDeviceID[0]);
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
	if (pRemote->sa_family == AF_BTH)
	{
		g_cTotalRecv_bth += iTotalBytes;
	}
	else
	{
		g_cTotalRecv_irda += iTotalBytes;
	}

	// send to client
	if (pRemote->sa_family == AF_BTH)
	{
		LogComment(TEXT("Send %d packets of size %d to Bluetooth client [%04x%08x]"), DEFAULT_PACKETS, DEFAULT_PACKET_SIZE,
			GET_NAP(pRemote_bth->btAddr), GET_SAP(pRemote_bth->btAddr));
	}
	else
	{
		LogComment(TEXT("Send %d packets of size %d to Irda client [%08x]"), DEFAULT_PACKETS, DEFAULT_PACKET_SIZE,
			*(int *)&pRemote_irda->irdaDeviceID[0]);
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
	if (pRemote->sa_family == AF_BTH)
	{
		g_cTotalSend_bth += iTotalBytes;
		LogComment(TEXT("Sent %d bytes and received %d bytes over Bluetooth so far"), g_cTotalSend_bth, g_cTotalRecv_bth);
	}
	else
	{
		g_cTotalSend_irda += iTotalBytes;
		LogComment(TEXT("Sent %d bytes and received %d bytes over Irda so far"), g_cTotalSend_irda, g_cTotalRecv_irda);
	}
	shutdown(sock, 0);
	closesocket(sock);
	free(pBuffer);

	return 0;

FAIL_EXIT:
	shutdown(sock, 0);
	closesocket(sock);
	if (pBuffer)
	{
		free(pBuffer);
	}
	return -1;
}

DWORD WINAPI ListenThread(LPVOID lpParameter)
{
	SOCKET listenSock, dataSock;
	SOCKADDR *pLocal, *pRemote;
	BYTE bLocalBuffer[100], bRemoteBuffer[100];
	SOCKADDR_IRDA *pLocal_irda;
	SOCKADDR_BTH *pLocal_bth;
	TCHAR tszName[40];
	int iLen, iResult;
	DWORD dwResult = 0;
	ULONG ulRecordHandle = NULL;
	FD_SET fd;
	TIMEVAL timeOut;

	listenSock = (SOCKET) lpParameter;

	pLocal = (SOCKADDR *)&bLocalBuffer[0];
	pRemote = (SOCKADDR *)&bRemoteBuffer[0];
	pLocal_irda = (SOCKADDR_IRDA *)pLocal;
	pLocal_bth = (SOCKADDR_BTH *)pLocal;

	iLen = sizeof(bLocalBuffer);
	if(SOCKET_ERROR == getsockname(listenSock, (SOCKADDR *)pLocal, &iLen))
	{
		LogFail(TEXT("getsockname() failed, error = %d"), WSAGetLastError());
		goto FAIL_EXIT;
	}
	memset(tszName, 0, sizeof(tszName));
	if (pLocal->sa_family == AF_IRDA)
	{
		MultiByteToWideChar(CP_ACP, 0, pLocal_irda->irdaServiceName, sizeof(pLocal_irda->irdaServiceName), 
			tszName, sizeof(tszName)/sizeof(tszName[0]));
	}

	if (pLocal->sa_family == AF_BTH)
	{
		LogComment(TEXT("Wait for incoming Bluetooth connection on channel %d"), pLocal_bth->port);
		if (!RegisterSDP(&ulRecordHandle))
		{
			LogFail(TEXT("Failed to register SDP record"));
			goto FAIL_EXIT;
		}
	}
	else
	{
		LogVerbose(TEXT("Wait for incoming Irda connection on name %s"), tszName);
	}

	FD_ZERO (&fd);
	FD_SET (listenSock, &fd);
	memset (&timeOut, 0, sizeof (timeOut));
	timeOut.tv_sec = 30;

	if ((iResult = select (0, &fd, NULL, NULL, &timeOut)) == SOCKET_ERROR) 
	{
		LogFail(TEXT("select() failed, error = %d"), WSAGetLastError());
		goto FAIL_EXIT;
	}

	if ((pLocal->sa_family == AF_BTH) && (!RemoveSDP(ulRecordHandle)))
	{
		LogFail(TEXT("Failed to remove SDP record"));
		goto FAIL_EXIT;	
	}
	if (iResult)
	{
		memset(pRemote, 0, sizeof(bRemoteBuffer));
		iLen = sizeof(bRemoteBuffer);
		dataSock = accept(listenSock, (struct sockaddr *)pRemote, &iLen);
		if (pLocal->sa_family == AF_BTH)
		{
			LogComment(TEXT("Accepted connection from [%04x%08x]"), GET_NAP(((SOCKADDR_BTH *)pRemote)->btAddr), GET_SAP(((SOCKADDR_BTH *)pRemote)->btAddr));
		}
		if (dataSock == INVALID_SOCKET)
		{
			LogWarn1(TEXT("accept() failed, error = %d"), WSAGetLastError());
		}
		else
		{
			dwResult = RecvSend(dataSock);
		}
	}
	else
	{
		LogComment(TEXT("No connection attempted"));
	}

	return dwResult;
FAIL_EXIT:
	return -1;
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
							_T("BTIRSERVER"),	// Module name to be used in logging
							LOGZONE(SLOG_SPACE_NET, SLOG_BLUETOOTH | SLOG_OBEX),	// Logging zones used by default
							pmp			    // Forward the Module params passed on the cmd line
							);

	// TODO:  Any module-specific initialization here

	WSADATA wsd;
	SOCKADDR_BTH local_bth;
	SOCKADDR_IRDA local_irda;
	int iResult, iLen;
	DWORD dwResult;
	BYTE			setBuffer[sizeof(IAS_SET) + 1024];
	IAS_SET *		pSet = (IAS_SET *) &setBuffer[0];
	
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

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsd);
	if (iResult != 0)
	{
		LogFail(TEXT("WSAStartup failed, error = %d"), iResult);
		goto FAIL_EXIT;
	}

	// Read local Bluetooth device address
	memset(&local_bth, 0, sizeof(local_bth));
	local_bth.addressFamily = AF_BTH;

	g_listenSock_bth = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
	if (g_listenSock_bth == INVALID_SOCKET)
	{
		LogWarn2(TEXT("socket() failed, error = %d"), WSAGetLastError());
		LogWarn2(TEXT("Bluetooth transport is DISABLED"));
	}
	else if (SOCKET_ERROR == bind(g_listenSock_bth, (const struct sockaddr *)&local_bth,  sizeof(local_bth)))
	{
		LogWarn2(TEXT("bind() failed, error = %d"), WSAGetLastError());
		LogWarn2(TEXT("Bluetooth transport is DISABLED"));
	}
	else
	{
		iLen = sizeof(local_bth);
		if (SOCKET_ERROR == getsockname(g_listenSock_bth, (struct sockaddr*)&local_bth, &iLen))
		{
			LogWarn2(TEXT("getsockname() failed, error = %d"), WSAGetLastError());
			LogWarn2(TEXT("Bluetooth transport is DISABLED"));
		}
		else
		{
			LogComment(TEXT("Local Bluetooth address [%04x%08x]"), GET_NAP(local_bth.btAddr), GET_SAP(local_bth.btAddr));

			if ((GET_NAP(local_bth.btAddr) == 0) && (GET_SAP(local_bth.btAddr) == 0))
			{
				LogWarn2(TEXT("Failed to read Bluetooth address"));
				LogWarn2(TEXT("Please check if hardware is working properly"));
				LogWarn2(TEXT("Bluetooth transport is DISABLED"));
			}
			else
			{
				// LogComment(TEXT("Bluetooth transport is ENABLED"));
				g_bRunBluetooth = TRUE;
			}
		}
	}

	shutdown(g_listenSock_bth, 0);
	closesocket(g_listenSock_bth);
	g_listenSock_bth = INVALID_SOCKET;

	if (g_bRunBluetooth)
	{
		// Listen
		g_listenSock_bth = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
		if (g_listenSock_bth == INVALID_SOCKET)
		{
			LogFail(TEXT("socket() failed, error = %d"), WSAGetLastError());
			goto FAIL_EXIT;
		}
		memset(&local_bth, 0, sizeof(local_bth));
		local_bth.addressFamily = AF_BTH;
		local_bth.port = DEFAULT_PORT;
		if (SOCKET_ERROR == bind(g_listenSock_bth, (const struct sockaddr *)&local_bth, sizeof(local_bth)))
		{
			LogWarn2(TEXT("bind() failed, error = %d"), WSAGetLastError());	
			LogWarn2(TEXT("Bluetooth transport is DISABLED"));
			g_bRunBluetooth = FALSE;
		}
		else
		{
			LogComment(TEXT("Bluetooth transport is ENABLED"));
			if (SOCKET_ERROR == listen(g_listenSock_bth, 5))
			{
				LogFail(TEXT("listen() failed, error = %d"), WSAGetLastError());
				goto FAIL_EXIT;
			}
		}
	}

	// Listen
	g_listenSock_irda = socket(AF_IRDA, SOCK_STREAM, 0);
	if (g_listenSock_irda == INVALID_SOCKET)
	{
		LogWarn2(TEXT("socket() failed, error = %d"), WSAGetLastError());
		LogWarn2(TEXT("Irda transport is DISABLED"));
	}
	else
	{
		memset(&local_irda, 0, sizeof(local_irda));
		local_irda.irdaAddressFamily = AF_IRDA;
		strncpy(&local_irda.irdaServiceName[0], DEFAULT_NAME, sizeof(local_irda.irdaServiceName));
		
		if (SOCKET_ERROR == bind(g_listenSock_irda, (const struct sockaddr *)&local_irda, sizeof(local_irda)))
		{
			LogWarn2(TEXT("bind() failed, error = %d"), WSAGetLastError());
			LogWarn2(TEXT("Irda transport is DISABLED"));			
		}
		else
		{
			LogComment(TEXT("Irda transport is ENABLED"));
			g_bRunIrda = TRUE;

			// register IAS
			memset (pSet, 0, sizeof (setBuffer));
			memcpy (&pSet->irdaClassName[0], &local_irda.irdaServiceName[0], sizeof (pSet->irdaClassName));
			memcpy (&pSet->irdaAttribName[0], &local_irda.irdaServiceName, sizeof (pSet->irdaAttribName));
			pSet->irdaAttribType = IAS_ATTRIB_INT;
			pSet->irdaAttribute.irdaAttribInt = 1;
			iLen = sizeof (setBuffer);
			if (SOCKET_ERROR == setsockopt(g_listenSock_irda, SOL_IRLMP, IRLMP_IAS_SET, (char *)pSet, iLen))
			{
				LogFail(TEXT("setsockopt() failed, error = %d"), WSAGetLastError());
				goto FAIL_EXIT;
			}

			if (SOCKET_ERROR == listen(g_listenSock_irda, 5))
			{
				LogFail(TEXT("listen() failed, error = %d"), WSAGetLastError());
				goto FAIL_EXIT;
			}
		}

	}

	if (!g_bRunBluetooth && !g_bRunIrda)
	{
		LogFail(TEXT("Neither Bluetooth nor Irda is enabled"));

		goto FAIL_EXIT;
	}


	LogVerbose(TEXT("-InitializeStressModule"));

	return TRUE;

FAIL_EXIT:
	if ((g_listenSock_bth != INVALID_SOCKET) && (g_listenSock_bth != NULL))
	{
		shutdown(g_listenSock_bth, 0);
		closesocket(g_listenSock_bth);
	}

	if ((g_listenSock_irda != INVALID_SOCKET) && (g_listenSock_irda != NULL))
	{
		shutdown(g_listenSock_irda, 0);
		closesocket(g_listenSock_irda);
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
	
	UINT uiResult = CESTRESS_PASS;
	DWORD dwExitCode = 0;
	HANDLE hListenThread[2];
	DWORD dwThreadID[2];
	int i;

	memset(&hListenThread[0], 0, sizeof(hListenThread));
	if (g_bRunBluetooth)
	{
		hListenThread[0] = CreateThread(NULL, 0, ListenThread, (LPVOID)(g_listenSock_bth), 0 , &dwThreadID[0]);
		if (hListenThread[0] == NULL)
		{
			LogFail(TEXT("CreateThread() failed, error %d\n"), GetLastError());	
			goto FAIL_EXIT;
		}
	}
	if (g_bRunIrda)
	{
		hListenThread[1] = CreateThread(NULL, 0, ListenThread, (LPVOID)(g_listenSock_irda), 0 , &dwThreadID[1]);
		if (hListenThread[1] == NULL)
		{
			LogFail(TEXT("CreateThread() failed, error %d\n"), GetLastError());	
			goto FAIL_EXIT;
		}
	}

	for (i = 0; i < 2; i ++)
	{
		if (hListenThread[i] != NULL)
		{
			WaitForSingleObject(hListenThread[i], INFINITE);
			if (!GetExitCodeThread(hListenThread[i], &dwExitCode))
			{
				LogWarn1(TEXT("GetExitCodeThread failed, error = %d"), GetLastError());
			}
			if (dwExitCode != 0)
			{
				uiResult = CESTRESS_FAIL;
			}
			CloseHandle(hListenThread[i]);
		}
	}

	// You must return one of these values:

	LogVerbose(TEXT("-DoStressIteration"));
	return uiResult;
	//return CESTRESS_FAIL;
	//return CESTRESS_WARN1;
	//return CESTRESS_WARN2;
	//return CESTRESS_ABORT;  // Use carefully.  This will cause your module to be terminated immediately.

FAIL_EXIT:

	LogVerbose(TEXT("-DoStressIteration"));
	
	return CESTRESS_FAIL;
}



/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
	LogVerbose(TEXT("+TerminateStressModule"));

	// TODO:  Do test-specific clean-up here.

	if ((g_hEvent != INVALID_HANDLE_VALUE) && (g_hEvent != NULL))
	{
		CloseHandle (g_hEvent);
	}

	if ((g_listenSock_bth != INVALID_SOCKET) && (g_listenSock_bth != NULL))
	{
		shutdown(g_listenSock_bth, 0);
		closesocket(g_listenSock_bth);
	}

	if ((g_listenSock_irda != INVALID_SOCKET) && (g_listenSock_irda != NULL))
	{
		shutdown(g_listenSock_irda, 0);
		closesocket(g_listenSock_irda);
	}

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


