//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#include "testmain.h"
#include "common.h"
#include "MaskTest.h"


BOOL
MaskTest::Init(){

	return TRUE;
}

DWORD MaskTest::ThreadRun() {
     if(IsTerminated() == TRUE)
        return GetLastError();

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ MaskTest::ThreadRun() enterted\r\n")));

	switch(m_dwCaseID){
        	case 1:  
        		CardEventMasksTest () ;    
        		break ;
        default:
        		g_pKato->Log(LOG_DETAIL,TEXT("Don't know how to handle test group %d\r\n"), m_dwCaseID);
        		SetResult(FALSE);
        		break ;
      }

	DEBUGMSG(ZONE_FUNCTION, (TEXT("- MaskTest::ThreadRun() \r\n")));
	
	return 0;
}

VOID MaskTest::CardEventMasksTest (){
    	CARD_EVENT_MASK_PARMS MaskParms;
	STATUS stat;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ MaskTest::EventMaskTest() enterted\r\n")));

	//test CardGetEventMask()
	//try null MaskParms
    	stat = CardGetEventMask(g_hClient, NULL);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardGetEventMask: Thread %u for Socket %u Func %u: Null MaskParms, should NOT return CERR_SUCCESS"),
				 m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try null client
	MaskParms.hSocket.uSocket = (UCHAR)m_uLocalSock ;
    	MaskParms.hSocket.uFunction = (UCHAR)m_uLocalFunc ;
    	stat = CardGetEventMask(NULL, &MaskParms);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardGetEventMask: Thread %u for Socket %u Func %u: Null client, should NOT return CERR_SUCCESS"),
				 m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto CLEANUP;
	}


	//now get mask
    	stat = CardGetEventMask(g_hClient, &MaskParms);
	if(stat != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardGetEventMask: Thread %u for Socket %u Func %u: NOT return CERR_SUCCESS"),
				 m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto CLEANUP;
	}

	UINT16 fOldMask = MaskParms.fEventMask;

	//test CardSetEventMask()
	//try null MaskParms
    	stat = CardSetEventMask(g_hClient, NULL);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardSetEventMask: Thread %u for Socket %u Func %u: Null MaskParms, should NOT return CERR_SUCCESS"),
				 m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//try null client
	MaskParms.hSocket.uSocket = (UCHAR)m_uLocalSock ;
    	MaskParms.hSocket.uFunction = (UCHAR)m_uLocalFunc ;
    	stat = CardSetEventMask(NULL, &MaskParms);
	if(stat == CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardSetEventMask: Thread %u for Socket %u Func %u: Null client, should NOT return CERR_SUCCESS"),
				 m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//now set event mask
	UINT16 fNewMask = (UINT16)(rand() %0xFFFF);
	MaskParms.fEventMask = fNewMask;
    	stat = CardSetEventMask(g_hClient, &MaskParms);
	if(stat != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardSetEventMask: Thread %u for Socket %u Func %u: NOT return CERR_SUCCESS"),
				 m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
	}

	//read out to compare:
    	stat = CardGetEventMask(g_hClient, &MaskParms);
	if(stat != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardGetEventMask: Thread %u for Socket %u Func %u: NOT return CERR_SUCCESS"),
				 m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
		goto CLEANUP;
	}
	if(fNewMask != MaskParms.fEventMask){
		g_pKato->Log(LOG_FAIL,TEXT("CardGetEventMask: Thread %u for Socket %u Func %u: the mask is not set!should be 0x%x, actually it is: 0x%x"),
				 m_dwThreadID, m_uLocalSock, m_uLocalFunc, fNewMask, MaskParms.fEventMask);
		SetResult(FALSE);
		goto CLEANUP;
	}

	//now restore the old mask value:
	MaskParms.fEventMask = fOldMask;
    	stat = CardSetEventMask(g_hClient, &MaskParms);
	if(stat != CERR_SUCCESS){
		g_pKato->Log(LOG_FAIL,TEXT("CardSetEventMask: Thread %u for Socket %u Func %u: NOT return CERR_SUCCESS"),
				 m_dwThreadID, m_uLocalSock, m_uLocalFunc);
		SetResult(FALSE);
	}

CLEANUP:	
	DEBUGMSG(ZONE_FUNCTION, (TEXT("- MaskTest::EventMaskTest()\r\n")));

}
	
