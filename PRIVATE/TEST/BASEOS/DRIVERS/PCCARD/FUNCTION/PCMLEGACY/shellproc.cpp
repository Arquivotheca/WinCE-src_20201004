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
	Shellproc.cpp

Abstract:

    Interface between test dll and tux
    
--*/

#include "testmain.h"
#include "common.h"
#include "resource.h"
#include "ddlxioct.h"
#include "regmani.h"

//globals 
//DECLARE_CARDSERVICES_TABLE;

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato * g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION g_csProcess;

//Handle for this dll
HINSTANCE g_hInst = NULL;

//The handle for pcmcia.dll
HINSTANCE hInst = NULL;

CARD_CLIENT_HANDLE  	g_hClient = NULL;
CLIENT_CONTEXT g_ClientData = {0};

USHORT		uTestCategory;
BOOL		bCardNotInSlot;
BOOL		bRegUpdated;
DWORD		dwTotalThread;
USHORT		g_Vcc;

SOCKET_DESCRIPTOR	g_SocketDescs[MAX_SOCKETS];
CARD_DESCRIPTOR		g_CardDescs[MAX_SOCKETS*2];
DWORD		g_dwTotalSockets;
DWORD		g_dwTotalCards;

HINSTANCE	g_hPCCardDll = NULL;

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

	_T("Single-function card test in one socket"),	0,	              0,					0,	NULL,

	_T("Request configuration test"), 	1,	  	1,        	11001,  		TestDispatchEntry,
	_T("Modify configuration test"),    1,   		1,        	11002,  		TestDispatchEntry,
	_T("Access config registers test"),    1,   		1,        	11003,  		TestDispatchEntry,
	_T("Normal window test"),	1,	              1,					12001,	TestDispatchEntry,
	_T("Window test with invalid inputs"),	1,	              1,					12002,	TestDispatchEntry,
	_T("Request/Release IRQ test"), 	1,	  	1,        	13001,  		TestDispatchEntry,
	_T("EventMask Test"), 	1,	  	1,        	14001,  		TestDispatchEntry,
	_T("Tuple Full Test"), 	1,	  	1,        	16001,  		TestDispatchEntry,
	_T("Tuple Full Test - Multithreaded"), 1,	  	1,        	16002,  		TestDispatchEntry,
	_T("Tuple Full Test 2"), 	1,	  	1,        	16003,  		TestDispatchEntry,
	_T("Additional: CardGetFirstTuple"),	1,		1,			16101,			TestDispatchEntry,
	_T("Additional: CardGetNextTuple"),	1,		1,			16102,			TestDispatchEntry,


	_T("Multiple function card test in two sockets"),	0,	              0,					0,	NULL,

	_T("Request configuration test"), 	1,	  	1,        	21001,  		TestDispatchEntry,
	_T("Modify configuration test"),    1,   		1,        	21002,  		TestDispatchEntry,
	_T("Access config registers test"),    1,   		1,        	21003,  		TestDispatchEntry,
	_T("Normal window test"),	1,	              1,					22001,	TestDispatchEntry,
	_T("Window test with invalid inputs"),	1,	              1,					22002,	TestDispatchEntry,
	_T("Request/Release IRQ test"), 	1,	  	1,        	23001,  		TestDispatchEntry,
	_T("EventMask Test"), 	1,	  	1,        	24001,  		TestDispatchEntry,
	_T("Tuple Full Test"), 	1,	  	1,        	26001,  		TestDispatchEntry,
	_T("Tuple Full Test - Multithreaded"), 1,	  	1,        	26002,  		TestDispatchEntry,
	_T("Additional: CardGetFirstTuple"),	1,		1,			26101,			TestDispatchEntry,
	_T("Additional: CardGetNextTuple"),	1,		1,			26102,			TestDispatchEntry,
	_T("Additional: CardGetTupleData"), 1,		1,			26103,			TestDispatchEntry,

	_T("card test in all sockets"),	0,	              0,					0,	NULL,

	_T("Request configuration test"), 	1,	  	1,        	31001,  		TestDispatchEntry,
	_T("Modify configuration test"),    1,   		1,        	31002,  		TestDispatchEntry,
	_T("Access config registers test"),    1,   		1,        	31003,  		TestDispatchEntry,
	_T("Normal window test"),	1,	              1,					32001,	TestDispatchEntry,
	_T("Window test with invalid inputs"),	1,	              1,					32002,	TestDispatchEntry,
	_T("Request/Release IRQ test"), 	1,	  	1,        	33001,  		TestDispatchEntry,
	_T("EventMask Test"), 	1,	  	1,        	34001,  		TestDispatchEntry,
	_T("Tuple Full Test"), 	1,	  	1,        	36001,  		TestDispatchEntry,
	_T("Tuple Full Test - Multithreaded"), 1,	  	4,        	36002,  		TestDispatchEntry,
	_T("Tuple Full Test 2"), 	1,	  	1,        	36003,  		TestDispatchEntry,
	_T("Additional: CardGetFirstTuple"),	1,		1,			36101,			TestDispatchEntry,
	_T("Additional: CardGetNextTuple"),	1,		1,			36102,			TestDispatchEntry,

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
    DWORD dwRet;

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
			g_pKato = (CKato *)KatoGetDefaultObject();

		//get funciton pointers of those APIs in pcc_serv
		if (g_hPCCardDll == NULL ) {
		    g_hPCCardDll = LoadLibrary(TEXT("pcc_serv.dll"));
		    if (g_hPCCardDll == NULL) {
		        DEBUGMSG(ZONE_ERROR, (TEXT("LoadLibrary(pcc_serv.dll) failed %d\r\n"),GetLastError()));
		        ASSERT(FALSE);
		        return SPR_NOT_HANDLED;
		    }
		    else {
		        pfnEnumSocket = (PFN_EnumSocket)GetProcAddress(g_hPCCardDll, TEXT("EnumSocket"));
		        pfnEnumCard = (PFN_EnumCard)GetProcAddress(g_hPCCardDll, TEXT("EnumCard"));
		        pfnGetSocketStatus = (PFN_GetSocketStatus)GetProcAddress(g_hPCCardDll, TEXT("GetSocketStatus"));
		        pfnGetSocketIndex= (PFN_GetSocketIndex)GetProcAddress(g_hPCCardDll, TEXT("GetSocketIndex"));
		    }
		}
		
             if(pfnEnumCard == NULL || pfnEnumSocket == NULL || pfnGetSocketIndex == NULL || pfnGetSocketStatus == NULL){
                 NKDbgPrintfW(_T("Failed to get API pointers from pcc_serv.dll!!!"));
                 return SPR_NOT_HANDLED;
             }
		//update registry
		if(UpdateLegacyRegistry() == FALSE){
                   NKDbgPrintfW(_T("Failed to update registry!!!!"));
			return SPR_NOT_HANDLED;		
		}
		//load apis
		if(PCMCIAWR_Init(TEXT("pcmcia.dll")) == FALSE){
			return SPR_NOT_HANDLED;
		}
		
            // Initialize our global critical section.
            InitializeCriticalSection(&g_csProcess);

		uTestCategory = 0xFF;
		bCardNotInSlot = FALSE;
		bRegUpdated = FALSE;
		g_Vcc = 50; //by default the voltage is set to 5V

		memset(g_SocketDescs, 0, sizeof(g_SocketDescs));
		memset(g_CardDescs, 0, sizeof(g_CardDescs));
		g_dwTotalCards = 0;
		g_dwTotalSockets = 0;

		
		CARD_REGISTER_PARMS crParam;

		//register client now
		g_ClientData.uClientID = MAIN_THREAD_ID;
		//create event:
		g_ClientData.hEvent = CreateEvent(0L,  FALSE,  FALSE , NULL);
		if (g_ClientData.hEvent == NULL){
	   		g_pKato->Log(LOG_DETAIL, TEXT("CreateEvent() FAILed:\r\n"));
			return SPR_NOT_HANDLED;
		}

		crParam.fAttributes = CLIENT_ATTR_IO_DRIVER; //mem cards are rare now
		crParam.fEventMask   = 0xFFFF;
		crParam.uClientData = (UINT32) &g_ClientData;

		g_hClient = CardRegisterClient(CallBackFn_Client, &crParam);
	   	dwRet = WaitForSingleObject( g_ClientData.hEvent, TEST_WAIT_TIME);
		if (g_hClient == NULL || dwRet == WAIT_TIMEOUT){
	   		g_pKato->Log(LOG_DETAIL,TEXT("CardRegisterClient() FAILed\r\n"));
			return SPR_NOT_HANDLED;
		}
		g_ClientData.hClient = g_hClient;
		ResetEvent(g_ClientData.hEvent);

            return SPR_HANDLED;        

        // --------------------------------------------------------------------
        // Message: SPM_UNLOAD_DLL
        //
        case SPM_UNLOAD_DLL: 
            DEBUGMSG(ZONE_FUNCTION, (_T("ShellProc(SPM_UNLOAD_DLL, ...) called")));

		//restore registry
		RestoreLegacytRegistry();
		
		//deregister client	
		if(CardDeregisterClient(g_hClient) != CERR_SUCCESS){
			g_pKato->Log(LOG_FAIL, TEXT("Can not deregister client!\n"));
			return SPR_FAIL;
		}

            // This is a good place to delete our global critical section.
            DeleteCriticalSection(&g_csProcess);

            //Free PCMCIA.dll
		if(hInst)
			FreeLibrary(hInst);

		if(g_hPCCardDll)
			FreeLibrary(g_hPCCardDll);

	      PCMCIAWR_DeInit();
		  
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


            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_BEGIN_GROUP
        //
        case SPM_BEGIN_GROUP:
            DEBUGMSG(ZONE_FUNCTION, (_T("ShellProc(SPM_BEGIN_GROUP, ...) called")));
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: PCLegacy.DLL"));
            
            return SPR_HANDLED;

        // --------------------------------------------------------------------
        // Message: SPM_END_GROUP
        //
        case SPM_END_GROUP:
            DEBUGMSG(ZONE_FUNCTION, (_T("ShellProc(SPM_END_GROUP, ...) called")));
            g_pKato->EndLevel(_T("END GROUP: PCLegacy.DLL"));
            
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


//Command line function is not used here
VOID
ProcessCmdLine(LPCTSTR	szCmdLine){

	LPCTSTR	pCmdLine = NULL;
      return; 
}

#define PCCLEGACY_SOURCE_PATH	_T("\\Drivers\\PCCARD")
#define PCCLEGACY_BACKUP_PATH	_T("\\Drivers\\PCCARDBACKUP")
#define PCCOLD_SOURCE_PATH	_T("\\Drivers\\PCMCIA")
#define PCCOLD_BACKUP_PATH	_T("\\Drivers\\PCMCIABACKUP")
#define PCCLEGACY_PCM_SOURCE_PATH	_T("\\Drivers\\PCCARD\\PCMCIA\\TEMPLATE\\PCMCIA")
#define PCCLEGACY_PCM_BACKUP_PATH	_T("\\Drivers\\PCMCIATEMP")
#define PCCLEGACY_NOCONFIG_VALUE	1
#define PCCLEGACY_DUMMY_DETECTENTRY   _T("\\Drivers\\PCMCIA\\Detect\\01")
#define PCCLEGACY_DUMMY_DRVENTRY   _T("\\Drivers\\PCMCIA\\PCMDUMMYDRV")


//stole from reg_update.exe, this function will add an entry for the dummy driver,
//so whne any card plug in, the dummy driver will be loaded. 
BOOL 
AddDummyEntry(VOID){

	TCHAR    strDll[20], strEntry[20];
	HKEY hNewKey=NULL;
	HKEY hNewKey2=NULL;
	DWORD dwDisposition=0;

	memset(strDll,0x0,20*sizeof(TCHAR));
	memset(strEntry,0x0,20*sizeof(TCHAR));

	wcscpy(strDll,TEXT("pcc_dummydr.dll"));
	wcscpy(strEntry,TEXT("DetectIntr"));

	SetLastError(0);

	NKMSG(TEXT("Updating Registery... "));
	
	// Create registry entry at top of Detect List
 	if(ERROR_SUCCESS != RegCreateKeyEx( HKEY_LOCAL_MACHINE, PCCLEGACY_DUMMY_DETECTENTRY, 
							0, TEXT(""), 0, 
							0, NULL, 
							&hNewKey, &dwDisposition ))
		return FALSE;

  	if(ERROR_SUCCESS != RegSetValueEx(hNewKey, TEXT("Dll"), 
 						0, REG_SZ, (const BYTE *)strDll, 20*sizeof(TCHAR) )){
 		RegCloseKey(hNewKey);
 		return FALSE;
  	}

	if(ERROR_SUCCESS != RegSetValueEx(hNewKey, TEXT("Entry"), 
 						0, REG_SZ, (const BYTE *)strEntry, 20*sizeof(TCHAR) )){
		RegCloseKey(  hNewKey ); 
 		return FALSE;
	}
	
	RegCloseKey(  hNewKey ); 

 	//Create equivalent driver registry entry
	memset(strDll,0x0,20*sizeof(TCHAR));
	memset(strEntry,0x0,20*sizeof(TCHAR));

	wcscpy(strDll,TEXT("pcc_dummydr.dll"));
	wcscpy(strEntry,TEXT("tst"));

	if(ERROR_SUCCESS != RegCreateKeyEx( HKEY_LOCAL_MACHINE, PCCLEGACY_DUMMY_DRVENTRY, 
							0, TEXT(""), 0, 
							0, NULL, 
							&hNewKey2, &dwDisposition )){
		return FALSE;
	}

  	if(ERROR_SUCCESS != RegSetValueEx(hNewKey2, DEVLOAD_DLLNAME_VALNAME, 
 						0, DEVLOAD_DLLNAME_VALTYPE, (const BYTE *)strDll, (wcslen(strDll)+1)*sizeof(TCHAR)) ){
 		RegCloseKey(hNewKey2);
 		return FALSE;
  	}
 		

 	if(ERROR_SUCCESS != RegSetValueEx(hNewKey2, DEVLOAD_PREFIX_VALNAME, 
 						0,DEVLOAD_PREFIX_VALTYPE, (const BYTE *)strEntry, (wcslen(strEntry)+1)*sizeof(TCHAR) )){
 		RegCloseKey(hNewKey2);
 		return FALSE;
  	}
 
	RegCloseKey(  hNewKey2 ); 

	return TRUE;

}

BOOL 
UpdateLegacyRegistry(){
	
	CRegManipulate RegMani(HKEY_LOCAL_MACHINE);
	
	//make a backup for PCCARD16bit registry template
	if(RegMani.CopyAKey(PCCLEGACY_BACKUP_PATH, PCCLEGACY_SOURCE_PATH) == FALSE){
       	g_pKato->Log(LOG_DETAIL, _T("Can not make a backup of PCCard 16bit driver related registry!"));
		return FALSE;
	}

	//make a backup for old pcmcia registry 
	if(RegMani.CopyAKey(PCCOLD_BACKUP_PATH, PCCOLD_SOURCE_PATH) == FALSE){
       	g_pKato->Log(LOG_DETAIL, _T("Can not make a backup of PCMCIA old driver related registry!"));
		return FALSE;
	}

	//make a temp backup for PCCARD legacy registry 
	if(RegMani.CopyAKey(PCCLEGACY_PCM_BACKUP_PATH, PCCLEGACY_PCM_SOURCE_PATH) == FALSE){
       	g_pKato->Log(LOG_DETAIL, _T("Can not make a backup of PCCard legacy driver related registry!"));
		return FALSE;
	}

	//delete original template
	if(RegMani.DeleteAKey(PCCLEGACY_SOURCE_PATH) == FALSE){
       	g_pKato->Log(LOG_DETAIL, _T("Can not delete template!"));
		return FALSE;
	}

	//delete old PCMCIA registry
	if(RegMani.DeleteAKey(PCCOLD_SOURCE_PATH) == FALSE){
       	g_pKato->Log(LOG_DETAIL, _T("Can not old PCMCIA registry!"));
		return FALSE;
	}

	//restore the PCMCIA legacy registry
	if(RegMani.CopyAKey( PCCLEGACY_PCM_SOURCE_PATH, PCCLEGACY_PCM_BACKUP_PATH) == FALSE){
       	g_pKato->Log(LOG_DETAIL, _T("Can not make a backup of PCCard legacy driver related registry!"));
		return FALSE;
	}

	//delete the temp backup for PCCARD legacy registry 
	if(RegMani.DeleteAKey(PCCLEGACY_PCM_BACKUP_PATH) == FALSE){
       	g_pKato->Log(LOG_DETAIL, _T("Can not delete template!"));
		return FALSE;
	}

	if(AddDummyEntry() == FALSE){
       	g_pKato->Log(LOG_DETAIL, _T("Can not add dummy driver entry!"));
		return FALSE;
	}
	return TRUE;
}

BOOL 
RestoreLegacytRegistry(){

	CRegManipulate RegMani(HKEY_LOCAL_MACHINE);
	PREG_KEY_INFO	pKeyInfo = NULL;
	U_ITEM	u = {0};

	//delete dummy driver entry, it might not exist so ignore false return
	RegMani.DeleteAKey(PCCLEGACY_DUMMY_DRVENTRY);

	//only do this if backup key exists
	if(RegMani.IsAKeyValidate(PCCLEGACY_BACKUP_PATH) == TRUE){
		//delete original template
		if(RegMani.DeleteAKey(PCCLEGACY_SOURCE_PATH) == FALSE){
	       	g_pKato->Log(LOG_DETAIL, _T("Can not delete template!"));
			return FALSE;
		}
		//restore template key
		if(RegMani.CopyAKey(PCCLEGACY_SOURCE_PATH, PCCLEGACY_BACKUP_PATH) == FALSE){
	       	g_pKato->Log(LOG_DETAIL, _T("Can not restore PCCard 16bit driver related registry!"));
			return FALSE;
		}

		//delete backup template
		if(RegMani.DeleteAKey(PCCLEGACY_BACKUP_PATH) == FALSE){
	       	g_pKato->Log(LOG_DETAIL, _T("Can not delete backup template!"));
		}
	}
	if(RegMani.IsAKeyValidate(PCCOLD_BACKUP_PATH) == TRUE){
		//delete original old pcmcia driver
		if(RegMani.DeleteAKey(PCCOLD_SOURCE_PATH) == FALSE){//ignore this, it might not exist
	       	g_pKato->Log(LOG_DETAIL, _T("Can not delete old pcmcia registry!"));
		}
		//restore pcmcia key
		if(RegMani.CopyAKey(PCCOLD_SOURCE_PATH, PCCOLD_BACKUP_PATH) == FALSE){
	       	g_pKato->Log(LOG_DETAIL, _T("Can not restore old pcmcia driver related registry!"));
			return FALSE;
		}

		//delete backup pcmcia key
		if(RegMani.DeleteAKey(PCCOLD_BACKUP_PATH) == FALSE){
	       	g_pKato->Log(LOG_DETAIL, _T("Can not delete backup pcmcia key!"));
		}
	}
	
	return TRUE;
}

