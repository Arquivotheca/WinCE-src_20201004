//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
/*++
Module Name:  
	Wintest.cpp

Abstract:

    Tests related with PCCard windows operations 

--*/

#include "testmain.h"
#include "common.h"
#include "WinTest.h"

//constant defines only used in the following ThreadRun() API.
#define	WINTST_BASE_ADDR		0x3f8
#define	WINTST_WIN_SIZE		8
#define	WINTST_WIN_INTERVAL	0x10

UINT32 WinTest::m_uIOBaseAddr = IO_START_ADDRESS;
BOOL
WinTest::Init(){

      InitializeCriticalSection(&m_cs);
	return TRUE;
}


DWORD WinTest::ThreadRun() {
     if(IsTerminated() == TRUE)
        return GetLastError();

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ WinTest::ThreadRun() enterted\r\n")));

	switch(m_dwCaseID){
		case 1: //test on CardRequestConfiguration
			Test_CardRequestandMapWindow();
			break;
		case 2: //test focus on invalid inputs 
			Test_InvalidParas();
			break;
		default:	
   			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u Func %u: Invalid test case\r\n"), 
   			                                        m_dwThreadID, m_uLocalSock, m_uLocalFunc);
			break;
	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- WinTest::ThreadRun()\r\n")));

	return GetLastError();
}

#define	MAX_NUM_WINDOWS		16
#define	DEFAULT_MEMWIN_SIZE	0x400   //  1k
#define	DEFAULT_IOWIN_SIZE	0x40	//  256	

VOID WinTest::Test_CardRequestandMapWindow (){

	CARD_WINDOW_HANDLE hIOWin[MAX_NUM_WINDOWS], hMemWin[MAX_NUM_WINDOWS], hAttriWin;
	DWORD	dwIOWNum, dwMemWNum;
	CARD_SOCKET_HANDLE	hSocket = {m_uLocalSock, m_uLocalFunc};
	CARD_WINDOW_PARMS	cwParams;
	BOOL				bFound;
	UINT32				uGranularity = 1;
	PVOID				pVirtAddr = NULL;
	DWORD				dwIndex;
	STATUS				status;
	DWORD				dwError;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ WinTest::Test_CardRequestandMapWindow() enterted\r\n")));

	//for MFC cards, if it is not the first function, it may not be able to get attrib window and common window
	if(m_uLocalFunc > 0)
		return;

	//request an attribute window, it should success
	cwParams.hSocket = hSocket;
	cwParams.fAttributes = WIN_ATTR_ATTRIBUTE;
	cwParams.fAccessSpeed = WIN_SPEED_USE_WAIT;
	cwParams.uWindowSize = DEFAULT_MEMWIN_SIZE;
	hAttriWin = NULL;
	SetLastError(0);
	hAttriWin = CardRequestWindow(g_hClient, &cwParams);
	if(hAttriWin == NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: Can not request attribute window! Error = 0x%lx\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  GetLastError());
		SetResult(FALSE);
		return;
	}
	
	//request common memory window
	bFound = FALSE;
	for (dwMemWNum = 0; dwMemWNum < MAX_NUM_WINDOWS; dwMemWNum ++){
		cwParams.fAttributes = 0;
		cwParams.fAccessSpeed = WIN_SPEED_USE_WAIT;
		cwParams.uWindowSize = DEFAULT_MEMWIN_SIZE;
		hMemWin[dwMemWNum]= NULL;
		SetLastError(0);
		hMemWin[dwMemWNum]= CardRequestWindow(g_hClient, &cwParams);
		if(hMemWin[dwMemWNum] == NULL){
			break;
		}
		bFound = TRUE;
	}  
	if(bFound == TRUE){
		g_pKato->Log(LOG_DETAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: total %d common memory windows found! \r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  dwMemWNum);
	}
	else{
		g_pKato->Log(LOG_DETAIL,(LPTSTR)TEXT(" Thread %u Socket %u Func %u: WARNING: No common memory window found! \r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
	}

	//request IO windows
	bFound = FALSE;
	for (dwIOWNum = 0; dwIOWNum < MAX_NUM_WINDOWS; dwIOWNum ++){
		cwParams.fAttributes = WIN_ATTR_IO_SPACE;
		cwParams.fAccessSpeed = WIN_SPEED_USE_WAIT;
		cwParams.uWindowSize = DEFAULT_IOWIN_SIZE;
		hIOWin[dwIOWNum]= NULL;
		SetLastError(0);
		hIOWin[dwIOWNum]= CardRequestWindow(g_hClient, &cwParams);
		if(hIOWin[dwIOWNum] == NULL){
			break;
		}
		bFound = TRUE;
	}  
	if(bFound == TRUE){
		g_pKato->Log(LOG_DETAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: total %d IO windows found! \r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  dwIOWNum);
	}
	else{
		g_pKato->Log(LOG_DETAIL,(LPTSTR)TEXT(" Thread %u Socket %u Func %u: WARNING: No IO window found! \r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
	}


	//now map these windows

	//map attribute window
	pVirtAddr = CardMapWindow(hAttriWin, 0, DEFAULT_MEMWIN_SIZE, &uGranularity);
	if(pVirtAddr == NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: Can not map attribute window! Error = 0x%lx\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  GetLastError());
		SetResult(FALSE);
		goto LReleaseWindow;
	}

	//we only map common mem windows
	pVirtAddr = NULL;
	pVirtAddr = CardMapWindow(hMemWin[0], 0, DEFAULT_MEMWIN_SIZE, &uGranularity);
	dwError = GetLastError();
	if(pVirtAddr == NULL && dwError != CERR_OUT_OF_RESOURCE){
		
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: Can not map common mem window! Error = 0x%lx\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc, dwError);
		SetResult(FALSE);
		goto LReleaseWindow;
	}

	//map IO windows
	EnterCriticalSection(&m_cs);
	UINT uStartAddr = m_uIOBaseAddr;
	for(dwIndex = 0; dwIndex < dwIOWNum; dwIndex++){
		pVirtAddr = NULL;
		g_pKato->Log(LOG_DETAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: mapping IO window: base address = 0x%x, size = 0x%x \r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc, uStartAddr, DEFAULT_IOWIN_SIZE);
		pVirtAddr = CardMapWindow(hIOWin[dwIndex], uStartAddr, DEFAULT_IOWIN_SIZE, &uGranularity);
		dwError = GetLastError();
		if(pVirtAddr == NULL && dwError != CERR_OUT_OF_RESOURCE){
			g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: Can not map IO window no. %d! Error = 0x%lx\r\n"),
						  m_dwThreadID, m_uLocalSock, m_uLocalFunc, dwIndex,  dwError);
			SetResult(FALSE);
			goto LReleaseWindow;
		}
             else{
			g_pKato->Log(LOG_DETAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map IO window succeeded!\r\n"),
						  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
             }
		WinTest::m_uIOBaseAddr +=  DEFAULT_IOWIN_SIZE;
		uStartAddr = m_uIOBaseAddr;
	}

      LeaveCriticalSection(&m_cs);
	//sth. bad may happen?
	Sleep(TEST_IDLE_TIME);

LReleaseWindow:
	//release attribute window
	status = CardReleaseWindow(hAttriWin);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: release attribute window failed! return = 0x%lx\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  status);
		SetResult(FALSE);
	}

	//release common mem windows
	for(dwIndex = 0; dwIndex < dwMemWNum; dwIndex++){
		status = CardReleaseWindow(hMemWin[dwIndex]);
		if(status != CERR_SUCCESS){
			g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: release common mem window no. %d failed! return = 0x%lx\r\n"),
						  m_dwThreadID, m_uLocalSock, m_uLocalFunc, dwIndex, status);
			SetResult(FALSE);
		}
	}

	//release IO windows
	for(dwIndex = 0; dwIndex < dwIOWNum; dwIndex++){
		status = CardReleaseWindow(hIOWin[dwIndex]);
		if(status != CERR_SUCCESS){
			g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: release IO window no. %d failed! return = 0x%lx\r\n"),
						  m_dwThreadID, m_uLocalSock, m_uLocalFunc, dwIndex, status);
		}
	}


	DEBUGMSG(ZONE_FUNCTION, (TEXT("- WinTest::Test_CardRequestandMapWindow() exit\r\n")));

}

VOID WinTest::Test_InvalidParas (){

	CARD_WINDOW_HANDLE hWin;
	CARD_SOCKET_HANDLE	hSocket = {m_uLocalSock, m_uLocalFunc};
	CARD_WINDOW_PARMS	cwParams;
	UINT32				uGranularity = 1;
	PVOID				pVirtAddr = NULL, pVirtAddr1 = NULL;
	STATUS				status;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ WinTest::Test_InvalidParas()\r\n")));


	cwParams.hSocket = hSocket;
	cwParams.fAttributes = 0;
	cwParams.fAccessSpeed = WIN_SPEED_USE_WAIT;
	cwParams.uWindowSize = DEFAULT_MEMWIN_SIZE;
	hWin = NULL;

	//request window using null WINPARAMS, should fail
	hWin = CardRequestWindow(g_hClient, NULL);
	if(hWin != NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: Request mem window using NULL winParam should fail!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}

	//request window using wrong socket numer, should fail
	cwParams.hSocket.uSocket = m_uLocalSock + TEST_MAX_CARDS;
	hWin = CardRequestWindow(g_hClient, &cwParams);
	if(hWin != NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: Request mem window using wrong socket number should fail!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		return;
	}
	cwParams.hSocket.uSocket = m_uLocalSock;

	//request window using wrong function numer, should fail
	cwParams.hSocket.uFunction = m_uLocalFunc + TEST_MAX_CARDS;
	hWin = CardRequestWindow(g_hClient, &cwParams);
	if(hWin != NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: Request mem window using wrong socket number should fail!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	cwParams.hSocket.uFunction = m_uLocalFunc;

	//normal request, should success
	hWin = CardRequestWindow(g_hClient, &cwParams);
	if(hWin == NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: Request mem window failed!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}

	
	//map null window, should fail
	pVirtAddr = CardMapWindow(NULL, 0, DEFAULT_MEMWIN_SIZE, &uGranularity);
	if(pVirtAddr != NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map NULL window should fail!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}

	//map window with invalid offset, should fail
	pVirtAddr = CardMapWindow(hWin, (UINT32)-1, DEFAULT_MEMWIN_SIZE, &uGranularity);
	if(pVirtAddr != NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map window with invalid offset should fail!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}

	//map window with invalid size, should fail
	pVirtAddr = CardMapWindow(hWin, 0, (UINT32)-1, &uGranularity);
	if(pVirtAddr != NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map window with invalid size should fail!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}

	//map window, should success
	pVirtAddr = CardMapWindow(hWin, 0, DEFAULT_MEMWIN_SIZE, &uGranularity);
	if(pVirtAddr == NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map window failed!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}

	//map this window again, should success with same virtual address returned
	pVirtAddr1 = CardMapWindow(hWin, 0, DEFAULT_MEMWIN_SIZE, &uGranularity);
	if(pVirtAddr1 == NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: re-map window failed!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}
	else if(pVirtAddr != pVirtAddr1){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: two mappings got different virtual address: (1st)0x%x != (2nd)0x%x!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc, pVirtAddr, pVirtAddr1);
		SetResult(FALSE);
		goto LReleaseWindow;
	}

	//release null window, should fail
	status = CardReleaseWindow(NULL);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: release null window should fail! return = 0x%lx\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  status);
		SetResult(FALSE);
	}


LReleaseWindow:
	//release attribute window
	status = CardReleaseWindow(hWin);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: release attribute window failed! return = 0x%lx\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  status);
		SetResult(FALSE);
	}

	//mapping a releaseed window, should fail
	pVirtAddr = CardMapWindow(NULL, 0, DEFAULT_MEMWIN_SIZE, &uGranularity);
	if(pVirtAddr != NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map released window should fail!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}
	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- WinTest::Test_InvalidParas()\r\n")));

}

