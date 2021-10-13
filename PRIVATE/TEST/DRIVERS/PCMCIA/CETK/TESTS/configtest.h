//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#ifndef _CONFIGTEST_H_
#define _CONFIGTEST_H_

#include "minithread.h"

//config test parameters
typedef struct {
	int    	nTestFunc;
	int    	fMemory  ;
	int   	uCallRR  ;
	int    	nHCLIENT       ;
	int    	nCONFIGINFO    ;
	UINT16 	fAttrib   ;
	UINT8  	fInterfaceType;
	UINT8	uVcc      ;
	UINT8	uVpp1     ;
	UINT8	uVpp2     ;
	UINT8  	fRegisters;
	UINT8  	uConfigReg;
	UINT8  	uStatusReg;
	UINT8  	uPinReg   ;
	UINT8  	uCopyReg  ;
	UINT8  	uExtStatus;
	STATUS 	uExpectRet;
	int		nHCLIENT2;
	STATUS  uExpectRet2;
	int    nIRQStream ;
}  CONFIGTESTPARAMS, *LPCONFIGTESTPARAMS;

//test options
#define CALL_REQUEST_CONF 0x0001
#define CALL_RELEASE_CONF 0x0010
#define CALL_REQUEST_IRQ 0x0100
#define CALL_RELEASE_IRQ 0x1000

//stream definitions 
#define MY_STREAM_ECHO       	0
#define MY_STREAM_DIAL_IN    	1
#define MY_STREAM_DIAL_OUT1  	2
#define MY_STREAM_DIAL_OUT2  	3
#define MY_STREAM_DIAL_OUT3  	4


#define REGISTER_WAIT_TIME		3000

#define MAX_CASES 39

#ifndef _STDEF_
#define _STDEF_	

static CONFIGTESTPARAMS configpara[MAX_CASES] =
{
// ** ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// ** nTestF fMemory  uCallRR  nHCLIENT1 nCONFIG fAttr fType  uVcc  uVpp1 uVpp2  fReg uConfigR uStatusR uPinR uCOpyR uExtS  uExpectR  nHCLIENT2 uExpectR2 nIRQStream  
// ** ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Case 1:
// for Modem: address = 0x3F8:  uConfigReg = 32:
  { 0x1,	0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,  1, CERR_SUCCESS,  0  },
// for Modem: address = 0x2F8:  uConfigReg = 33:
  { 0x1,  0,   0x1111,     1,       1,    0x102,  0x02,  50,   0,   0,     0x3,   33,     0x61 ,   0,    0,      0,  CERR_SUCCESS, 1,  CERR_SUCCESS,  0  },
// for Modem: address = 0x3E8:  uConfigReg = 34:
 { 0x1,   0,   0x1111,     1,       1,    0x102,  0x02,  50,   0,   0,     0x3,   34,     0x61 ,   0,    0,      0,  CERR_SUCCESS,  1, CERR_SUCCESS,     0  },
// for Modem: address = 0x2E8:  uConfigReg = 35:
  { 0x1,  0,   0x1111,     1,       1,    0x102,  0x02,  50,   0,   0,     0x3,   35,     0x61 ,   0,    0,      0,  CERR_SUCCESS,   1,  CERR_SUCCESS,     0  },
//
// Test_Sub2:
// for Modem: address = 0x2E8:  uConfigReg = 35:
  { 0x2,  0,   0x1111,     1,       1,    0x102,  0x02,  50,   0,   0,     0x3,   35,     0x61 ,   0,    0,      0,  CERR_SUCCESS,  1,  CERR_SUCCESS,     0  },
//
//
//	Case 6:
// Test not call Release Config and IRQ in the first run. Expected Release Window fail
// for Modem: address = 0x2E8:  uConfigReg = 35:
  { 0x2,  0,   0x0101,     1,       1,   0x102,  0x02,  50,   0,   0,     0x1F,   35,     0x61 ,   0,    0,      0,  CERR_SUCCESS, 1,  CERR_SUCCESS,     0  },
//
//
// register as memory Card
  { 0x2, 1,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,  1,    CERR_SUCCESS,     0  },

//
//  ======  Parameter checking: ======
// Case 8:
// Don't call RequestConfiguration().     Not call RequestConfig() RequestIRQ() : BUT Call Both Release
  { 0x2,  0,   0x1110,     1,       1,  0x102,  0x02,  50,   0,   0,     0x3,   35,     0x61 ,   0,    0,      0, CERR_SUCCESS,     1, CERR_SUCCESS,     0  },
  { 0x2,  0,   0x1010,     1,       1,  0x102,  0x02,  50,   0,   0,     0x3,   35,     0x61 ,   0,    0,      0, CERR_SUCCESS,     1, CERR_SUCCESS,     0  },
//
//
//
// Case 10: CardRequestConfiguration()
//	NULL, Invalid handle, NULL pCardParam,   invalic socket
// for Modem: address = 0x2E8:  uConfigReg = 35:   IRQ call back Function is not called. ???
  { 0x2, 0,   0x1111,     0,    1,    0x102,  0x02,  50,   0,   0,     0x3,   35,     0x61 ,   0,    0,      0, CERR_BAD_HANDLE,   1, CERR_SUCCESS,     0 },
  { 0x2, 0,   0x1111,     2,    1,    0x102,  0x02,  50,   0,   0,     0x3,   35,     0x61 ,   0,    0,      0, CERR_BAD_HANDLE,   1, CERR_BAD_HANDLE,  0 },
  { 0x2, 0,   0x1111,     1,    0,    0x102,  0x02,  50,   0,   0,     0x3,   35,     0x61 ,   0,    0,      0, CERR_BAD_ARGS,       1, CERR_SUCCESS   ,   0 },
// Case 13
  { 0x2,  0,   0x1111,     4,   1,    0x102,  0x02,  50,   0,   0,     0x3,   35,     0x61 ,   0,    0,      0, CERR_BAD_SOCKET,    1, CERR_SUCCESS   ,   0 },
//
// Case 14: CardReleaseConfiguration()
//	NULL, Invalid handle,  invalid socket
  { 0x2,  1,   0x1111,     1,    1, 0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,       0,   CERR_BAD_HANDLE,     0  },
  { 0x2,  1,   0x1111,     1,    1, 0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,       2,   CERR_BAD_HANDLE,     0  },
  { 0x2,  1,   0x1111,     1,    1, 0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,       4,   CERR_BAD_SOCKET,     0  },
//
//
// Case 17, 18:
//	B:	run case 17,18  or run smoke suite twice: same result:2049
// Call RequestIRQ(), Not call ReleaseIRQ(): expect ReleaseConfiGuration will handle this
// for Modem: address = 0x2F8:  uConfigReg = 33:   IRQ call back Function is not called. ???
  { 0x2, 0,   0x0111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   33,     0x61 ,   0,    0,      0,  CERR_SUCCESS,      1,  CERR_SUCCESS,     0  },
  { 0x2, 0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   33,     0x61 ,   0,    0,      0,  CERR_SUCCESS,      1,  CERR_SUCCESS,     0  },
//
//
//
//	Case 19:
// Test In Threads: USE nTestFunc == 1:  Test_Sub1() handle threads
// for Modem: address = 0x3F8:  uConfigReg = 32:
  { 0x1,   0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,      1,  CERR_SUCCESS,     0  },
//
//
//	Use different CARD_CONFIG_INFO.
// Modem: address = 0x2F8:  uConfigReg = 33: 																	
// Case 21: .fInterfaceType = CFG_IFACE_MEMORY
  { 0x1,   0,   0x1111,     1,       1,    0x102,  0x01,  50,   0,   0,     0x3,   33,     0x61 ,   0,    0,      0,  CERR_SUCCESS,      1, CERR_SUCCESS,     0  },
// Case 22:.fRegisters =0x1F
  { 0x1,   0,   0x1111,     1,       1,    0x102,  0x02,  50,   0,   0,     0x1F,  33,     0x61 ,   0,    0,     0,  CERR_SUCCESS,      1,  CERR_SUCCESS,     2  },
// Case 23:.uStatusReg = 0x63: interrupt pending:
  { 0x1,  0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x1F,  0x80|33, 0x63 ,   0,    0,      0,  CERR_SUCCESS,      1, CERR_SUCCESS,    3  },
// Case 24:.uStatusReg = 0xED:   almost everything except interrupt pending
  { 0x1,  0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x1F,  0x80|33, 0xED ,   0,    0,      0,  CERR_SUCCESS,      1, CERR_SUCCESS,    3  },
// Case 25:.uStatusReg = 0xFF:   everything: uCopyReg = 0xFF: shoud affect noththing
  { 0x1, 0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x1F,  0x80|33, 0xFF ,   0,    0xFF, 0xFF, CERR_SUCCESS,   1,  CERR_SUCCESS,    3  },
//
//
//
// Case 25: Dial OUT1:   my office phone 63551: Neet to soft reset the card
  { 0x1,   0,   0x1111,     1,       1,  0x102,  0x02,  50,   0,   0,     0x3,   0x80|33, 0x61 ,   0,    0,      0,  CERR_SUCCESS,      1,   CERR_SUCCESS,     2  },
// Case 26: Dial In.
  { 0x1,  0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   34,     0x61 ,   0,    0,      0,  CERR_SUCCESS,      1,   CERR_SUCCESS,     1 },
//
// Case 27:  B
//	 use different config: address = 0x555  uConfigReg = 55: expect SUCCESS
  { 0x2,  0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   55,     0x61 ,   0,    0,      0,  CERR_SUCCESS,      1,   CERR_SUCCESS,     0  },
//
//
// ** ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// **nTestF nThreads nClits fMemory  uCallRR  nHCLIENT1 nCONFIG uSock1 uFunc1 fAttr fType  uVcc  uVpp1 uVpp2  fReg uConfigR uStatusR uPinR uCOpyR uExtS  uExpectR       nHCLIENT2  uSocket2 uFunc2  uExpectR2
// ** ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// Case 28-31: test CardAccessConfigurationRegister()
// nTest = 5:  test CardAccessConfigurationRegister()
  { 0x5,  0,     0x1111,     1,       1,  0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,      1,  CERR_SUCCESS,     0  },
// Parameter checking:
// Case 29: passing NULL hClient handle for READ register: Vw succeeds.  Pass NULL hClient handle for write, will fail:
// change to case 29. passing NULL to WRITE shoulndt fail! spec change and corrected
//{S1_CRConfig, 0x00,  { 0x5,      0,       1,      0,     0x1111,     0,       1,     0,     0,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,      1,      0,   0,     CERR_BAD_HANDLE,     0  }},

  { 0x5,  0,     0x1111,     0,       1,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,      1, CERR_SUCCESS,     0  },
  { 0x5,  0,     0x1111,     4,       1,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_BAD_SOCKET,  4,    CERR_BAD_SOCKET,     0  },
// Case 31: pvalue = Null in cardacessconfigregister. return value = CERR_BAD_ARGS.

  { 0x5,  0,     0x1111,     3,       1,  0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_BAD_ARGS,      1,  CERR_BAD_ARGS,     0  },
// Case 32:
  { 0x1,	0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,  1, CERR_SUCCESS,  0  },
// Case 33:
  { 0x1,	0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,  1, CERR_SUCCESS,  0  },
// Case 34:
// Set/unset IRQWakeup:
  { 0x201,	0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,  1, CERR_SUCCESS,  0  },
// Case 35:
// Set/unset KeepPowered:
  { 0x201,	0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,  1, CERR_SUCCESS,  0  },
// Case 36:
// Set/unset NoSuspendUnload:
  { 0x201,	0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,  1, CERR_SUCCESS,  0  },
// Case 37:
// call CardModifyConfiguration with invalid parameters:
  { 0x201,	0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,  1, CERR_SUCCESS,  0  },
// Case 38: wrong hClient handel in cardacessconfigregister. return value = CERR_BAD_HANDLE.
  { 0x5,  0,     0x1111,     2,       1,  0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,      1,  CERR_BAD_HANDLE,     0  },
// Case 39 test cardpower functionalities:
  { 0x301,	0,   0x1111,     1,       1,   0x102,  0x02,  50,   0,   0,     0x3,   32,     0x61 ,   0,    0,      0,  CERR_SUCCESS,  1, CERR_SUCCESS,  0  },

};
#endif // _STDEF_

class ConfigTest : public MiniThread {
public:
	ConfigTest(DWORD dwTestCase, DWORD dwThreadNo, USHORT	uSock, USHORT uFunc) :
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
	~ConfigTest(){;};
private:
	virtual DWORD ThreadRun();
       BOOL SubTestReleaseConfig(
	PCLIENT_CONTEXT    pData,
	HANDLE             hClient,
	CARD_WINDOW_HANDLE hWindow,
	CARD_SOCKET_HANDLE   hSocket);

	BOOL Test_Sub1(
	PCLIENT_CONTEXT      pData,
	CARD_CLIENT_HANDLE   hClient,
	CARD_WINDOW_HANDLE   hWindow,
	PCARD_CONFIG_INFO    pconfInfo);

	BOOL Test_Sub2(
	PCLIENT_CONTEXT      pData,
	CARD_CLIENT_HANDLE   hClient,
	CARD_WINDOW_HANDLE   hWindow,
	PCARD_CONFIG_INFO    pconfInfo);

	BOOL Test_AccessRegister(
	PCLIENT_CONTEXT      pData,
	CARD_CLIENT_HANDLE   hClient,
	CARD_WINDOW_HANDLE   hWindow,
	PCARD_CONFIG_INFO    pconfInfo);

	BOOL Test_CardModifyConfig(
	PCLIENT_CONTEXT      pData,
	CARD_CLIENT_HANDLE   hClient,
	CARD_WINDOW_HANDLE   hWindow,
	PCARD_CONFIG_INFO    pconfInfo);

	BOOL CardModifyConfig_InvalidParas(
	CARD_CLIENT_HANDLE   hClient,
	CARD_SOCKET_HANDLE	 hSocket,
	UINT16				 fAttr);

	BOOL Test_CardPowerOnOff(
	PCLIENT_CONTEXT      pData,
	CARD_CLIENT_HANDLE   hClient,
	CARD_WINDOW_HANDLE   hWindow,
	PCARD_CONFIG_INFO    pconfInfo);


	DWORD	dwCaseID;
	DWORD 	dwThreadID;
	int		nTestFunc, nHCLIENT, nHCLIENT2, nIRQStream;
	int		fMemory, nConfigInfo;
	UINT	iByteStream, IsrLogIndex, IsrPrintIndex;
	UINT16	fAttrib;
	STATUS 	uExpectRet, uExpectRet2;
	UINT8  	uCopyReg, uConfigReg, uStatusReg, uPinReg, uVcc, uVpp1, uVpp2, uExtStatus;
	UINT8  	fInterfaceType, fRegisters;
	UINT32   uCallRR   ;
	DWORD   LastError     ;  // Was used to call SetLastError before return.

	USHORT	uLocalSock, uLocalFunc;

	// used between SubTestConfig() and Test_SubXX()
	BOOL	 fDeregisterClientDone;
	BOOL	 fReleaseWindowDone;
	BOOL	 fReleaseIRQDone;
	BOOL	 fReleaseExclDone;

	CLIENT_CONTEXT client_data;
	
};

VOID ConfISRFunction(UINT32 uContext);

#endif

