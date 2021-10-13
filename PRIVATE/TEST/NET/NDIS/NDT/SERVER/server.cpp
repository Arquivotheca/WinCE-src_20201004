//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "Deamon.h"
#include "Utils.h"
#include "Log.h"

//------------------------------------------------------------------------------

DWORD Run()
{
   DWORD rc = ERROR_SUCCESS;
   WSADATA wsaData;
   CDeamon *pDeamon = NULL;
   HANDLE hFinishEvent = NULL;

   // Create a finish event
   hFinishEvent = CreateEvent(NULL, TRUE, FALSE, _T("NDTServer"));
   if (hFinishEvent == NULL) {
      LogErr(_T("Run: Failed to create a finish event"));
      rc = GetLastError();
      goto cleanUp;
   }

   // When event all ready exists let set it and finish
   if (GetLastError() == ERROR_ALREADY_EXISTS) {
      LogMsg(_T("Run: Second NDTServerCE instance detected"));
      LogMsg(_T("Run: Set finish event for first instance and exit"));
      SetEvent(hFinishEvent);
      goto cleanUp;
   }

   // Initialize socket enviroment
   rc = WSAStartup(MAKEWORD(1,1), &wsaData);
   if (rc != 0) {
      LogErr(_T("Run: Failed to initialize winsock API"));
      rc = ERROR_NO_NETWORK;
      goto cleanUp;
   }

   // And connection deamon now
   pDeamon = new CDeamon(NDT_CMD_PORT);
   if (pDeamon == NULL) {
      LogErr(_T("Run: Failed to create a CDeamon instance"));
      rc = ERROR_OUTOFMEMORY;
      goto cleanUp;
   }

   // Start connection deamon thread
   rc = pDeamon->Start(hFinishEvent);
   if (rc != ERROR_SUCCESS) {
      LogErr(_T("Run: Failed to start a connection deamon thread"));
      goto cleanUp;
   }

   // Let know that we do some work and wait for end
   LogMsg(_T("Run: Started the deamon and go wait for finish event"));
   
   // Wait for interrupt
   WaitForSingleObject(hFinishEvent, INFINITE);
   
cleanUp:

   // Stop deamon when exist
   if (pDeamon != NULL) {
      pDeamon->Stop();
      delete pDeamon;
   }

   // Close sockets
   WSACleanup();

   // Close event
   if (hFinishEvent!= NULL) CloseHandle(hFinishEvent);

   // Let know what happen
   LogMsg(_T("Run: Stopped the deamon and exit"));
   return rc;
}

//------------------------------------------------------------------------------

BOOL ParseParameters(LPTSTR szCommandLine)
{
   LPTSTR argv[64];
   INT argc = 64;
   BOOL bOk = FALSE;

   CommandLineToArgs(szCommandLine, &argc, argv);
   g_dwLogLevel = GetOptionLong(&argc, argv, g_dwLogLevel, _T("v"));
   if (argc > 0) goto cleanUp;
   bOk = TRUE;
   
cleanUp:
   return bOk;
}

//------------------------------------------------------------------------------

void PrintUsage()
{
   LogMsg(_T("NDTServerCE [-v log_level] [-s]"));
   LogMsg(_T(""));
   LogMsg(_T("  -v log_level : Log Output Level (0-15, default 10)"));
   LogMsg(_T(""));
}

//------------------------------------------------------------------------------

int WINAPI WinMain(
   HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd
)
{
   DWORD rc = ERROR_SUCCESS;

   // Set log prefix
   g_szLogPrefix = _T("NDTServer");

   // Just let know that we was run
   LogMsg(_T("----------------------------------------------------------"));
   LogMsg(_T(" NDTServer - Version 0.01"));
   LogMsg(_T("----------------------------------------------------------"));

   // Parse parameters & print usage and exit when failed
   if (!ParseParameters(lpCmdLine)) {
      PrintUsage();
      goto cleanUp;
   }

   // Run test itself
   rc = Run();
   
cleanUp:
   return rc;
}

//------------------------------------------------------------------------------
