//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#ifndef _WINTEST_H_
#define _WINTEST_H_

#include "minithread.h"

//config test parameters
typedef struct {
	int    	nTestFunc;
	int    	nClients ;
	UINT16 	fAttri       ;
	UINT8  	fAccessSpeed ;
	UINT32 	uWindowSize  ;

	int    	nHandle      ;	
	UINT   	uCardAddr    ;
	UINT   	uSize        ;	
	int    	nPGranularity;
	int    	nExpectRet	  ;
	DWORD  	dwExpectEr	  ;
							
	int    	nHandle2     ;
	UINT16 	fAttri2      ;
	UINT8  	fAccessSpeed2;
	STATUS 	uExpectRet   ;
}  WINTESTPARAMS, *LPWINTESTPARAMS;

//test options
#define TST_READ_TUPLE_FAILED					1
#define TST_MAPWINDOW_MASSIVE_FAILED		2
#define TST_MAPWINDOW_IN_THREAD_FAILED		3
#define TST_MAPWINDOW_FAILED					4
#define TST_READ_TUPLE_FAILED_IN_THREAD   5

#define MAX_NAME_LENGTH   64
#define MAX_EVENTS         5
#define EX_STATE_UNOWNED    0
#define EX_STATE_REQUESTING 1
#define EX_STATE_ALMOST     2
#define EX_STATE_OWNED      3
#define EX_STATE_RELEASING  4



#define MAX_WINCASES 28

#ifndef _STDEF_
#define _STDEF_	

static WINTESTPARAMS winarr[MAX_WINCASES] =
{
//**                                                                                                                                                      CardRequestWindow()                                                                                                                  CardMapWindow()                                                                                                                                           CardModifyWindow()
//** ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//**  nTestF nClients fAttri fAccessSpeed   uWindowSize  nHandle uCardAddr uSize  nPGra  nExpectRet  dwExpectEr   nHandle2  fAttri2 fAccessSpeed2  uExpectRet
//** ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Modem Card:
  { 0x1,   1,     5,  WIN_SPEED_USE_WAIT, 0x400,        1,     0x2E8,  0x10,   1,     1,          0,               1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
  { 0x1,   1,     5,  WIN_SPEED_USE_WAIT, 0x400,        1,     0x2F8,  0x40,   1,     1,          0,               1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
  { 0x1,   1,     5,  WIN_SPEED_USE_WAIT, 0x400,        1,     0x3E8,  0x8,    1,     1,          0,               1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
// For Modem CARD:
// Case 4: fAttri = 4: Memory window: uWindowSize: CEPC: PCM_MEM_WIN_SIZE =8K= 0x2000: Smallest one
  { 0x1,   1,      4,  WIN_SPEED_USE_WAIT, 0x400,      1,     0x0,   0x50, 1,     1,         0,                1,   0, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
// changing ret test case #5 to CERR_BAD_ARGS
// Case 5: fAttri = 6: Memory attribute window: uWindowSize: CEPC: PCM_MEM_WIN_SIZE =8K= 0x2000: Smallest 1
  { 0x1,   1,      6,  WIN_SPEED_USE_WAIT, 0x400,           1,  0x3F8,  0x100, 1,     1,         0,                1,   1, WIN_SPEED_USE_WAIT, CERR_BAD_ARGS},
// Case 6: fAttri = 7: IO Attri window: uWindowSize: CEPC: PCM_MEM_WIN_SIZE =64K= 0x10000: Smallest 1
  { 0x1,   1,      7,  WIN_SPEED_USE_WAIT, 0x400,           1,  0x3F8,  0x100, 1,     1,         0,                1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},

//
// ucardAddress = 0,   IO window:  
// Case 7:
  { 0x1,   1,      5,  WIN_SPEED_USE_WAIT, 0x400,        1,     0x0,    0x200,  1,     1,          0,               1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
// Case 8:       address=0xFE00: P1: CERR_SUCCESS:  ROSE17: CERR_OUT_OF_RESOURCE
  { 0x1,   1,      5,  WIN_SPEED_USE_WAIT, 0x400,        1,     0xFE00, 0x400,  1,     1, CERR_OUT_OF_RESOURCE,     1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
// Case 9:   uSize = 1 byte
  { 0x1,   1,      5,  WIN_SPEED_USE_WAIT, 0x400,      1,     0x12345,0x1,    1,     1,          0,               1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
//
//
// Case 10:
//       Allocate 2 bytes Map 4 bytes:  uCard Address = 0x2E8
 { 0x1,    1,      5,  WIN_SPEED_USE_WAIT, 0x2,             1,   0x2E8,  0x4,    1,     1,      CERR_BAD_SIZE ,      1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
//       Allocate 2 bytes Map 2 bytes:  uCard Address = 0x100
 { 0x1,     1,      5,  WIN_SPEED_USE_WAIT, 0x2,             1,   0x100,  0x2,    1,     1,          0,               1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
//
// Case 12:     Allocate 2 bytes Map 3 bytes:  uCard Address = 0
 { 0x1,     1,      5,  WIN_SPEED_USE_WAIT, 0x2,             1,   0x0,    0x3,    1,     1,      CERR_BAD_SIZE,       1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
// case 13      Allocate 2 bytes Map 1 bytes:  uCard Address = 0x2
  { 0x1,    1,      5,  WIN_SPEED_USE_WAIT, 0x2,             1,   0x0,    0x1,    1,     1,          0,               1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
// Case 14:
// Parameter checking:  CardMapWindow():
// hWindow = NULL,    Invalid
 { 0x1,    1,       5,  WIN_SPEED_USE_WAIT, 0x400,          0,   0x2F8,  0x100,  1,     0,     CERR_BAD_WINDOW,     1,   1, WIN_SPEED_USE_WAIT, CERR_BAD_WINDOW},
  { 0x1,   1,       5,  WIN_SPEED_USE_WAIT, 0x400,          2,   0x2F8,  0x100,  1,     0,     CERR_BAD_WINDOW,     1,   1, WIN_SPEED_USE_WAIT, CERR_BAD_WINDOW},
// Case 16:  pGranularity = NULL: Expect:t successful
  { 0x1,   1,       5,  WIN_SPEED_USE_WAIT, 0x400,          1,   0x2F8,  0x100,  0,     1,     0,                   1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
// Case 17:
// For CardModifyWindow()
// hWindow = NULL,    Invalid
 { 0x1,    1,       5,  WIN_SPEED_USE_WAIT, 0x200,          1,     0x2E8,  0x10,   1,     1,     CERR_SUCCESS,        0,   1, WIN_SPEED_USE_WAIT, CERR_BAD_WINDOW},
 { 0x1,    1,       5,  WIN_SPEED_USE_WAIT, 0x200,          1,     0x2E8,  0x10,   1,     1,     CERR_SUCCESS,        2,   1, WIN_SPEED_USE_WAIT, CERR_BAD_WINDOW},
// Case 19:  Test in Threads:
// fAttri = 5 ( IO_SPACE + ENABLE)
 { 0x1,     1,      5,  WIN_SPEED_USE_WAIT, 0x400,          1,     0x2F8,  0x100,   1,  1,          0x0,            1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
// Case 20:   10 thread
   { 0x1,   1,      5,  WIN_SPEED_USE_WAIT, 0x400,          1,     0x1,    0x16,    1,  1,          0x0,            1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
// 21
 { 0x1,     1,      5,  WIN_SPEED_USE_WAIT, 0x400,          1,     0x4F8,  0x500,   1,  1,          0x0,            1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
// Case 22:   Massive Testing: CEPC:  PCMCIA_MEM_WIN_SIZE 0x2000: smallest
   { 0x1000,  50,     4,  WIN_SPEED_USE_WAIT, 0x1000,          1,     0x10,   0x8,       1,  1,          0,               1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
// case 23: Test MapWindow  in 1 thread:   memory card map to 64MB
// register as memory card
// request  commom memory space: 800K   0xC5000:   map sixe = 800K : only can map once
// request  commom memory space: 1024K  0x100000: will fail in ROSE17: maximum 0xFeFF
// request  commom memory space: 32 K   0x8000
  { 0x10,    1,      4,  WIN_SPEED_USE_WAIT, 0x8000,          1,     0x0,    0x8000,    1,  1,          0x0,            1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
//
// Case 24:
//  Request less than 800 K window: 0xC5000 : ROSE20's limit for common space: com1:
  { 0x10,    1,      4,  WIN_SPEED_USE_WAIT, 0xC000,          1,     0x0,    0xC00,    1,  1,          0x0,            1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
//
// case 25: Test CardModifywindow(): hClinet = invalid
 { 0x1,    1,       5,  WIN_SPEED_USE_WAIT, 0x400,      1,     0x2E8,    0x10,    1,  1,     CERR_SUCCESS,        3,   1, WIN_SPEED_USE_WAIT, CERR_BAD_WINDOW},
// case 26: regression 
{ 0x1,    1,      7,  WIN_SPEED_USE_WAIT, 0x100,          1,     0,    0x200,    1,  1,          0x0,            1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
//case 27: Invalid parameter test 
  { 0x11,   1,     5,  WIN_SPEED_USE_WAIT, 0x400,        1,     0x2E8,  0x10,   1,     1,          0,               1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS},
//case 28: collide window test 
  { 0x12,   1,     5,  WIN_SPEED_USE_WAIT, 0x400,        1,     0x2E8,  0x10,   1,     1,          0,               1,   1, WIN_SPEED_USE_WAIT, CERR_SUCCESS}
};
#endif // _STDEF_

class WinTest : public MiniThread {
public:
	WinTest(DWORD dwTestCase, DWORD dwThreadNo, USHORT	uSock, USHORT uFunc) :
			MiniThread (0,THREAD_PRIORITY_NORMAL,TRUE),
			dwCaseID(dwTestCase),
			dwThreadID(dwThreadNo),
			uLocalSock(uSock),
			uLocalFunc(uFunc){
		NKDMSG(TEXT("WinTest: CaseID: %u; Thread No. %u; Socket %u; Function %u"), 
					dwCaseID, dwThreadID, uLocalSock, uLocalFunc);
		SetResult(TRUE);
	};
	BOOL Init();
	~WinTest(){;};
private:
	virtual DWORD ThreadRun();
	CARD_CLIENT_HANDLE  MyRegisterClient(UINT uClientAttri, PCLIENT_CONTEXT pData );
	BOOL RegisterClientAndWindow(UINT uClientAttri, CARD_CLIENT_HANDLE * phClient, CARD_WINDOW_HANDLE * phWindow, PCLIENT_CONTEXT pData );
	VOID Test_SpecialCase ();
	VOID Test_InvalidParas ();
	VOID Test_CollideWindow ();


	
	DWORD	dwCaseID;
	DWORD 	dwThreadID;
	BOOL      gfModemCard;
	int     	nTestFunc, nClients;

	//==============================================================
	// For CardRequestWindow()
	//==============================================================
	UINT16  	fAttri;
	UINT8   	fAccessSpeed;
	UINT32  	uWindowSize;

	//==============================================================
	// For CardMapWindow()
	//==============================================================
	int     	nHandle;
	UINT    	uCardAddr, uSize;
	int     	nPGranularity;
	int     	nExpectRet;
	DWORD   dwExpectEr;

	//==============================================================
	// For CardModifyWindow()
	//==============================================================
	int     	nHandle2;
	UINT16  	fAttri2;
	UINT8   	fAccessSpeed2;
	STATUS  uExpectRet;

	int     	nThreadsDone;
	int     	nThreadsExit;

	USHORT	uLocalSock, uLocalFunc;

	CLIENT_CONTEXT client_data;
	
};

VOID ISRFunction(UINT32 uContext);

#endif

