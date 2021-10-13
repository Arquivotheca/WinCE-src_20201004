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
// exported by OEMRasModem.dll:
//
//		1) InitializeStressModule
//		2) DoStressIteration
//		3) TerminateStressModule
//
// These functions are invoked by stressmod.exe to run CE Stress
//

#include "OEMRasStress.h"

BOOL InitializeStressModule (
							 /*[in]*/ MODULE_PARAMS* pmp, 
							 /*[out]*/ UINT* pnThreads
							 )
{
	DWORD	argc;
	DWORD	nRetries = DEFAULT_RETRIES;
	TCHAR	strUserCmdLine[MAX_CMD_LINE];
	TCHAR	strToken[MAX_PARAMS];
	TCHAR	strRASConnectionType[MAX_PARAMS];

	// Set this value to indicate the number of threads
	// that you want your module to run.  The module container
	// that loads this DLL will manage the life-time of these
	// threads.  Each thread will call your DoStressIteration()
	// function in a loop.

	*pnThreads = 1;

	InitializeStressUtils (
		TEXT("OEMRasModemStress"),
		LOGZONE(SLOG_SPACE_NET, SLOG_RAS),	
		pmp
		);


	// Any module-specific initialization here
	LogComment(TEXT("User Command line: %s\n"), pmp->tszUser);

	// Process command line parameters and set global variables
	_tcscpy(strUserCmdLine, pmp->tszUser);
	argc = NumParams(pmp->tszUser);

	_tcscpy(strToken, _tcstok(strUserCmdLine, L" \0\n"));
	while (argc > 0) {
		//
		// Data transfer?
		//
		if (!_tcsicmp(strToken, L"-d")) 
		{
			_tcscpy(strToken, _tcstok(NULL, L" \0\n"));

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
		//
		// RAS connection Type
		//
		else if (!_tcsicmp(strToken, L"-t")) 
		{
			_tcscpy(strToken, _tcstok(NULL, L" \0\n"));

			_tcscpy(strRASConnectionType, strToken);

			//
			// What type of RAS connection? Default PPTP.
			//
			if (!_tcsicmp(strToken, TEXT("l2tp")))
			{
				eRasConnection = RAS_VPN_L2TP;
			}
			else if (!_tcsicmp(strToken, TEXT("null")))
			{
				eRasConnection = RAS_DCC_MODEM;
			}
			else if (!_tcsicmp(strToken, TEXT("modem")))
			{
				eRasConnection = RAS_PPP_MODEM;
			}
			else if (!_tcsicmp(strToken, TEXT("pppoe")))
			{
				eRasConnection = RAS_PPP_PPPoE;
			}
			else
			{
				// Use PPTP
			}

			argc--;
		}
		//
		// Baudrate for modem or DCC connections
		//
		else if (!_tcsicmp(strToken, L"-b")) 
		{
			_tcscpy(strToken, _tcstok(NULL, L" \0\n"));

			dwBaudrate = _ttol(strToken);
			LogComment(TEXT("Baudrate: %d"), dwBaudrate);
			argc--;
		}
		//
		// Server phone number
		//
		else if (!_tcsicmp(strToken, L"-s")) 
		{
			_tcscpy(strToken, _tcstok(NULL, L" \0\n"));

			_tcsncpy(strPhoneNum, strToken, MAX_USER_NAME_PW);
			LogComment(TEXT("Server: %s"), strPhoneNum);
			argc--;
		}
		//
		// Number of packets (of size 1024) sent per iteration
		//
		else if (!_tcsicmp(strToken, L"-n")) 
		{
			_tcscpy(strToken, _tcstok(NULL, L" \0\n"));

			dwNumSends = _ttol(strToken);
			argc--;
		}
		else 
		{
			LogComment(TEXT("Unknown Command Line parameter: %s"), strToken);
			break;
		}
		
		if (argc == 1) break;
		_tcscpy(strToken, _tcstok(NULL, L" \0\n"));

		argc--;
	}

	//
	// Entry Name?
	//
	if (eRasConnection == RAS_VPN_L2TP)
	{
		_stprintf(strEntryName, TEXT("L2TP @ %s"), strPhoneNum);
	}
	else if (eRasConnection == RAS_VPN_PPTP)
	{
		_stprintf(strEntryName, TEXT("PPTP @ %s"), strPhoneNum);
	}
	else if (eRasConnection == RAS_DCC_MODEM)
	{
		_stprintf(strEntryName, TEXT("Serial @ %d"), dwBaudrate);
	}
	else if (eRasConnection == RAS_PPP_MODEM)
	{
		_stprintf(strEntryName, TEXT("MODEM @ %s"), strPhoneNum);
	}
	else if (eRasConnection == RAS_PPP_PPPoE)
	{
		_stprintf(strEntryName, TEXT("PPPOE"));
	}

	return TRUE;
}

UINT DoStressIteration (
						/*[in]*/ HANDLE hThread, 
						/*[in]*/ DWORD ThreadID, 
						/*[in,out]*/ LPVOID pv /*unused*/)
{
	//
	// We want only one module to be running at any one time
	// Since there are going to be multiple modules using 
	// one I/O resource (modem), we will use mutex'es for 
	// mutual exclusion
	//
	bool bStatus = false;
	HANDLE	hRasServerMutex;
	TCHAR	szMutexID[] = L"OEMRasModemStressMutex";

	hRasServerMutex = CreateMutex(NULL, TRUE, szMutexID);
	WaitForSingleObject(hRasServerMutex, INFINITE);

	// Do the tests here.
	LogComment(TEXT("Running RAS Stress for Entry: %s"), strEntryName);
	bStatus = RunConnectStress();

	ReleaseMutex(hRasServerMutex);
	CloseHandle(hRasServerMutex);

	return bStatus? CESTRESS_PASS: CESTRESS_FAIL;
}

DWORD TerminateStressModule (void)
{
	// TODO:  Do test-specific clean-up here.


	// This will be used as the process exit code.
	// It is not currently used by the harness.

	return ((DWORD) -1);
}

BOOL WINAPI DllMain(
					HANDLE hInstance, 
					ULONG dwReason, 
					LPVOID lpReserved
					)
{
	return TRUE;
}


//
// Returns the number of parameters in the user command line
// If the command line is: -s kagha01 -s6 kagha02 -h -c, value returned 
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

	LogComment(TEXT("Number of Arguments = %d\n"), len);
	return(len);
}

//
// Sequence: Create an entry name, connect to it, disconnect.
//
bool RunConnectStress() 
{
	// Create a phone book entry for this connection
	if (false == CreatePhoneBookEntry()) 
	{
		LogFail(TEXT("Can not create Entry: %s\n"), strEntryName);
		return false;
	}

	// Connect
	if (false ==  ConnectRasConnection()) 
	{
		LogFail(TEXT("Can not Connect using Entry: %s\n"), strEntryName);

		return false;
	}
	else 
	{
		LogComment(TEXT("+++ Connected Successfully. +++"));
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
	
	// Disconnect
	if (false == DisconnectRasConnection())
	{
		LogFail(TEXT("Can not Disconnect using Entry: %s\n"), strEntryName);

		return false;
	}
	else 
	{
		LogComment(TEXT("--- Disconnected Successfully ---"));
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
	BYTE RasEntryBuf[ENTRYBUFSIZE];
	DWORD dwRasBufSize = sizeof(RasEntryBuf);
	LPRASENTRY lpRasEntry;					  
	RASDIALPARAMS DialParams;						
	DWORD dwErr;									
	BYTE DevConfigBuf[DEVBUFSIZE];
	DWORD dwDevConfigSize;
	LPBYTE lpDevConfig = DevConfigBuf + DEVCONFIGOFFSET;
	LPVARSTRING lpVarString = (LPVARSTRING)&DevConfigBuf;

	lpRasEntry = (LPRASENTRY)RasEntryBuf;
	lpRasEntry->dwSize = sizeof(RASENTRY);
	DialParams.dwSize = sizeof(RASDIALPARAMS);

	lpRasEntry->dwSize = sizeof(RASENTRY);
	if (dwErr = RasGetEntryProperties(NULL, TEXT(""), lpRasEntry, &dwRasBufSize,
	  NULL, NULL))
	{
		LogWarn1(TEXT("Unable to create default RASENTRY structure: %d"), dwErr);

		return false;
	}

	// Fill in default parameters:
	_tcscpy(DialParams.szUserName, strUserName);
	DialParams.szPhoneNumber[0] = TEXT('\0');
	DialParams.szCallbackNumber[0] = TEXT('\0');
	_tcscpy(DialParams.szPassword, strPassword);
	_tcscpy(DialParams.szDomain, strDomain);

	lpRasEntry->dwfOptions =  RASEO_ProhibitPAP | RASEO_ProhibitEAP;
	lpRasEntry->dwCountryCode = 1;
	lpRasEntry->szAreaCode[0] = TEXT('\0');
	_tcscpy(lpRasEntry->szLocalPhoneNumber, strPhoneNum);
	lpRasEntry->dwfNetProtocols = RASNP_Ip;
	lpRasEntry->dwFramingProtocol = RASFP_Ppp;
	lpRasEntry->dwCustomAuthKey = 0;

	//
	// Get a device number
	//
	if (eRasConnection == RAS_VPN_L2TP || eRasConnection == RAS_VPN_PPTP)
	{
		if (dwErr = GetRasDeviceNum(RASDT_Vpn, dwDeviceNum))
			return false;
		
		//
		// Get Device name and type
		//
		if (dwErr = GetRasDeviceName(lpRasEntry->szDeviceName, dwDeviceNum))
			return false;
	
		if (dwErr = GetRasDeviceType(lpRasEntry->szDeviceType, dwDeviceNum))
			return false;
	}
	else if (eRasConnection == RAS_DCC_MODEM)
	{
		if (dwErr = GetRasDeviceNum(RASDT_Direct, dwDeviceNum))
			return false;	

		if ((dwDeviceNum = GetUnimodemDeviceName(lpRasEntry->szDeviceName,
											TEXT("direct"))) == 0xFFFFFFFF)
		{
			LogWarn1(TEXT("Unable to find default serial device"));

			return false;
		}
	}
	else if (eRasConnection == RAS_PPP_MODEM)
	{
		if (dwErr = GetRasDeviceNum(RASDT_Modem, dwDeviceNum))
			return false;	
	}
	else if (eRasConnection == RAS_PPP_PPPoE)
	{
		if (dwErr = GetRasDeviceNum(RASDT_PPPoE , dwDeviceNum))
			return false;	
	}

	// Check for old entry with same name and delete it if found:
	if (dwErr = RasValidateEntryName(NULL, strEntryName))
	{
		if (dwErr != ERROR_ALREADY_EXISTS)
		{
			LogWarn1(TEXT("Entry \"%s\" name validation failed: %d"),
									strEntryName, dwErr);

			return false;
		}

		if (dwErr = RasDeleteEntry(NULL, strEntryName))
		{
			LogWarn1(TEXT("Unable to delete old entry: %d"),dwErr);

			return false;
		}
	}

	_tcscpy(DialParams.szEntryName, strEntryName);

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
		LogWarn1(TEXT("\"RasSetEntryProperties\" failed: %s"), dwErr);

		return false;
	}

	if (dwErr = RasSetEntryDialParams(NULL, &DialParams, FALSE))
	{
		LogWarn1(TEXT("\"RasSetEntryDialParams\" failed: %s"),dwErr);

		return false;
	}

	//
	// Set baudrate for DCC or modem connections only
	//
	if (eRasConnection == RAS_DCC_MODEM || eRasConnection == RAS_PPP_MODEM)
	{
		if (dwErr = RasLinkSetBaudrate(strEntryName, dwBaudrate, 'N', 8, 1))
		{
			return false;
		}
	}

	//
	// If still here, then everything else passed
	// 
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
	_tcscpy(DialParams.szEntryName, strEntryName);
	if (dwRet = RasGetEntryDialParams(NULL, &DialParams, &bPasswordSaved))
	{
		LogWarn1(TEXT("Error reading dialing parameters: %d"), dwRet);
	}

	//
	// We will be using the synchronous version of RasDial
	// In this case, RasDial will only return after connection either passed or 
	// failed.
	// 
	if (dwRet = RasDial(NULL, NULL, &DialParams, 0, NULL, &hRasConn))
	{
		LogWarn1(TEXT("\"RasDial\" failed: %d"), dwRet);

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
		LogComment(TEXT("No open RAS connections"));
		
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

	LogVerbose(TEXT("All RAS connections dropped"));
	
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
		LogWarn1(TEXT("LocalAlloc(pBuf) Failed."));
		return false;
	}

	if(WSAStartup(WSAVerReq, &WSAData) != 0) 
	{
		LogWarn1(TEXT("WSAStartup Failed\r\n"));
		
		return false;
	}
		
	// get address as a number
	if ((dwAddr = inet_addr(szServer)) == INADDR_NONE) 
	{
		// Try by name
		lpHostData = gethostbyname(szServer);
		
		if (lpHostData == NULL) 
		{
			LogWarn1(TEXT("Could not resolve the given host name, %s"), szServer);
			
			goto Cleanup;
		}
		else
		{
			dwAddr = (DWORD) (*((IN_ADDR **) lpHostData->h_addr_list))->S_un.S_addr;
		}
	}
	
	if((sock = socket(AF_INET, iSockType, 0)) == INVALID_SOCKET)
	{
		LogWarn1(TEXT("socket() failed, error = %d"), WSAGetLastError());
		
		goto Cleanup;
	}
	
	sClientAddrInfo.sin_port        = 0;
	sClientAddrInfo.sin_addr.s_addr = htonl(INADDR_ANY);
	sClientAddrInfo.sin_family      = AF_INET;
	
	if(bind(sock, (SOCKADDR *)&sClientAddrInfo, sizeof(sClientAddrInfo)) == SOCKET_ERROR)
	{
		LogWarn1(TEXT("bind() failed, error = %d"), WSAGetLastError());
		
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
			LogWarn1(TEXT("connect() failed, error = %d"), WSAGetLastError());
			goto Cleanup;
		}
		
		int iOptVal = 1;
		
		// Turn off Nagle algorithm
		if(setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, 
							(char *)&iOptVal, sizeof(iOptVal)) == SOCKET_ERROR)
		{
			LogWarn1(TEXT("setsocktopt() failed, error = %d."), WSAGetLastError());
		}
		
		// Send the packet
		DWORD i=0;
		for (i=0;i<dwNumSends;i++)
		{
			if((dwNumBytesSent = send(sock, pBuf, dwPacketSize, 0)) != dwPacketSize)
			{
				LogWarn1(TEXT("send() failed, number bytes sent = %d, error = %d"), 
												dwNumBytesSent, WSAGetLastError());
			}
			else 
			{
				LogComment(TEXT("TCP Packet Number: %d sent"), i);
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
				LogWarn1(TEXT("sendto() failed, number bytes sent = %d, error = %d"), 
												dwNumBytesSent, WSAGetLastError());
			}
			else 
			{
				LogComment(TEXT("UDP Packet Number: %d sent"), i);
			}
		}
	}
	
	LogVerbose(TEXT("Client sent a packet (%s): %d Bytes\r\n"), 
					(iSockType == SOCK_STREAM)? L"TCP": L"UDP", dwNumBytesSent);

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
		LogWarn1(TEXT("Unable to find \"RasEnumDevices\" buffer size: %s"),dwErr);

		return dwErr;
	}

	lpRasDevInfo = (LPRASDEVINFO)LocalAlloc(LPTR, dwBufSize);
	if (!lpRasDevInfo)
	{
		LogWarn1(TEXT("Couldn't allocate memory for \"RasEnumDevices\""));
		
		return ERROR_READING_DEVICENAME;
	}
	lpRasDevInfo->dwSize = sizeof(RASDEVINFO); 

	if (dwErr = RasEnumDevices(lpRasDevInfo, &dwBufSize, &dwNumDevices))
	{
		LogWarn1(TEXT("\"RasEnumDevices\" failed: %s"), dwErr);

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
		LogWarn1(TEXT("%lu devices available; no device of type %s in list"),
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
	BYTE RasEntryBuf[ENTRYBUFSIZE];
	DWORD dwRasBufSize = sizeof(RasEntryBuf);
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
	lpRasEntry = (LPRASENTRY)RasEntryBuf;
	lpRasEntry->dwSize = sizeof(RASENTRY);

	// Retrieve device configuration:
	if (dwErr = RasGetEntryProperties(NULL, lpszEntryName, lpRasEntry, &dwRasBufSize,
	  lpDevConfig, &dwDevConfigSize))
	{
		LogWarn1(TEXT("Unable to read properties of entry \"%s\": %d"),
		  				lpszEntryName, dwErr);
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
		LogWarn1(TEXT("Couldn't get default device configuration"));

		return ERROR_CANNOT_SET_PORT_INFO;
	}
	lpDevConfig = DevConfigBuf + DEVCONFIGOFFSET;

	if (lRet = lineInitialize(&hLineApp, NULL, lineCallbackFunc, TEXT("OEMRasClient"),
	  &dwNumDevs))
	{
		LogWarn1(TEXT("Can't initialize TAPI: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));
		
		return ERROR_CANNOT_SET_PORT_INFO;
    }

	if (lRet = lineNegotiateAPIVersion(hLineApp, 0, 0x00010000, 0x00020001,
		  &dwAPIVersion, &ExtensionID))
	{
		LogWarn1(
		  TEXT("TAPI version negotiation failed: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));
		lineShutdown(hLineApp);
		
		return ERROR_CANNOT_SET_PORT_INFO;
	}

	if (lRet = lineOpen(hLineApp, dwDeviceNum, &hLine, dwAPIVersion, 0, 0,
	  LINECALLPRIVILEGE_NONE, LINEMEDIAMODE_DATAMODEM, NULL))
	{
		LogWarn1(
		  TEXT("Couldn't open TAPI device %lu: error %.8x (%s)"),
		  dwDeviceNum, lRet, FormatLineError(lRet, szErr));
		lineShutdown(hLineApp);
		
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
		LogWarn1(TEXT("Couldn't reset baudrate: error %.8x (%s)"),
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
		LogWarn1(TEXT("Couldn't reset parity: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));
		dwErr = ERROR_CANNOT_SET_PORT_INFO;
	}

	// Set the wordlength:
	UCD.dwOption = UNIMDM_OPT_BYTESIZE;
	UCD.dwValue = (DWORD)bWordLength;

	if ((lRet = lineDevSpecific(hLine, 0, NULL, &UCD, sizeof(UCD))) < 0)
	{
		LogWarn1(TEXT("Couldn't reset wordlength: error %.8x (%s)"),
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
		LogWarn1(TEXT("Couldn't reset stopbits: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));
	}

	if (lRet = lineClose(hLine))
		LogWarn1(TEXT("lineClose: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));

	if (lRet = lineShutdown(hLineApp))
		LogWarn1(TEXT("lineShutdown: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szErr));
	lRet = (long)dwErr;

	if (dwErr = RasSetEntryProperties(NULL, lpszEntryName, lpRasEntry, dwRasBufSize,
	  lpDevConfig, dwDevConfigSize))
	{
		LogWarn1(TEXT("\"RasSetEntryProperties\" failed: %d"), dwErr);
		return dwErr;
	}
	
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
		LogWarn1(TEXT("\"RasEnumDevices\": buffer error"));

		return ERROR_READING_DEVICENAME;
	}
	lpszDeviceName[0] = TEXT('\0');

	// find the buffer size needed
	dwErr = RasEnumDevices(NULL, &dwBufSize, &dwNumDevices);
	if (!dwBufSize)
	{
		LogWarn1(TEXT("Unable to find \"RasEnumDevices\" buffer size: %s"), dwErr);
		return dwErr;
	}

	lpRasDevInfo = (LPRASDEVINFO)LocalAlloc(LPTR, dwBufSize);
	if (!lpRasDevInfo)
	{
		LogWarn1(TEXT("Couldn't allocate memory for \"RasEnumDevices\""));

		return ERROR_READING_DEVICENAME;
	}
	lpRasDevInfo->dwSize = sizeof(RASDEVINFO); 

	if (dwErr = RasEnumDevices(lpRasDevInfo, &dwBufSize, &dwNumDevices))
	{
		LogWarn1(TEXT("\"RasEnumDevices\" failed: %s"), dwErr);

		LocalFree(lpRasDevInfo);
		return dwErr;
	}

	if (dwDeviceNum >= dwNumDevices)
	{
		LogWarn1(TEXT("%lu devices available; device %lu not in list"),
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
		LogWarn1(TEXT("\"RasEnumDevices\": buffer error"));
		
		return ERROR_READING_DEVICENAME;
	}
	lpszDeviceType[0] = TEXT('\0');

    // find the buffer size needed
	dwErr = RasEnumDevices(NULL, &dwBufSize, &dwNumDevices);
	if (!dwBufSize)
	{
		LogWarn1(TEXT("Unable to find \"RasEnumDevices\" buffer size: %s"), dwErr);

		return dwErr;
	}

	lpRasDevInfo = (LPRASDEVINFO)LocalAlloc(LPTR, dwBufSize);
	if (!lpRasDevInfo)
	{
		LogWarn1(TEXT("Couldn't allocate memory for \"RasEnumDevices\""));
		
		return ERROR_READING_DEVICENAME;
	}
	lpRasDevInfo->dwSize = sizeof(RASDEVINFO); 

	if (dwErr = RasEnumDevices(lpRasDevInfo, &dwBufSize, &dwNumDevices))
	{
		LogWarn1(TEXT("\"RasEnumDevices\" failed: %s"), dwErr);

		LocalFree(lpRasDevInfo);
		return dwErr;
	}

	if (dwDeviceNum >= dwNumDevices)
	{
		LogWarn1(TEXT("%lu devices available; device %lu not in list"),
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
		LogWarn1(TEXT("Device type %s not recognized"), lRet);
		return FALSE;
    }

	if (lRet = lineInitialize(&hLineApp, NULL, lineCallbackFunc, TEXT("OEMRasClient"),
	  &dwNumDevs))
	{
		LogWarn1(TEXT("Can't initialize TAPI: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szBuff));
		return 0xFFFFFFFF;
    }

	for (dwDeviceNum = 0; dwDeviceNum < dwNumDevs; dwDeviceNum++)
	{
		if (lRet = lineNegotiateAPIVersion(hLineApp, dwDeviceNum, 0x00010000,
		  0x00020001, &dwAPIVersion, &ExtensionID))
		{
			LogWarn1(
			  TEXT("TAPI version negotiation failed for device %lu: error %.8x (%s)"),
			  dwDeviceNum, lRet, FormatLineError(lRet, szBuff));
			continue;
		}

		lpDevCaps->dwTotalSize = DEVCAPSSIZE;
		if (lRet = lineGetDevCaps(hLineApp, dwDeviceNum, dwAPIVersion, 0,
		  lpDevCaps))
			LogWarn1(
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
		LogWarn1(TEXT("No matching Unimodem device found"));
		dwDeviceNum = 0xFFFFFFFF;
	}

	if (lRet = lineShutdown(hLineApp))
		LogWarn1(TEXT("lineShutdown: error %.8x (%s)"),
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
		LogWarn1(TEXT("Can't initialize TAPI: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szBuff));
		return 0xFFFFFFFF;
    }

	for (dwDeviceNum = 0; dwDeviceNum < dwNumDevs; dwDeviceNum++)
	{
		if (lRet = lineNegotiateAPIVersion(hLineApp, dwDeviceNum, 0x00010000,
		  0x00020001, &dwAPIVersion, &ExtensionID))
		{
			LogWarn1(
			  TEXT("TAPI version negotiation failed for device %lu: error %.8x (%s)"),
			  dwDeviceNum, lRet, FormatLineError(lRet, szBuff));
			continue;
		}

		lpDevCaps->dwTotalSize = DEVCAPSSIZE;
		if (lRet = lineGetDevCaps(hLineApp, dwDeviceNum, dwAPIVersion, 0,
		  lpDevCaps))
		{
			LogWarn1(
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
				LogWarn1(TEXT("TAPI device %lu isn't Unimodem"),
				  dwDeviceNum);
				dwDeviceNum = 0xFFFFFFFF;
			}
			else if (lpDevConfig != NULL)
			{
				lpDevConfig->dwTotalSize = DEVBUFSIZE;

				if (lRet = lineGetDevConfig(dwDeviceNum, lpDevConfig,
				  TEXT("comm/datamodem")))
				{
					LogWarn1(
					  TEXT("Couldn't read TAPI device %lu configuration: error %.8x (%s)"),
					  dwDeviceNum, lRet, FormatLineError(lRet, szBuff));
					dwDeviceNum = 0xFFFFFFFF;
				}
			}
			break;
		}
	}

	if (lRet = lineShutdown(hLineApp))
		LogWarn1(TEXT("lineShutdown: error %.8x (%s)"),
		  lRet, FormatLineError(lRet, szBuff));

	if ((dwDeviceNum >= dwNumDevs) && (dwDeviceNum < 0xFFFFFFFF))
	{
		LogWarn1(TEXT("No match for device %s"), lpszDeviceName);
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
