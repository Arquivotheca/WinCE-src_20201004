//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************


#include <windows.h>
#include <types.h>
#include <ceddk.h>
#include <cardserv.h>
#include <tuple.h>
#include <string.h>
#include <memory.h>
#include <nkintr.h>
#include <sockserv.h>
#include <hw16550.h>





BOOL FAR PASCAL  LibMain(HANDLE hModule, DWORD dwReason, LPVOID lpRes)
{
	TCHAR szDebug[256];

   	switch ( dwReason ){
      		case DLL_PROCESS_ATTACH:
         		NKDbgPrintfW(TEXT( "dummydr: DLL_PROCESS_ATTACH\r\n") );
			wsprintf(szDebug, TEXT("hModule = 0x%lx\r\n"), hModule);
			OutputDebugString(szDebug);
         		break;

      		case DLL_THREAD_ATTACH:
         		NKDbgPrintfW(TEXT( "dummydr: DLL_THREAD_ATTACH\r\n" ));
         		break;

      		case DLL_PROCESS_DETACH:
         		NKDbgPrintfW(TEXT( "dummydr: DLL_PROCESS_DETACH\r\n" ));
			wsprintf(szDebug, TEXT("hModule = 0x%lx\r\n"), hModule);
			OutputDebugString(szDebug);
         		break;

      		case DLL_THREAD_DETACH:
         		NKDbgPrintfW(TEXT( "dummydr: DLL_THREAD_DETACH\r\n" ));
         		break;

      		default:
         		NKDbgPrintfW(TEXT( "dummydr: unexpected failure\r\n" ));
         		return FALSE;
   	}

   	return TRUE;
}




DWORD tst_Init (DWORD Index)
{
	return 1;
}

BOOL tst_Deinit(DWORD dwData)
{
	return TRUE;
}


DWORD tst_Open (DWORD dwData, DWORD dwAccess, DWORD dwShareMode)
{
	return 2;
}

BOOL tst_Close (DWORD dwData)
{
	return TRUE;
}


DWORD tst_Read (DWORD dwData, LPVOID pBuf, DWORD Len)
{
	return 0;
}

DWORD tst_Write (DWORD dwData, LPCVOID pBuf, DWORD Len)
{
	return 0;
}


DWORD
tst_Seek (DWORD dwData, long pos, DWORD type)
{
	return (DWORD)-1;
}



void tst_PowerUp(DWORD dwData)
{
	return ;
}

void tst_PowerDown(DWORD dwData)
{
	return ;
}


BOOL tst_IOControl(DWORD dwOpenData,
	DWORD dwCode,   // iTestCase
	PBYTE pBufIn,	 // LPTESTPARAMS
	DWORD dwLenIn,	 // nTotalCases
	PBYTE pBufOut,	 // lpszCmdLine
	DWORD dwLenOut, // length of lpszCmdLine
	PDWORD pdwActualOut)
{
	return TRUE;
}





/// detection routine
LPWSTR
DetectIntr(
    CARD_SOCKET_HANDLE hSock,
    UCHAR DevType,
    LPWSTR DevKey,
    DWORD  DevKeyLen
    )
{
 	LPWSTR lpRet=NULL;

	lpRet = DevKey;
      wcscpy(DevKey, TEXT("PCMTEST"));
	return lpRet;
}

