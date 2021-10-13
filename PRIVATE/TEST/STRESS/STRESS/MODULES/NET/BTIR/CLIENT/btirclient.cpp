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
#include <af_irda.h>
#include <devload.h>
#include <stressutils.h>
#include "util.h"

// Global variables
static ServerHash g_BTServers;
static SOCKADDR_IRDA g_IRServerAddr;

static HANDLE g_hEvent = NULL;
static BOOL g_bRunBluetooth = FALSE;
static BOOL g_bRunIrda = FALSE;

#define BT_STATUS_REGPATH L"COMM\\CESTRESS\\Bluetooth"

typedef struct _BTStatus
{
WCHAR wszHardwareID[MAX_PATH];	// Bluetooth hardware PnP ID
DWORD dwInquiries;				// # of BT inquires done
DWORD dwSDPs;					// # of SDP inquiries done
DWORD dwConnections;			// # of successful rfcomm connections
DWORD dwConnErrors;				// # of rfcomm connection errors
DWORD dwBytesSent;				// # of bytes sent
DWORD dwSendErrors;				// # of send errors
DWORD dwBytesRecv;				// # of bytes received
DWORD dwRecvErrors;				// # of receive errors
} BT_STATUS;

#define IR_STATUS_REGPATH L"COMM\\CESTRESS\\IR"

typedef struct _IRStatus
{
DWORD dwInquiries;				// # of IR inquires done
DWORD dwSDPs;					// # of SDP inquiries done
DWORD dwConnections;			// # of successful IR connections
DWORD dwConnErrors;				// # of IR connection errors
DWORD dwBytesSent;				// # of bytes sent
DWORD dwSendErrors;				// # of send errors
DWORD dwBytesRecv;				// # of bytes received
DWORD dwRecvErrors;				// # of receive errors
} IR_STATUS;

static BT_STATUS g_btStatus;
static IR_STATUS g_irStatus;

DWORD ReadBTStatus()
{
	DWORD dwResult = 0;
	HKEY hKey;
	DWORD dwType;
	LPBYTE lpData;
	DWORD cbData;

	dwResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, BT_STATUS_REGPATH, 0, 0, &hKey);
	if (ERROR_SUCCESS == dwResult)
	{
		dwType = 0;
		lpData = (LPBYTE)g_btStatus.wszHardwareID;
		cbData = sizeof(g_btStatus.wszHardwareID);
		RegQueryValueEx(hKey, L"HardwareID", 0, &dwType, lpData, &cbData);
		ASSERT(REG_SZ == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_btStatus.dwInquiries;
		cbData = sizeof(g_btStatus.dwInquiries);
		RegQueryValueEx(hKey, L"Inquiries", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_btStatus.dwSDPs;
		cbData = sizeof(g_btStatus.dwSDPs);
		RegQueryValueEx(hKey, L"SDPs", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_btStatus.dwConnections;
		cbData = sizeof(g_btStatus.dwConnections);
		RegQueryValueEx(hKey, L"Connections", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_btStatus.dwConnErrors;
		cbData = sizeof(g_btStatus.dwConnErrors);
		RegQueryValueEx(hKey, L"ConnErrors", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_btStatus.dwBytesSent;
		cbData = sizeof(g_btStatus.dwBytesSent);
		RegQueryValueEx(hKey, L"BytesSent", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_btStatus.dwSendErrors;
		cbData = sizeof(g_btStatus.dwSendErrors);
		RegQueryValueEx(hKey, L"SendErrors", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_btStatus.dwBytesRecv;
		cbData = sizeof(g_btStatus.dwBytesRecv);
		RegQueryValueEx(hKey, L"BytesRecv", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_btStatus.dwRecvErrors;
		cbData = sizeof(g_btStatus.dwRecvErrors);
		RegQueryValueEx(hKey, L"RecvErrors", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		RegCloseKey(hKey);
	}

	return dwResult;
}

DWORD WriteBTStatus()
{
	DWORD dwResult = 0;
	HKEY hKey;
	DWORD dwDisp;
	LPBYTE lpData;
	DWORD cbData;

	dwResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, BT_STATUS_REGPATH, 0, NULL, 0, 0, NULL, &hKey, &dwDisp);
	if (ERROR_SUCCESS == dwResult)
	{
		lpData = (LPBYTE)g_btStatus.wszHardwareID;
		cbData = sizeof(g_btStatus.wszHardwareID);
		RegSetValueEx(hKey, L"HardwareID", 0, REG_SZ, lpData, cbData);

		lpData = (LPBYTE)&g_btStatus.dwInquiries;
		cbData = sizeof(g_btStatus.dwInquiries);
		RegSetValueEx(hKey, L"Inquiries", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_btStatus.dwSDPs;
		cbData = sizeof(g_btStatus.dwSDPs);
		RegSetValueEx(hKey, L"SDPs", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_btStatus.dwConnections;
		cbData = sizeof(g_btStatus.dwConnections);
		RegSetValueEx(hKey, L"Connections", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_btStatus.dwConnErrors;
		cbData = sizeof(g_btStatus.dwConnErrors);
		RegSetValueEx(hKey, L"ConnErrors", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_btStatus.dwBytesSent;
		cbData = sizeof(g_btStatus.dwBytesSent);
		RegSetValueEx(hKey, L"BytesSent", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_btStatus.dwSendErrors;
		cbData = sizeof(g_btStatus.dwSendErrors);
		RegSetValueEx(hKey, L"SendErrors", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_btStatus.dwBytesRecv;
		cbData = sizeof(g_btStatus.dwBytesRecv);
		RegSetValueEx(hKey, L"BytesRecv", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_btStatus.dwRecvErrors;
		cbData = sizeof(g_btStatus.dwRecvErrors);
		RegSetValueEx(hKey, L"RecvErrors", 0, REG_DWORD, lpData, cbData);

		RegCloseKey(hKey);
	}

	return dwResult;
}

DWORD ReadIRStatus()
{
	DWORD dwResult = 0;
	HKEY hKey;
	DWORD dwType;
	LPBYTE lpData;
	DWORD cbData;

	dwResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, IR_STATUS_REGPATH, 0, 0, &hKey);
	if (ERROR_SUCCESS == dwResult)
	{
		dwType = 0;
		lpData = (LPBYTE)&g_irStatus.dwInquiries;
		cbData = sizeof(g_irStatus.dwInquiries);
		RegQueryValueEx(hKey, L"Inquiries", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_irStatus.dwSDPs;
		cbData = sizeof(g_irStatus.dwSDPs);
		RegQueryValueEx(hKey, L"SDPs", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_irStatus.dwConnections;
		cbData = sizeof(g_irStatus.dwConnections);
		RegQueryValueEx(hKey, L"Connections", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_irStatus.dwConnErrors;
		cbData = sizeof(g_irStatus.dwConnErrors);
		RegQueryValueEx(hKey, L"ConnErrors", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_irStatus.dwBytesSent;
		cbData = sizeof(g_irStatus.dwBytesSent);
		RegQueryValueEx(hKey, L"BytesSent", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_irStatus.dwSendErrors;
		cbData = sizeof(g_irStatus.dwSendErrors);
		RegQueryValueEx(hKey, L"SendErrors", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_irStatus.dwBytesRecv;
		cbData = sizeof(g_irStatus.dwBytesRecv);
		RegQueryValueEx(hKey, L"BytesRecv", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		dwType = 0;
		lpData = (LPBYTE)&g_irStatus.dwRecvErrors;
		cbData = sizeof(g_irStatus.dwRecvErrors);
		RegQueryValueEx(hKey, L"RecvErrors", 0, &dwType, lpData, &cbData);
		ASSERT(REG_DWORD == dwType);

		RegCloseKey(hKey);
	}

	return dwResult;
}

DWORD WriteIRStatus()
{
	DWORD dwResult = 0;
	HKEY hKey;
	DWORD dwDisp;
	LPBYTE lpData;
	DWORD cbData;

	dwResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, IR_STATUS_REGPATH, 0, NULL, 0, 0, NULL, &hKey, &dwDisp);
	if (ERROR_SUCCESS == dwResult)
	{
		lpData = (LPBYTE)&g_irStatus.dwInquiries;
		cbData = sizeof(g_irStatus.dwInquiries);
		RegSetValueEx(hKey, L"Inquiries", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_irStatus.dwSDPs;
		cbData = sizeof(g_irStatus.dwSDPs);
		RegSetValueEx(hKey, L"SDPs", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_irStatus.dwConnections;
		cbData = sizeof(g_irStatus.dwConnections);
		RegSetValueEx(hKey, L"Connections", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_irStatus.dwConnErrors;
		cbData = sizeof(g_irStatus.dwConnErrors);
		RegSetValueEx(hKey, L"ConnErrors", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_irStatus.dwBytesSent;
		cbData = sizeof(g_irStatus.dwBytesSent);
		RegSetValueEx(hKey, L"BytesSent", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_irStatus.dwSendErrors;
		cbData = sizeof(g_irStatus.dwSendErrors);
		RegSetValueEx(hKey, L"SendErrors", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_irStatus.dwBytesRecv;
		cbData = sizeof(g_irStatus.dwBytesRecv);
		RegSetValueEx(hKey, L"BytesRecv", 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_irStatus.dwRecvErrors;
		cbData = sizeof(g_irStatus.dwRecvErrors);
		RegSetValueEx(hKey, L"RecvErrors", 0, REG_DWORD, lpData, cbData);

		RegCloseKey(hKey);
	}

	return dwResult;
}

DWORD PerformInquiry(unsigned int LAP, unsigned char length, BT_ADDR *addresses, unsigned char num_responses)
{
	DWORD dwResult = 0;
	unsigned int cDiscoveredDevices = 0;
	BthInquiryResult inquiryResultList[128];
	unsigned int i = 0;
	DWORD dwStartTime = 0, dwEndTime = 0;

	// perform inquiry
	g_btStatus.dwInquiries ++;
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
		else if (cDiscoveredDevices < num_responses)
		{
			// now get the list of connected devices and append to addresses[]

			unsigned short handles[20];
			int cHandlesReturned = 0;
			
			dwResult = BthGetBasebandHandles(sizeof(handles)/sizeof(handles[0]), handles, &cHandlesReturned);

			if (dwResult != ERROR_SUCCESS)
			{
				LogWarn2(TEXT("BthGetBasebandHandles failed with error %d"), dwResult);
			}
			else
			{
				LogComment(TEXT("%u devices currently connected"), cHandlesReturned);

				for (i = 0; i < (unsigned int) cHandlesReturned; i++)
				{
					BOOL bAdd = TRUE;
					BT_ADDR deviceAddress;

					// get the address from the handle
					if (BthGetAddress(handles[i], &deviceAddress) != ERROR_SUCCESS )
					{
						LogWarn2(TEXT("Could not get address for handle 0x%08x"), handles[i]);
						continue;
					}

					// check to see if it was in the inquiry list already
					for (int j = 0; j < (int)cDiscoveredDevices; j++)
					{
						if (addresses[j] == deviceAddress)
						{
							// already in the inquiry list
							bAdd = FALSE;
							break;
						}
					}

					if (bAdd)
					{
						addresses[cDiscoveredDevices++] = deviceAddress;
						
						LogComment(TEXT("Added connected device addr = %04x%08x"), 
							GET_NAP(deviceAddress), 
							GET_SAP(deviceAddress)
						);
					}
					
					if (cDiscoveredDevices == num_responses)
					{
						LogWarn2(TEXT("can't add more addresses: array is full"));
						break;
					}
					
				}
			
			}
			
		}
		
	}


	return dwResult;
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

	// Can't run when another instance of btir client is running
	g_hEvent = CreateEvent (NULL, FALSE, FALSE, TEXT(DEFAULT_NAME));
	if ((dwResult = GetLastError ()) == ERROR_ALREADY_EXISTS)
	{
		LogWarn2(TEXT("Another BTIRCLIENT instance is already running.  Bailing ..."));
		goto FAIL_EXIT;
	}
	if (g_hEvent == NULL)
	{
		LogFail(TEXT("CreateEvent failed, error = %d"), dwResult);
		goto FAIL_EXIT;
	}

	memset(&g_IRServerAddr, 0, sizeof(g_IRServerAddr));

	// Initialize status
	memset(&g_btStatus, 0, sizeof(g_btStatus));
	memset(&g_irStatus, 0, sizeof(g_irStatus));
	ReadBTStatus();
	ReadIRStatus();

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
					
					// Get BT device ID
					LPTSTR lpszPnpList;
					DWORD dwSize = 0;
					wcscpy(g_btStatus.wszHardwareID, L"UNKNOWN_ID");

					// TODO :  This only works for PCMCIA cards. Need a way to do the same for USB/SDIO.
					EnumPnpIds(NULL, &dwSize);
					if (dwSize)
					{
						lpszPnpList = (LPTSTR)LocalAlloc(LPTR, dwSize);
						if (lpszPnpList && (ERROR_SUCCESS == EnumPnpIds(lpszPnpList, &dwSize)))
						{
							// BUGBUG : Assume the first one is Bluetooth card
							wcscpy(g_btStatus.wszHardwareID, lpszPnpList);
						}
						LocalFree(lpszPnpList);
					}
						
					// Get BT device version
					unsigned char hci_version;
					unsigned short hci_revision;
					unsigned char lmp_version;
					unsigned short lmp_subversion;
					unsigned short manufacturer;
					unsigned char lmp_features;

					if (ERROR_SUCCESS == BthReadLocalVersion(&hci_version, &hci_revision, &lmp_version, &lmp_subversion, &manufacturer, &lmp_features))
					{
						wsprintf(g_btStatus.wszHardwareID, L"%s(%02x-%04x-%02x-%04x-%04x-%02x)", 
							g_btStatus.wszHardwareID, hci_version, hci_revision, lmp_version, lmp_subversion, manufacturer, lmp_features);
					}
					else
					{
						wsprintf(g_btStatus.wszHardwareID, L"%s(UNKNOWN_VERSION)", g_btStatus.wszHardwareID);
					}
				}
			}
		}

		shutdown(sock, 0);
		closesocket(sock);
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
		g_irStatus.dwInquiries ++;
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

	int				i, iLen, iBytes, iTotalBytes;
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

			// find a server
			if (g_BTServers.empty())
			{
				// list is empty, search for a server
				FindServers (g_BTServers, 8, 1);
				g_btStatus.dwSDPs += g_BTServers.size();

				if (g_BTServers.empty())
				{
					// list is still empty, quit this iteration
					LogComment(TEXT("No available Bluetooth enabled btir server in range"));
					shutdown(sock, 0);
					closesocket(sock);
					continue;
				}
			}			

			// get the server's address and port number
			for (ServerHash::iterator it = g_BTServers.begin(), itEnd = g_BTServers.end(); it != itEnd; ++it)
			{
				remote_bth.btAddr = it->first;
				remote_bth.port = it->second;
				break;
			}

			// connect to the server			
			LogComment(TEXT("Connect to Bluetooth server [%04x%08x] on channel %d"), 
				GET_NAP(remote_bth.btAddr), GET_SAP(remote_bth.btAddr), remote_bth.port);
			if (SOCKET_ERROR == connect(sock, (struct sockaddr *)&remote_bth, iLen))
			{
				g_BTServers.clear(); // current server invalid, force a new inquiry on next iteration
				g_btStatus.dwConnErrors ++;
				LogFail(TEXT("connect() failed, error = %d"), WSAGetLastError());
				shutdown(sock, 0);
				closesocket(sock);
				continue;
			}
			g_btStatus.dwConnections ++;
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
			memcpy (&pQuery->irdaClassName[0], remote_irda.irdaServiceName, 
				sizeof (pQuery->irdaClassName) < sizeof (remote_irda.irdaServiceName) ? sizeof (pQuery->irdaClassName) : sizeof (remote_irda.irdaServiceName));
			pQuery->irdaAttribType = IAS_ATTRIB_INT;
			memcpy (&pQuery->irdaAttribName[0], remote_irda.irdaServiceName,
				sizeof (pQuery->irdaAttribName) < sizeof (remote_irda.irdaServiceName) ? sizeof (pQuery->irdaAttribName) : sizeof (remote_irda.irdaServiceName));
			g_irStatus.dwSDPs ++;
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
				g_irStatus.dwConnErrors ++;
				LogFail(TEXT("connect() failed, error = %d"), WSAGetLastError());
				shutdown(sock, 0);
				closesocket(sock);
				continue;
			}
			g_irStatus.dwConnections ++;
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
				if (i % 2 == 0)
				{
					g_btStatus.dwSendErrors ++;
				}
				else
				{
					g_irStatus.dwSendErrors ++;
				}
				LogFail(TEXT("send() failed, error = %d"), WSAGetLastError());
				goto FAIL_EXIT;
			}
			iTotalBytes += iBytes;
		}
		if (i % 2 == 0)
		{
			g_btStatus.dwBytesSent += iTotalBytes;
		}
		else
		{
			g_irStatus.dwBytesSent += iTotalBytes;
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
			if (iBytes == 0)
			{
				iBytes = SOCKET_ERROR;
				WSASetLastError(WSAECONNRESET);
			}
			if (iBytes == SOCKET_ERROR)
			{
				if (i % 2 == 0)
				{
					g_btStatus.dwRecvErrors ++;
				}
				else
				{
					g_irStatus.dwRecvErrors ++;
				}
				LogFail(TEXT("recv() failed, error = %d"), WSAGetLastError());
				goto FAIL_EXIT;
			}
			if (!VerifyBuffer(pBuffer, iBytes, iTotalBytes))
			{
				if (i % 2 == 0)
				{
					g_btStatus.dwRecvErrors ++;
				}
				else
				{
					g_irStatus.dwRecvErrors ++;
				}
				goto FAIL_EXIT;
			}
			iTotalBytes += iBytes;
		}
		if (i % 2 == 0)
		{
			g_btStatus.dwBytesRecv += iTotalBytes;
		}
		else
		{
			g_irStatus.dwBytesRecv += iTotalBytes;
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

	// Update status
	WriteBTStatus();
	WriteIRStatus();

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
		LogComment(TEXT("Sent %d bytes and received %d bytes over Bluetooth"), g_btStatus.dwBytesSent, g_btStatus.dwBytesRecv);
	}
	if (g_bRunIrda)
	{
		LogComment(TEXT("Sent %d bytes and received %d bytes over Irda"), g_irStatus.dwBytesSent, g_irStatus.dwBytesRecv);
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
