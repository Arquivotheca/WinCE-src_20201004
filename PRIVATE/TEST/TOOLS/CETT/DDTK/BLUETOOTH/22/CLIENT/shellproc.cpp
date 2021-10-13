//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//

#include <winsock2.h>
#include <winbase.h>
#include <windows.h>
#include <katoex.h>
#include <tux.h>
#include <ws2bth.h>
#include "util.h"

int ExchangeBTAddr(BD_ADDR *pRemote, BD_ADDR *pLocal);
int BeginTest(DWORD dwUniqueID);
int EndTest(DWORD dwUniqueID);
int TestDone(void);

//--------------------- Tux & Kato Variable Initilization ---------------------

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato			*g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO	*g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION	g_csProcess;

//-------------------------- Global & extern variables ------------------------


//-------------------------- Windows CE specific code -------------------------

#ifdef UNDER_CE

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) 
{
   return TRUE;
}
#endif
 
/*-----------------------------------------------------------------------------
  Function		: ShellProc()
  Description	:
  Parameters	:
  Returns		:
  Comments		:
------------------------------------------------------------------------------*/

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) 
{
   WSADATA wsd;
   HOSTENT *pHostEnt;
   int retcode;
   char szServerName[256];

   switch (uMsg) 
   {
	  //------------------------------------------------------------------------
      // Message: SPM_LOAD_DLL
      //
      // Sent once to the DLL immediately after it is loaded.  The spParam 
      // parameter will contain a pointer to a SPS_LOAD_DLL structure.  The DLL
      // should set the fUnicode member of this structre to TRUE if the DLL is
      // built with the UNICODE flag set.  By setting this flag, Tux will ensure
      // that all strings passed to your DLL will be in UNICODE format, and all
      // strings within your function table will be processed by Tux as UNICODE.
      // The DLL may return SPR_FAIL to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_LOAD_DLL: {
         // If we are UNICODE, then tell Tux this by setting the following flag.
         #ifdef UNICODE
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
         #endif
			
         // Get/Create our global logging object.
         g_pKato = (CKato*)KatoGetDefaultObject();

         // Initialize our global critical section.
         InitializeCriticalSection(&g_csProcess);

		 // Initialize winsock2.
		 if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
		 {
			 OutStr(TEXT("WSAStartup failed with error %d\n"), WSAGetLastError());
			 WSACleanup();

			 return SPR_NOT_HANDLED;
		 }

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_UNLOAD_DLL
      //
      // Sent once to the DLL immediately before it is unloaded.
      //------------------------------------------------------------------------

      case SPM_UNLOAD_DLL: {
		 if (!g_bNoServer && (TestDone() != SUCCESS))
		 {
			 OutStr(TEXT("TestDone call failed\n"));
		 }

		 // Cleanup winsock2.
		 if (WSACleanup() != 0)
		 {
			 OutStr(TEXT("WSACleanup failed with error %d\n"), WSAGetLastError());

			 return SPR_NOT_HANDLED;
		 }

         // This is a good place to destroy our global critical section.
         DeleteCriticalSection(&g_csProcess);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_SHELL_INFO
      //
      // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
      // some useful information about its parent shell and environment.  The
      // spParam parameter will contain a pointer to a SPS_SHELL_INFO structure.
      // The pointer to the structure may be stored for later use as it will
      // remain valid for the life of this Tux Dll.  The DLL may return SPR_FAIL
      // to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_SHELL_INFO: {
         // Store a pointer to our shell info for later use.
         g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_REGISTER
      //
      // This is the only ShellProc() message that a DLL is required to handle
      // (except for SPM_LOAD_DLL if you are UNICODE).  This message is sent
      // once to the DLL immediately after the SPM_SHELL_INFO message to query
      // the DLL for it’s function table.  The spParam will contain a pointer to
      // a SPS_REGISTER structure.  The DLL should store its function table in
      // the lpFunctionTable member of the SPS_REGISTER structure.  The DLL may
      // return SPR_FAIL to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_REGISTER: {
         ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;

		 // Tux can only be client.
		 g_bIsServer = FALSE;

		 // No server name means to skip connection tests.
		 if (g_pShellInfo->szDllCmdLine[0] == '\0')
		 {
			 g_bNoServer = TRUE;
		 }
		 else 
		 {
			 memset((void *)&g_ServerAddr, 0, sizeof(g_ServerAddr));
			 g_ServerAddr.sin_family = AF_INET;
			 g_ServerAddr.sin_port = htons(DEFAULT_PORT);

			 // Assume it is an IPv4 address.
			 WideCharToMultiByte(CP_ACP, 0, g_pShellInfo->szDllCmdLine, -1, (char *)szServerName, sizeof(szServerName), NULL, NULL);
			 g_ServerAddr.sin_addr.s_addr = inet_addr(szServerName);
			 
			 if ((g_ServerAddr.sin_addr.s_addr == INADDR_NONE) || 
				 (g_ServerAddr.sin_addr.s_addr == 0))
			 {
				 // Not an IPv4 address. Try to resolve server name.
				if ((pHostEnt = gethostbyname((char *)szServerName)) == NULL)
				{
					 OutStr(TEXT("gethostbyname failed with error %d\n"), WSAGetLastError());
					OutStr(TEXT("Failed to resovle server name \"%s\"\n"), g_pShellInfo->szDllCmdLine); 

					return SPR_NOT_HANDLED;
				}
			 
				if (pHostEnt->h_addrtype != AF_INET)
				{
					OutStr(TEXT("Failed resovle server name to an INET address\n"));

					return SPR_NOT_HANDLED;
				}
				
				memcpy((void *)(&g_ServerAddr.sin_addr), (void *)(pHostEnt->h_addr_list[0]), pHostEnt->h_length); 
			 }
			
			 OutStr(TEXT("Server name %s, port %d\n"), g_pShellInfo->szDllCmdLine, ntohs(g_ServerAddr.sin_port));

			 // Get local Bluetooth device address.
			 if ((retcode = DiscoverLocalRadio(&g_LocalRadioAddr)) != SUCCESS)
			 {
				 OutStr(TEXT("DiscoverLocalRadio failed with error %d"), retcode);

				 return SPR_NOT_HANDLED;
			 }

			 // Exchange Bluetooth device address.
			 if ((retcode = ExchangeBTAddr(&g_RemoteRadioAddr, &g_LocalRadioAddr)) != SUCCESS)
			 {
				 OutStr(TEXT("ExchangeBTAddr failed with error %d\n"), retcode);

				 return SPR_NOT_HANDLED;
			 }
		 }
		 
         #ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
         #else
            return SPR_HANDLED;
         #endif
      }

      //------------------------------------------------------------------------
      // Message: SPM_START_SCRIPT
      //
      // Sent to the DLL immediately before a script is started.  It is sent to
      // all Tux DLLs, including loaded Tux DLLs that are not in the script.
      // All DLLs will receive this message before the first TestProc() in the
      // script is called.
      //------------------------------------------------------------------------

      case SPM_START_SCRIPT: {
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_STOP_SCRIPT
      //
      // Sent to the DLL when the script has stopped.  This message is sent when
      // the script reaches its end, or because the user pressed stopped prior
      // to the end of the script.  This message is sent to all Tux DLLs,
      // including loaded Tux DLLs that are not in the script.
      //------------------------------------------------------------------------

      case SPM_STOP_SCRIPT: {
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_BEGIN_GROUP
      //
      // Sent to the DLL before a group of tests from that DLL is about to be
      // executed.  This gives the DLL a time to initialize or allocate data for
      // the tests to follow.  Only the DLL that is next to run receives this
      // message.  The prior DLL, if any, will first receive a SPM_END_GROUP
      // message.  For global initialization and de-initialization, the DLL
      // should probably use SPM_START_SCRIPT and SPM_STOP_SCRIPT, or even
      // SPM_LOAD_DLL and SPM_UNLOAD_DLL.
      //------------------------------------------------------------------------

      case SPM_BEGIN_GROUP: {
         g_pKato->BeginLevel(0, TEXT("BEGIN GROUP: BTW22.DLL"));

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_END_GROUP
      //
      // Sent to the DLL after a group of tests from that DLL has completed
      // running.  This gives the DLL a time to cleanup after it has been run.
      // This message does not mean that the DLL will not be called again to run
      // tests; it just means that the next test to run belongs to a different
      // DLL.  SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL to track when it
      // is active and when it is not active.
      //------------------------------------------------------------------------

      case SPM_END_GROUP: {
         g_pKato->EndLevel(TEXT("END GROUP: BTW22.DLL"));

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_BEGIN_TEST
      //
      // Sent to the DLL immediately before a test executes.  This gives the DLL
      // a chance to perform any common action that occurs at the beginning of
      // each test, such as entering a new logging level.  The spParam parameter
      // will contain a pointer to a SPS_BEGIN_TEST structure, which contains
      // the function table entry and some other useful information for the next
      // test to execute.  If the ShellProc function returns SPR_SKIP, then the
      // test case will not execute.
      //------------------------------------------------------------------------

      case SPM_BEGIN_TEST: {
         // Start our logging level.
         LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
		 g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                             TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                             pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                             pBT->dwRandomSeed);

		 // Tests need to be run sequencially.
		 EnterCriticalSection(&g_csProcess);

		 if (!g_bNoServer && (BeginTest(pBT->lpFTE->dwUniqueID) != SUCCESS))
		 {
			 OutStr(TEXT("BeginTest call failed\n"));

			 return SPR_SKIP;
		 }

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_END_TEST
      //
      // Sent to the DLL after a single test executes from the DLL.  This gives
      // the DLL a time perform any common action that occurs at the completion
      // of each test, such as exiting the current logging level.  The spParam
      // parameter will contain a pointer to a SPS_END_TEST structure, which
      // contains the function table entry and some other useful information for
      // the test that just completed.
      //------------------------------------------------------------------------

      case SPM_END_TEST: {
         // End our logging level.
         LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;

		 if (!g_bNoServer && (EndTest(pET->lpFTE->dwUniqueID) != SUCCESS))
		 {
			 OutStr(TEXT("EndTest call failed\n"));
		 }

		 // Give server some time to finish up.
		 Sleep(1500);

		 // Tests need to run sequentially.
		 LeaveCriticalSection(&g_csProcess);

         g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                           pET->lpFTE->lpDescription,
                           pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                           pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                           pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                           pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
         
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_EXCEPTION
      //
      // Sent to the DLL whenever code execution in the DLL causes and exception
      // fault.  By default, Tux traps all exceptions that occur while executing
      // code inside a Tux DLL.
      //------------------------------------------------------------------------

      case SPM_EXCEPTION: {
		 g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
         
		 return SPR_HANDLED;
      }
   }

   return SPR_NOT_HANDLED;
}

/**************************************************************
**** internal functions
***************************************************************/

int ConnectToServer(SOCKET *pSock)
{
	*pSock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (*pSock == INVALID_SOCKET)
	{
		OutStr(TEXT("socket call failed with error %d\n"), WSAGetLastError());

		return FAIL;
	}

	if (connect(*pSock, (SOCKADDR *)&g_ServerAddr,  sizeof(g_ServerAddr)) == SOCKET_ERROR)
	{
		OutStr(TEXT("connect call failed with error %d\n"), WSAGetLastError());

		return FAIL;
	}

	return SUCCESS;
}

int SendRequest(SOCKET sock, DWORD dwCommand, void *pData, DWORD cData)
{
	CONTROL_PACKET packet;
	int retcode;

	// Don't want to overflow the packet.
	if (cData > sizeof(packet.Data))
	{
		OutStr(TEXT("Data portion of control packet too large\n"));
		OutStr(TEXT("Parts can't fit in discarded\n"));
		cData = sizeof(packet.Data);
	}
	
	// Fill the packet.
	memset((void *)&packet, 0, sizeof(packet));
	packet.dwCommand = dwCommand;
	if (pData && cData)
	{
		memcpy((void *)&packet.Data, (void *)pData, cData);
	}
	
	// Send.
	retcode = SendControlPacket(sock, &packet);
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("SendControlPacket failed with error %d\n"), retcode);
	
		return FAIL;
	}

	return SUCCESS;
}

int ReceiveResponse(SOCKET sock, DWORD dwCommand, void *pData, DWORD cData)
{
	CONTROL_PACKET packet;
	int retcode;

	retcode = 0;
	
	// Receive.
	retcode = ReceiveControlPacket(sock, &packet);
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("ReceiveControlPacket failed with error %d\n"), retcode);
	
		return FAIL;
	}

	if (packet.dwCommand != dwCommand)
	{
		OutStr(TEXT("Command of received packet does not match. Expected %d, got %d.\n"), dwCommand, packet.dwCommand);

		return FAIL;
	}

	if (pData && cData)
	{
		memcpy((void *)pData, (void *)&packet.Data, cData < sizeof(packet.Data) ? cData : sizeof(packet.Data));
	}

	return SUCCESS;
}

int	DisconnectFromServer(SOCKET sock)
{
	shutdown(sock, 0);

	closesocket(sock);

	return SUCCESS;
}


int ExchangeBTAddr(BD_ADDR *pRemote, BD_ADDR *pLocal)
{
	SOCKET sock;
	int retcode;

	retcode = ConnectToServer(&sock);
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("ConnectToServer failed with error %d\n"), retcode);
	
		return FAIL;
	}

	retcode = SendRequest(sock, EXCHANGE_BT_ADDR, (void *)pLocal, sizeof(*pLocal));
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("SendRequest failed with error %d\n"), retcode);
		DisconnectFromServer(sock);

		return FAIL;
	}
			
	retcode = ReceiveResponse(sock, EXCHANGE_BT_ADDR, (void *)pRemote, sizeof(*pRemote));
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("ReceiveResponse failed with error %d\n"), retcode);
		DisconnectFromServer(sock);

		return FAIL;
	}

	DisconnectFromServer(sock);

	return SUCCESS;
}

int BeginTest(DWORD dwUniqueID)
{
	SOCKET sock;
	int retcode;

	if (g_bNoServer)
		return SUCCESS;

	retcode = ConnectToServer(&sock);
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("ConnectToServer failed with error %d\n"), retcode);
	
		return FAIL;
	}

	retcode = SendRequest(sock, BEGIN_TEST, (void *)&dwUniqueID, sizeof(dwUniqueID));
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("SendRequest failed with error %d\n"), retcode);
		DisconnectFromServer(sock);

		return FAIL;
	}
			
	retcode = ReceiveResponse(sock, BEGIN_TEST, (void *)&dwUniqueID, sizeof(dwUniqueID));
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("ReceiveResponse failed with error %d\n"), retcode);
		DisconnectFromServer(sock);

		return FAIL;
	}

	DisconnectFromServer(sock);

	return SUCCESS;
}

int EndTest(DWORD dwUniqueID)
{
	SOCKET sock;
	int retcode;

	if (g_bNoServer)
		return SUCCESS;

	retcode = ConnectToServer(&sock);
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("ConnectToServer failed with error %d\n"), retcode);
	
		return FAIL;
	}

	retcode = SendRequest(sock, END_TEST, (void *)&dwUniqueID, sizeof(dwUniqueID));
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("SendRequest failed with error %d\n"), retcode);
		DisconnectFromServer(sock);

		return FAIL;
	}
			
	retcode = ReceiveResponse(sock, END_TEST, (void *)&dwUniqueID, sizeof(dwUniqueID));
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("ReceiveResponse failed with error %d\n"), retcode);
		DisconnectFromServer(sock);

		return FAIL;
	}

	DisconnectFromServer(sock);

	return SUCCESS;
}

int TestDone(void)
{
	SOCKET sock;
	int retcode;

	if (g_bNoServer)
		return SUCCESS;

	retcode = ConnectToServer(&sock);
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("ConnectToServer failed with error %d\n"), retcode);
	
		return FAIL;
	}

	retcode = SendRequest(sock, TEST_DONE, (void *)NULL, 0);
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("SendRequest failed with error %d\n"), retcode);
		DisconnectFromServer(sock);

		return FAIL;
	}
			
	retcode = ReceiveResponse(sock, TEST_DONE, (void *)NULL, 0);
	if (retcode != SUCCESS)
	{
		OutStr(TEXT("ReceiveResponse failed with error %d\n"), retcode);
		DisconnectFromServer(sock);

		return FAIL;
	}

	DisconnectFromServer(sock);

	return SUCCESS;
}

// END OF FILE