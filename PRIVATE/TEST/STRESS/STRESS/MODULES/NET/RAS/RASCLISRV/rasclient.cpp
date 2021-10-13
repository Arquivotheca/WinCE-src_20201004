//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
// This file will contain the implementation of the following three functions
// exported by RasCliSrv.dll:
//
//		1) InitializeStressModule
//		2) DoStressIteration
//		3) TerminateStressModule
//
// These functions are invoked by stressmod.exe to run CE Stress
//

#include "RasCliSrv.h"

RegReportTypes_t g_RegReportTypes;
HINSTANCE g_hInstance=NULL;
BOOL g_bOutOfMemory = FALSE;

#define MY_tcscpy(a, b)\
{\
	LPTSTR _str = (b);\
	if(_str == NULL)\
	{\
		LogComment(TEXT("Invalid Command line\r\n"));\
		ShowHelp();\
		return FALSE;\
	}\
	else\
	{\
		_tcscpy((a),_str);\
	}\
}

#define CHECK_STR(str)\
{\
	if((str) == NULL)\
	{\
		LogComment(TEXT("Invalid Command line\r\n"));\
		ShowHelp();\
		return FALSE;\
	}\
}

#define ShowHelp()\
{\
	LogComment(TEXT("Module command line options: \r\n"));\
	LogComment(TEXT("-b <baudrate> -d <Data transfer \"On\"|\"Off\"> -s <PhoneNum or ServerName> -u <user name> -p <pwd> -m <domain> [?]"));\
}

#define DISPLAY_REPORT_REG_VALUES()\
{\
	LogVerbose(TEXT("\n\n\tg_RegReportTypes.dwBytesXmited=%d\n"), g_RegReportTypes.dwBytesXmited);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwBytesRcved=%d\n"), g_RegReportTypes.dwBytesRcved);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwFramesXmited=%d\n"), g_RegReportTypes.dwFramesXmited);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwFramesRcved=%d\n"), g_RegReportTypes.dwFramesRcved);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwCrcErr=%d\n"), g_RegReportTypes.dwCrcErr);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwTimeoutErr=%d\n"), g_RegReportTypes.dwTimeoutErr);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwAlignmentErr=%d\n"), g_RegReportTypes.dwAlignmentErr);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwHardwareOverrunErr=%d\n"), g_RegReportTypes.dwHardwareOverrunErr);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwFramingErr=%d\n"), g_RegReportTypes.dwFramingErr);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwBufferOverrunErr=%d\n"), g_RegReportTypes.dwBufferOverrunErr);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwConnects=%d\n"), g_RegReportTypes.dwConnects);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwDisconnects=%d\n"), g_RegReportTypes.dwDisconnects);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwErrors=%d\n"), g_RegReportTypes.dwErrors);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwTCPData=%d\n"), g_RegReportTypes.dwTCPData);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwUDPData=%d\n"), g_RegReportTypes.dwUDPData);\
	LogVerbose(TEXT("\tg_RegReportTypes.dwICMPPings=%d\n\n"), g_RegReportTypes.dwICMPPings);\
}

//
// Where do we want to log results to, since there are multiple registries
// depending on whether we are doing PPP, PPTP etc.
//
#define WHICH_KEY_TO_USE()\
{\
	switch(eRasConnection)\
	{\
		case RAS_DCC_MODEM:\
		case RAS_PPP_MODEM:\
			lpKeyClass = RAS_REPORT_VALUES_REG_KEY_PPP;\
			break;\
\
		case RAS_PPP_PPPoE:\
			lpKeyClass = RAS_REPORT_VALUES_REG_KEY_PPPoE;\
			break;\
\
		case RAS_VPN_PPTP:\
			lpKeyClass = RAS_REPORT_VALUES_REG_KEY_PPTP;\
			break;\
\
		case RAS_VPN_L2TP:\
			lpKeyClass = RAS_REPORT_VALUES_REG_KEY_L2TP;\
			break;\
\
		default:\
			lpKeyClass = RAS_REPORT_VALUES_REG_KEY_PPP;\
	}\
}

BOOL InitializeStressModule (
							 /*[in]*/ MODULE_PARAMS* pmp, 
							 /*[out]*/ UINT* pnThreads
							 )
{
	DWORD	argc, dwNumRunningModules=0;
	DWORD	nRetries = DEFAULT_RETRIES;
	TCHAR	strUserCmdLine[MAX_CMD_LINE+1];
	TCHAR	strToken[MAX_PARAMS];

	// Set this value to indicate the number of threads
	// that you want your module to run.  The module container
	// that loads this DLL will manage the life-time of these
	// threads.  Each thread will call your DoStressIteration()
	// function in a loop.

	*pnThreads = 1;

	InitializeStressUtils (
		TEXT("RasCliSrvDSDModule"),
		LOGZONE(SLOG_SPACE_NET, SLOG_RAS),	
		pmp
		);

	//
	// We want only one RASCliSrv module to run at any one time
	//
	dwNumRunningModules=GetRunningModuleCount(g_hInstance);
	LogVerbose(TEXT("dwNumRunningModules(RasCliSrv)=%d\n"), dwNumRunningModules);
	if (dwNumRunningModules > 1)
	{
		LogComment(TEXT("Another instance of RasCliSrv.dll is running. Returning FALSE."));
		return FALSE;
	}
	
	// Any module-specific initialization here
	LogVerbose(TEXT("User Command line: %s\n"), pmp->tszUser);

	if(pmp->tszUser == NULL)
	{
		LogComment(TEXT("You need to specify a command line (server name, user name, password, domain).\r\n"));
		LogComment(TEXT("Note for a DCC connection (with no authentication), please just specify the baudrate (needed).\r\n"));
		ShowHelp();
		return FALSE;
	}
	// Process command line parameters and set global variables
	wcsncpy(strUserCmdLine, pmp->tszUser, MAX_CMD_LINE);
	strUserCmdLine[MAX_CMD_LINE]=L'0';
	argc = NumParams(pmp->tszUser);

	_tcscpy(strToken, _tcstok(strUserCmdLine, L" \0\n"));
	while (argc > 0) 
	{
		//
		// Data transfer?
		//
		if (!_tcsicmp(strToken, L"-d")) 
		{
			MY_tcscpy(strToken, _tcstok(NULL, L" \0\n"));
			CHECK_STR(strToken);
			
			//
			// Off -> No Data transfer
			// On -> Data transfer included
			//
			if (!_tcsicmp(strToken, TEXT("off")))
			{
				bDataTransfer = FALSE;
			}
			else if (!_tcsicmp(strToken, TEXT("on")))
			{
				bDataTransfer = TRUE;
			}
			argc--;
		}
		else if (!_tcsicmp(strToken, L"-b")) 
		{
			MY_tcscpy(strToken, _tcstok(NULL, L" \0\n"));
			CHECK_STR(strToken);

			dwBaudrate = _ttol(strToken);
			LogVerbose(TEXT("Baudrate: %d"), dwBaudrate);
			argc--;
		}
		else if (!_tcsicmp(strToken, L"-u")) 
		{
			MY_tcscpy(strToken, _tcstok(NULL, L" \0\n"));
			CHECK_STR(strToken);

			_tcsncpy(strUserName, strToken, UNLEN+1);
			LogComment(TEXT("UserName: %s"), strUserName);
			argc--;
		}
		else if (!_tcsicmp(strToken, L"-p")) 
		{
			MY_tcscpy(strToken, _tcstok(NULL, L" \0\n"));
			CHECK_STR(strToken);

			_tcsncpy(strPassword, strToken, PWLEN+1);
			LogVerbose(TEXT("Password: %s"), strPassword);
			argc--;
		}
		else if (!_tcsicmp(strToken, L"-m")) 
		{
			MY_tcscpy(strToken, _tcstok(NULL, L" \0\n"));
			CHECK_STR(strToken);

			_tcsncpy(strDomain, strToken, DNLEN+1);
			LogVerbose(TEXT("Domain: %s"), strDomain);
			argc--;
		}
		else if (!_tcsicmp(strToken, L"-s")) 
		{
			MY_tcscpy(strToken, _tcstok(NULL, L" \0\n"));
			CHECK_STR(strToken);

			_tcsncpy(strPhoneNum, strToken, MAX_USER_NAME_PW+1);
			LogVerbose(TEXT("ServerName or phone number: %s"), strPhoneNum);
			argc--;
		}
		else if (!_tcsicmp(strToken, L"?")) 
		{
			ShowHelp();
			break;
		}
		else 
		{
			LogVerbose(TEXT("Unknown Command Line parameter: %s"), strToken);
			break;
		}
		
		if (argc == 1) break;
		MY_tcscpy(strToken, _tcstok(NULL, L" \0\n"));

		argc--;
	}

	//
	// Determine how to connect. First try on PPP/DCC, if that fails,
	// then try PPTP. We can add L2TP/PPPoE/modem etc later on. 
	// If all fail to connect, return FALSE (i.e. do not start the module)
	//
	// Note that a data receiver server should also be running on the PPP/PPTP 
	// server; otherwise we will running simple pings and no data transfer.
	// 
	if (DetermineRasLinkToRunStress() == false)
	{
		LogAbort(TEXT("Unable to run connect on PPP/PPTP/L2TP. Aborting...\n"));
		return FALSE;
	}
	
	DisconnectRasConnection();
	InitReportValuesFromReg();
 	return TRUE;
}

UINT DoStressIteration (
						/*[in]*/ HANDLE hThread, 
						/*[in]*/ DWORD ThreadID, 
						/*[in,out]*/ LPVOID pv /*unused*/)
{
	bool bStatus = false;
	g_bOutOfMemory = FALSE;

	LogComment(TEXT("Running RAS Stress for Entry: %s"), strEntryName);
	bStatus = RunConnectStress();
	if(bStatus) 
	{
		LogComment(TEXT("RAS Client Connect/Data/Disconnect stress (Server: \"%s\", Entry: \"%s\") PASSED"), strPhoneNum, strEntryName);
	}
	else 
	{
		LogWarn1(TEXT("RAS Client Connect/Data/Disconnect stress (Server: \"%s\", Entry: \"%s\") FAILED"), strPhoneNum, strEntryName);
	}
	
	GetRasStats();
	WriteRegValues();
	DisconnectRasConnection();
	
	if(g_bOutOfMemory) return CESTRESS_WARN1;
	return bStatus? CESTRESS_PASS: CESTRESS_FAIL;
}

DWORD TerminateStressModule (void)
{
	// TODO:  Do test-specific clean-up here.

	// This will be used as the process exit code.
	// It is not currently used by the harness.

	GetRasStats();
	WriteRegValues();
	DisconnectRasConnection();
	return ((DWORD) -1);
}

BOOL WINAPI DllMain(
					HANDLE hInstance, 
					ULONG dwReason, 
					LPVOID lpReserved
					)
{
	g_hInstance = (HINSTANCE) hInstance;
	return TRUE;
}


//
// Returns the number of parameters in the user command line
// If the command line is: -s abc -s6 def -h -c, value returned 
// will be 6
//
DWORD	NumParams(LPTSTR str) 
{
	DWORD len=0;

	while (*str != TEXT('\0')) 
	{
		if (*str == TEXT(' ') || *str == TEXT('\0') || *str == TEXT('\n')) 
		{
			len++;
		}
		str++;
	}
	len++;

	LogVerbose(TEXT("Number of Arguments = %d\n"), len);
	return(len);
}

//
// Sequence: Create an entry name, connect to it, disconnect.
//
bool RunConnectStress() 
{
	//
	// Disconnect all connections before starting a new connection
	//
	DisconnectRasConnection();
	
	// Create a phone book entry for this connection
	if (false == CreatePhoneBookEntry()) 
	{
		LogWarn1(TEXT("Can not create Entry: %s\n"), strEntryName);
		g_RegReportTypes.dwErrors++;
		
		return false;
	}

	// Connect
	if (false ==  ConnectRasConnection()) 
	{
		LogWarn1(TEXT("Can not Connect to Server: %s using Entry: %s\n"), strPhoneNum, strEntryName);
		DisconnectRasConnection();
		g_RegReportTypes.dwErrors++;
		
		return false;
	}
	else 
	{
		LogVerbose(TEXT("+++ Connected Successfully to %s. +++"), strPhoneNum);
		g_RegReportTypes.dwConnects++;
	}
	
	Sleep(DEFAULT_SLEEP);

	// Data transfer?
	if (bDataTransfer == TRUE)
	{
		//
		// Send Data over this RAS connection here
		// 
		SendData(SOCK_STREAM);
		Sleep(DEFAULT_SLEEP);
		
		SendData(SOCK_DGRAM);
		Sleep(DEFAULT_SLEEP);
	}
	else
	{
		//
		// Run some pings (100) on the RAS connection just created.
		//
		PingLink("ppp_peer", 100);
		g_RegReportTypes.dwICMPPings += 100;
	}

	Sleep(DEFAULT_SLEEP);

	// 
	// Get Ras Link Statistics for this active connection
	// 
	GetRasStats();

	// Disconnect
	if (false == DisconnectRasConnection())
	{
		LogWarn1(TEXT("Can not Disconnect from Server: %s using Entry: %s\n"), strPhoneNum, strEntryName);
		g_RegReportTypes.dwErrors++;

		return false;
	}
	else 
	{
		LogVerbose(TEXT("--- Disconnected Successfully ---"));
		g_RegReportTypes.dwDisconnects++;
	}

	Sleep(DEFAULT_SLEEP);
	return true;
}

//
// This entry will need to be created as the first step. Later on,
// this entry will be used to connect and disconnect.
//
bool CreatePhoneBookEntry()  
{
//	BYTE RasEntryBuf[ENTRYBUFSIZE];
	DWORD dwRasBufSize = ENTRYBUFSIZE;
	LPRASENTRY lpRasEntry;					  
	RASDIALPARAMS DialParams;						
	DWORD dwErr;									
	BYTE DevConfigBuf[DEVBUFSIZE];
	DWORD dwDevConfigSize;
	LPBYTE lpDevConfig = DevConfigBuf + DEVCONFIGOFFSET;
	LPVARSTRING lpVarString = (LPVARSTRING)&DevConfigBuf;

	lpRasEntry = (LPRASENTRY)LocalAlloc(LPTR, ENTRYBUFSIZE);
	if(lpRasEntry == NULL)
	{
		LogComment(TEXT("LocalAlloc() failed with %d"), GetLastError());
		return false;
	}

//	lpRasEntry = (LPRASENTRY)RasEntryBuf;
	lpRasEntry->dwSize = sizeof(RASENTRY);
	DialParams.dwSize = sizeof(RASDIALPARAMS);

	lpRasEntry->dwSize = sizeof(RASENTRY);
	if (dwErr = RasGetEntryProperties(NULL, TEXT(""), lpRasEntry, &dwRasBufSize,
	  NULL, NULL))
	{
		LogVerbose(TEXT("Unable to create default RASENTRY structure: %d"), dwErr);

		LocalFree(lpRasEntry);
		return false;
	}

	// Fill in default parameters:
	_tcscpy(DialParams.szUserName, strUserName);
	DialParams.szPhoneNumber[0] = TEXT('\0');
	DialParams.szCallbackNumber[0] = TEXT('\0');
	_tcscpy(DialParams.szPassword, strPassword);
	_tcscpy(DialParams.szDomain, strDomain);

	lpRasEntry->dwfOptions =  RASEO_ProhibitPAP | RASEO_ProhibitEAP | RASEO_NoUserPwRetryDialog;
	lpRasEntry->dwCountryCode = 1;
	lpRasEntry->szAreaCode[0] = TEXT('\0');
	wcsncpy(lpRasEntry->szLocalPhoneNumber, strPhoneNum, MAX_USER_NAME_PW);
	lpRasEntry->dwfNetProtocols = RASNP_Ip;
	lpRasEntry->dwFramingProtocol = RASFP_Ppp;
	lpRasEntry->dwCustomAuthKey = 0;

	//
	// Get a device number
	//
	if (eRasConnection == RAS_VPN_L2TP || eRasConnection == RAS_VPN_PPTP)
	{
		if (dwErr = GetRasDeviceNum(RASDT_Vpn, dwDeviceNum))
		{
			LocalFree(lpRasEntry);
			return false;	
		}
		
		//
		// Get Device name and type
		//
		if (dwErr = GetRasDeviceName(lpRasEntry->szDeviceName, dwDeviceNum))
		{
			LocalFree(lpRasEntry);
			return false;	
		}
	
		if (dwErr = GetRasDeviceType(lpRasEntry->szDeviceType, dwDeviceNum))
		{
			LocalFree(lpRasEntry);
			return false;	
		}
	}
	else if (eRasConnection == RAS_DCC_MODEM)
	{
		if (dwErr = GetRasDeviceNum(RASDT_Direct, dwDeviceNum))
		{
			LocalFree(lpRasEntry);
			return false;	
		}

		if ((dwDeviceNum = GetUnimodemDeviceName(lpRasEntry->szDeviceName,
											TEXT("direct"))) == 0xFFFFFFFF)
		{
			LogVerbose(TEXT("Unable to find default serial device"));
			LocalFree(lpRasEntry);
			return false;
		}
	}
	else if (eRasConnection == RAS_PPP_MODEM)
	{
		if (dwErr = GetRasDeviceNum(RASDT_Modem, dwDeviceNum))
		{
			LocalFree(lpRasEntry);
			return false;	
		}
	}
	else if (eRasConnection == RAS_PPP_PPPoE)
	{
		if (dwErr = GetRasDeviceNum(RASDT_PPPoE , dwDeviceNum))
		{
			LocalFree(lpRasEntry);
			return false;	
		}
	}

	// Check for old entry with same name and delete it if found:
	if (dwErr = RasValidateEntryName(NULL, strEntryName))
	{
		if (dwErr != ERROR_ALREADY_EXISTS)
		{
			LogVerbose(TEXT("Entry \"%s\" name validation failed: %d"),
									strEntryName, dwErr);

			LocalFree(lpRasEntry);
			return false;
		}

		if (dwErr = RasDeleteEntry(NULL, strEntryName))
		{
			LogVerbose(TEXT("Unable to delete old entry: %d"), dwErr);

			LocalFree(lpRasEntry);
			return false;
		}
	}

	_tcsncpy(DialParams.szEntryName, strEntryName, RAS_MaxEntryName + 1);

	dwDevConfigSize = DEVCONFIGSIZE;
	if (GetUnimodemDevConfig(lpRasEntry->szDeviceName, lpVarString) == 0xFFFFFFFF)
	{
		// could be VPN connectoid
		lpDevConfig = NULL;
		dwDevConfigSize = 0;

		// VPN or PPPoE does not support custom-scripting DLL
		_tcscpy(lpRasEntry->szScript, TEXT(""));
		lpRasEntry->dwfOptions &= (~RASEO_CustomScript);
	}

	// Save default entry configuration
	if (dwErr = RasSetEntryProperties(NULL, strEntryName, lpRasEntry, dwRasBufSize,
	  NULL, 0))
	{
		LogVerbose(TEXT("\"RasSetEntryProperties\" failed: %d"), dwErr);

		LocalFree(lpRasEntry);
		return false;
	}

	if (dwErr = RasSetEntryDialParams(NULL, &DialParams, FALSE))
	{
		LogVerbose(TEXT("\"RasSetEntryDialParams\" failed: %d"), dwErr);

		LocalFree(lpRasEntry);
		return false;
	}

	//
	// Set baudrate for DCC or modem connections only
	//
	if (eRasConnection == RAS_DCC_MODEM || eRasConnection == RAS_PPP_MODEM)
	{
		if (dwErr = RasLinkSetBaudrate(strEntryName, dwBaudrate, 'N', 8, 1))
		{
			LocalFree(lpRasEntry);
			return false;
		}
	}

	//
	// If still here, then everything else passed
	//
	LocalFree(lpRasEntry);
	return true;
}

//
// Connect 
//
bool ConnectRasConnection()
{
	RASDIALPARAMS DialParams;
	BOOL bPasswordSaved;
	HRASCONN hRasConn=NULL;
	DWORD dwRet;

	DialParams.dwSize = sizeof(RASDIALPARAMS);
	_tcsncpy(DialParams.szEntryName, strEntryName, RAS_MaxEntryName + 1);
	if (dwRet = RasGetEntryDialParams(NULL, &DialParams, &bPasswordSaved))
	{
		LogVerbose(TEXT("Error reading dialing parameters: %d"), dwRet);
	}

	//
	// We will be using the synchronous version of RasDial
	// In this case, RasDial will only return after connection either passed or 
	// failed.
	// 
	if (dwRet = RasDial(NULL, NULL, &DialParams, 0, NULL, &hRasConn))
	{
		LogWarn1(TEXT("\"RasDial\" failed: %d"), dwRet);

		//
		// Did it fail because of memory related issues? Don't keep on failing!
		//
		if(dwRet == ERROR_NOT_ENOUGH_MEMORY || dwRet == ERROR_OUTOFMEMORY)
		{
			g_bOutOfMemory = TRUE;
			LogWarn1(TEXT("Running low on memory ... g_bOutOfMemory = TRUE\r\n"));
		}
		return false;
	}

	return true;
}

//
// Disconnect
//
bool DisconnectRasConnection()
{
	RASCONN RasConnBuf[10];		// Allowing 10 max RAS connections
	DWORD dwRasConnBufSize;
	DWORD dwConnections;
	DWORD dwErr;
	DWORD i;

	RasConnBuf[0].dwSize = sizeof(RASCONN);
	dwRasConnBufSize = sizeof(RasConnBuf);

	if (dwErr = RasEnumConnections(RasConnBuf, &dwRasConnBufSize, &dwConnections))
	{
		LogWarn1(TEXT("\"RasEnumConnections\" error: %d"), dwErr);
		
		return false;
	}

	//
	// Any open RAS connections?
	//
	if (!dwConnections)
	{
		return true;
	}
	
	LogVerbose(TEXT("%lu open RAS connection(s)"), dwConnections);

	//
	// Disconnect all the active RAS connections
	//
	for (i = 0; i < dwConnections; ++i)
	{
		if ((strEntryName == NULL) || (_tcscmp(strEntryName, RasConnBuf[i].szEntryName) == 0))
		{
			if (dwErr = RasHangUp(RasConnBuf[i].hrasconn))
			{
				LogWarn1(TEXT("Error hanging up connection #%lu: %d"),i+1, dwErr);

				//
				// At least kill the rest of the RAS connections
				// 
				continue;
			}
		}
	}
	
	return true;
}

//
//
//
bool SendData(int iSockType)
{
	WSADATA			WSAData;
	DWORD			dwAddr = htonl(INADDR_ANY);
	DWORD			dwNumBytesSent=0;
    WORD			WSAVerReq = MAKEWORD(1,1);
	char			*pBuf			= NULL;
	SOCKET			sock = INVALID_SOCKET;
	SOCKADDR_IN		sClientAddrInfo;
	LPHOSTENT		lpHostData;

	//
	// We assume that the server is running on ppp_peer (Where we are connected
	// by a RAS connection). We will try to connect to that to send data.
	//
	char *szServer="ppp_peer";
	pBuf = (char *)LocalAlloc(LPTR, sizeof(char)*DEFAULT_PACKET_SIZE);
	if (pBuf == NULL)
	{
		LogVerbose(TEXT("LocalAlloc(pBuf) Failed."));
		return false;
	}

	if(WSAStartup(WSAVerReq, &WSAData) != 0) 
	{
		LogVerbose(TEXT("WSAStartup Failed\r\n"));
		LocalFree(pBuf);
		return false;
	}
		
	// get address as a number
	if ((dwAddr = inet_addr(szServer)) == INADDR_NONE) 
	{
		// Try by name
		lpHostData = gethostbyname(szServer);
		
		if (lpHostData == NULL) 
		{
			LogComment(TEXT("Could not resolve the given host name, %s"), szServer);
			
			goto Cleanup;
		}
		else
		{
			dwAddr = (DWORD) (*((IN_ADDR **) lpHostData->h_addr_list))->S_un.S_addr;
		}
	}
	
	if((sock = socket(AF_INET, iSockType, 0)) == INVALID_SOCKET)
	{
		LogComment(TEXT("socket() failed, error = %d"), WSAGetLastError());
		
		goto Cleanup;
	}
	
	sClientAddrInfo.sin_port        = 0;
	sClientAddrInfo.sin_addr.s_addr = htonl(INADDR_ANY);
	sClientAddrInfo.sin_family      = AF_INET;
	
	if(bind(sock, (SOCKADDR *)&sClientAddrInfo, sizeof(sClientAddrInfo)) == SOCKET_ERROR)
	{
		LogVerbose(TEXT("bind() failed, error = %d"), WSAGetLastError());
		
		goto Cleanup;
	}
	
	if(iSockType == SOCK_STREAM) 
	{
		sClientAddrInfo.sin_port        = htons(DEFAULT_PORT);
		sClientAddrInfo.sin_addr.s_addr = dwAddr;
		sClientAddrInfo.sin_family      = AF_INET;
		
		if(connect(sock, (SOCKADDR *)&sClientAddrInfo, 
									sizeof(sClientAddrInfo)) == SOCKET_ERROR) 
		{
			LogVerbose(TEXT("connect() failed, error = %d"), WSAGetLastError());
			goto Cleanup;
		}
		
		int iOptVal = 1;
		
		// Turn off Nagle algorithm
		if(setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, 
							(char *)&iOptVal, sizeof(iOptVal)) == SOCKET_ERROR)
		{
			LogVerbose(TEXT("setsocktopt() failed, error = %d."), WSAGetLastError());
		}
		
		// Send the packet
		DWORD i=0;
		for (i=0;i<dwNumSends;i++)
		{
			if((dwNumBytesSent = send(sock, pBuf, dwPacketSize, 0)) != dwPacketSize)
			{
				LogVerbose(TEXT("send() failed, number bytes sent = %d, error = %d"), 
												dwNumBytesSent, WSAGetLastError());
				g_RegReportTypes.dwErrors++;
			}
			else 
			{
				LogVerbose(TEXT("TCP Packet Number: %d sent"), i);
				g_RegReportTypes.dwTCPData += dwNumBytesSent;
			}
		}
	}
	else 
	{
		sClientAddrInfo.sin_port        = htons(DEFAULT_PORT);
		sClientAddrInfo.sin_addr.s_addr = dwAddr;
		sClientAddrInfo.sin_family      = AF_INET;
		
		// Send the packet
		DWORD i=0;
		for (i=0;i<dwNumSends;i++)
		{
			if((dwNumBytesSent = sendto(sock, pBuf, dwPacketSize, 0, 
										(SOCKADDR *)&sClientAddrInfo, 
										sizeof(sClientAddrInfo))) != dwPacketSize)
			{
				LogVerbose(TEXT("sendto() failed, number bytes sent = %d, error = %d"), 
												dwNumBytesSent, WSAGetLastError());
				g_RegReportTypes.dwErrors++;
			}
			else 
			{
				LogVerbose(TEXT("UDP Packet Number: %d sent"), i);
				g_RegReportTypes.dwUDPData += dwNumBytesSent;
			}
		}
	}

Cleanup:
	
	WSACleanup();
	LocalFree(pBuf);
	
	return false;
}


//
//
//
DWORD GetRasDeviceNum(LPTSTR lpszDeviceType, DWORD& dwDeviceNum)
{
	LPRASDEVINFO lpRasDevInfo = NULL;
	DWORD dwBufSize = 0;					  
	DWORD dwNumDevices = 0;						
	DWORD i;
	DWORD dwErr;

	dwDeviceNum = -1;

	// find the buffer size needed
	dwErr = RasEnumDevices(NULL, &dwBufSize, &dwNumDevices);
	if (!dwBufSize)
	{
		LogVerbose(TEXT("Unable to find \"RasEnumDevices\" buffer size: %d"),dwErr);

		return dwErr;
	}

	lpRasDevInfo = (LPRASDEVINFO)LocalAlloc(LPTR, dwBufSize);
	if (!lpRasDevInfo)
	{
		LogVerbose(TEXT("Couldn't allocate memory for \"RasEnumDevices\""));
		
		return ERROR_READING_DEVICENAME;
	}
	lpRasDevInfo->dwSize = sizeof(RASDEVINFO); 

	if (dwErr = RasEnumDevices(lpRasDevInfo, &dwBufSize, &dwNumDevices))
	{
		LogVerbose(TEXT("\"RasEnumDevices\" failed: %d"), dwErr);

		LocalFree(lpRasDevInfo);
		return dwErr;
	}

	//
	// Find the device which matches and return that device number
	//
	for (i = 0; i < dwNumDevices; ++i)
	{
		if (_tcsicmp(lpszDeviceType, (lpRasDevInfo + i)->szDeviceType) == 0)
		{
			if (eRasConnection == RAS_VPN_L2TP)
			{
				if (_tcsstr((lpRasDevInfo + i)->szDeviceName, TEXT("L2TP")))
				{
					//
					// The name of this line contains L2TP. This must be 
					// an L2TP line. Return this device number and break
					// 
					dwDeviceNum = i;
					break;
				}
				else
				{
					continue;
				}
			}
			else if (eRasConnection == RAS_VPN_PPTP)
			{
				dwDeviceNum = i;
				break;
			}
		}
	}
	
	if (dwDeviceNum < 0)
	{
		LogVerbose(TEXT("%lu devices available; no device of type %s in list"),
													dwNumDevices, lpszDeviceType);

		LocalFree(lpRasDevInfo);
		return ERROR_READING_DEVICETYPE;
	}

	LocalFree(lpRasDevInfo);
	return 0;
} 

//
//
//
DWORD RasLinkSetBaudrate(LPTSTR lpszEntryName, DWORD dwBaudrate, BYTE bParity,
  BYTE bWordLength, float fStopBits)
{
//	BYTE RasEntryBuf[ENTRYBUFSIZE];
	DWORD dwRasBufSize = ENTRYBUFSIZE;
	LPRASENTRY lpRasEntry;
	DWORD dwErr;
	BYTE DevConfigBuf[DEVBUFSIZE];
	DWORD dwDevConfigSize = DEVCONFIGSIZE;
	LPBYTE lpDevConfig = DevConfigBuf + DEVCONFIGOFFSET;
	LPVARSTRING lpVarString = (LPVARSTRING)&DevConfigBuf;
	HLINEAPP hLineApp;	
	HLINE hLine;
	LINEEXTENSIONID ExtensionID;
	DWORD dwDeviceNum, dwAPIVersion, dwNumDevs;
    UNIMDM_CHG_DEVCFG UCD;
	long lRet = 0;
	TCHAR szErr[80];
//	lpRasEntry = (LPRASENTRY)RasEntryBuf;
	lpRasEntry = (LPRASENTRY)LocalAlloc(LPTR, ENTRYBUFSIZE);
	if(lpRasEntry == NULL)
	{
		LogComment(TEXT("LocalAlloc() failed with %d"), GetLastError());
		return -1;
	}
	lpRasEntry->dwSize = sizeof(RASENTRY);

	// Retrieve device configuration:
	if (dwErr = RasGetEntryProperties(NULL, lpszEntryName, lpRasEntry, &dwRasBufSize,
	  lpDevConfig, &dwDevConfigSize))
	{
		LogVerbose(TEXT("Unable to read properties of entry \"%s\": %d"),
		  				lpszEntryName, dwErr);
		LocalFree(lpRasEntry);
		return dwErr;
	}

	if (dwDevConfigSize) // If device configuration already exists, use it
	{
		lpDevConfig = NULL;
		lpVarString->dwTotalSize = DEVBUFSIZE; // Fill in rest of VARSTRING structure
		lpVarString->dwNeededSize = DEVBUFSIZE;
		lpVarString->dwUsedSize = DEVBUFSIZE;
		lpVarString->dwStringFormat = STRINGFORMAT_BINARY;
		lpVarString->dwStringSize = DEVCONFIGSIZE;
		lpVarString->dwStringOffset = DEVCONFIGOFFSET;
	}	

	if (dwDeviceNum = GetUnimodemDevConfig(lpRasEntry->szDeviceName, lpVarString)
	  == 0xFFFFFFFF)
	{
		LogVerbose(TEXT("Couldn't get default device configuration"));
		LocalFree(lpRasEntry);
		return ERROR_CANNOT_SET_PORT_INFO;
	}
	lpDevConfig = DevConfigBuf + DEVCONFIGOFFSET;

	if (lRet = lineInitialize(&hLineApp, NULL, lineCallbackFunc, TEXT("OEMRasClient"),
	  &dwNumDevs))
	{
		LogVerbose(TEXT("Can't initialize TAPI: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));
		LocalFree(lpRasEntry);
		return ERROR_CANNOT_SET_PORT_INFO;
    }

	if (lRet = lineNegotiateAPIVersion(hLineApp, 0, 0x00010000, 0x00020001,
		  &dwAPIVersion, &ExtensionID))
	{
		LogVerbose(
		  TEXT("TAPI version negotiation failed: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));
		lineShutdown(hLineApp);
		LocalFree(lpRasEntry);
		return ERROR_CANNOT_SET_PORT_INFO;
	}

	if (lRet = lineOpen(hLineApp, dwDeviceNum, &hLine, dwAPIVersion, 0, 0,
	  LINECALLPRIVILEGE_NONE, LINEMEDIAMODE_DATAMODEM, NULL))
	{
		LogVerbose(
		  TEXT("Couldn't open TAPI device %lu: error %.8x (%s)"),
		  dwDeviceNum, lRet, FormatLineError(lRet, szErr));
		lineShutdown(hLineApp);
		LocalFree(lpRasEntry);
		return ERROR_CANNOT_SET_PORT_INFO;
	} 

	// Set up the lineDevSpecific data block:
	UCD.dwCommand = UNIMDM_CMD_CHG_DEVCFG;
	UCD.lpszDeviceClass = TEXT("comm/datamodem");
	UCD.lpDevConfig = lpVarString;
	dwErr = 0;

	// Set the baudrate:
	UCD.dwOption = UNIMDM_OPT_BAUDRATE;
	UCD.dwValue = dwBaudrate;

	// lineDevSpecific is an asynchronous function; however, there is no need to
	//   wait for the LINE_REPLY callback
	if ((lRet = lineDevSpecific(hLine, 0, NULL, &UCD, sizeof(UCD))) < 0)
	{
		LogVerbose(TEXT("Couldn't reset baudrate: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));
		dwErr = ERROR_CANNOT_SET_PORT_INFO;
	}
	
	// Set the parity:
	UCD.dwOption = UNIMDM_OPT_PARITY;
	switch (bParity)
	{
		case 'o':
		case 'O':
			UCD.dwValue = ODDPARITY;
			break;
		case 'e':
		case 'E':
			UCD.dwValue = EVENPARITY;
			break;
		case 'm':
		case 'M':
			UCD.dwValue = MARKPARITY;
			break;
		case 's':
		case 'S':
			UCD.dwValue = SPACEPARITY;
			break;
		case 'n':
		case 'N':
		default:
			UCD.dwValue = NOPARITY;
			break;
	}
	if ((lRet = lineDevSpecific(hLine, 0, NULL, &UCD, sizeof(UCD))) < 0)
	{
		LogVerbose(TEXT("Couldn't reset parity: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));
		dwErr = ERROR_CANNOT_SET_PORT_INFO;
	}

	// Set the wordlength:
	UCD.dwOption = UNIMDM_OPT_BYTESIZE;
	UCD.dwValue = (DWORD)bWordLength;

	if ((lRet = lineDevSpecific(hLine, 0, NULL, &UCD, sizeof(UCD))) < 0)
	{
		LogVerbose(TEXT("Couldn't reset wordlength: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));
		
		dwErr = ERROR_CANNOT_SET_PORT_INFO;
	}

	// Set the stopbits:
	UCD.dwOption = UNIMDM_OPT_STOPBITS;
	if (fStopBits == 1.5)
		UCD.dwValue = ONE5STOPBITS;
	else if (fStopBits == 2)
		UCD.dwValue = TWOSTOPBITS;
	else
		UCD.dwValue = ONESTOPBIT;

	if ((lRet = lineDevSpecific(hLine, 0, NULL, &UCD, sizeof(UCD))) < 0)
	{
		LogVerbose(TEXT("Couldn't reset stopbits: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));
	}

	if (lRet = lineClose(hLine))
		LogVerbose(TEXT("lineClose: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));

	if (lRet = lineShutdown(hLineApp))
		LogVerbose(TEXT("lineShutdown: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));
	lRet = (long)dwErr;

	if (dwErr = RasSetEntryProperties(NULL, lpszEntryName, lpRasEntry, dwRasBufSize,
	  lpDevConfig, dwDevConfigSize))
	{
		LogVerbose(TEXT("\"RasSetEntryProperties\" failed: %d"), dwErr);
		LocalFree(lpRasEntry);
		return dwErr;
	}
	
	LocalFree(lpRasEntry);
	return (DWORD)lRet;
}

//
//
//
DWORD GetRasDeviceName(LPTSTR lpszDeviceName, DWORD dwDeviceNum)
{
	LPRASDEVINFO lpRasDevInfo = NULL;
	DWORD dwBufSize = 0;
	DWORD dwNumDevices = 0;	

	DWORD dwErr;

	if (lpszDeviceName == NULL)
	{
		LogVerbose(TEXT("\"RasEnumDevices\": buffer error"));

		return ERROR_READING_DEVICENAME;
	}
	lpszDeviceName[0] = TEXT('\0');

	// find the buffer size needed
	dwErr = RasEnumDevices(NULL, &dwBufSize, &dwNumDevices);
	if (!dwBufSize)
	{
		LogVerbose(TEXT("Unable to find \"RasEnumDevices\" buffer size: %d"), dwErr);
		return dwErr;
	}

	lpRasDevInfo = (LPRASDEVINFO)LocalAlloc(LPTR, dwBufSize);
	if (!lpRasDevInfo)
	{
		LogVerbose(TEXT("Couldn't allocate memory for \"RasEnumDevices\""));

		return ERROR_READING_DEVICENAME;
	}
	lpRasDevInfo->dwSize = sizeof(RASDEVINFO); 

	if (dwErr = RasEnumDevices(lpRasDevInfo, &dwBufSize, &dwNumDevices))
	{
		LogVerbose(TEXT("\"RasEnumDevices\" failed: %d"), dwErr);

		LocalFree(lpRasDevInfo);
		return dwErr;
	}

	if (dwDeviceNum >= dwNumDevices)
	{
		LogVerbose(TEXT("%lu devices available; device %lu not in list"),
		  dwNumDevices, dwDeviceNum);

		LocalFree(lpRasDevInfo);
		return ERROR_READING_DEVICENAME;
	}

	_tcsncpy(lpszDeviceName, (lpRasDevInfo + dwDeviceNum)->szDeviceName,
	  RAS_MaxDeviceName);
	lpszDeviceName[RAS_MaxDeviceName] = TEXT('\0');

	LocalFree(lpRasDevInfo);
	return 0;
}

//
//
//
DWORD GetRasDeviceType(LPTSTR lpszDeviceType, DWORD dwDeviceNum)
{
	LPRASDEVINFO lpRasDevInfo = NULL;
	DWORD dwBufSize = 0;
	DWORD dwNumDevices = 0;
	DWORD dwErr;

	if (lpszDeviceType == NULL)
	{
		LogVerbose(TEXT("\"RasEnumDevices\": buffer error"));
		
		return ERROR_READING_DEVICENAME;
	}
	lpszDeviceType[0] = TEXT('\0');

    // find the buffer size needed
	dwErr = RasEnumDevices(NULL, &dwBufSize, &dwNumDevices);
	if (!dwBufSize)
	{
		LogVerbose(TEXT("Unable to find \"RasEnumDevices\" buffer size: %d"), dwErr);

		return dwErr;
	}

	lpRasDevInfo = (LPRASDEVINFO)LocalAlloc(LPTR, dwBufSize);
	if (!lpRasDevInfo)
	{
		LogVerbose(TEXT("Couldn't allocate memory for \"RasEnumDevices\""));
		
		return ERROR_READING_DEVICENAME;
	}
	lpRasDevInfo->dwSize = sizeof(RASDEVINFO); 

	if (dwErr = RasEnumDevices(lpRasDevInfo, &dwBufSize, &dwNumDevices))
	{
		LogVerbose(TEXT("\"RasEnumDevices\" failed: %d"), dwErr);

		LocalFree(lpRasDevInfo);
		return dwErr;
	}

	if (dwDeviceNum >= dwNumDevices)
	{
		LogVerbose(TEXT("%lu devices available; device %lu not in list"),
		  											dwNumDevices, dwDeviceNum);
		
		LocalFree(lpRasDevInfo);
		return ERROR_READING_DEVICENAME;
	}

	_tcsncpy(lpszDeviceType, (lpRasDevInfo + dwDeviceNum)->szDeviceType,
	  RAS_MaxDeviceType);
	lpszDeviceType[RAS_MaxDeviceType] = TEXT('\0');

	LocalFree(lpRasDevInfo);
	return 0;
}

//
//
//
DWORD GetUnimodemDeviceName(LPTSTR lpszDeviceName, LPTSTR lpszDeviceType)
{
	BYTE bDevCapsBuf[DEVCAPSSIZE];
	LPLINEDEVCAPS lpDevCaps = (LPLINEDEVCAPS)bDevCapsBuf;
	HLINEAPP hLineApp;
	LINEEXTENSIONID ExtensionID;
	BYTE bDeviceType = 0xFF;
	DWORD dwDeviceNum, dwAPIVersion, dwNumDevs;
	TCHAR szBuff[120];
	long lRet = 0;

	if (_tcsicmp(lpszDeviceType, TEXT("serial")) == 0)
	{
		_tcscpy(lpszDeviceType, TEXT("direct"));
		bDeviceType = UNIMODEM_SERIAL_DEVICE;
	}
	else if (_tcsicmp(lpszDeviceType, TEXT("direct")) == 0)
		bDeviceType = UNIMODEM_SERIAL_DEVICE;

	else if (_tcsicmp(lpszDeviceType, TEXT("external")) == 0)
	{
		_tcscpy(lpszDeviceType, TEXT("modem"));
		bDeviceType = UNIMODEM_HAYES_DEVICE;
	}
	else if (_tcsicmp(lpszDeviceType, TEXT("modem")) == 0)
		bDeviceType = UNIMODEM_HAYES_DEVICE;

	else if (_tcsicmp(lpszDeviceType, TEXT("PCMCIA")) == 0)
	{
		_tcscpy(lpszDeviceType, TEXT("modem"));
		bDeviceType = UNIMODEM_PCMCIA_DEVICE;
	}

	else if (_tcsicmp(lpszDeviceType, TEXT("infrared")) == 0)
	{
		_tcscpy(lpszDeviceType, TEXT("direct"));
		bDeviceType = UNIMODEM_IR_DEVICE;
	}

	if (bDeviceType == 0xFF)
	{
		LogVerbose(TEXT("Device type %s not recognized"), lpszDeviceType);
		return FALSE;
    }

	if (lRet = lineInitialize(&hLineApp, NULL, lineCallbackFunc, TEXT("OEMRasClient"),
	  &dwNumDevs))
	{
		LogVerbose(TEXT("Can't initialize TAPI: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szBuff));
		return 0xFFFFFFFF;
    }

	for (dwDeviceNum = 0; dwDeviceNum < dwNumDevs; dwDeviceNum++)
	{
		if (lRet = lineNegotiateAPIVersion(hLineApp, dwDeviceNum, 0x00010000,
		  0x00020001, &dwAPIVersion, &ExtensionID))
		{
			LogVerbose(
			  TEXT("TAPI version negotiation failed for device %lu: error %.8x (%s)"),
			  dwDeviceNum, lRet, FormatLineError(lRet, szBuff));
			continue;
		}

		lpDevCaps->dwTotalSize = DEVCAPSSIZE;
		if (lRet = lineGetDevCaps(hLineApp, dwDeviceNum, dwAPIVersion, 0,
		  lpDevCaps))
			LogVerbose(
			  TEXT("Couldn't read TAPI device %lu capabilities: error %.8x (%s)"),
			  dwDeviceNum, lRet, FormatLineError(lRet, szBuff));

		else
		{
			_tcsncpy(lpszDeviceName, (LPTSTR)(bDevCapsBuf + lpDevCaps->dwLineNameOffset),
			  RAS_MaxDeviceName);
			lpszDeviceName[RAS_MaxDeviceName] = TEXT('\0');

			// check device name if intended to make a null modem connection
			if (bDeviceType == UNIMODEM_SERIAL_DEVICE)
			{
				if (_tcsnicmp(lpszDeviceName, TEXT("Serial Cable"), 12) == 0)
					break;
				else if (_tcsnicmp(lpszDeviceName, TEXT("SER2410 UNIMODEM"), 16) == 0)
					break;
			}
			else
				break;
		}
	}

	if (dwDeviceNum == dwNumDevs)
	{
		LogVerbose(TEXT("No matching Unimodem device found"));
		dwDeviceNum = 0xFFFFFFFF;
	}

	if (lRet = lineShutdown(hLineApp))
		LogVerbose(TEXT("lineShutdown: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szBuff));
	return dwDeviceNum;
}

//
//
//
DWORD GetUnimodemDevConfig(LPTSTR lpszDeviceName, LPVARSTRING lpDevConfig)
{
	BYTE bDevCapsBuf[1024];
	LPLINEDEVCAPS lpDevCaps = (LPLINEDEVCAPS)bDevCapsBuf;
	HLINEAPP hLineApp;
	LINEEXTENSIONID ExtensionID;
	DWORD dwDeviceNum, dwAPIVersion, dwNumDevs;
	TCHAR szBuff[120];
	long lRet = 0;

	if (lRet = lineInitialize(&hLineApp, NULL, lineCallbackFunc, TEXT("OEMRasClient"),
	  &dwNumDevs))
	{
		LogVerbose(TEXT("Can't initialize TAPI: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szBuff));
		return 0xFFFFFFFF;
    }

	for (dwDeviceNum = 0; dwDeviceNum < dwNumDevs; dwDeviceNum++)
	{
		if (lRet = lineNegotiateAPIVersion(hLineApp, dwDeviceNum, 0x00010000,
		  0x00020001, &dwAPIVersion, &ExtensionID))
		{
			LogVerbose(
			  TEXT("TAPI version negotiation failed for device %lu: error %.8x (%s)"),
			  dwDeviceNum, lRet, FormatLineError(lRet, szBuff));
			continue;
		}

		lpDevCaps->dwTotalSize = DEVCAPSSIZE;
		if (lRet = lineGetDevCaps(hLineApp, dwDeviceNum, dwAPIVersion, 0,
		  lpDevCaps))
		{
			LogVerbose(
			  TEXT("Couldn't read TAPI device %lu capabilities: error %.8x (%s)"),
			  dwDeviceNum, lRet, FormatLineError(lRet, szBuff));
			continue;
		}

		if ((lpszDeviceName == NULL) ||
		  (_tcscmp((LPTSTR)(bDevCapsBuf + lpDevCaps->dwLineNameOffset), lpszDeviceName) == 0))
		{
			if (_tcsicmp((LPTSTR)(bDevCapsBuf + lpDevCaps->dwProviderInfoOffset),
			  TEXT("UNIMODEM")) != 0)
			{
				LogVerbose(TEXT("TAPI device %lu isn't Unimodem"),
				  dwDeviceNum);
				dwDeviceNum = 0xFFFFFFFF;
			}
			else if (lpDevConfig != NULL)
			{
				lpDevConfig->dwTotalSize = DEVBUFSIZE;

				if (lRet = lineGetDevConfig(dwDeviceNum, lpDevConfig,
				  TEXT("comm/datamodem")))
				{
					LogVerbose(
					  TEXT("Couldn't read TAPI device %lu configuration: error %.8x (%s)"),
					  dwDeviceNum, lRet, FormatLineError(lRet, szBuff));
					dwDeviceNum = 0xFFFFFFFF;
				}
			}
			break;
		}
	}

	if (lRet = lineShutdown(hLineApp))
		LogVerbose(TEXT("lineShutdown: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szBuff));

	if ((dwDeviceNum >= dwNumDevs) && (dwDeviceNum < 0xFFFFFFFF))
	{
		dwDeviceNum = 0xFFFFFFFF;
	}
	
	return dwDeviceNum;
}

//
//
//
void CALLBACK lineCallbackFunc(DWORD dwDevice, DWORD dwMsg,
  DWORD dwCallbackInstance, DWORD dwParam1, DWORD dwParam2, DWORD dwParam3)
{
	TCHAR szBuff[1024];

	LogVerbose(TEXT("TAPI callback function: %s"),
	  FormatLineCallback(szBuff, dwDevice, dwMsg, dwCallbackInstance,
	  dwParam1, dwParam2, dwParam3));
}

DWORD PingLink(char *lpszHostname, DWORD dwNumPings)
{
	DWORD errorCode = 0;

	const DWORD TimeOut = 3000;

	WSADATA WsaData;
	HANDLE IcmpHandle;
	struct hostent *hostp = NULL;
    IPAddr address = 0;
	DWORD numberOfReplies;
	DWORD dwTotalNumReplies;
	DWORD dwTotalNumTries;
	DWORD i;
    DWORD err;
    struct in_addr addr;
	PICMP_ECHO_REPLY reply;
	char SendBuffer[SENDSIZE];
	char RcvBuffer[RCVSIZE];
	TCHAR szReplyString[256];

	strcpy(SendBuffer, "abcdefghijklmnopqrstuvwxyz!!@@@");

	err = WSAStartup(0x0101, &WsaData);
	if (err)
	{
		errorCode = GetLastError();
		LogVerbose(TEXT("PingLink: WSAStartup failed, error %lu: - aborting"), errorCode);
		return errorCode;
	}

	hostp = gethostbyname(lpszHostname);
	if (hostp)
		address = *(long *)hostp->h_addr;
	else
	{
		LogComment(TEXT("PingLink: could not get IP address for \"%hs\" - aborting"), lpszHostname);
		g_RegReportTypes.dwErrors++;
		return IP_BAD_DESTINATION;
	}
	addr.s_addr = address;

	if ((IcmpHandle = IcmpCreateFile()) == INVALID_HANDLE_VALUE)
	{
		return errorCode;
	}

	// Allow 4 * dwNumPings tries to get dwNumPings
	// Ping fails if less than dwNumPings of reply comes back for 4 * dwNumPings request
	dwTotalNumTries = 0;
	dwTotalNumReplies = 0;
	for (i = 0; i < 4 * dwNumPings; i ++)
	{
		errorCode = 0;
		numberOfReplies = IcmpSendEcho(	IcmpHandle, 
										address, 
										SendBuffer, 
										SENDSIZE, 
										NULL,
								 	    RcvBuffer, 
								 	    RCVSIZE, 
								 	    TimeOut);

		dwTotalNumTries++;

		if (numberOfReplies == 0)
		{
			errorCode = GetLastError();
			LogComment(TEXT("PingLink: transmit failed, error %lu"), errorCode);
		}
		else
		{
			reply = (PICMP_ECHO_REPLY)RcvBuffer;

			while (numberOfReplies--)
			{
				dwTotalNumReplies ++;
				addr.S_un.S_addr = reply->Address;

				_stprintf(szReplyString, TEXT("PingLink: Reply from %hs:"), inet_ntoa(addr));

				if (reply->Status == IP_SUCCESS)
				{
					_stprintf(szReplyString + _tcslen(szReplyString),
					  TEXT("Echo size=%d "), reply->DataSize);

					if (reply->RoundTripTime)
						_stprintf(szReplyString + _tcslen(szReplyString),
						  TEXT("time=%lums "), reply->RoundTripTime);
					else
						_stprintf(szReplyString + _tcslen(szReplyString),
						  TEXT("time<10ms "));

					_stprintf(szReplyString + _tcslen(szReplyString),
					  TEXT("TTL=%u\n"), (UINT)reply->Options.Ttl);

					LogVerbose(szReplyString);
				}
				else
				{
					errorCode = reply->Status;
					_stprintf(szReplyString + _tcslen(szReplyString),
					  TEXT("Error %lu\n"), errorCode);
					LogVerbose(szReplyString);
				}
				reply++;
			}	
		}

		if (dwTotalNumReplies >= dwNumPings)
		{
			errorCode = 0;
			break;
		}
	}

	if (dwTotalNumReplies < dwNumPings)
	{
		LogVerbose(TEXT("Got only %ul of reply for %ul tries"), dwTotalNumReplies, 4 * dwNumPings);
		g_RegReportTypes.dwErrors += (4 * dwNumPings - dwTotalNumReplies);
		errorCode = 0xffffffff;
	}

	g_RegReportTypes.dwICMPPings += dwTotalNumReplies;
	IcmpCloseHandle(IcmpHandle);
	return errorCode;
}

//
// GetRasStats()
//
bool GetRasStats()
{
	RASCONN RasConnBuf[10];
	DWORD dwRasConnBufSize;
	DWORD dwConnections;
	DWORD dwErr;
	DWORD i;
	RAS_STATS RasLinkStats;

	RasConnBuf[0].dwSize = sizeof(RASCONN);
	dwRasConnBufSize = sizeof(RasConnBuf);

	if (dwErr = RasEnumConnections(RasConnBuf, &dwRasConnBufSize, &dwConnections))
	{
		LogVerbose(TEXT("\"RasEnumConnections\" error: %d\nFailed to get RAS_STATS"), dwErr);
		
		return false;
	}
	
	LogVerbose(TEXT("%lu open RAS connection(s)"), dwConnections);

	//
	// Get Stats for the active RAS connections
	//
	for (i = 0; i < dwConnections; ++i)
	{
		//
		// Get stats for RasConnBuf[i].hrasconn
		//
		RasLinkStats.dwSize=sizeof(RAS_STATS);
		dwErr = RasGetLinkStatistics(RasConnBuf[i].hrasconn, 0, &RasLinkStats);
		if (dwErr != ERROR_SUCCESS)
		{
			LogVerbose(TEXT("RasGetLinkStatistics failed with %d\n"), dwErr);
			continue;
		}
		else
		{
			//
			// Update the variables with the statistics obtained.
			//
			g_RegReportTypes.dwBytesXmited += RasLinkStats.dwBytesXmited;
			g_RegReportTypes.dwBytesRcved += RasLinkStats.dwBytesRcved;
			g_RegReportTypes.dwFramesXmited += RasLinkStats.dwFramesXmited;
			g_RegReportTypes.dwFramesRcved += RasLinkStats.dwFramesRcved;
			g_RegReportTypes.dwCrcErr += RasLinkStats.dwCrcErr;
			g_RegReportTypes.dwTimeoutErr += RasLinkStats.dwTimeoutErr;
			g_RegReportTypes.dwAlignmentErr += RasLinkStats.dwAlignmentErr;
			g_RegReportTypes.dwHardwareOverrunErr += RasLinkStats.dwHardwareOverrunErr;
			g_RegReportTypes.dwFramingErr += RasLinkStats.dwFramingErr;
			g_RegReportTypes.dwBufferOverrunErr += RasLinkStats.dwBufferOverrunErr;

			DISPLAY_REPORT_REG_VALUES();
		}
	}
	
	return true;
}

// 
// Write results in g_RegReportTypes to registry
//
bool WriteRegValues()
{
	DWORD dwResult = 0;
	HKEY hKey;
	DWORD dwDisp;
	LPBYTE lpData;
	DWORD cbData;
	LPWSTR lpKeyClass;

	DISPLAY_REPORT_REG_VALUES();
	WHICH_KEY_TO_USE();
	
	dwResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, lpKeyClass, 0, NULL, 0, 0, NULL, &hKey, &dwDisp);
	if (ERROR_SUCCESS == dwResult)
	{
		lpData = (LPBYTE)&g_RegReportTypes.dwConnects;
		cbData = sizeof(g_RegReportTypes.dwConnects);
		RegSetValueEx(hKey, CONNECTS_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwDisconnects;
		cbData = sizeof(g_RegReportTypes.dwDisconnects);
		RegSetValueEx(hKey, DISCONNECTS_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwErrors;
		cbData = sizeof(g_RegReportTypes.dwErrors);
		RegSetValueEx(hKey, ERRORS_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwTCPData;
		cbData = sizeof(g_RegReportTypes.dwTCPData);
		RegSetValueEx(hKey, TCP_DATA_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwUDPData;
		cbData = sizeof(g_RegReportTypes.dwUDPData);
		RegSetValueEx(hKey, UDP_DATA_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwICMPPings;
		cbData = sizeof(g_RegReportTypes.dwICMPPings);
		RegSetValueEx(hKey, ICMP_PINGS_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwBytesXmited;
		cbData = sizeof(g_RegReportTypes.dwConnects);
		RegSetValueEx(hKey, BYTES_XMITTED_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwBytesRcved;
		cbData = sizeof(g_RegReportTypes.dwBytesRcved);
		RegSetValueEx(hKey, BYTES_RECVD_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwFramesRcved;
		cbData = sizeof(g_RegReportTypes.dwFramesRcved);
		RegSetValueEx(hKey, FRAMES_RECVD_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwFramesXmited;
		cbData = sizeof(g_RegReportTypes.dwFramesXmited);
		RegSetValueEx(hKey, FRAMES_XMITED_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwCrcErr;
		cbData = sizeof(g_RegReportTypes.dwCrcErr);
		RegSetValueEx(hKey, CRC_ERR_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwTimeoutErr;
		cbData = sizeof(g_RegReportTypes.dwTimeoutErr);
		RegSetValueEx(hKey, TIMEOUT_ERR_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwAlignmentErr;
		cbData = sizeof(g_RegReportTypes.dwAlignmentErr);
		RegSetValueEx(hKey, ALIGNMENT_ERR_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwHardwareOverrunErr;
		cbData = sizeof(g_RegReportTypes.dwHardwareOverrunErr);
		RegSetValueEx(hKey, HARDWARE_OVERRUN_ERR_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwFramingErr;
		cbData = sizeof(g_RegReportTypes.dwFramingErr);
		RegSetValueEx(hKey, FRAMING_ERR_REG, 0, REG_DWORD, lpData, cbData);

		lpData = (LPBYTE)&g_RegReportTypes.dwBufferOverrunErr;
		cbData = sizeof(g_RegReportTypes.dwBufferOverrunErr);
		RegSetValueEx(hKey, BUFFER_OVERRUN_ERR_REG, 0, REG_DWORD, lpData, cbData);		

		RegCloseKey(hKey);
	}
	else
	{
		return false;
	}

	return true;

}

//
// At the start of every iteration update the values reported already
// from the registry. This ensures that we continue with what we reported
// and don't start from 0 every time.
//
bool InitReportValuesFromReg()
{
	DWORD dwResult = 0;
	HKEY hKey;
	DWORD dwType;
	LPWSTR lpKeyClass;
	LPBYTE lpData;
	DWORD cbData;

	WHICH_KEY_TO_USE();

	dwResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpKeyClass, 0, 0, &hKey);
	if (ERROR_SUCCESS == dwResult)
	{
		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwConnects;
		cbData = sizeof(g_RegReportTypes.dwConnects);
		RegQueryValueEx(hKey, CONNECTS_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwDisconnects;
		cbData = sizeof(g_RegReportTypes.dwDisconnects);
		RegQueryValueEx(hKey, DISCONNECTS_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwErrors;
		cbData = sizeof(g_RegReportTypes.dwErrors);
		RegQueryValueEx(hKey, ERRORS_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwTCPData;
		cbData = sizeof(g_RegReportTypes.dwTCPData);
		RegQueryValueEx(hKey, TCP_DATA_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwUDPData;
		cbData = sizeof(g_RegReportTypes.dwUDPData);
		RegQueryValueEx(hKey, UDP_DATA_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwICMPPings;
		cbData = sizeof(g_RegReportTypes.dwICMPPings);
		RegQueryValueEx(hKey, ICMP_PINGS_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwBytesXmited;
		cbData = sizeof(g_RegReportTypes.dwConnects);
		RegQueryValueEx(hKey, BYTES_XMITTED_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwBytesRcved;
		cbData = sizeof(g_RegReportTypes.dwBytesRcved);
		RegQueryValueEx(hKey, BYTES_RECVD_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwFramesRcved;
		cbData = sizeof(g_RegReportTypes.dwFramesRcved);
		RegQueryValueEx(hKey, FRAMES_RECVD_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwFramesXmited;
		cbData = sizeof(g_RegReportTypes.dwFramesXmited);
		RegQueryValueEx(hKey, FRAMES_XMITED_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwCrcErr;
		cbData = sizeof(g_RegReportTypes.dwCrcErr);
		RegQueryValueEx(hKey, CRC_ERR_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwTimeoutErr;
		cbData = sizeof(g_RegReportTypes.dwTimeoutErr);
		RegQueryValueEx(hKey, TIMEOUT_ERR_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwAlignmentErr;
		cbData = sizeof(g_RegReportTypes.dwAlignmentErr);
		RegQueryValueEx(hKey, ALIGNMENT_ERR_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwHardwareOverrunErr;
		cbData = sizeof(g_RegReportTypes.dwHardwareOverrunErr);
		RegQueryValueEx(hKey, HARDWARE_OVERRUN_ERR_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwFramingErr;
		cbData = sizeof(g_RegReportTypes.dwFramingErr);
		RegQueryValueEx(hKey, FRAMING_ERR_REG, 0, &dwType, lpData, &cbData);

		dwType = 0;
		lpData = (LPBYTE)&g_RegReportTypes.dwBufferOverrunErr;
		cbData = sizeof(g_RegReportTypes.dwBufferOverrunErr);
		RegQueryValueEx(hKey, BUFFER_OVERRUN_ERR_REG, 0, &dwType, lpData, &cbData);	

		RegCloseKey(hKey);
	}
	else
	{
		//
		// Reg does not exist. Init all variables to 0.
		//
		g_RegReportTypes.dwBytesXmited = 0;
		g_RegReportTypes.dwBytesRcved = 0;
		g_RegReportTypes.dwFramesXmited = 0;
		g_RegReportTypes.dwFramesRcved = 0;
		g_RegReportTypes.dwCrcErr = 0;
		g_RegReportTypes.dwTimeoutErr = 0;
		g_RegReportTypes.dwAlignmentErr = 0;
		g_RegReportTypes.dwHardwareOverrunErr = 0;
		g_RegReportTypes.dwFramingErr = 0;
		g_RegReportTypes.dwBufferOverrunErr = 0;
	}

	DISPLAY_REPORT_REG_VALUES();
	return true;
}

//
// DetermineRasLinkToRunStress()
//
bool DetermineRasLinkToRunStress()
{
	//
	// Make sure we call RasHangUp for every RasDial call, even if there was a failure
	//
	DisconnectRasConnection();
	
	//
	// Sequence: PPP(DCC)->Modem->PPPoE->PPTP->L2TP/IPSec
	//
	eRasConnection = RAS_DCC_MODEM;
	_stprintf(strEntryName, TEXT("Serial @ %d"), dwBaudrate);

	if (false == CreatePhoneBookEntry()) 
	{
		LogComment(TEXT("Can not create Entry: %s\n"), strEntryName);
		return false;
	}

	// Connect
	if (false ==  ConnectRasConnection()) 
	{
		LogComment(TEXT("Can not Connect over DCC using Entry: %s\n"), strEntryName);
		DisconnectRasConnection();
	}
	else 
	{
		LogVerbose(TEXT("+++ Connected Successfully over DCC. +++"));
		LogVerbose(TEXT("Using Entry %s and connecting over DCC"), strEntryName);

		return true;
	}

	eRasConnection = RAS_PPP_MODEM;	
	_stprintf(strEntryName, TEXT("MODEM @ %s"), strPhoneNum);

	if (false == CreatePhoneBookEntry()) 
	{
		LogComment(TEXT("Can not create Entry: %s\n"), strEntryName);
		return false;
	}

	// Connect
	if (false ==  ConnectRasConnection()) 
	{
		LogComment(TEXT("Can not Connect to %s using Entry: %s\n"), strPhoneNum, strEntryName);
		DisconnectRasConnection();
	}
	else 
	{
		LogVerbose(TEXT("+++ Connected Successfully over modem to %s. +++"), strPhoneNum);
		LogVerbose(TEXT("Using Entry %s and connecting over Modem"), strEntryName);

		return true;
	}

	eRasConnection = RAS_PPP_PPPoE;
	_stprintf(strEntryName, TEXT("PPPOE"));

	if (false == CreatePhoneBookEntry()) 
	{
		LogComment(TEXT("Can not create Entry: %s\n"), strEntryName);
		return false;
	}

	// Connect
	if (false ==  ConnectRasConnection()) 
	{
		LogComment(TEXT("Can not Connect to PPPoE using Entry: %s\n"), strEntryName);
		DisconnectRasConnection();
	}
	else 
	{
		LogVerbose(TEXT("+++ Connected Successfully over PPPoE. +++"));
		LogVerbose(TEXT("Using Entry %s and connecting over PPPoE"), strEntryName);

		return true;
	}

	eRasConnection = RAS_VPN_PPTP;	
	_stprintf(strEntryName, TEXT("PPTP @ %s"), strPhoneNum);

	if (false == CreatePhoneBookEntry()) 
	{
		LogComment(TEXT("Can not create Entry: %s\n"), strEntryName);
		return false;
	}

	// Connect
	if (false ==  ConnectRasConnection()) 
	{
		LogComment(TEXT("Can not Connect to Server: %s using Entry: %s\n"), strPhoneNum, strEntryName);
		DisconnectRasConnection();
	}
	else 
	{
		LogVerbose(TEXT("+++ Connected Successfully over VPN (PPTP). +++"));
		LogVerbose(TEXT("Using Entry %s, server %s, and connecting over PPTP"), strEntryName, strPhoneNum);

		return true;
	}

	eRasConnection = RAS_VPN_L2TP;
	_stprintf(strEntryName, TEXT("L2TP @ %s"), strPhoneNum);

	if (false == CreatePhoneBookEntry()) 
	{
		LogComment(TEXT("Can not create Entry: %s\n"), strEntryName);
		return false;
	}
	
	// Connect
	if (false ==  ConnectRasConnection()) 
	{
		LogComment(TEXT("Can not Connect to Server: %s using Entry: %s\n"), strPhoneNum, strEntryName);
		DisconnectRasConnection();
	}
	else 
	{
		LogVerbose(TEXT("+++ Connected Successfully over VPN (L2TP). +++"));
		LogVerbose(TEXT("Using Entry %s, server %s, and connecting over L2TP"), strEntryName, strPhoneNum);

		return true;
	}

	return false;
}
