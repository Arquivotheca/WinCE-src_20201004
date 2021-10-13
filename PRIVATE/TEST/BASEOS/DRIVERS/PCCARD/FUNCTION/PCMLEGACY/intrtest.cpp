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
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
/*++
Module Name:  
	Intrtest.cpp

Abstract:

    Tests related with Request/Release IRQs 

--*/

#include "testmain.h"
#include "common.h"
#include "IntrTest.h"



BOOL
IntrTest::Init(){

	if(NormalRequestConfig(g_hClient, m_uLocalSock, m_uLocalFunc) == FALSE){
		return FALSE;
	}
	return TRUE;
}


DWORD IntrTest::ThreadRun() {

     if(IsTerminated() == TRUE)
        return GetLastError();

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ IntrTest::ThreadRun() enterted\r\n")));
	
	switch(m_dwCaseID){
		case 1: //test on CardRequestConfiguration
			Test_CardRequestIRQ();
			break;
		default:	
   			g_pKato->Log(LOG_DETAIL,TEXT("***Thread %u for Socket %u Func %u: Invalid test case\r\n"), 
   			                                        m_dwThreadID, m_uLocalSock, m_uLocalFunc);
			break;
	}

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- IntrTest::ThreadRun()\r\n")));

	return GetLastError();
}


/* 
@Func ISRFunction is installed by the call to function CardRequestIRQ...
CardServices calls this ISRFunction when....
*/
VOID ISRFunction(UINT32 uContext)
{
	return;
}   // LogDisplayThread

VOID IntrTest::Test_CardRequestIRQ(){
	STATUS status;
	CARD_SOCKET_HANDLE hSocket = {m_uLocalSock, m_uLocalFunc};

	//normal request IRQ
	status = CardRequestIRQ(g_hClient, hSocket, ISRFunction, 0);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Func %u: request IRQ failed!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;

	}

	//wait for a while, to see whether something unexpected will happen or not
	Sleep(TEST_IDLE_TIME);
	
	//request again, should fail
	status = CardRequestIRQ(g_hClient, hSocket, ISRFunction, 0);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Func %u: The secound request IRQ call should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}

	//release IRQ using invalid socket number, should fail
	hSocket.uSocket= m_uLocalSock + TEST_MAX_CARDS;
	status = CardReleaseIRQ(g_hClient, hSocket);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardReleaseIRQ(),  Thread %u for Socket %u Func %u: Release IRQ using invalid socket number should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	hSocket.uSocket = m_uLocalSock;
	
	//relase IRQ using invalid function number, should fail
	hSocket.uFunction = m_uLocalFunc + TEST_MAX_CARDS;
	status = CardReleaseIRQ(g_hClient, hSocket);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardReleaseIRQ(),  Thread %u for Socket %u Func %u: Release IRQ using invalid function number should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	hSocket.uFunction = m_uLocalFunc;

	//release IRQ
	status = CardReleaseIRQ(g_hClient, hSocket);
	if(status != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Func %u: Release IRQ failed!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}

	
	//use invalid socket number, should fail
	hSocket.uSocket= m_uLocalSock + TEST_MAX_CARDS;
	status = CardRequestIRQ(g_hClient, hSocket, ISRFunction, 0);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Func %u: Request IRQ using invalid socket number should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	hSocket.uSocket = m_uLocalSock;
	
	//use invalid function number, should fail
	hSocket.uFunction = m_uLocalFunc + TEST_MAX_CARDS;
	status = CardRequestIRQ(g_hClient, hSocket, ISRFunction, 0);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Func %u: Request IRQ using invalid function number should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	hSocket.uFunction = m_uLocalFunc;

	//pass null ISR function in, should fail
	status = CardRequestIRQ(g_hClient, hSocket, NULL, 0);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardRequestIRQ(),  Thread %u for Socket %u Func %u: Request IRQ using invalid ISR function should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		return;
	}
	
	//release again, should fail
	status = CardReleaseIRQ(g_hClient, hSocket);
	if(status == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT(":Test_CardReleaseIRQ(),  Thread %u for Socket %u Func %u: The secound release IRQ call should fail!\r\n"),
									m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
	}

}
