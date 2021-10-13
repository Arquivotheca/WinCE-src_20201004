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

UINT32 WinTest::m_uIOBaseAddr = 4;

BOOL
WinTest::Init(){

      InitializeCriticalSection(&m_cs);
	return TRUE;
}

//constant defines only used in the following ThreadRun() API.
#define	WINTST_BASE_ADDR		0x3f8
#define	WINTST_WIN_SIZE		8
#define	WINTST_WIN_INTERVAL	0x10

DWORD WinTest::ThreadRun() {

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
#define	DEFAULT_MEMWIN_SIZE	0x100000   //  1M
#define	DEFAULT_IOWIN_SIZE	0x100	//  1k	

VOID WinTest::Test_CardRequestandMapWindow (){

	CARD_WINDOW_HANDLE hIOWin[MAX_NUM_WINDOWS], hMemWin[MAX_NUM_WINDOWS];
	DWORD	dwIOWNum, dwMemWNum;
	CARD_SOCKET_HANDLE	hSocket = {m_uLocalSock, m_uLocalFunc};
	CARD_WINDOW_PARMS	cwParams;
	BOOL				bFound;
	UINT32				uGranularity = 1;
	PVOID				pVirtAddr = NULL;
	DWORD				dwIndex;
	STATUS				status;
	CARD_WINDOW_ADDRESS	cwAddr= {0};
	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ WinTest::Test_CardRequestandMapWindow() enterted\r\n")));

	//for MFC cards, if it is not the first function, it may not be able to get attrib window and common window
	if(m_uLocalFunc > 0)
		return;

	cwParams.hSocket = hSocket;
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

      EnterCriticalSection(&m_cs);
	//we only map first common mem window
	cwAddr.hSocket.uFunction = m_uLocalFunc;
	cwAddr.hSocket.uSocket = m_uLocalSock;
	cwAddr.uCardAddress =0x10;
	cwAddr.uSize = DEFAULT_MEMWIN_SIZE - 4;
	status = CardMapWindowPhysical(hMemWin[0], &cwAddr);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: Can not map common mem window! Error = 0x%lx\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  status);
		SetResult(FALSE);
		goto LReleaseWindow;
	}
	else if(cwAddr.uCardAddress > 0x10){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map common mem window! Error uCardAddress 0x%x > 0x10(expected)\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  cwAddr.uCardAddress);
		SetResult(FALSE);
		goto LReleaseWindow;
	}
	else if(cwAddr.hSocket.uSocket!= m_uLocalSock){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map common mem window! Error socket ID %d != %d(expected)\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  cwAddr.hSocket.uSocket, m_uLocalSock);
		SetResult(FALSE);
		goto LReleaseWindow;
	}
	else if(cwAddr.hSocket.uFunction != m_uLocalFunc ){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map common mem window! Error function ID %d != %d(expected)\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  cwAddr.hSocket.uFunction, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}
	else if(cwAddr.uWindowPhAddr == NULL){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map common mem window! Error physical addr should not be NULL\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}

	//map IO windows
	cwAddr.hSocket.uFunction = m_uLocalFunc;
	cwAddr.hSocket.uSocket = m_uLocalSock;
	cwAddr.uCardAddress = m_uIOBaseAddr;
	cwAddr.uSize = DEFAULT_IOWIN_SIZE - 4;
	for(dwIndex = 0; dwIndex < dwIOWNum; dwIndex++){
		g_pKato->Log(LOG_DETAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: Before mapping IO window: base address = 0x%x, size = 0x%x \r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc, cwAddr.uCardAddress, cwAddr.uSize);
		status = CardMapWindowPhysical(hIOWin[dwIndex], &cwAddr);
		g_pKato->Log(LOG_DETAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: After mapping IO window: base address = 0x%x, size = 0x%x \r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc, cwAddr.uCardAddress, cwAddr.uSize);
		if(status != CERR_SUCCESS ){
			g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT(" WARNING!!! Thread %u Socket %u Func %u: Can not map IO window! Error = 0x%lx\r\n"),
						  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  status);
			g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT(" WARNING!!! Please verify this failure is caused IO resource is not avaliable. other causes should be considered as a BUG!!!"));
		}
		else if(cwAddr.hSocket.uSocket!= m_uLocalSock){
			g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map IO window! Error socket ID %d != %d(expected)\r\n"),
						  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  cwAddr.hSocket.uSocket, m_uLocalSock);
			SetResult(FALSE);
			goto LReleaseWindow;
		}
		else if(cwAddr.hSocket.uFunction != m_uLocalFunc ){
			g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map IO window! Error function ID %d != %d(expected)\r\n"),
						  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  cwAddr.hSocket.uFunction, m_uLocalFunc);
			SetResult(FALSE);
			goto LReleaseWindow;
		}
		else if(cwAddr.uWindowPhAddr == NULL){
			g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("WARNING!!!  Thread %u Socket %u Func %u: map IO window! Error physical addr should not be NULL\r\n"),
						  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
			g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("WARNING!!!Please verify that physical address can start from 0 !!!  "));
		}
             else{
			g_pKato->Log(LOG_DETAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map IO window succeeded!, phywinaddr = 0x%x\r\n"),
						  m_dwThreadID, m_uLocalSock, m_uLocalFunc, cwAddr.uWindowPhAddr);
             }

		WinTest::m_uIOBaseAddr =  cwAddr.uCardAddress + cwAddr.uSize + 4;
		cwAddr.uCardAddress = WinTest::m_uIOBaseAddr;
        	cwAddr.uSize = DEFAULT_IOWIN_SIZE - 4;
	}
	//sth. bad may happen?
	Sleep(TEST_IDLE_TIME);

LReleaseWindow:
	//release common mem windows
	for(dwIndex = 0; dwIndex < dwMemWNum; dwIndex++){
		status = CardReleaseWindow(hMemWin[dwIndex]);
		if(status != CERR_SUCCESS){
			g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: release common mem window no. %d failed! return = 0x%lx\r\n"),
						  m_dwThreadID, m_uLocalSock, m_uLocalFunc, dwIndex, status);
			SetResult(FALSE);
		}
		else{
			NKDbgPrintfW(_T("Rleased mem window %d"), dwIndex);
		}
	}

	//release IO windows
	for(dwIndex = 0; dwIndex < dwIOWNum; dwIndex++){
		status = CardReleaseWindow(hIOWin[dwIndex]);
		if(status != CERR_SUCCESS){
			g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: release IO window no. %d failed! return = 0x%lx\r\n"),
						  m_dwThreadID, m_uLocalSock, m_uLocalFunc, dwIndex, status);
		}
		else{
			NKDbgPrintfW(_T("Rleased IO window %d"), dwIndex);
		}
	}


      LeaveCriticalSection(&m_cs);

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- WinTest::Test_CardRequestandMapWindow() exit\r\n")));

}

VOID WinTest::Test_InvalidParas (){

	CARD_WINDOW_HANDLE hWin;
	CARD_SOCKET_HANDLE	hSocket = {m_uLocalSock, m_uLocalFunc};
	CARD_WINDOW_PARMS	cwParams;
	UINT32				uGranularity = 1;
	PVOID				pVirtAddr = NULL, pVirtAddr1 = NULL;
	STATUS				status;
	CARD_WINDOW_ADDRESS	cwAddr= {0};
	UINT uOldAddr;

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
		SetResult(FALSE);
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

	
	cwAddr.hSocket.uFunction = m_uLocalFunc;
	cwAddr.hSocket.uSocket = m_uLocalSock;
	cwAddr.uCardAddress =0x10;
	cwAddr.uSize = DEFAULT_MEMWIN_SIZE - 4;

	//map null window, should fail
	status = CardMapWindowPhysical(NULL, &cwAddr);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map NULL window should fail!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}

	//map null winaddr, should fail
	status = CardMapWindowPhysical(hWin, NULL);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map window using NULL addr structure should fail!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}

	//map window using invalid ucardaddr, should fail
	cwAddr.uCardAddress =0xFFFFFFFF;
	status = CardMapWindowPhysical(hWin, &cwAddr);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map window using invalid uCardAddr should fail!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}
	cwAddr.uCardAddress = 0x10;

	//map window using invalid size, should fail
	cwAddr.uSize = 0xFFFFFFFF;
	status = CardMapWindowPhysical(hWin, &cwAddr);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map window using invalid uSize should fail!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}
	cwAddr.uSize = DEFAULT_MEMWIN_SIZE - 4;

	//map window with normal input, should success
	status = CardMapWindowPhysical(hWin, &cwAddr);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map window failed!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}

	cwAddr.uSize = DEFAULT_MEMWIN_SIZE - 8;
	uOldAddr = cwAddr.uCardAddress =0x100;
	//map this window again, should success with updated uCardAddr
	status = CardMapWindowPhysical(hWin, &cwAddr);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: 2nd mapping window failed!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto LReleaseWindow;
	}
	else if(uOldAddr < cwAddr.uCardAddress){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: 2nd mapping: uCardAddres 0x%x > original addr 0x%x\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc, cwAddr.uCardAddress, uOldAddr);
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
	//release window
	status = CardReleaseWindow(hWin);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: release attribute window failed! return = 0x%lx\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc,  status);
		SetResult(FALSE);
	}

	//mapping a releaseed window, should fail
	status = CardMapWindowPhysical(hWin, &cwAddr);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,(LPTSTR)TEXT("  Thread %u Socket %u Func %u: map window after window released should fail!\r\n"),
					  m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
	}
	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- WinTest::Test_InvalidParas()\r\n")));

}

