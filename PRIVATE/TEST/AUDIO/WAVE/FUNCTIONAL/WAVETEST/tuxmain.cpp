//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#include "TUXMAIN.H"
#include "TEST_WAVETEST.H"	//WAVETEST
#include "TEST_WAVEIN.H"	//WAVETEST
#include "TUXFUNCTIONTABLE.H"	//WAVETEST Tux Function Table moved to separate file

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

HINSTANCE g_hInst = NULL;
HINSTANCE g_hExeInst = NULL;
HINSTANCE g_hDllInst = NULL;

// extern "C"
#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI DllMain(HANDLE hInstance,ULONG dwReason,LPVOID lpReserved) {
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(lpReserved);

	g_hDllInst = (HINSTANCE)hInstance;
    
	switch( dwReason ) {
	        case DLL_PROCESS_ATTACH:
			Debug(TEXT("%s: DLL_PROCESS_ATTACH\r\n"), MODULE_NAME);    
			return TRUE;
        	case DLL_PROCESS_DETACH:
			Debug(TEXT("%s: DLL_PROCESS_DETACH\r\n"), MODULE_NAME);
			return TRUE;
		case DLL_THREAD_ATTACH:
        	case DLL_THREAD_DETACH:  
	    	    	return TRUE;
    	}
    	return FALSE;
}

#ifdef __cplusplus
} // end extern "C"
#endif

void LOG(LPTSTR szFmt, ...) {
	va_list va;

	if(g_pKato) {
		va_start(va, szFmt);
		g_pKato->LogV( LOG_DETAIL, szFmt, va);
		va_end(va);
	}
}

BOOL ProcessCommandLine(LPCTSTR);	//WAVETEST

SHELLPROCAPI ShellProc(UINT uMsg,SPPARAM spParam) {
	LPSPS_BEGIN_TEST pBT = {0};
	LPSPS_END_TEST pET = {0};
	
	switch (uMsg) {
		case SPM_LOAD_DLL: 
			Debug(_T("ShellProc(SPM_LOAD_DLL, ...) called\r\n"));
#ifdef UNICODE
			((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
#endif
			g_pKato = (CKato*)KatoGetDefaultObject();
			return SPR_HANDLED;        
	        case SPM_UNLOAD_DLL: 
        		Debug(_T("ShellProc(SPM_UNLOAD_DLL, ...) called"));
			return SPR_HANDLED;      
		case SPM_SHELL_INFO:
			Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));
			g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
			g_hInst      = g_pShellInfo->hLib;
			g_hExeInst   = g_pShellInfo->hInstance;            
			if(!ProcessCommandLine((TCHAR*)g_pShellInfo->szDllCmdLine))	//WAVETEST
				return SPR_FAIL;					//WAVETEST
			return SPR_HANDLED;
		case SPM_REGISTER:
			Debug(_T("ShellProc(SPM_REGISTER, ...) called\r\n"));
			((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
#ifdef UNICODE
			return SPR_HANDLED | SPF_UNICODE;
#else
			return SPR_HANDLED;
#endif
		case SPM_START_SCRIPT:
			Debug(_T("ShellProc(SPM_START_SCRIPT, ...) called\r\n"));                       
			return SPR_HANDLED;
		case SPM_STOP_SCRIPT:
			return SPR_HANDLED;
		case SPM_BEGIN_GROUP:
			Debug(_T("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n"));
			g_pKato->BeginLevel(0, _T("BEGIN GROUP: FSDTEST.DLL"));
            		return SPR_HANDLED;
		case SPM_END_GROUP:
			Debug(_T("ShellProc(SPM_END_GROUP, ...) called\r\n"));
			g_pKato->EndLevel(_T("END GROUP: TUXDEMO.DLL"));
			return SPR_HANDLED;
		case SPM_BEGIN_TEST:
			Debug(_T("ShellProc(SPM_BEGIN_TEST, ...) called\r\n"));
			pBT = (LPSPS_BEGIN_TEST)spParam;
			g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
				_T("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
				pBT->lpFTE->lpDescription, pBT->dwThreadCount,
				pBT->dwRandomSeed);
			return SPR_HANDLED;
		case SPM_END_TEST:
			Debug(_T("ShellProc(SPM_END_TEST, ...) called\r\n"));
			pET = (LPSPS_END_TEST)spParam;
			g_pKato->EndLevel(_T("END TEST: \"%s\", %s, Time=%u.%03u"),
				pET->lpFTE->lpDescription,
				pET->dwResult == TPR_SKIP ? _T("SKIPPED") :
				pET->dwResult == TPR_PASS ? _T("PASSED") :
				pET->dwResult == TPR_FAIL ? _T("FAILED") : _T("ABORTED"),
				pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);
			return SPR_HANDLED;
		case SPM_EXCEPTION:
			Debug(_T("ShellProc(SPM_EXCEPTION, ...) called\r\n"));
			g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
			return SPR_HANDLED;
		default:
			Debug(_T("ShellProc received bad message: 0x%X\r\n"), uMsg);
			ASSERT(!"Default case reached in ShellProc!");
			return SPR_NOT_HANDLED;
	}
}

/*
 * WAVETEST 
 */


BOOL ChangeGlobalDWORD(TCHAR* szSeps,DWORD *dwGlobal) {
	TCHAR *szToken;
	int iRet;

	szToken=_tcstok(NULL,szSeps);
	iRet=_stscanf(szToken,TEXT("%u"),dwGlobal);
	return (iRet==1);
}

BOOL ProcessCommandLine(LPCTSTR szDLLCmdLine)
{
	TCHAR *szToken;
	TCHAR *szSwitch;
	TCHAR *szCmdLine=(TCHAR*)szDLLCmdLine;
	TCHAR szSeps[]=TEXT(" ,\t\n");
	TCHAR szSwitches[]=TEXT("idewsa?");

	szToken=_tcstok(szCmdLine,szSeps);
	while(szToken!=NULL) {
		if(szToken[0]==TEXT('-')) {
			szSwitch=_tcschr(szSwitches,szToken[1]);
			if(szSwitch) {
				switch(*szSwitch) {
					case 'a':
						if(szToken[2]=='o') {
							ChangeGlobalDWORD(szSeps,&g_dwAllowance);	
						}
						else if(szToken[2]=='i') {
							ChangeGlobalDWORD(szSeps,&g_dwInAllowance);
						}
						else goto default2;
					break;
					case 's': 
						ChangeGlobalDWORD(szSeps,&g_dwSleepInterval);
					break;
					case 'i':
						g_interactive=TRUE;
					break;
					case 'd':
						ChangeGlobalDWORD(szSeps,&g_duration);
					break;
					case 'e':
						g_useSound=FALSE;
					break;
					case 'w':
						ChangeGlobalDWORD(szCmdLine,&g_dwDeviceNum);
					break;

					case '?':
						LOG(TEXT("usage:  s tux -o -d wavetest tux_parameters -c \"dll_parameters\"\n"));
						LOG(TEXT("Tux parameters:  please refer to s tux -?\n"));
						LOG(TEXT("DLL parameters: [-i] [-d duration] [-e] [-w deviceID] [-ao allowance] [-ai allowance] [-s interval]\n"));
						LOG(TEXT("\t-ao\texpected playtime allowance (in ms,default=200)\n"));
						LOG(TEXT("\t-ai\texpected capturetime allowance (in ms,default=200)\n"));
						LOG(TEXT("\t-i\tturn on interaction\n"));
						LOG(TEXT("\t-d\tduration (in seconds,default=1)\n"));
						LOG(TEXT("\t-e\tturn off sound when capturing\n"));
						LOG(TEXT("\t-s\tsleep interval for CALLBACK_NULL (in ms,default=50)\n"));
						LOG(TEXT("\t-w\ttest specific deviceID (default=1, first device)\n"));
						LOG(TEXT("\t-?\tthis help message\n"));
						g_bSkipIn=g_bSkipOut=TRUE;
						return FALSE;
					break;
					default:
					default2:
						LOG(TEXT("ParseCommandLine:  Unknown Switch \"%s\"\n"),szToken);
				}
			}
			else LOG(TEXT("ParseCommandLine:  Unknown Switch \"%s\"\n"),szToken);

		}
		else LOG(TEXT("ParseCommandLine:  Unknown Parameter \"%s\"\n"),szToken);
		szToken=_tcstok(NULL,szSeps); 
	}
	return TRUE;
}
