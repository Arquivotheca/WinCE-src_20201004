//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#ifndef _MASKTEST_H_
#define _MASKTEST_H_

#include "minithread.h"

//config test parameters
typedef struct {
	int	nWhich;
	int 	nTrials;
}  MASKTESTPARAMS, *LPMASKTESTPARAMS;

#define REQUEST_EXCLUSIVE  8
#define MAX_NUM  10         
#define BADARGS   15

#define MAX_MASKCASES 18

#ifndef _STDEF_
#define _STDEF_	

static MASKTESTPARAMS maskarr[MAX_MASKCASES] =
{
//    nValue    trials   
  {    1,        6},
  {    1,        6},
  {    1,        6},
  {    2,        6},
  {  	 2,        6},
  {    2,        6},
  {    3,        6},
  {    3,        6},
  {    3,        6},
  {    4,        6},
  {    4,        6},
  {    4,        6},
  {    5,        1},
  {    6,        1},

};
#endif // _STDEF_


typedef STATUS (*CardCBack) (CARD_EVENT,CARD_SOCKET_HANDLE,PCARD_EVENT_PARMS) ;

class MaskTest : public MiniThread {
public:
	MaskTest(DWORD dwTestCase, DWORD dwThreadNo, USHORT	uSock, USHORT uFunc) :
			MiniThread (0,THREAD_PRIORITY_NORMAL,TRUE),
			dwCaseID(dwTestCase),
			dwThreadID(dwThreadNo),
			uLocalSock(uSock),
			uLocalFunc(uFunc){
		NKDMSG(TEXT("ConfigTest: CaseID: %u; Thread No. %u; Socket %u; Function %u"), 
					dwCaseID, dwThreadID, uLocalSock, uLocalFunc);
		SetResult(TRUE);
	};

	BOOL Init();
    	VOID RequestMask (UINT16 mask) ;
    	VOID ReleaseMask () ; // Note help on CardReleaseSocketMask
    	VOID GetEventMask () ;
    	VOID SetEventMask (UINT16) ;
    	VOID GetStatus () ;
    	VOID ResetFunction () ;
    	STATUS GetCurrentStatus () ;
    	UINT16 GetCurrentMask () ;
    	VOID  Generic (UINT16) ;
	VOID EventMask_InvalidParaTest ();
	VOID SocketMask_InvalidParaTest ();
	~MaskTest(){;};

private:
	virtual DWORD ThreadRun();
    	VOID SetGlobal () ;
    	VOID SetLocal () ;
    	int  IsLocal () ;
    	int  IsGlobal () ;
	VOID CardGetStatusTest();
	VOID CardEventMasksTest ();
	VOID CardResetFunctionTest();
	VOID CardFunctionsTest();
	VOID CleanupSettings();
	VOID TestRegisterClient();
	
	DWORD	dwCaseID;
	DWORD 	dwThreadID;
	USHORT	uLocalSock, uLocalFunc;
	int		nWhich, nTrials;
	CARD_CLIENT_HANDLE hClient ;

    	// Useage data
    	STATUS  stat ;            // Most recent return state
    	UINT16  fMask ;           // Current event mask
    	CardCBack	callback;	
    	UINT16  fLocalExpMask ;   // Expected with calls to getEventMask
    	UINT16  fGlobalExpMask ;  // Expected with calls to getEventMask
    	UINT16  fAttr ;     // 0 = global, 0xFFFF = local
};

#endif

