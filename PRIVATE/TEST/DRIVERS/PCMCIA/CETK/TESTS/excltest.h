//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#ifndef _EXCLTEST_H_
#define _EXCLTEST_H_

#include "minithread.h"

//exclusiveaccess test parameters
typedef struct {
	int		nTestFunc;
	int    	fManualAction;
	UINT   	fAttriC   ;     // For CardRegisterClient()
	int    	uCallRR   ;
	int    	nHCLIENT  ;
	STATUS 	uExpectRet;	
	UINT32 	uEventGotExpected ;
	UINT32 	uEventGotExpected1;
	int    	nHCLIENT2  ;
	STATUS 	uExpectRet2;	
	UINT32 	uEventGotExpected2 ;
}  EXCLTESTPARAMS, *LPEXCLTESTPARAMS;

#define TST_INIT_RATS_FAILED		0
#define TST_EXCLUSIVE_FAILED		2
#define TST_EXCLUSIVE2_FAILED		3

#define MAX_NAME_LENGTH   64
#define MAX_EVENTS         5

#define EX_STATE_UNOWNED    0
#define EX_STATE_REQUESTING 1
#define EX_STATE_ALMOST     2
#define EX_STATE_OWNED      3
#define EX_STATE_RELEASING  4

#define CALL_REQUEST 0x01
#define CALL_RELEASE 0x10

#define MAX_CASES_EXCL 12

#ifndef _STDEF_
#define _STDEF_	

static EXCLTESTPARAMS exclarr[MAX_CASES_EXCL]={
 // **					 							             fAttriC for CardRegisterClient()  CardRequestExclusive()						  								  |  CardReleaseExclusive()
// ** ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//nTestF fManual fAttriC  uCallRR   nHCLIENT1  uExpectR1  uEvent   uEvent1  nHCLIENT2   uExpectR2  uEvent2
// ** ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// nTestFunc = 0x0: using  slot0 in all other CARDxxxx apis.
// fAttri = 0x9: MEMORY Client:
{ 0x0,   0,        0x9,   0x11,     1,    0x00,     0x60,    0x0,       1,   0x00,     0x45  },
// fAttri = 0xA: IO Client:
{ 0x0,   0,        0xA,   0x11,     1,    0x00,     0x60,    0x0,        1,  0x00,     0x45  },
// fAttri = 0xA: IO Client:  Not calling RequestExclusive
{ 0x0,   0,        0xA,   0x10,     1,    0x00,     0x60,    0x0,        1,  0x1E,     0x0  },
// Paramter checkinG BAD_HANDLE
{ 0x0,      0,     0x9,   0x11,     0,    0x21,     0x00,      0x0,         1,   0x1E,     0x40  },
{ 0x0,      0,     0xA,   0x11,     2,    0x21,     0x00,      0x0,         1,   0x1E,     0x40  },
// case 6: Only release window, CardGetExclusive() still should succeed
{ 0x0,      0,        0xA,   0x11,    3,   0,        0x60,      0x0,         1,    0x00,     0x45  },
// Case 7:		 invalid socket 0xFF : expect return CERR_BAD_SOCKET:  return CERR_NO_CARD: WON't FIX
{ 0x0,     0,     0x9,   0x11,     4,      0x14,     0x00,      0x0,         1,    0x1E,     0x40  },
// Case 8,9: Parameter checking: Not calling RequestExclusive: only test ReleaseExclusive
{ 0x0,     0,        0xA,   0x10,     1,     0x00,     0x60,      0x0,         0,     0x21,     0x00  },
{ 0x0,     0,        0xA,   0x10,     1,     0x00,     0x60,      0x0,         4,     0x0B,     0x00  },
// Case 10:	5 threads: each has one client:  Case 2823  currently hang due to thread2 not getting CE_EXCLUSIVE_COMPLETE
// register 1 clinet for  each Thread: start from  client_data 0:
{ 0x0,      0,        0xA,    0x11,     1,    0x00,     0x60,     0x80,        1,     0x00,     0x40  },
// Cas 11: 2 Threads: return CERR_IN_USE when get CE_EXCLUSIVE_REQUEST:
// So no Client get EXCLUSIVE.
//  expected second thread failing to get the exclusive: Request as memory window
{ 100,    0,        0xC,   0x11,     1,       0x00,     0x60,   0x60,          1,     0x00,     0x45  },
//Case 12: Need to run twice:
//	 call CardRequestExclusive(),  BUT DON'T call CardReleaseExclusive()
{ 0x0,    0,        0x9,  0x01,     1,        0x00,     0x60,       0x0,         1,    0x00,     0x45  },
};
#endif // _STDEF_

class ExclTest : public MiniThread {
public:
	ExclTest(DWORD dwTestCase, DWORD dwThreadNo, USHORT	uSock, USHORT uFunc) :
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
	~ExclTest(){;};
private:
	virtual DWORD ThreadRun();
	
	DWORD	dwCaseID;
	DWORD 	dwThreadID;
	int		nTestFunc, fManualAction, nHCLIENT, nHCLIENT2;
	UINT	fAttriC;
	STATUS 	uExpectRet, uExpectRet2;
	UINT32 	uEventGotExpected,  uEventGotExpected1,  uEventGotExpected2;
	UINT32   uCallRR   ;
	UINT    	uSocketForWindow;    // get from  nTestFunc & 0x1: CardRequestWindow() should always succeeds
	DWORD   LastError     ;  // Was used to call SetLastError before return.
	USHORT	uLocalSock, uLocalFunc;
	CLIENT_CONTEXT client_data;
	
};

#endif

