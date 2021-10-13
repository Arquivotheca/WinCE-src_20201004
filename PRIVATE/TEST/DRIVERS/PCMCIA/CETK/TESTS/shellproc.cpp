//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#include "testmain.h"
#include "common.h"
#include "resource.h"
#include "ddlxioct.h"

//globals 
DECLARE_CARDSERVICES_TABLE;

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
DDLXKato_Talk* g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION g_csProcess;

//Handle for this dll
HINSTANCE g_hInst = NULL;

//The handle for pcmcia.dll
HINSTANCE hInst = NULL;

USHORT		uSocket;
USHORT  	uFunction;
BOOL		bCardNotInSlot;
BOOL		bRegUpdated;
DWORD		dwTotalThread;
USHORT		g_Vcc;

//******************************************************************************
//***** Windows CE specific code
//******************************************************************************
#ifdef UNDER_CE

#ifdef __cplusplus
extern "C" {
#endif
BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {

	g_hInst = (HINSTANCE)hInstance;
   	return TRUE;
}
#ifdef __cplusplus
}
#endif


#endif


extern "C" {

// --------------------------------------------------------------------
// Tux testproc function table
//
FUNCTION_TABLE_ENTRY g_lpFTE[] = {
	_T("ConfigTest-SFC in Sock 0"),	0,	              0,					0,	NULL,
	_T("addr 0x3F8 uConfigReg 32"), 	1,	  	1,        	11001,  		TestDispatchEntry,
	_T("addr 0x2F8 uConfigReg 33"),    1,   		1,        	11002,  		TestDispatchEntry,
	_T("addr 0x3E8 uConfigReg 34"),    1,   		1,        	11003,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35"), 	1,   		1,        	11004,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35 (different call order)"), 	1,		1,        	11005,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35(missing some calls) "),		1,   1,       11006, TestDispatchEntry,
	_T("register as memcard"),	1,  	1,	11007,	TestDispatchEntry,
	_T("Don't request before release 1"),	1,  	1,	11008,	TestDispatchEntry,
	_T("Don't request before release 2"),	1,  	1,	11009, 	TestDispatchEntry,
	_T("NULL Handle"),		1,	1,	11010,	TestDispatchEntry,
	_T("Invalid Handle"),	1,  	1,    11011, 	TestDispatchEntry,
	_T("NULL CardParam"), 	1,  	1,   	11012, 	TestDispatchEntry,
	_T("Invalid Socket"), 	1,  	1,    11013, 	TestDispatchEntry,
	_T("NULL Handle"),		1,	1,	11014,	TestDispatchEntry,
	_T("Invalid Handle"),	1,  	1,	11015, 	TestDispatchEntry,
	_T("Invalid Socket"),    	1,  	1,   	11016, 	TestDispatchEntry,
	_T("partial release 1"),	1,	1,	11017,	TestDispatchEntry,
	_T("partial release 2"), 	1,  	1,    11018, 	TestDispatchEntry,
	_T("Config Test"),		1,	2,	11019,	TestDispatchEntry,
	_T("fInterfaceType = CFG_IFACE_MEMORY"),	1,	1,	11020,	TestDispatchEntry,
	_T("fRegisters =0x1F"),		1,	1,	11021,	TestDispatchEntry,
	_T("uStatusReg = 0x63"),	1,	1,	11022,	TestDispatchEntry,
	_T("uStatusReg = 0xED"),		1,  1,       11023, TestDispatchEntry,
	_T("uStatusReg = 0xFF"),  1,  1,       11024, TestDispatchEntry,
	_T("Dial Out Test"),	1,  1,      11025, TestDispatchEntry,
	_T("Dial In Test"),	1,  1,      11026, TestDispatchEntry,
	_T("addr 0x555 uConfigReg 55"),	1,  1,       11027, TestDispatchEntry,
	_T("CardAccessConfigurationRegister"),	1,  1,      11028, TestDispatchEntry,
	_T("Null hClient for Read"),	1,  1,       11029, TestDispatchEntry,
	_T("Null hClient for Write"),	1,  1,       11030, TestDispatchEntry,
	_T("Null pvalue"),	1,	1,		 11031, TestDispatchEntry,
	_T("Null Params for register"),	1,	1,		 11032, TestDispatchEntry,
	_T("Invalid Params for register"),	1,	1,		 11033, TestDispatchEntry,
	_T("CardModifyConfiguration:IRQWakeup"),	1,	1,		 11034, TestDispatchEntry,
	_T("CardModifyConfiguration:KeepPowered"),	1,	1,		 11035, TestDispatchEntry,
	_T("CardModifyConfiguration:NoSuspendUnload"),	1,	1,		 11036, TestDispatchEntry,
	_T("CardModifyConfiguration:InValidParas"),	1,	1,		 11037, TestDispatchEntry,
	_T("CardAccessConfigurationRegister,wrong client handle"),	1,  1,      11038, TestDispatchEntry,
	_T("CardPowerOn and Off"),	1,	1,		 11039, TestDispatchEntry,
	
	_T("ExclTests-SFC in Sock 0"),	0,	              0,					0,	NULL,
	_T("fAttri = 0x9: MEMORY Client"), 	1,	  	1,        	15001,  		TestDispatchEntry,
	_T("fAttri = 0xA: IO Client"), 	1,	  	1,        	15002,  		TestDispatchEntry,
	_T("fAttri = 0xA: IO Client:  Not calling RequestExclusive"), 	2,	  	1,        	15003,  		TestDispatchEntry,
	_T("Paramter checking NULL HANDLE"), 	1,	  	1,        	15004,  		TestDispatchEntry,
	_T("Paramter checking BAD HANDLE"), 	1,	  	1,        	15005,  		TestDispatchEntry,
	_T("Only release window before calling CardGetExclusive()"), 	1,	  	1,        	15006,  		TestDispatchEntry,
	_T("Invalid socket 0xFF, function 0xFF"), 	1,	  	1,        	15007,  		TestDispatchEntry,
	_T("Not calling RequestExclusive: only test ReleaseExclusive"), 	1,	  	1,        	15008,  		TestDispatchEntry,
	_T("Not calling RequestExclusive: only test ReleaseExclusive 2"), 	1,	  	1,        	15009,  		TestDispatchEntry,
	_T("Multithreaded test,5 threads"), 	1,	  	5,        	15010,  		TestDispatchEntry,
	_T("Multithreaded test,2 threads"), 	1,	  	2,        	15011,  		TestDispatchEntry,
	_T("Call CardRequestExclusive(),  BUT DON'T call CardReleaseExclusive()"), 	1,	  	1,        	15012,  		TestDispatchEntry,

	_T("IntrTests-SFC in Sock 0"),	0,	              0,					0,	NULL,
	_T("Modem: addr 0x2E8 uConfigReg 35"), 	1,	  	1,        	13001,  		TestDispatchEntry,
	_T("MultiThreaded Test"), 	1,	  	4,        	13002,  		TestDispatchEntry,
	_T("CardRequestIRQ(Null Parameter)"), 	1,	  	1,        	13006,  		TestDispatchEntry,
	_T("CardRequestIRQ (Bad Socket)"), 	1,	  	1,        	13007,  		TestDispatchEntry,
	_T("CardRequestIRQ (Bad Function"), 	1,	  	1,        	13008,  		TestDispatchEntry,
	_T("CardRequestIRQ (All true params)"), 	1,	  	1,        	13009,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 1"), 	1,	  	1,        	13010,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 2"), 	1,	  	1,        	13011,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 3"), 	1,	  	1,        	13012,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 4 (Bad Params)"), 	1,	  	1,        	13013,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 5 (all good params)"), 	1,	  	1,        	13014,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35 not calling ReleaseConfiguration()"), 	1,	  	1,        	13015,  		TestDispatchEntry,
	_T("complementary for case15"), 	1,	  	1,        	13016,  		TestDispatchEntry,
	_T("addr 0x3F8 uConfigReg 32 not call CardReleaseIRQ()"), 	1,	  	1,        	13017,  		TestDispatchEntry,
	_T("Comlementary for case 17"), 1,	  	1,        	13018,  		TestDispatchEntry,
	_T("Skips CardReleaseExclusive"), 	1,	  	1,        	13019,  		TestDispatchEntry,
	_T("Complementary for case 19"), 	1,	  	1,        	13020,  		TestDispatchEntry,

	_T("MaskTests- SFC in Sock 0"),	 0,	              0,					0,	NULL,
	_T("CardGetStatus"), 	1,	  	1,        	14001,  		TestDispatchEntry,
	_T("CardGetStatus - Multithreaded"), 1,	  	2,        	14002,  		TestDispatchEntry,
	_T("CardGetStatus - Multithreaded"), 	1,	  	3,        	14003,  		TestDispatchEntry,
	_T("CardEventMasks"), 	1,	  	1,        	14004,  		TestDispatchEntry,
	_T("CardEventMaks - Multithreaded"), 	1,	  	2,        	14005,  		TestDispatchEntry,
	_T("CardEventMaks - Multithreaded)"), 	1,	  	3,        	14006,  		TestDispatchEntry,
	_T("CardResetFunction"), 	1,	  	1,        	14007,  		TestDispatchEntry,
	_T("CardResetFunction - Multithreaded"), 	1,	  	2,        	14008,  		TestDispatchEntry,
	_T("CardResetFunction - Multithreaded)"), 	1,	  	3,        	14009,  		TestDispatchEntry,
	_T("CardFunction"), 	1,	  	1,        	14010,  		TestDispatchEntry,
	_T("CardFunction - Multithreaded"), 	1,	  	2,        	14011,  		TestDispatchEntry,
	_T("CardFunction - Multithreaded"), 	1,	  	3,        	14012,  		TestDispatchEntry,
	_T("get/set event-Invalidparas"), 	1,	  	2,        	14013,  		TestDispatchEntry,
	_T("request/release socketevent-Invalidparas"), 	1,	  	3,        	14014,  		TestDispatchEntry,

	_T("TupleTests-SFC in Sock 0"),	0,	              0,					0,	NULL,
	_T("Tuple Full Test"), 	1,	  	1,        	16001,  		TestDispatchEntry,
	_T("Tuple Full Test - Multithreaded"), 1,	  	4,        	16002,  		TestDispatchEntry,
	_T("Tuple Full Test 2"), 	1,	  	1,        	16003,  		TestDispatchEntry,
	_T("Additional: CardGetFirstTuple"),	1,		1,			16101,			TestDispatchEntry,
	_T("Additional: CardGetNextTuple"),	1,		1,			16102,			TestDispatchEntry,
	_T("Additional: CardGetTupleData"), 1,		1,			16103,			TestDispatchEntry,
	_T("Additional: CardGetParsedTuple1"), 1,		1,			16104,			TestDispatchEntry,
	_T("Additional: CardGetParsedTuple2"), 1,		1,			16105,			TestDispatchEntry,

	_T("WinTests-SFC in Sock 0"),	0,	              0,					0,	NULL,
	_T("CardAdress=0x2E8, Size=0x10"),	1,	              1,					12001,	TestDispatchEntry,
	_T("CardAdress=0x2F8, Size=0x40"),	1,	              1,					12002,	TestDispatchEntry,
	_T("CardAdress=0x3E8, Size=0x08"),	1,	              1,					12003,	TestDispatchEntry,
	_T("disable common window"),		1,	              1,					12004,	TestDispatchEntry,
	_T("attribute window to IO window"),	1,	              1,					12005,	TestDispatchEntry,
	_T("IO attribute window to IO window"),	1,	              1,					12006,	TestDispatchEntry,
	_T("0x0 offset for CardMapWindow"),	1,	              1,					12007,	TestDispatchEntry,
	_T("Out Of Resource test"),	1,	              1,					12008,	TestDispatchEntry,
	_T("Card Address=0x12345, Size=0x1"),	1,	              1,					12009,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 4 bytes : uCard addr 0x2E8 "),	1,	              1,					12010,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 2 bytes : uCard addr 0x100"),	1,	              1,					12011,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 3 bytes : uCard addr 0x0"),	1,	              1,					12012,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 1 bytes : uCard addr 0x2"),	1,	              1,					12013,	TestDispatchEntry,
	_T("hWindow=NULL"),	1,	              1,					12014,	TestDispatchEntry,
	_T("hWindow=INVALID"),	1,	              1,					12015,	TestDispatchEntry,
	_T("Granularity=NULL"),	1,	              1,					12016,	TestDispatchEntry,
	_T("hWindow=NULL2"),	1,	              1,					12017,	TestDispatchEntry,
	_T("hWindow=INVALID 2"),	1,	              1,					12018,	TestDispatchEntry,
	_T("5 threads test"),	1,	              5,					12019,	TestDispatchEntry,
	_T("10 threads test"),	1,	              10,					12020,	TestDispatchEntry,
	_T("3 threads test"),	1,	              3,					12021,	TestDispatchEntry,
	_T("mem window map upto 64MB"),	1,	              1,					12023,	TestDispatchEntry,
	_T("Request less than 800 K window: Card addr 0xC5000"),	1,	              1,					12024,	TestDispatchEntry,
	_T("CardModifyWindow with invalid client"),	1,	              1,					12025,	TestDispatchEntry,
	_T("Regression test case for CardMapWindow1 "),	1,	              1,					12026,	TestDispatchEntry,
	_T("Test window functions with invalid paras"),	1,	              1,					12027,	TestDispatchEntry,
	_T("Test collide windows"),	1,	              1,					12028,	TestDispatchEntry,

	_T("ConfigTest-SFC in Sock 1"),	0,	              0,					0,	NULL,
	_T("addr 0x3F8 uConfigReg 32"), 	1,	  	1,        	21001,  		TestDispatchEntry,
	_T("addr 0x2F8 uConfigReg 33"),    1,   		1,        	21002,  		TestDispatchEntry,
	_T("addr 0x3E8 uConfigReg 34"),    1,   		1,        	21003,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35"), 	1,   		1,        	21004,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35 (different call order)"), 	1,		1,        	21005,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35(missing some calls) "),		1,   1,       21006, TestDispatchEntry,
	_T("register as memcard"),	1,  	1,	21007,	TestDispatchEntry,
	_T("Don't request before release 1"),	1,  	1,	21008,	TestDispatchEntry,
	_T("Don't request before release 2"),	1,  	1,	21009, 	TestDispatchEntry,
	_T("NULL Handle"),		1,	1,	21010,	TestDispatchEntry,
	_T("Invalid Handle"),	1,  	1,    21011, 	TestDispatchEntry,
	_T("NULL CardParam"), 	1,  	1,   	21012, 	TestDispatchEntry,
	_T("Invalid Socket"), 	1,  	1,    21013, 	TestDispatchEntry,
	_T("NULL Handle"),		1,	1,	21014,	TestDispatchEntry,
	_T("Invalid Handle"),	1,  	1,	21015, 	TestDispatchEntry,
	_T("Invalid Socket"),    	1,  	1,   	21016, 	TestDispatchEntry,
	_T("partial release 1"),	1,	1,	21017,	TestDispatchEntry,
	_T("partial release 2"), 	1,  	1,    21018, 	TestDispatchEntry,
	_T("Config Test"),		1,	2,	21019,	TestDispatchEntry,
	_T("fInterfaceType = CFG_IFACE_MEMORY"),	1,	1,	21020,	TestDispatchEntry,
	_T("fRegisters =0x1F"),		1,	1,	21021,	TestDispatchEntry,
	_T("uStatusReg = 0x63"),	1,	1,	21022,	TestDispatchEntry,
	_T("uStatusReg = 0xED"),		1,  1,       21023, TestDispatchEntry,
	_T("uStatusReg = 0xFF"),  1,  1,       21024, TestDispatchEntry,
	_T("Dial Out Test"),	1,  1,      21025, TestDispatchEntry,
	_T("Dial In Test"),	1,  1,      21026, TestDispatchEntry,
	_T("addr 0x555 uConfigReg 55"),	1,  1,       21027, TestDispatchEntry,
	_T("CardAccessConfigurationRegister"),	1,  1,      21028, TestDispatchEntry,
	_T("Null hClient for Read"),	1,  1,       21029, TestDispatchEntry,
	_T("Null hClient for Write"),	1,  1,       21030, TestDispatchEntry,
	_T("Null pvalue"),	1,	1,		 21031, TestDispatchEntry,
	_T("Null Params for register"),	1,	1,		 21032, TestDispatchEntry,
	_T("Invalid Params for register"),	1,	1,		 21033, TestDispatchEntry,
	_T("CardModifyConfiguration:IRQWakeup"),	1,	1,		 21034, TestDispatchEntry,
	_T("CardModifyConfiguration:KeepPowered"),	1,	1,		 21035, TestDispatchEntry,
	_T("CardModifyConfiguration:NoSuspendUnload"),	1,	1,		 21036, TestDispatchEntry,
	_T("CardModifyConfiguration:InValidParas"),	1,	1,		 21037, TestDispatchEntry,
	_T("CardAccessConfigurationRegister,wrong client handle"),	1,  1,      21038, TestDispatchEntry,
	_T("CardPowerOn and Off"),	1,	1,		 21039, TestDispatchEntry,
	
	_T("ExclTests-SFC in Sock 1"),	0,	              0,					0,	NULL,
	_T("fAttri = 0x9: MEMORY Client"), 	1,	  	1,        	25001,  		TestDispatchEntry,
	_T("fAttri = 0xA: IO Client"), 	1,	  	1,        	25002,  		TestDispatchEntry,
	_T("fAttri = 0xA: IO Client:  Not calling RequestExclusive"), 	2,	  	1,        	25003,  		TestDispatchEntry,
	_T("Paramter checking NULL HANDLE"), 	1,	  	1,        	25004,  		TestDispatchEntry,
	_T("Paramter checking BAD HANDLE"), 	1,	  	1,        	25005,  		TestDispatchEntry,
	_T("Only release window before calling CardGetExclusive()"), 	1,	  	1,        	25006,  		TestDispatchEntry,
	_T("Invalid socket 0xFF, function 0xFF"), 	1,	  	1,        	25007,  		TestDispatchEntry,
	_T("Not calling RequestExclusive: only test ReleaseExclusive"), 	1,	  	1,        	25008,  		TestDispatchEntry,
	_T("Not calling RequestExclusive: only test ReleaseExclusive 2"), 	1,	  	1,        	25009,  		TestDispatchEntry,
	_T("Multithreaded test,5 threads"), 	1,	  	5,        	25010,  		TestDispatchEntry,
	_T("Multithreaded test,2 threads"), 	1,	  	2,        	25011,  		TestDispatchEntry,
	_T("Call CardRequestExclusive(),  BUT DON'T call CardReleaseExclusive()"), 	1,	  	1,        	25012,  		TestDispatchEntry,

	_T("IntrTests-SFC in Sock 1"),	0,	              0,					0,	NULL,
	_T("Modem: addr 0x2E8 uConfigReg 35"), 	1,	  	1,        	23001,  		TestDispatchEntry,
	_T("MultiThreaded Test"), 	1,	  	4,        	23002,  		TestDispatchEntry,
	_T("CardRequestIRQ(Null Parameter)"), 	1,	  	1,        	23006,  		TestDispatchEntry,
	_T("CardRequestIRQ (Bad Socket)"), 	1,	  	1,        	23007,  		TestDispatchEntry,
	_T("CardRequestIRQ (Bad Function"), 	1,	  	1,        	23008,  		TestDispatchEntry,
	_T("CardRequestIRQ (All true params)"), 	1,	  	1,        	23009,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 1"), 	1,	  	1,        	23010,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 2"), 	1,	  	1,        	23011,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 3"), 	1,	  	1,        	23012,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 4 (Bad Params)"), 	1,	  	1,        	23013,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 5 (all good params)"), 	1,	  	1,        	23014,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35 not calling ReleaseConfiguration()"), 	1,	  	1,        	23015,  		TestDispatchEntry,
	_T("complementary for case15"), 	1,	  	1,        	23016,  		TestDispatchEntry,
	_T("addr 0x3F8 uConfigReg 32 not call CardReleaseIRQ()"), 	1,	  	1,        	23017,  		TestDispatchEntry,
	_T("Comlementary for case 17"), 1,	  	1,        	23018,  		TestDispatchEntry,
	_T("Skips CardReleaseExclusive"), 	1,	  	1,        	23019,  		TestDispatchEntry,
	_T("Complementary for case 19"), 	1,	  	1,        	23020,  		TestDispatchEntry,

	_T("MaskTests- SFC in Sock 1"),	 0,	              0,					0,	NULL,
	_T("CardGetStatus"), 	1,	  	1,        	24001,  		TestDispatchEntry,
	_T("CardGetStatus - Multithreaded"), 1,	  	2,        	24002,  		TestDispatchEntry,
	_T("CardGetStatus - Multithreaded"), 	1,	  	3,        	24003,  		TestDispatchEntry,
	_T("CardEventMasks"), 	1,	  	1,        	24004,  		TestDispatchEntry,
	_T("CardEventMaks - Multithreaded"), 	1,	  	2,        	24005,  		TestDispatchEntry,
	_T("CardEventMaks - Multithreaded)"), 	1,	  	3,        	24006,  		TestDispatchEntry,
	_T("CardResetFunction"), 	1,	  	1,        	24007,  		TestDispatchEntry,
	_T("CardResetFunction - Multithreaded"), 	1,	  	2,        	24008,  		TestDispatchEntry,
	_T("CardResetFunction - Multithreaded)"), 	1,	  	3,        	24009,  		TestDispatchEntry,
	_T("CardFunction"), 	1,	  	1,        	24010,  		TestDispatchEntry,
	_T("CardFunction - Multithreaded"), 	1,	  	2,        	24011,  		TestDispatchEntry,
	_T("CardFunction - Multithreaded"), 	1,	  	3,        	24012,  		TestDispatchEntry,
	_T("get/set event-Invalidparas"), 	1,	  	2,        	24013,  		TestDispatchEntry,
	_T("request/release socketevent-Invalidparas"), 	1,	  	3,        	24014,  		TestDispatchEntry,

	_T("TupleTests-SFC in Sock 1"),	0,	              0,					0,	NULL,
	_T("Tuple Full Test"), 	1,	  	1,        	26001,  		TestDispatchEntry,
	_T("Tuple Full Test - Multithreaded"), 1,	  	4,        	26002,  		TestDispatchEntry,
	_T("Tuple Full Test 2"), 	1,	  	1,        	26003,  		TestDispatchEntry,
	_T("Additional: CardGetFirstTuple"),	1,		1,			26101,			TestDispatchEntry,
	_T("Additional: CardGetNextTuple"),	1,		1,			26102,			TestDispatchEntry,
	_T("Additional: CardGetTupleData"), 1,		1,			26103,			TestDispatchEntry,
	_T("Additional: CardGetParsedTuple1"), 1,		1,			26104,			TestDispatchEntry,
	_T("Additional: CardGetParsedTuple2"), 1,		1,			26105,			TestDispatchEntry,

	_T("WinTests-SFC in Sock 1"),	0,	              0,					0,	NULL,
	_T("CardAdress=0x2E8, Size=0x10"),	1,	              1,					22001,	TestDispatchEntry,
	_T("CardAdress=0x2F8, Size=0x40"),	1,	              1,					22002,	TestDispatchEntry,
	_T("CardAdress=0x3E8, Size=0x08"),	1,	              1,					22003,	TestDispatchEntry,
	_T("disable common window"),		1,	              1,					22004,	TestDispatchEntry,
	_T("attribute window to IO window"),	1,	              1,					22005,	TestDispatchEntry,
	_T("IO attribute window to IO window"),	1,	              1,					22006,	TestDispatchEntry,
	_T("0x0 offset for CardMapWindow"),	1,	              1,					22007,	TestDispatchEntry,
	_T("Out Of Resource test"),	1,	              1,					22008,	TestDispatchEntry,
	_T("Card Address=0x22345, Size=0x1"),	1,	              1,					22009,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 4 bytes : uCard addr 0x2E8 "),	1,	              1,					22010,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 2 bytes : uCard addr 0x100"),	1,	              1,					22011,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 3 bytes : uCard addr 0x0"),	1,	              1,					22012,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 1 bytes : uCard addr 0x2"),	1,	              1,					22013,	TestDispatchEntry,
	_T("hWindow=NULL"),	1,	              1,					22014,	TestDispatchEntry,
	_T("hWindow=INVALID"),	1,	              1,					22015,	TestDispatchEntry,
	_T("Granularity=NULL"),	1,	              1,					22016,	TestDispatchEntry,
	_T("hWindow=NULL2"),	1,	              1,					22017,	TestDispatchEntry,
	_T("hWindow=INVALID 2"),	1,	              1,					22018,	TestDispatchEntry,
	_T("5 threads test"),	1,	              5,					22019,	TestDispatchEntry,
	_T("10 threads test"),	1,	              10,					22020,	TestDispatchEntry,
	_T("3 threads test"),	1,	              3,					22021,	TestDispatchEntry,
	_T("mem window map upto 64MB"),	1,	              1,					22023,	TestDispatchEntry,
	_T("Request less than 800 K window: Card addr 0xC5000"),	1,	              1,					22024,	TestDispatchEntry,
	_T("CardModifyWindow with invalid client"),	1,	              1,					22025,	TestDispatchEntry,
	_T("Regression test case for CardMapWindow1 "),	1,	              1,					22026,	TestDispatchEntry,
	_T("Test window functions with invalid paras"),	1,	              1,					22027,	TestDispatchEntry,
	_T("Test collide windows"),	1,	              1,					22028,	TestDispatchEntry,

	_T("ConfigTest-MFC in Sock 0"),	0,	              0,					0,	NULL,
	_T("addr 0x3F8 uConfigReg 32"), 	1,	  	1,        	31001,  		TestDispatchEntry,
	_T("addr 0x2F8 uConfigReg 33"),    1,   		1,        	31002,  		TestDispatchEntry,
	_T("addr 0x3E8 uConfigReg 34"),    1,   		1,        	31003,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35"), 	1,   		1,        	31004,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35 (different call order)"), 	1,		1,        	31005,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35(missing some calls) "),		1,   1,       31006, TestDispatchEntry,
	_T("register as memcard"),	1,  	1,	31007,	TestDispatchEntry,
	_T("Don't request before release 1"),	1,  	1,	31008,	TestDispatchEntry,
	_T("Don't request before release 2"),	1,  	1,	31009, 	TestDispatchEntry,
	_T("NULL Handle"),		1,	1,	31010,	TestDispatchEntry,
	_T("Invalid Handle"),	1,  	1,    31011, 	TestDispatchEntry,
	_T("NULL CardParam"), 	1,  	1,   	31012, 	TestDispatchEntry,
	_T("Invalid Socket"), 	1,  	1,    31013, 	TestDispatchEntry,
	_T("NULL Handle"),		1,	1,	31014,	TestDispatchEntry,
	_T("Invalid Handle"),	1,  	1,	31015, 	TestDispatchEntry,
	_T("Invalid Socket"),    	1,  	1,   	31016, 	TestDispatchEntry,
	_T("partial release 1"),	1,	1,	31017,	TestDispatchEntry,
	_T("partial release 2"), 	1,  	1,    31018, 	TestDispatchEntry,
	_T("Config Test"),		1,	2,	31019,	TestDispatchEntry,
	_T("fInterfaceType = CFG_IFACE_MEMORY"),	1,	1,	31020,	TestDispatchEntry,
	_T("fRegisters =0x1F"),		1,	1,	31021,	TestDispatchEntry,
	_T("uStatusReg = 0x63"),	1,	1,	31022,	TestDispatchEntry,
	_T("uStatusReg = 0xED"),		1,  1,       31023, TestDispatchEntry,
	_T("uStatusReg = 0xFF"),  1,  1,       31024, TestDispatchEntry,
	_T("Dial Out Test"),	1,  1,      31025, TestDispatchEntry,
	_T("Dial In Test"),	1,  1,      31026, TestDispatchEntry,
	_T("addr 0x555 uConfigReg 55"),	1,  1,       31027, TestDispatchEntry,
	_T("CardAccessConfigurationRegister"),	1,  1,      31028, TestDispatchEntry,
	_T("Null hClient for Read"),	1,  1,       31029, TestDispatchEntry,
	_T("Null hClient for Write"),	1,  1,       31030, TestDispatchEntry,
	_T("Null pvalue"),	1,	1,		 31031, TestDispatchEntry,
	_T("Null Params for register"),	1,	1,		 31032, TestDispatchEntry,
	_T("Invalid Params for register"),	1,	1,		 31033, TestDispatchEntry,
	_T("CardModifyConfiguration:IRQWakeup"),	1,	1,		 31034, TestDispatchEntry,
	_T("CardModifyConfiguration:KeepPowered"),	1,	1,		 31035, TestDispatchEntry,
	_T("CardModifyConfiguration:NoSuspendUnload"),	1,	1,		 31036, TestDispatchEntry,
	_T("CardModifyConfiguration:InValidParas"),	1,	1,		 31037, TestDispatchEntry,
	_T("CardAccessConfigurationRegister,wrong client handle"),	1,  1,      31038, TestDispatchEntry,
	_T("CardPowerOn and Off"),	1,	1,		 31039, TestDispatchEntry,
	
	_T("ExclTests-MFC in Sock 0"),	0,	              0,					0,	NULL,
	_T("fAttri = 0x9: MEMORY Client"), 	1,	  	1,        	35001,  		TestDispatchEntry,
	_T("fAttri = 0xA: IO Client"), 	1,	  	1,        	35002,  		TestDispatchEntry,
	_T("fAttri = 0xA: IO Client:  Not calling RequestExclusive"), 	2,	  	1,        	35003,  		TestDispatchEntry,
	_T("Paramter checking NULL HANDLE"), 	1,	  	1,        	35004,  		TestDispatchEntry,
	_T("Paramter checking BAD HANDLE"), 	1,	  	1,        	35005,  		TestDispatchEntry,
	_T("Only release window before calling CardGetExclusive()"), 	1,	  	1,        	35006,  		TestDispatchEntry,
	_T("Invalid socket 0xFF, function 0xFF"), 	1,	  	1,        	35007,  		TestDispatchEntry,
	_T("Not calling RequestExclusive: only test ReleaseExclusive"), 	1,	  	1,        	35008,  		TestDispatchEntry,
	_T("Not calling RequestExclusive: only test ReleaseExclusive 2"), 	1,	  	1,        	35009,  		TestDispatchEntry,
	_T("Multithreaded test,5 threads"), 	1,	  	5,        	35010,  		TestDispatchEntry,
	_T("Multithreaded test,2 threads"), 	1,	  	2,        	35011,  		TestDispatchEntry,
	_T("Call CardRequestExclusive(),  BUT DON'T call CardReleaseExclusive()"), 	1,	  	1,        	35012,  		TestDispatchEntry,

	_T("IntrTests-MFC in Sock 0"),	0,	              0,					0,	NULL,
	_T("Modem: addr 0x2E8 uConfigReg 35"), 	1,	  	1,        	33001,  		TestDispatchEntry,
	_T("MultiThreaded Test"), 	1,	  	4,        	33002,  		TestDispatchEntry,
	_T("CardRequestIRQ(Null Parameter)"), 	1,	  	1,        	33006,  		TestDispatchEntry,
	_T("CardRequestIRQ (Bad Socket)"), 	1,	  	1,        	33007,  		TestDispatchEntry,
	_T("CardRequestIRQ (Bad Function"), 	1,	  	1,        	33008,  		TestDispatchEntry,
	_T("CardRequestIRQ (All true params)"), 	1,	  	1,        	33009,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 1"), 	1,	  	1,        	33010,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 2"), 	1,	  	1,        	33011,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 3"), 	1,	  	1,        	33012,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 4 (Bad Params)"), 	1,	  	1,        	33013,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 5 (all good params)"), 	1,	  	1,        	33014,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35 not calling ReleaseConfiguration()"), 	1,	  	1,        	33015,  		TestDispatchEntry,
	_T("complementary for case15"), 	1,	  	1,        	33016,  		TestDispatchEntry,
	_T("addr 0x3F8 uConfigReg 32 not call CardReleaseIRQ()"), 	1,	  	1,        	33017,  		TestDispatchEntry,
	_T("Comlementary for case 17"), 1,	  	1,        	33018,  		TestDispatchEntry,
	_T("Skips CardReleaseExclusive"), 	1,	  	1,        	33019,  		TestDispatchEntry,
	_T("Complementary for case 19"), 	1,	  	1,        	33020,  		TestDispatchEntry,

	_T("MaskTests- MFC in Sock 0"),	 0,	              0,					0,	NULL,
	_T("CardGetStatus"), 	1,	  	1,        	34001,  		TestDispatchEntry,
	_T("CardGetStatus - Multithreaded"), 1,	  	2,        	34002,  		TestDispatchEntry,
	_T("CardGetStatus - Multithreaded"), 	1,	  	3,        	34003,  		TestDispatchEntry,
	_T("CardEventMasks"), 	1,	  	1,        	34004,  		TestDispatchEntry,
	_T("CardEventMaks - Multithreaded"), 	1,	  	2,        	34005,  		TestDispatchEntry,
	_T("CardEventMaks - Multithreaded)"), 	1,	  	3,        	34006,  		TestDispatchEntry,
	_T("CardResetFunction"), 	1,	  	1,        	34007,  		TestDispatchEntry,
	_T("CardResetFunction - Multithreaded"), 	1,	  	2,        	34008,  		TestDispatchEntry,
	_T("CardResetFunction - Multithreaded)"), 	1,	  	3,        	34009,  		TestDispatchEntry,
	_T("CardFunction"), 	1,	  	1,        	34010,  		TestDispatchEntry,
	_T("CardFunction - Multithreaded"), 	1,	  	2,        	34011,  		TestDispatchEntry,
	_T("CardFunction - Multithreaded"), 	1,	  	3,        	34012,  		TestDispatchEntry,
	_T("get/set event-Invalidparas"), 	1,	  	2,        	34013,  		TestDispatchEntry,
	_T("request/release socketevent-Invalidparas"), 	1,	  	3,        	34014,  		TestDispatchEntry,

	_T("TupleTests-MFC in Sock 0"),	0,	              0,					0,	NULL,
	_T("Tuple Full Test"), 	1,	  	1,        	36001,  		TestDispatchEntry,
	_T("Tuple Full Test - Multithreaded"), 1,	  	4,        	36002,  		TestDispatchEntry,
	_T("Tuple Full Test 2"), 	1,	  	1,        	36003,  		TestDispatchEntry,
	_T("Additional: CardGetFirstTuple"),	1,		1,			36101,			TestDispatchEntry,
	_T("Additional: CardGetNextTuple"),	1,		1,			36102,			TestDispatchEntry,
	_T("Additional: CardGetTupleData"), 1,		1,			36103,			TestDispatchEntry,
	_T("Additional: CardGetParsedTuple1"), 1,		1,			36104,			TestDispatchEntry,
	_T("Additional: CardGetParsedTuple2"), 1,		1,			36105,			TestDispatchEntry,

	_T("WinTests-MFC in Sock 0"),	0,	              0,					0,	NULL,
	_T("CardAdress=0x2E8, Size=0x10"),	1,	              1,					32001,	TestDispatchEntry,
	_T("CardAdress=0x2F8, Size=0x40"),	1,	              1,					32002,	TestDispatchEntry,
	_T("CardAdress=0x3E8, Size=0x08"),	1,	              1,					32003,	TestDispatchEntry,
	_T("disable common window"),		1,	              1,					32004,	TestDispatchEntry,
	_T("attribute window to IO window"),	1,	              1,					32005,	TestDispatchEntry,
	_T("IO attribute window to IO window"),	1,	              1,					32006,	TestDispatchEntry,
	_T("0x0 offset for CardMapWindow"),	1,	              1,					32007,	TestDispatchEntry,
	_T("Out Of Resource test"),	1,	              1,					32008,	TestDispatchEntry,
	_T("Card Address=0x32345, Size=0x1"),	1,	              1,					32009,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 4 bytes : uCard addr 0x2E8 "),	1,	              1,					32010,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 2 bytes : uCard addr 0x100"),	1,	              1,					32011,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 3 bytes : uCard addr 0x0"),	1,	              1,					32012,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 1 bytes : uCard addr 0x2"),	1,	              1,					32013,	TestDispatchEntry,
	_T("hWindow=NULL"),	1,	              1,					32014,	TestDispatchEntry,
	_T("hWindow=INVALID"),	1,	              1,					32015,	TestDispatchEntry,
	_T("Granularity=NULL"),	1,	              1,					32016,	TestDispatchEntry,
	_T("hWindow=NULL2"),	1,	              1,					32017,	TestDispatchEntry,
	_T("hWindow=INVALID 2"),	1,	              1,					32018,	TestDispatchEntry,
	_T("5 threads test"),	1,	              5,					32019,	TestDispatchEntry,
	_T("10 threads test"),	1,	              10,					32020,	TestDispatchEntry,
	_T("3 threads test"),	1,	              3,					32021,	TestDispatchEntry,
	_T("mem window map upto 64MB"),	1,	              1,					32023,	TestDispatchEntry,
	_T("Request less than 800 K window: Card addr 0xC5000"),	1,	              1,					32024,	TestDispatchEntry,
	_T("CardModifyWindow with invalid client"),	1,	              1,					32025,	TestDispatchEntry,
	_T("Regression test case for CardMapWindow1 "),	1,	              1,					32026,	TestDispatchEntry,
	_T("Test window functions with invalid paras"),	1,	              1,					32027,	TestDispatchEntry,
	_T("Test collide windows"),	1,	              1,					32028,	TestDispatchEntry,

	_T("ConfigTest-MFC in Sock 1"),	0,	              0,					0,	NULL,
	_T("addr 0x3F8 uConfigReg 32"), 	1,	  	1,        	41001,  		TestDispatchEntry,
	_T("addr 0x2F8 uConfigReg 33"),    1,   		1,        	41002,  		TestDispatchEntry,
	_T("addr 0x3E8 uConfigReg 34"),    1,   		1,        	41003,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35"), 	1,   		1,        	41004,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35 (different call order)"), 	1,		1,        	41005,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35(missing some calls) "),		1,   1,       41006, TestDispatchEntry,
	_T("register as memcard"),	1,  	1,	41007,	TestDispatchEntry,
	_T("Don't request before release 1"),	1,  	1,	41008,	TestDispatchEntry,
	_T("Don't request before release 2"),	1,  	1,	41009, 	TestDispatchEntry,
	_T("NULL Handle"),		1,	1,	41010,	TestDispatchEntry,
	_T("Invalid Handle"),	1,  	1,    41011, 	TestDispatchEntry,
	_T("NULL CardParam"), 	1,  	1,   	41012, 	TestDispatchEntry,
	_T("Invalid Socket"), 	1,  	1,    41013, 	TestDispatchEntry,
	_T("NULL Handle"),		1,	1,	41014,	TestDispatchEntry,
	_T("Invalid Handle"),	1,  	1,	41015, 	TestDispatchEntry,
	_T("Invalid Socket"),    	1,  	1,   	41016, 	TestDispatchEntry,
	_T("partial release 1"),	1,	1,	41017,	TestDispatchEntry,
	_T("partial release 2"), 	1,  	1,    41018, 	TestDispatchEntry,
	_T("Config Test"),		1,	2,	41019,	TestDispatchEntry,
	_T("fInterfaceType = CFG_IFACE_MEMORY"),	1,	1,	41020,	TestDispatchEntry,
	_T("fRegisters =0x1F"),		1,	1,	41021,	TestDispatchEntry,
	_T("uStatusReg = 0x63"),	1,	1,	41022,	TestDispatchEntry,
	_T("uStatusReg = 0xED"),		1,  1,       41023, TestDispatchEntry,
	_T("uStatusReg = 0xFF"),  1,  1,       41024, TestDispatchEntry,
	_T("Dial Out Test"),	1,  1,      41025, TestDispatchEntry,
	_T("Dial In Test"),	1,  1,      41026, TestDispatchEntry,
	_T("addr 0x555 uConfigReg 55"),	1,  1,       41027, TestDispatchEntry,
	_T("CardAccessConfigurationRegister"),	1,  1,      41028, TestDispatchEntry,
	_T("Null hClient for Read"),	1,  1,       41029, TestDispatchEntry,
	_T("Null hClient for Write"),	1,  1,       41030, TestDispatchEntry,
	_T("Null pvalue"),	1,	1,		 41031, TestDispatchEntry,
	_T("Null Params for register"),	1,	1,		 41032, TestDispatchEntry,
	_T("Invalid Params for register"),	1,	1,		 41033, TestDispatchEntry,
	_T("CardModifyConfiguration:IRQWakeup"),	1,	1,		 41034, TestDispatchEntry,
	_T("CardModifyConfiguration:KeepPowered"),	1,	1,		 41035, TestDispatchEntry,
	_T("CardModifyConfiguration:NoSuspendUnload"),	1,	1,		 41036, TestDispatchEntry,
	_T("CardModifyConfiguration:InValidParas"),	1,	1,		 41037, TestDispatchEntry,
	_T("CardAccessConfigurationRegister,wrong client handle"),	1,  1,      41038, TestDispatchEntry,
	_T("CardPowerOn and Off"),	1,	1,		 41039, TestDispatchEntry,
	
	_T("ExclTests-MFC in Sock 1"),	0,	              0,					0,	NULL,
	_T("fAttri = 0x9: MEMORY Client"), 	1,	  	1,        	45001,  		TestDispatchEntry,
	_T("fAttri = 0xA: IO Client"), 	1,	  	1,        	45002,  		TestDispatchEntry,
	_T("fAttri = 0xA: IO Client:  Not calling RequestExclusive"), 	2,	  	1,        	45003,  		TestDispatchEntry,
	_T("Paramter checking NULL HANDLE"), 	1,	  	1,        	45004,  		TestDispatchEntry,
	_T("Paramter checking BAD HANDLE"), 	1,	  	1,        	45005,  		TestDispatchEntry,
	_T("Only release window before calling CardGetExclusive()"), 	1,	  	1,        	45006,  		TestDispatchEntry,
	_T("Invalid socket 0xFF, function 0xFF"), 	1,	  	1,        	45007,  		TestDispatchEntry,
	_T("Not calling RequestExclusive: only test ReleaseExclusive"), 	1,	  	1,        	45008,  		TestDispatchEntry,
	_T("Not calling RequestExclusive: only test ReleaseExclusive 2"), 	1,	  	1,        	45009,  		TestDispatchEntry,
	_T("Multithreaded test,5 threads"), 	1,	  	5,        	45010,  		TestDispatchEntry,
	_T("Multithreaded test,2 threads"), 	1,	  	2,        	45011,  		TestDispatchEntry,
	_T("Call CardRequestExclusive(),  BUT DON'T call CardReleaseExclusive()"), 	1,	  	1,        	45012,  		TestDispatchEntry,

	_T("IntrTests-MFC in Sock 1"),	0,	              0,					0,	NULL,
	_T("Modem: addr 0x2E8 uConfigReg 35"), 	1,	  	1,        	43001,  		TestDispatchEntry,
	_T("MultiThreaded Test"), 	1,	  	4,        	43002,  		TestDispatchEntry,
	_T("CardRequestIRQ(Null Parameter)"), 	1,	  	1,        	43006,  		TestDispatchEntry,
	_T("CardRequestIRQ (Bad Socket)"), 	1,	  	1,        	43007,  		TestDispatchEntry,
	_T("CardRequestIRQ (Bad Function"), 	1,	  	1,        	43008,  		TestDispatchEntry,
	_T("CardRequestIRQ (All true params)"), 	1,	  	1,        	43009,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 1"), 	1,	  	1,        	43010,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 2"), 	1,	  	1,        	43011,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 3"), 	1,	  	1,        	43012,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 4 (Bad Params)"), 	1,	  	1,        	43013,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 5 (all good params)"), 	1,	  	1,        	43014,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35 not calling ReleaseConfiguration()"), 	1,	  	1,        	43015,  		TestDispatchEntry,
	_T("complementary for case15"), 	1,	  	1,        	43016,  		TestDispatchEntry,
	_T("addr 0x3F8 uConfigReg 32 not call CardReleaseIRQ()"), 	1,	  	1,        	43017,  		TestDispatchEntry,
	_T("Comlementary for case 17"), 1,	  	1,        	43018,  		TestDispatchEntry,
	_T("Skips CardReleaseExclusive"), 	1,	  	1,        	43019,  		TestDispatchEntry,
	_T("Complementary for case 19"), 	1,	  	1,        	43020,  		TestDispatchEntry,

	_T("MaskTests- MFC in Sock 1"),	 0,	              0,					0,	NULL,
	_T("CardGetStatus"), 	1,	  	1,        	44001,  		TestDispatchEntry,
	_T("CardGetStatus - Multithreaded"), 1,	  	2,        	44002,  		TestDispatchEntry,
	_T("CardGetStatus - Multithreaded"), 	1,	  	3,        	44003,  		TestDispatchEntry,
	_T("CardEventMasks"), 	1,	  	1,        	44004,  		TestDispatchEntry,
	_T("CardEventMaks - Multithreaded"), 	1,	  	2,        	44005,  		TestDispatchEntry,
	_T("CardEventMaks - Multithreaded)"), 	1,	  	3,        	44006,  		TestDispatchEntry,
	_T("CardResetFunction"), 	1,	  	1,        	44007,  		TestDispatchEntry,
	_T("CardResetFunction - Multithreaded"), 	1,	  	2,        	44008,  		TestDispatchEntry,
	_T("CardResetFunction - Multithreaded)"), 	1,	  	3,        	44009,  		TestDispatchEntry,
	_T("CardFunction"), 	1,	  	1,        	44010,  		TestDispatchEntry,
	_T("CardFunction - Multithreaded"), 	1,	  	2,        	44011,  		TestDispatchEntry,
	_T("CardFunction - Multithreaded"), 	1,	  	3,        	44012,  		TestDispatchEntry,
	_T("get/set event-Invalidparas"), 	1,	  	2,        	44013,  		TestDispatchEntry,
	_T("request/release socketevent-Invalidparas"), 	1,	  	3,        	44014,  		TestDispatchEntry,

	_T("TupleTests-MFC in Sock 1"),	0,	              0,					0,	NULL,
	_T("Tuple Full Test"), 	1,	  	1,        	46001,  		TestDispatchEntry,
	_T("Tuple Full Test - Multithreaded"), 1,	  	4,        	46002,  		TestDispatchEntry,
	_T("Tuple Full Test 2"), 	1,	  	1,        	46003,  		TestDispatchEntry,
	_T("Additional: CardGetFirstTuple"),	1,		1,			46101,			TestDispatchEntry,
	_T("Additional: CardGetNextTuple"),	1,		1,			46102,			TestDispatchEntry,
	_T("Additional: CardGetTupleData"), 1,		1,			46103,			TestDispatchEntry,
	_T("Additional: CardGetParsedTuple1"), 1,		1,			46104,			TestDispatchEntry,
	_T("Additional: CardGetParsedTuple2"), 1,		1,			46105,			TestDispatchEntry,

	_T("WinTests-MFC in Sock 1"),	0,	              0,					0,	NULL,
	_T("CardAdress=0x2E8, Size=0x10"),	1,	              1,					42001,	TestDispatchEntry,
	_T("CardAdress=0x2F8, Size=0x40"),	1,	              1,					42002,	TestDispatchEntry,
	_T("CardAdress=0x3E8, Size=0x08"),	1,	              1,					42003,	TestDispatchEntry,
	_T("disable common window"),		1,	              1,					42004,	TestDispatchEntry,
	_T("attribute window to IO window"),	1,	              1,					42005,	TestDispatchEntry,
	_T("IO attribute window to IO window"),	1,	              1,					42006,	TestDispatchEntry,
	_T("0x0 offset for CardMapWindow"),	1,	              1,					42007,	TestDispatchEntry,
	_T("Out Of Resource test"),	1,	              1,					42008,	TestDispatchEntry,
	_T("Card Address=0x42345, Size=0x1"),	1,	              1,					42009,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 4 bytes : uCard addr 0x2E8 "),	1,	              1,					42010,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 2 bytes : uCard addr 0x100"),	1,	              1,					42011,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 3 bytes : uCard addr 0x0"),	1,	              1,					42012,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 1 bytes : uCard addr 0x2"),	1,	              1,					42013,	TestDispatchEntry,
	_T("hWindow=NULL"),	1,	              1,					42014,	TestDispatchEntry,
	_T("hWindow=INVALID"),	1,	              1,					42015,	TestDispatchEntry,
	_T("Granularity=NULL"),	1,	              1,					42016,	TestDispatchEntry,
	_T("hWindow=NULL2"),	1,	              1,					42017,	TestDispatchEntry,
	_T("hWindow=INVALID 2"),	1,	              1,					42018,	TestDispatchEntry,
	_T("5 threads test"),	1,	              5,					42019,	TestDispatchEntry,
	_T("10 threads test"),	1,	              10,					42020,	TestDispatchEntry,
	_T("3 threads test"),	1,	              3,					42021,	TestDispatchEntry,
	_T("mem window map upto 64MB"),	1,	              1,					42023,	TestDispatchEntry,
	_T("Request less than 800 K window: Card addr 0xC5000"),	1,	              1,					42024,	TestDispatchEntry,
	_T("CardModifyWindow with invalid client"),	1,	              1,					42025,	TestDispatchEntry,
	_T("Regression test case for CardMapWindow1 "),	1,	              1,					42026,	TestDispatchEntry,
	_T("Test window functions with invalid paras"),	1,	              1,					42027,	TestDispatchEntry,
	_T("Test collide windows"),	1,	              1,					42028,	TestDispatchEntry,

	_T("ConfigTest-SFC in 2 Sockets "),	0,	              0,					0,	NULL,
	_T("addr 0x3F8 uConfigReg 32"), 	1,	  	1,        	51001,  		TestDispatchEntry,
	_T("addr 0x2F8 uConfigReg 33"),    1,   		1,        	51002,  		TestDispatchEntry,
	_T("addr 0x3E8 uConfigReg 34"),    1,   		1,        	51003,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35"), 	1,   		1,        	51004,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35 (different call order)"), 	1,		1,        	51005,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35(missing some calls) "),		1,   1,       51006, TestDispatchEntry,
	_T("register as memcard"),	1,  	1,	51007,	TestDispatchEntry,
	_T("Don't request before release 1"),	1,  	1,	51008,	TestDispatchEntry,
	_T("Don't request before release 2"),	1,  	1,	51009, 	TestDispatchEntry,
	_T("NULL Handle"),		1,	1,	51010,	TestDispatchEntry,
	_T("Invalid Handle"),	1,  	1,    51011, 	TestDispatchEntry,
	_T("NULL CardParam"), 	1,  	1,   	51012, 	TestDispatchEntry,
	_T("Invalid Socket"), 	1,  	1,    51013, 	TestDispatchEntry,
	_T("NULL Handle"),		1,	1,	51014,	TestDispatchEntry,
	_T("Invalid Handle"),	1,  	1,	51015, 	TestDispatchEntry,
	_T("Invalid Socket"),    	1,  	1,   	51016, 	TestDispatchEntry,
	_T("partial release 1"),	1,	1,	51017,	TestDispatchEntry,
	_T("partial release 2"), 	1,  	1,    51018, 	TestDispatchEntry,
	_T("Config Test"),		1,	2,	51019,	TestDispatchEntry,
	_T("fInterfaceType = CFG_IFACE_MEMORY"),	1,	1,	51020,	TestDispatchEntry,
	_T("fRegisters =0x1F"),		1,	1,	51021,	TestDispatchEntry,
	_T("uStatusReg = 0x63"),	1,	1,	51022,	TestDispatchEntry,
	_T("uStatusReg = 0xED"),		1,  1,       51023, TestDispatchEntry,
	_T("uStatusReg = 0xFF"),  1,  1,       51024, TestDispatchEntry,
	_T("Dial Out Test"),	1,  1,      51025, TestDispatchEntry,
	_T("Dial In Test"),	1,  1,      51026, TestDispatchEntry,
	_T("addr 0x555 uConfigReg 55"),	1,  1,       51027, TestDispatchEntry,
	_T("CardAccessConfigurationRegister"),	1,  1,      51028, TestDispatchEntry,
	_T("Null hClient for Read"),	1,  1,       51029, TestDispatchEntry,
	_T("Null hClient for Write"),	1,  1,       51030, TestDispatchEntry,
	_T("Null pvalue"),	1,	1,		 51031, TestDispatchEntry,
	_T("Null Params for register"),	1,	1,		 51032, TestDispatchEntry,
	_T("Invalid Params for register"),	1,	1,		 51033, TestDispatchEntry,
	_T("CardModifyConfiguration:IRQWakeup"),	1,	1,		 51034, TestDispatchEntry,
	_T("CardModifyConfiguration:KeepPowered"),	1,	1,		 51035, TestDispatchEntry,
	_T("CardModifyConfiguration:NoSuspendUnload"),	1,	1,		 51036, TestDispatchEntry,
	_T("CardModifyConfiguration:InValidParas"),	1,	1,		 51037, TestDispatchEntry,
	_T("CardAccessConfigurationRegister,wrong client handle"),	1,  1,      51038, TestDispatchEntry,
	_T("CardPowerOn and Off"),	1,	1,		 51039, TestDispatchEntry,
	
	_T("ExclTests-SFC in 2 Sockets"),	0,	              0,					0,	NULL,
	_T("fAttri = 0x9: MEMORY Client"), 	1,	  	1,        	55001,  		TestDispatchEntry,
	_T("fAttri = 0xA: IO Client"), 	1,	  	1,        	55002,  		TestDispatchEntry,
	_T("fAttri = 0xA: IO Client:  Not calling RequestExclusive"), 	2,	  	1,        	55003,  		TestDispatchEntry,
	_T("Paramter checking NULL HANDLE"), 	1,	  	1,        	55004,  		TestDispatchEntry,
	_T("Paramter checking BAD HANDLE"), 	1,	  	1,        	55005,  		TestDispatchEntry,
	_T("Only release window before calling CardGetExclusive()"), 	1,	  	1,        	55006,  		TestDispatchEntry,
	_T("Invalid socket 0xFF, function 0xFF"), 	1,	  	1,        	55007,  		TestDispatchEntry,
	_T("Not calling RequestExclusive: only test ReleaseExclusive"), 	1,	  	1,        	55008,  		TestDispatchEntry,
	_T("Not calling RequestExclusive: only test ReleaseExclusive 2"), 	1,	  	1,        	55009,  		TestDispatchEntry,
	_T("Multithreaded test,5 threads"), 	1,	  	5,        	55010,  		TestDispatchEntry,
	_T("Multithreaded test,2 threads"), 	1,	  	2,        	55011,  		TestDispatchEntry,
	_T("Call CardRequestExclusive(),  BUT DON'T call CardReleaseExclusive()"), 	1,	  	1,        	55012,  		TestDispatchEntry,

	_T("IntrTests-SFC in 2 Sockets"),	0,	              0,					0,	NULL,
	_T("Modem: addr 0x2E8 uConfigReg 35"), 	1,	  	1,        	53001,  		TestDispatchEntry,
	_T("MultiThreaded Test"), 	1,	  	4,        	53002,  		TestDispatchEntry,
	_T("CardRequestIRQ(Null Parameter)"), 	1,	  	1,        	53006,  		TestDispatchEntry,
	_T("CardRequestIRQ (Bad Socket)"), 	1,	  	1,        	53007,  		TestDispatchEntry,
	_T("CardRequestIRQ (Bad Function"), 	1,	  	1,        	53008,  		TestDispatchEntry,
	_T("CardRequestIRQ (All true params)"), 	1,	  	1,        	53009,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 1"), 	1,	  	1,        	53010,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 2"), 	1,	  	1,        	53011,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 3"), 	1,	  	1,        	53012,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 4 (Bad Params)"), 	1,	  	1,        	53013,  		TestDispatchEntry,
	_T("CardReleaseIRQ Parameter Test 5 (all good params)"), 	1,	  	1,        	53014,  		TestDispatchEntry,
	_T("addr 0x2E8 uConfigReg 35 not calling ReleaseConfiguration()"), 	1,	  	1,        	53015,  		TestDispatchEntry,
	_T("complementary for case15"), 	1,	  	1,        	53016,  		TestDispatchEntry,
	_T("addr 0x3F8 uConfigReg 32 not call CardReleaseIRQ()"), 	1,	  	1,        	53017,  		TestDispatchEntry,
	_T("Comlementary for case 17"), 1,	  	1,        	53018,  		TestDispatchEntry,
	_T("Skips CardReleaseExclusive"), 	1,	  	1,        	53019,  		TestDispatchEntry,
	_T("Complementary for case 19"), 	1,	  	1,        	53020,  		TestDispatchEntry,

	_T("MaskTests- SFC in 2 Sockets"),	 0,	              0,					0,	NULL,
	_T("CardGetStatus"), 	1,	  	1,        	54001,  		TestDispatchEntry,
	_T("CardGetStatus - Multithreaded"), 1,	  	2,        	54002,  		TestDispatchEntry,
	_T("CardGetStatus - Multithreaded"), 	1,	  	3,        	54003,  		TestDispatchEntry,
	_T("CardEventMasks"), 	1,	  	1,        	54004,  		TestDispatchEntry,
	_T("CardEventMaks - Multithreaded"), 	1,	  	2,        	54005,  		TestDispatchEntry,
	_T("CardEventMaks - Multithreaded)"), 	1,	  	3,        	54006,  		TestDispatchEntry,
	_T("CardResetFunction"), 	1,	  	1,        	54007,  		TestDispatchEntry,
	_T("CardResetFunction - Multithreaded"), 	1,	  	2,        	54008,  		TestDispatchEntry,
	_T("CardResetFunction - Multithreaded)"), 	1,	  	3,        	54009,  		TestDispatchEntry,
	_T("CardFunction"), 	1,	  	1,        	54010,  		TestDispatchEntry,
	_T("CardFunction - Multithreaded"), 	1,	  	2,        	54011,  		TestDispatchEntry,
	_T("CardFunction - Multithreaded"), 	1,	  	3,        	54012,  		TestDispatchEntry,
	_T("get/set event-Invalidparas"), 	1,	  	2,        	54013,  		TestDispatchEntry,
	_T("request/release socketevent-Invalidparas"), 	1,	  	3,        	54014,  		TestDispatchEntry,

	_T("TupleTests-SFC in 2 Sockets "),	0,	              0,					0,	NULL,
	_T("Tuple Full Test"), 	1,	  	1,        	56001,  		TestDispatchEntry,
	_T("Tuple Full Test - Multithreaded"), 1,	  	4,        	56002,  		TestDispatchEntry,
	_T("Tuple Full Test 2"), 	1,	  	1,        	56003,  		TestDispatchEntry,
	_T("Additional: CardGetFirstTuple"),	1,		1,			56101,			TestDispatchEntry,
	_T("Additional: CardGetNextTuple"),	1,		1,			56102,			TestDispatchEntry,
	_T("Additional: CardGetTupleData"), 1,		1,			56103,			TestDispatchEntry,
	_T("Additional: CardGetParsedTuple1"), 1,		1,			56104,			TestDispatchEntry,
	_T("Additional: CardGetParsedTuple2"), 1,		1,			56105,			TestDispatchEntry,

	_T("WinTests-MFC in 2 Sockets"),	0,	              0,					0,	NULL,
	_T("CardAdress=0x2E8, Size=0x10"),	1,	              1,					52001,	TestDispatchEntry,
	_T("CardAdress=0x2F8, Size=0x40"),	1,	              1,					52002,	TestDispatchEntry,
	_T("CardAdress=0x3E8, Size=0x08"),	1,	              1,					52003,	TestDispatchEntry,
	_T("disable common window"),		1,	              1,					52004,	TestDispatchEntry,
	_T("attribute window to IO window"),	1,	              1,					52005,	TestDispatchEntry,
	_T("IO attribute window to IO window"),	1,	              1,					52006,	TestDispatchEntry,
	_T("0x0 offset for CardMapWindow"),	1,	              1,					52007,	TestDispatchEntry,
	_T("Out Of Resource test"),	1,	              1,					52008,	TestDispatchEntry,
	_T("Card Address=0x52345, Size=0x1"),	1,	              1,					52009,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 4 bytes : uCard addr 0x2E8 "),	1,	              1,					52010,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 2 bytes : uCard addr 0x100"),	1,	              1,					52011,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 3 bytes : uCard addr 0x0"),	1,	              1,					52012,	TestDispatchEntry,
	_T("Allocate 2 bytes Map 1 bytes : uCard addr 0x2"),	1,	              1,					52013,	TestDispatchEntry,
	_T("hWindow=NULL"),	1,	              1,					52014,	TestDispatchEntry,
	_T("hWindow=INVALID"),	1,	              1,					52015,	TestDispatchEntry,
	_T("Granularity=NULL"),	1,	              1,					52016,	TestDispatchEntry,
	_T("hWindow=NULL2"),	1,	              1,					52017,	TestDispatchEntry,
	_T("hWindow=INVALID 2"),	1,	              1,					52018,	TestDispatchEntry,
	_T("5 threads test"),	1,	              5,					52019,	TestDispatchEntry,
	_T("10 threads test"),	1,	              10,					52020,	TestDispatchEntry,
	_T("3 threads test"),	1,	              3,					52021,	TestDispatchEntry,
	_T("mem window map upto 64MB"),	1,	              1,					52023,	TestDispatchEntry,
	_T("Request less than 800 K window: Card addr 0xC5000"),	1,	              1,					52024,	TestDispatchEntry,
	_T("CardModifyWindow with invalid client"),	1,	              1,					52025,	TestDispatchEntry,
	_T("Regression test case for CardMapWindow1 "),	1,	              1,					52026,	TestDispatchEntry,
	_T("Test window functions with invalid paras"),	1,	              1,					52027,	TestDispatchEntry,
	_T("Test collide windows"),	1,	              1,					52028,	TestDispatchEntry,
	NULL,	                                        0,	0,					0,			NULL,
};	

}

// --------------------------------------------------------------------
SHELLPROCAPI 
ShellProc(
    UINT uMsg, 
    SPPARAM spParam ) 
// --------------------------------------------------------------------    
{
    LPSPS_BEGIN_TEST pBT = {0};
    LPSPS_END_TEST pET = {0};

    switch (uMsg) {
    
        // --------------------------------------------------------------------
        // Message: SPM_LOAD_DLL
        //
        case SPM_LOAD_DLL: 
            DEBUGMSG(ZONE_FUNCTION, (_T("ShellProc(SPM_LOAD_DLL, ...) called")));

            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif

            // turn on kato debugging
            KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);

            // Get/Create our global logging object.
//            g_pKato = (CKato*)KatoGetDefaultObject();

            // Initialize our global critical section.
            InitializeCriticalSection(&g_csProcess);

		uSocket = 0xFF;
		uFunction = 0xFF;
		bCardNotInSlot = FALSE;
		bRegUpdated = FALSE;
		g_Vcc = 50; //by default the voltage is set to 5V
		
		// init cardservices funcptrs
		hInst=InitCardServicesTable();
		if(!hInst)
		{
			g_pKato->Log(LOG_FAIL, TEXT("PCMCIA tests couldn't load pcmcia.dll !!!\n"));
			return SPR_NOT_HANDLED;
		}

            return SPR_HANDLED;        

        // --------------------------------------------------------------------
        // Message: SPM_UNLOAD_DLL
        //
        case SPM_UNLOAD_DLL: 
            DEBUGMSG(ZONE_FUNCTION, (_T("ShellProc(SPM_UNLOAD_DLL, ...) called")));

            // This is a good place to delete our global critical section.
            DeleteCriticalSection(&g_csProcess);

            //Free PCMCIA.dll
		if(hInst)
			FreeLibrary(hInst);

            return SPR_HANDLED;      

        // --------------------------------------------------------------------
        // Message: SPM_SHELL_INFO
        //
        case SPM_SHELL_INFO:
            DEBUGMSG(ZONE_FUNCTION, (_T("ShellProc(SPM_SHELL_INFO, ...) called")));
            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
            if( g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine ){
                // Display our Dlls command line if we have one.
                g_pKato->Log( LOG_DETAIL, 
                    _T("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);

                ProcessCmdLine(g_pShellInfo->szDllCmdLine);
            }

      
        	return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_REGISTER
        //
        case SPM_REGISTER:
            DEBUGMSG(ZONE_FUNCTION, (_T("ShellProc(SPM_REGISTER, ...) called")));
            
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            #ifdef UNICODE
                return SPR_HANDLED | SPF_UNICODE;
            #else
                return SPR_HANDLED;
            #endif
            
        // --------------------------------------------------------------------
        // Message: SPM_START_SCRIPT
        //
        case SPM_START_SCRIPT:


	           return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_STOP_SCRIPT
        //
        case SPM_STOP_SCRIPT:

		//Reset registry
		ResetRegistry();

            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_BEGIN_GROUP
        //
        case SPM_BEGIN_GROUP:
            DEBUGMSG(ZONE_FUNCTION, (_T("ShellProc(SPM_BEGIN_GROUP, ...) called")));
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: FSDTEST.DLL"));
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            DEBUGMSG(ZONE_FUNCTION, (_T("ShellProc(SPM_END_GROUP, ...) called")));
            g_pKato->EndLevel(_T("END GROUP: TUXDEMO.DLL"));
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_BEGIN_TEST
        //
        case SPM_BEGIN_TEST:
            DEBUGMSG(ZONE_FUNCTION, (_T("ShellProc(SPM_BEGIN_TEST, ...) called")));

            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                                _T("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                pBT->dwRandomSeed);

            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_END_TEST
        //
        case SPM_END_TEST:
            DEBUGMSG(ZONE_FUNCTION, (_T("ShellProc(SPM_END_TEST, ...) called")));

            // End our logging level.
            pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(_T("END TEST: \"%s\", %s, Time=%u.%03u"),
                              pET->lpFTE->lpDescription,
                              pET->dwResult == TPR_SKIP ? _T("SKIPPED") :
                              pET->dwResult == TPR_PASS ? _T("PASSED") :
                              pET->dwResult == TPR_FAIL ? _T("FAILED") : _T("ABORTED"),
                              pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_EXCEPTION
        //
        case SPM_EXCEPTION:
            DEBUGMSG(ZONE_FUNCTION, (_T("ShellProc(SPM_EXCEPTION, ...) called")));
            g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
            return SPR_HANDLED;

        default:
            DEBUGMSG(ZONE_ERROR, (_T("ShellProc received bad message: 0x%X"), uMsg));
            ASSERT(!"Default case reached in ShellProc!");
            return SPR_NOT_HANDLED;
    }
}

VOID
ProcessCmdLine(LPCTSTR	szCmdLine){

	LPCTSTR	pCmdLine = NULL;
	ULONG	uVccVal;
	
	if(szCmdLine == NULL) //no command line
		return;

	pCmdLine = szCmdLine; 

	while(pCmdLine[0] != 0){
		//find "-" 
		while((pCmdLine[0]) != _T('-') && pCmdLine[0] != 0){
				pCmdLine++;
	 	}

		if(pCmdLine[0] == 0) //commandline ends
			break; 
		else	//we found a '-'
			pCmdLine++;
		
		if(pCmdLine[0] != 'v' && pCmdLine[0] != 'V')//not the option we want
			continue; 
		else //yeah, we found 'v' or 'V'
			pCmdLine++; 

		uVccVal = 0;
		while(pCmdLine[0] == _T(' ')){ //skip possible blanks
			pCmdLine++;
		}
		
		if(pCmdLine[0]>= _T('0') && pCmdLine[0] <=_T('9')){//we found the voltage setting
			if(pCmdLine[1]>= _T('0') && pCmdLine[1] <=_T('9')){//it has two digits
				uVccVal = ((ULONG)pCmdLine[1] - ((ULONG)_T('0')));
				uVccVal += (((ULONG)pCmdLine[0] - ((ULONG)_T('0')))*10);
			}
			else{//just one digit
					uVccVal = ((ULONG)pCmdLine[0] - ((ULONG)_T('0')));
			}
			g_Vcc = (USHORT)uVccVal;
		}
		else{//illegal input, ignore it
			break;
		}
	}
       g_pKato->Log(LOG_DETAIL, _T("Vcc value is set to %1.1f!"), ((float)g_Vcc)/10.0);

		
	 return; 
}
