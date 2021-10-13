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
#include <stdlib.h>
#include <stdio.h>
#include "tchar.h"
#include <string.h>
#include <windows.h>

#ifndef UNDER_CE
#include <strmif.h>
#include <control.h>
#include <uuids.h>
#include <evcode.h>
#endif

#include <Winsock2.h.>
#include <Winuser.h>
#include "cpumon.h"
#include <halether.h>

TCHAR argv[20][256];
int argc=0;

HANDLE hThread =NULL;
HANDLE hThread2 =NULL;
DWORD dwThreadID;
SOCKET winSock;
bool record = true;
bool run = true;

UCHAR g_kitlBuffer[CETKPERF_KITL_BUFFER_POOL_SIZE];
UCHAR g_kitlID = 0;

SOCKET ConnectSocket(char* szIPAddress, int port) {
  DWORD dwDestAddr;
  SOCKADDR_IN sockAddrDest;
  SOCKET sockDest;
  
  // create a socket
  sockDest = socket(AF_INET, SOCK_STREAM, 0);
  if(sockDest == SOCKET_ERROR) {
    OutputDebugString(L"*** Could not create socket ***\n");
    return INVALID_SOCKET;
  }
  
  // convert address to in_Addr (binary) form
  dwDestAddr = inet_addr(szIPAddress);
  // Initialize SOCKADDR_IN with IP Address, port number and address family
  memcpy(&sockAddrDest.sin_addr, &dwDestAddr, sizeof(DWORD));
  sockAddrDest.sin_port = htons(port);
  sockAddrDest.sin_family = AF_INET;
  //attempt to connect to server
  do {
    if(connect(sockDest, (LPSOCKADDR) &sockAddrDest, sizeof(sockAddrDest)) != SOCKET_ERROR)
      break;
    else {
      OutputDebugString(L" *** Could not connect to server socket\n");
      Sleep(1000);
    }
  } while(true);

  return sockDest;
}

SOCKET ConnectSocket(hostent* lpHostEnt, int port) {
  SOCKADDR_IN sockAddrDest;
  SOCKET sockDest;
  
  // create a socket
  
  sockDest = socket(AF_INET, SOCK_STREAM, 0);
  if(sockDest == SOCKET_ERROR) {
    OutputDebugString(L"*** Could not create socket ***\n");
    return INVALID_SOCKET;
  }


  sockAddrDest.sin_addr = *((LPIN_ADDR)*lpHostEnt->h_addr_list);
  sockAddrDest.sin_port = htons(port);
  sockAddrDest.sin_family = AF_INET;
  //attempt to connect to server
  do {
    if(connect(sockDest, (LPSOCKADDR) &sockAddrDest, sizeof(sockAddrDest)) != SOCKET_ERROR)
      break;
    else 
    {
      OutputDebugString(L" *** Could not connect to server socket\n");
      Sleep(1000);
    }
  } while(true);
  
  return sockDest;
}


void ParseCmd(LPTSTR lpCmdLine)
{
    int idx = 0;

    argc=0;
	
    // Is there anything to Parse
    if (*lpCmdLine == '\0')
    	return;

    // Remove All White Space
    while (*lpCmdLine == ' ')
        {
        lpCmdLine++;
        }

    // Parse out first string
    do
        {
       	if (*lpCmdLine == ' ')
    	    {
    		argv[argc][idx] = '\0';
    		lpCmdLine++;
    		argc++;

      //Again Remove Spaces
    		while (*lpCmdLine == ' ')
    		    {
    			lpCmdLine++;
        		}
        	}
        else
        	{
            argv[argc][idx] = *lpCmdLine;
            idx++;
    		lpCmdLine++;
    	    }
        } 
    while (*lpCmdLine != '\0');

    argc++;
    return;
}

DWORD WINAPI  monitor_recv(PVOID pArg) {
  char buffer[256];
  DWORD cb = sizeof(buffer); 
  if(g_kitlID)
  {
    CallEdbgRecv(g_kitlID, (UCHAR*)buffer, &cb, INFINITE);
  }
  else
  {
    recv(winSock, buffer, 256, 0);
  }
  run = false;
  return NULL;
  
}

DWORD WINAPI  monitor(PVOID pArg) {
  char cStr[256];
  float cpu_util = 0.0;   
  MEMORYSTATUS lpBuffer;;
  
  while(run) {   
    if(record) {
      cpu_util = GetCurrentCPUUtilization();
      GlobalMemoryStatus(&lpBuffer);
      sprintf(cStr, "<CPU:%f><MEM:%d>", cpu_util, (lpBuffer.dwTotalPhys -lpBuffer.dwAvailPhys));
      if(g_kitlID)
      {
        CallEdbgSend(g_kitlID, (UCHAR*)cStr, strlen(cStr));
      }
      else
      {
        send(winSock, cStr, strlen(cStr), 0);
        fflush((FILE*) winSock);
      }
    }
    Sleep(2000);
  }
  return NULL;
}

bool isIPFormat(char* string) {
  for(unsigned int i=0; i < strlen(string); i++) {
    if(!isdigit(string[i]) && string[i] != '.')
      return false;
  }
  return true;
}

int
WINAPI
WinMain (
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine,
    int nCmdShow
    )
{
  HRESULT hr;
  char server[64];
  hostent* host = NULL;
  WORD wVersionRequested;
  WSADATA wsaData;
  int err;
  int ret = -1;

  HANDLE hSingleInstanceKernelObject = CreateMutex(NULL, FALSE, CETKPERF_RUNNING_MUTEX_GUID);
  if ( ERROR_ALREADY_EXISTS == GetLastError() )
  {
    goto exit;
  }

  CoInitializeEx(NULL,COINIT_MULTITHREADED);
  ParseCmd(lpCmdLine);
  
  memset(server, NULL, sizeof server);
  
  wcstombs(server, argv[0], wcslen(argv[0]));
  
  if(strcmp((char*)server, "kitl")==0)
  {
    if(!CallEdbgRegisterClient (
      &g_kitlID,                      // Service ID
      CETKPERF_KITL_SERVICE_NAME_A,   // Service name
      0,                              // Flags
      EDBG_WINDOW_SIZE,               // Window size 
      g_kitlBuffer                    // Buffer pool
      ))
    {
      OutputDebugString(TEXT("CallEdbgRegisterClient... failed"));
      goto exit;
    }
  }
  else
  {
    wVersionRequested = MAKEWORD( 2, 2 );
    err = WSAStartup( wVersionRequested, &wsaData );
    if ( err != 0 ) {
      // Tell the user that we could not find a usable WinSock DLL. 
      goto exit;
    }

    if(true == isIPFormat(server))
    {
      winSock = ConnectSocket(server, 8000);
    }
    else 
    {
      host = gethostbyname(server);
      if(NULL == host)
      {
        OutputDebugString(TEXT("Could not get host by name, exiting...\n"));
        goto exit;
      }
      winSock = ConnectSocket(host, 8000);
    }
  }
  
  hr=CalibrateCPUMon();
  if (FAILED(hr)){
    OutputDebugString(TEXT("Could not calibrate cpu!\n"));
    goto exit;
  }
  //Start monitoring the CPU
  hr=StartCPUMonitor();
  Sleep(2000); // sleep to aviod getting 100% for the 1st tick
  
  hThread = CreateThread(NULL, 0, monitor, NULL, 0, &dwThreadID);
  hThread2 = CreateThread(NULL, 0, monitor_recv, NULL, 0, &dwThreadID);
  
  do {
    Sleep(1000);
  } while(run);
  
//  exit(0);
  ret = 0;
exit:
  if(hThread != INVALID_HANDLE_VALUE) CloseHandle(hThread);
  if(hThread2 != INVALID_HANDLE_VALUE)CloseHandle(hThread2);
  if(g_kitlID)
  {
    CallEdbgDeregisterClient(g_kitlID);
  }
  else
  {
    closesocket(winSock);
    WSACleanup();
    if(host != NULL) delete host;
  }
  CoUninitialize();   

  if(hSingleInstanceKernelObject != INVALID_HANDLE_VALUE) ReleaseMutex(hSingleInstanceKernelObject);

  return ret;
}

