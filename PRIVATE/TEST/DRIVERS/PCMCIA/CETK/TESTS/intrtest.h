//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#ifndef _INTRTEST_H_
#define _INTRTEST_H_

#include "minithread.h"

//config test parameters
typedef struct {
	int    	nTestFunc;
	int    	fMemory  ;
	UINT8  	uConfigReg;
	UINT32   	uCallRR  ;
	int    	nHCLIENT1; 
	int    	nISRFUNC ;
    	UINT32 	uISRData;
	STATUS 	uExpectR;
	int		nHCLIENT2;
	STATUS  uExpectR2;
}  INTRTESTPARAMS, *LPINTRTESTPARAMS;

#define ISR_LOG_EVENTS  2048

typedef struct{
    UINT8 EventNum;
    UINT8 Reg1;
    UINT8 Reg2;
} ISR_LOG_BUFFER, *PISR_LOG_BUFFER;

#define CALL_REQUEST_CONFIG    0x01
#define CALL_RELEASE_CONFIG    0x10
#define CALL_REQUEST_IRQ     0x0100
#define CALL_RELEASE_IRQ     0x1000
#define CALL_REQUEST_EXCL  0x010000
#define CALL_RELEASE_EXCL  0x100000


#define LOG_LSR_EVENT         1
#define LOG_RBR_EVENT         2
#define LOG_TXE_EVENT         3
#define LOG_MSR_EVENT         4
#define LOG_TXE_TRANS_EVENT	  5


#define MAX_INTRCASES 31

#ifndef _STDEF_
#define _STDEF_	

static INTRTESTPARAMS intrarr[MAX_INTRCASES] =
{
// **nTestFunc  fMemory  uConfigReg uCallRR  nHCLIENT1  nISRFUNC  uISRData  uExpectR       nHCLIENT2  uExpectR2
// ** ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// for Modem: address = 0x2E8:  uConfigReg = 35:
{ 0x100,  0,    35,     0x111111,     0x1,    0x1,      (UINT)-1,     CERR_SUCCESS,      1,     CERR_SUCCESS },
// Case 2 : Test in Threads
// Thread testing:   each thread uses the hard coded socket #.  define in testintr.hdwLogStatus
// for Modem: address = 0x3E8:  uConfigReg = 34:
{ 0x100,   0,       34,     0x111111,     1,     1,      (UINT)-1,     CERR_SUCCESS,      1,        CERR_SUCCESS },
// Case 3 - 5
// Test continuosly dial out in the same config address
// for Modem: address = 0x2F8:  uConfigReg = 33: : Use Call OUT Stream.
{ 0x100,      0,      33,     0x111111,     1,      1,      (UINT)-1,     CERR_SUCCESS,      1,       CERR_SUCCESS },
{ 0x100,      0,      33,     0x111111,     1,       1,      (UINT)-1,     CERR_SUCCESS,      1,      CERR_SUCCESS },
{ 0x100,       0, 0x80|33,     0x111111,     1,    1,      (UINT)-1,     CERR_SUCCESS,      1,      CERR_SUCCESS },
// Paramter checking:
// For CardRequestIRQ:
// Case 6:   for Modem: address = 0x3E8: uConfigReg = 34:					NULL hClient
{ 0x100,     0,       34,     0x111111,     0,      1,      (UINT)-1,    CERR_BAD_HANDLE,     1,      CERR_BAD_ARGS  },
//                                                                                   Bad socket=3
{ 0x100,     0,       34,     0x111111,     4,      1,      (UINT)-1,    CERR_BAD_SOCKET,     1,      CERR_BAD_ARGS  },
//                                                                                       Bad Function=100
{ 0x100,     0,       34,     0x111111,     4,      1,      (UINT)-1,    CERR_BAD_SOCKET,     1,      CERR_BAD_ARGS  },
//                                                                                              No callback ISR function
{ 0x100,     0,       34,     0x111111,     1,        0,    (UINT)-1,    CERR_BAD_ARGS,       1,       CERR_BAD_ARGS  },
//

//
// Case 10:   Test CardReleaseIRQ()
// for Modem: address = 0x2E8:  uConfigReg = 35:
{ 0x100,      0,       35,     0x111111,     1,        1,      (UINT)-1,     CERR_SUCCESS,       0,        CERR_BAD_HANDLE},
{ 0x100,      0,       35,     0x111111,     1,        1,      (UINT)-1,     CERR_SUCCESS,       4,        CERR_BAD_SOCKET},
{ 0x100,      0,       35,     0x111111,     1,        1,      (UINT)-1,     CERR_SUCCESS,       4,        CERR_BAD_SOCKET},
//

// Case 13: Call all ReleaeXXX() and passing in BAD_SOCKET: uSocket = 99 uFunction=69
//  x86 DeregisterClient() not clean everything
{ 0x100,    0,       35,     0x111111,     1,         1,      (UINT)-1,     CERR_SUCCESS,      4,  CERR_BAD_SOCKET},
// follow up good case
{ 0x100,    0,       35,     0x111111,     1,        1,      (UINT)-1,     CERR_SUCCESS,       1,  CERR_SUCCESS},
//

// Case 15: for Modem: address = 0x2E8:  uConfigReg = 35:
// Not call ReleaseConfiguration():
// in BUILD 326:  ReleaseIRQ() succeeded:  DeregsiterClient() will clean out everything
{ 0x100,         0,       35,     0x111101,     1,       1,      (UINT)-1,     CERR_SUCCESS,      1,     CERR_SUCCESS},
//	 Following a good case to check the result of above case.
{ 0x100,         0,       35,     0x111111,     1,        1,      (UINT)-1,     CERR_SUCCESS,      1,      CERR_SUCCESS },

// for Modem: address = 0x3F8:  uConfigReg = 32: conflic with PC (x86): so put at the last
//  Not call CardReleaseIRQ()
{ 0x100,      0,       32,     0x110111,     1,   1,      (UINT)-1,     CERR_SUCCESS,      1,       CERR_SUCCESS },
// follow up good case:
// for Modem: address = 0x3F8:  uConfigReg = 32: conflic with PC (x86): so put at the last
{ 0x100,        0,       32,     0x111111,     1,       1,      (UINT)-1,     CERR_SUCCESS,      1,      CERR_SUCCESS },

// Not call CardReleaseExclusive():
{ 0x100,         0,      34,     0x011111,     1,     1,      (UINT)-1,     CERR_SUCCESS,      1,      CERR_SUCCESS },
// follow up good case:
{ 0x100,        0,      34,     0x111111,     1,         1,      (UINT)-1,     CERR_SUCCESS,      1,     CERR_SUCCESS },

};
#endif // _STDEF_

class IntrTest : public MiniThread {
public:
	IntrTest(DWORD dwTestCase, DWORD dwThreadNo, USHORT	uSock, USHORT uFunc) :
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
	BOOL LaunchISRLogThread();
	BOOL EndISRLogThread();
	~IntrTest(){;};
private:
	virtual DWORD ThreadRun();
	static DWORD WINAPI LogIsrThread( UINT Nothing );
	VOID InitIRQRegister(PCLIENT_CONTEXT pData);
	VOID StartIRQ(PCLIENT_CONTEXT pData);

	
	DWORD	dwCaseID;
	DWORD 	dwThreadID;
	int    	nTestFunc,	fMemoryCard;
	UINT8  	uConfigReg;

	//==============================================================
	// For CardRequestIRQ()
	//==============================================================
	UINT32 	uCallRR   ;
	int    	nHCLIENT  ;
	int    	nISRFUNC  ;
	UINT32 	uISRData  ;
	STATUS 	uExpectRet;

	//==============================================================
	// For CardReleaseIRQ()
	//==============================================================
	int    	nHCLIENT2  ;
	STATUS 	uExpectRet2;


	int    	nThreadsDone;
	int     	nThreadsDone2;   // for check CardResetFunction()
	int     	nThreadsExit;
	int     	nStream;
	UINT32	uCardAddress;
	TCHAR   Result[6];


	USHORT	uLocalSock, uLocalFunc;

	// used between SubTestConfig() and Test_SubXX()
	BOOL	 fDeregisterClientDone;
	BOOL	 fReleaseWindowDone;
	BOOL	 fReleaseIRQDone;
	BOOL	 fReleaseExclDone;
	BOOL	 fReleaseConfigDone;

	
	CLIENT_CONTEXT client_data;
	
};

VOID ISRFunction(UINT32 uContext);
VOID LogIsrEvent( UINT8 Event, UINT8 Reg1, UINT8 Reg2 );

#endif

