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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include "StdAfx.h"
#include "Deamon.h"
#include "Utils.h"
#include "NDTLog.h"
#include "Log.h"
#include <ntddndis.h>
#include "NDTMsgs.h"
#include "NDTLib.h"

#define NDTSERVER_STR		_T("NDTSERVER")

//------------------------------------------------------------------------------
// To support retail devices we need UI or at the least Message Box.
// Note that currently ndtserver.exe gets run manually by end user on support device
// so having dialog box is essential when it comes to run this test on retail device.
// Otherwise end user has to guess if this app is running or not.
typedef int (STDAPICALLTYPE *PMessageBox)(HWND,LPCTSTR , LPCTSTR , UINT) ;
PMessageBox g_lpMessageBox=NULL;
HINSTANCE g_hCoreDll=NULL;
BOOL EnableMsgBoxEntryPoint();
BOOL BuildAdapterListString(LPCTSTR szSystem, PTCHAR pszAdapter, DWORD cbSize);
BOOL GetIPAddrOfThisAdapter(PTCHAR tszAdapter, PTCHAR tszIPaddr);
//------------------------------------------------------------------------------

//Please do not use OUTPUT excessively to output normal messages as it has chance of becoming
//dialog boxes on retail devices.
inline void OUTPUT(DWORD dwLogLevel, LPCWSTR outputstring)
{
    if (g_lpMessageBox)
    {
		UINT uType = MB_OKCANCEL | MB_TOPMOST | MB_SETFOREGROUND;
		if (dwLogLevel == NDT_LOG_FAIL)
			uType |= MB_ICONERROR;
		else
			uType |= MB_ICONINFORMATION;

        g_lpMessageBox(NULL,outputstring,NDTSERVER_STR,uType);
    }
    else
    {
		if (dwLogLevel == NDT_LOG_FAIL)
			LogErr(outputstring);
		else
			LogMsg(outputstring);
    }
    return;
}

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
      OUTPUT(NDT_LOG_FAIL, (_T("Run: Failed to create a finish event")));
      rc = GetLastError();
      goto cleanUp;
   }

   // When event already exists let set it and finish
   if (GetLastError() == ERROR_ALREADY_EXISTS) {
	  if (!g_lpMessageBox)
	  {
		  SetEvent(hFinishEvent);
		  LogMsg(_T("Run: Second NDTServer instance detected, set finish event for it & exiting this instance"));
	  }
	  else
		  OUTPUT(NDT_LOG_PASS,(_T("Run: There already one NDTServer instance is running, so exiting this new instance")));
      
      goto cleanUp;
   }

   // Initialize socket enviroment
   rc = WSAStartup(MAKEWORD(1,1), &wsaData);
   if (rc != 0) {
      OUTPUT(NDT_LOG_FAIL, (_T("Run: Failed to initialize winsock API")));
      rc = ERROR_NO_NETWORK;
      goto cleanUp;
   }

   // And connection deamon now
   pDeamon = new CDeamon(NDT_CMD_PORT);
   if (pDeamon == NULL) {
      OUTPUT(NDT_LOG_FAIL, (_T("Run: Failed to create a CDeamon instance")));
      rc = ERROR_OUTOFMEMORY;
      goto cleanUp;
   }

   // Start connection deamon thread
   rc = pDeamon->Start(hFinishEvent);
   if (rc != ERROR_SUCCESS) {
      OUTPUT(NDT_LOG_FAIL, (_T("Run: Failed to start a connection deamon thread")));
      goto cleanUp;
   }

#define MAX_MSGBOX_SIZE		(4096)

   // Let know that we do some work and wait for end
   if (!g_lpMessageBox)
   {
	   LogMsg(_T("Run: Started the deamon. Run the same test again to kill all instances"));
	   // Wait for interrupt
	   WaitForSingleObject(hFinishEvent, INFINITE);
   }
   else
   {
	   PTCHAR szAdapters = (PTCHAR) new BYTE[MAX_MSGBOX_SIZE];
	   StringCchCopy(szAdapters, MAX_MSGBOX_SIZE/sizeof(TCHAR), _T("NDTServer is Running. Click OK to close.\n"));
	   BuildAdapterListString(NULL, szAdapters, MAX_MSGBOX_SIZE);

	   OUTPUT(NDT_LOG_PASS, szAdapters);
   }
   
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
   LogMsg(_T("NDTServer [-v log_level]"));
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

   //Get pointer to MessageBox() for images where UI can be shown.
   EnableMsgBoxEntryPoint();

   // Run test itself
   rc = Run();
   
   if (g_lpMessageBox)
   {
	   FreeLibrary(g_hCoreDll);
	   g_hCoreDll = NULL; g_lpMessageBox = NULL;
   }

cleanUp:
   return rc;
}


//------------------------------------------------------------------------------
BOOL EnableMsgBoxEntryPoint()
{
	HINSTANCE g_hCoreDll =  LoadLibrary(_T("COREDLL.DLL"));
	if (g_hCoreDll)
	{
		g_lpMessageBox = (PMessageBox ) GetProcAddress(g_hCoreDll,_T("MessageBoxW"));

		if (g_lpMessageBox)
			return TRUE;
		else
		{
			FreeLibrary(g_hCoreDll);
			g_hCoreDll = NULL; g_lpMessageBox = NULL;
		}
	}
	return FALSE;
}
//------------------------------------------------------------------------------
// szSystem:   NULL (Local) or IP address of remote CE device (Remote).
// pszAdapter: String which would return list of miniport adapters which one can directly
//             use in MessageBox().
// cbSize:     Size in number of bytes.
BOOL BuildAdapterListString(LPCTSTR szSystem, PTCHAR pszAdapter, DWORD cbSize)
{
   HRESULT hr = S_OK;
   BYTE mszAdapters[2048];
   TCHAR szAdapterFull[256];
   LPTSTR szAdapter = NULL;
   HANDLE hAdapter = NULL;
   NDIS_MEDIUM ndisMedium;
   BOOL fRet = FALSE;
   UINT  uiTestPhyMedium = NdisPhysicalMediumUnspecified;
   DWORD dwCcSize = cbSize/sizeof(TCHAR);
   BOOL bNoWiFi = TRUE;

   if (!pszAdapter) return FALSE;

   // Initialize the Logwrapper which is for some reason used by some functions in NDTLib.
   // If we don't initialize Logging here then those library functions might do Access violation while
   // logging results.
   NDTLogStartup(NDTSERVER_STR, NDT_LOG_PASS, NULL);

   // Initialize system
   hr = NDTStartup(szSystem);
   if (FAILED(hr)) {
      LogErr(g_szFailStartup, szSystem, hr);
      goto cleanUp;
   }

   hr = NDTQueryAdapters(szSystem, LPTSTR(mszAdapters), sizeof(mszAdapters));
   if (FAILED(hr)) {
      LogErr(g_szFailQueryAdapters, szSystem);
      goto cleanUp;
   }

   StringCchCat(pszAdapter, dwCcSize, _T("Network Adapters :Medium: IP\n"));

   szAdapter = LPTSTR(mszAdapters);

   while (*szAdapter != _T('\0')) {

	  hAdapter = NULL;
      // Open and bind adapter to get a medium
      ndisMedium = (NDIS_MEDIUM)-1;
      
      StringCchCopy(szAdapterFull, _countof(szAdapterFull), szAdapter);
      if (szSystem != NULL) {
         StringCchCat(szAdapterFull, _countof(szAdapterFull), _T("@"));
         StringCchCat(szAdapterFull, _countof(szAdapterFull), szSystem);
      }
      
      // Set option and try bind/unbind from adapter to get prefered medium
      hr = NDTOpen(szAdapterFull, &hAdapter);
      if ((FAILED(hr)) || (!hAdapter)) {
		  continue;
	  }

	  hr = NDTBind(hAdapter, FALSE, ndisMedium, &ndisMedium);
	  if (FAILED(hr)) {
		  LogErr(g_szFailBind, hr);
	  }

	  if (ndisMedium == NdisMedium802_3)
	  {
		  TCHAR szIPaddr[255]=_T("");
		  GetIPAddrOfThisAdapter(szAdapterFull, szIPaddr);

		  hr = NDTQueryInfo(
			  hAdapter, OID_GEN_PHYSICAL_MEDIUM, &uiTestPhyMedium,
			  sizeof(UINT), NULL, NULL
			  );

		  if (SUCCEEDED(hr))  
		  {
			  if (uiTestPhyMedium == NdisPhysicalMediumWirelessLan)
			  {
				  TCHAR szBuf[1024]=_T("");
				  hr = StringCchPrintf(szBuf, 1024, _T("%s :%s:%s\n"), szAdapterFull, _T("WiFi"), szIPaddr);
				  if (SUCCEEDED(hr))
					  StringCchCat(pszAdapter, dwCcSize, szBuf);

				  bNoWiFi = FALSE;
				  LogMsg(_T("Miniport %s is ** Wireless-LAN **"), szAdapterFull);
			  }
			  else if (uiTestPhyMedium == NdisPhysicalMediumNative802_11)
			  {
				  TCHAR szBuf[1024]=_T("");
				  hr = StringCchPrintf(szBuf, 1024, _T("%s :%s:%s\n"), szAdapterFull, _T("NativeWiFi"), szIPaddr);
				  if (SUCCEEDED(hr))
					  StringCchCat(pszAdapter, dwCcSize, szBuf);

				  bNoWiFi = FALSE;
				  LogMsg(_T("Miniport %s is ** Native WiFi-LAN **"), szAdapterFull);
			  }
		  }
		  else 
		  {
			  TCHAR szBuf[1024]=_T("");
			  hr = StringCchPrintf(szBuf, 1024, _T("%s :%s:%s\n"), szAdapterFull, _T("802.3"), szIPaddr);
			  if (SUCCEEDED(hr))
				  StringCchCat(pszAdapter, dwCcSize, szBuf);

			  LogMsg(_T("Miniport %s is ** NOT Wireless-LAN **"), szAdapterFull);
		  }
	  }

	  if (hAdapter)
	  {
		  hr = NDTUnbind(hAdapter);
		  if (FAILED(hr)) {
			  LogErr(g_szFailUnbind, hr);
		  }

		  hr = NDTClose(&hAdapter);
		  if (FAILED(hr)) {
			  LogErr(g_szFailClose, hr);
		  }
		  hAdapter = NULL;
	  }

      szAdapter += lstrlen(szAdapter) + 1;
   }

   if(bNoWiFi)
   {
	   StringCchCat(pszAdapter, dwCcSize, _T("No WiFi adapter is present or WiFi is turned off.\n"));
   }
   fRet = TRUE;
   
cleanUp:
   if (hAdapter != NULL) {
      hr = NDTClose(&hAdapter);
      if (FAILED(hr)) 
		  LogErr(g_szFailClose, hr);
   }

   hr = NDTCleanup(szSystem);
   if (FAILED(hr)) 
	   LogErr(g_szFailCleanup, szSystem, hr);

   // Finish up log
   NDTLogCleanup();
   
   return fRet;
}

#include <Iphlpapi.h>
#include <Ipexport.h>

 PIP_ADAPTER_INFO GetAllIPAdaptersInfo()
 {
   //Lets get all adapters info using IPHLP API
   PIP_ADAPTER_INFO pAdapterInfo = NULL;
   ULONG ulBuffLen = 0;

   DWORD dwRet = GetAdaptersInfo(pAdapterInfo,&ulBuffLen);
   if (dwRet == ERROR_BUFFER_OVERFLOW)
   {
	   pAdapterInfo = (PIP_ADAPTER_INFO) LocalAlloc(LPTR,ulBuffLen);
	   if (!pAdapterInfo)
	   {
		   LogErr(_T("Error in allocating memory for adapter info"));
		   return NULL;
	   }
   }
   else
   {
	   LogErr(_T("Error in calling GetAdaptersInfo 0x%x"),dwRet);
	   return NULL;
   }

   dwRet = GetAdaptersInfo(pAdapterInfo,&ulBuffLen);
   if (NO_ERROR != dwRet)
   {
	   LogErr(_T("Error in calling adapter GetAdaptersInfo, 2nd time 0x%x"),dwRet);
	   return NULL;
   }

   return pAdapterInfo;
 }

 void FreeAllIPAdaptersInfo(PIP_ADAPTER_INFO pAdapterInfo)
 {
	 if (pAdapterInfo)
		 LocalFree((HLOCAL)pAdapterInfo); 
 }

BOOL GetIPAddrOfThisAdapter(PTCHAR tszAdapter, PTCHAR tszIPaddr)
{
   PIP_ADAPTER_INFO pAdapterInfo = GetAllIPAdaptersInfo();
   PIP_ADAPTER_INFO pNext = pAdapterInfo;
   BOOL fRet = FALSE;

   while (pNext)
   {
	   CHAR szAdapter[255]="";

	   //This #ifdef is because IPHLPAPI returns adapter name in ANSI character set.
#ifdef UNICODE
	   WideCharToMultiByte(CP_ACP,0,tszAdapter,-1,szAdapter,255,NULL,NULL);
#else 
	   _tcscpy(szAdapter,tszAdapter);
#endif
	   if (_stricmp(szAdapter,pNext->AdapterName) == 0)
	   {
		   PCHAR szIP = pNext->IpAddressList.IpAddress.String;

#ifdef UNICODE
		   MultiByteToWideChar(CP_ACP, 0, szIP,strlen(szIP)+1, tszIPaddr, 255);
#else 
		   _tcscpy(tszIPaddr,szIP);
#endif
		   fRet = TRUE;
		   break;
	   }
	   pNext = pNext->Next;
   }

   FreeAllIPAdaptersInfo(pAdapterInfo);
   return fRet;
 }