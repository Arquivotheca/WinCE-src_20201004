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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#include "TUXMAIN.H"
#include "TEST_WAVETEST.H"         //WAVETEST
#include "TEST_WAVEIN.H"           //WAVETEST
#include "TUXFUNCTIONTABLE.H"      //WAVETEST Tux Function Table moved to separate file
#include <strsafe.h>               //Safe String Functions, i.e. StringCchCopy()

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

BOOL      g_bSave_WAV_file         = FALSE;
HINSTANCE g_hInst                  = NULL;
HINSTANCE g_hExeInst               = NULL;
HINSTANCE g_hDllInst               = NULL;
TCHAR*    g_pszString              = NULL;
TCHAR*    g_pszCSVFileName         = NULL;
TCHAR*    g_pszWAVFileName         = NULL;
TCHAR*    g_pszWaveCharacteristics = NULL;
TCHAR*    g_pszNext_token          = NULL;

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

void LOG(LPCWSTR szFmt, ...) {
    va_list va;

    if(g_pKato) {
        va_start(va, szFmt);
        g_pKato->LogV( LOG_DETAIL, szFmt, va);
        va_end(va);
    }
}

BOOL ProcessCommandLine(LPCTSTR);   //WAVETEST

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
            if(!ProcessCommandLine((TCHAR*)g_pShellInfo->szDllCmdLine)) //WAVETEST
                return SPR_FAIL;    //WAVETEST
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
            g_pKato->BeginLevel(0, _T("BEGIN GROUP: WAVETEST.DLL"));
            return SPR_HANDLED;
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called\r\n"));
            g_pKato->EndLevel(_T("END GROUP: WAVETEST.DLL"));
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
            return SPR_NOT_HANDLED;
    }
}

/*
 * WAVETEST 
 */

BOOL ChangeGlobalDWORD( TCHAR* szSeps, DWORD *dwGlobal ) {
    TCHAR *szToken;
    int iRet;
    szToken=_tcstok_s ( NULL, szSeps,&g_pszNext_token );
    iRet = _stscanf_s ( szToken, TEXT( "%u" ), dwGlobal );
    return ( iRet==1 );
}

BOOL ChangeGlobalTCHAR( TCHAR* szSeps, TCHAR* szToken ) {
    
    szToken = _tcstok_s( NULL, szSeps,&g_pszNext_token);  // advance to next token
    if( szToken && szToken[ 0 ] != TEXT( '-' ) ) g_pszString = szToken;
    else                                         g_pszString = NULL;
    return TRUE;
}

BOOL MakeGlobalCSVFileName( TCHAR* pszString  )
{
    HRESULT hr = S_OK;
    size_t size;
    TCHAR pszDefaultFileName[] = TEXT( "\\release\\audio_latency_test_results.csv" );
    if( g_pszString )
    {
        if( _tcscspn( g_pszString, TEXT( "." ) ) < _tcslen( g_pszString ) )
        {
            size = _tcslen( g_pszString ) + 1;
            g_pszCSVFileName = new TCHAR[size];
            hr = StringCchCopy(g_pszCSVFileName, size, g_pszString);
            if(FAILED(hr))
            {
                LOG( TEXT("ERROR: Copying CSV file path from command prompt failed, Error code=%x\n" ), hr );
                return FALSE;
            }
        } // if wcscspn
        else // !wcscspn
        {
            size = _tcslen( g_pszString ) + 5;
            g_pszCSVFileName = new TCHAR[size];
            hr = StringCchCopy(g_pszCSVFileName, size, g_pszString);
            if(FAILED(hr))
            {
                LOG( TEXT("ERROR: Copying CSV file path from command prompt failed, Error code=%x\n" ), hr );
                return FALSE;
            }
            hr = StringCchCat(g_pszCSVFileName, size, TEXT( ".csv" ));
            if(FAILED(hr))
            {
                LOG( TEXT("ERROR: Adding .csv extention to CSV file failed, Error code=%x\n" ), hr );
                return FALSE;
            }
        } // else !wcscspn
    } // if g_pszString
    else // !g_pszString
    {
        size = _tcslen( pszDefaultFileName ) + 1;
        g_pszCSVFileName = new TCHAR[size];
        hr = StringCchCopy(g_pszCSVFileName, size, pszDefaultFileName);
        if(FAILED(hr))
        {
            LOG( TEXT("ERROR: Copying CSV file path failed, Error code=%x\n" ), hr );
            return FALSE;
        }
    } // !g_pszString
    return TRUE;
} // MakeGlobalCSVFileName

BOOL MakeGlobalWAVFileName( TCHAR* pszString  )
{
    size_t size;
    HRESULT hr = S_OK;
    TCHAR pszDefaultFileName[] = TEXT( "\\release\\WaveInterOp.wav" );
    if( g_pszString )
    {
        if( _tcscspn( g_pszString, TEXT( "." ) ) < _tcslen( g_pszString ) )
        {
            size = _tcslen( g_pszString ) + 1;
            g_pszWAVFileName = new TCHAR[size];
            hr = StringCchCopy(g_pszWAVFileName, size, g_pszString);
            if(FAILED(hr))
            {
                LOG( TEXT("ERROR: Copying WAV file path from command prompt failed, Error code=%x\n" ), hr );
                return FALSE;
            }
        }
        else
        {
            size = _tcslen( g_pszString ) + 5;
            g_pszWAVFileName = new TCHAR[size];
            hr = StringCchCopy(g_pszWAVFileName, size, g_pszString);
            if(FAILED(hr))
            {
                LOG( TEXT("ERROR: Copying WAV file path from command prompt failed, Error code=%x\n" ), hr );
                return FALSE;
            }
            hr = StringCchCat(g_pszWAVFileName, size, TEXT( ".wav" ));
            if(FAILED(hr))
            {
                LOG( TEXT("ERROR: Adding .wav extention to WAV file failed, Error code=%x\n" ), hr );
                return FALSE;
            }
        }
    }
    else
    {
        size = _tcslen( pszDefaultFileName ) + 1; 
        g_pszWAVFileName = new TCHAR[size];
        hr = StringCchCopy(g_pszWAVFileName, size, pszDefaultFileName);
        if(FAILED(hr))
        {
            LOG( TEXT("ERROR: Copying WAV file path failed, Error code=%x\n" ), hr );
            return FALSE;
        }
    }
    
    return TRUE;
} // MakeGlobalWAVFileName

BOOL ProcessCommandLine(LPCTSTR szDLLCmdLine)
{
     TCHAR  pszDefaultWaveCharacteristics[] = TEXT( "1000_11025_1_8" );
     TCHAR* szToken;
     TCHAR* szSwitch;
     TCHAR* szCmdLine=(TCHAR*)szDLLCmdLine;
     TCHAR  szSeps[]=TEXT(" ,\t\n");
     TCHAR  szSwitches[]=TEXT("icdeftvwsa?hp");

     HRESULT hr = S_OK;
     size_t size;
     
     szToken=_tcstok_s (szCmdLine,szSeps,&g_pszNext_token);
     while(szToken!=NULL)
     {
          if(szToken[0]==TEXT('-'))
          {
               szSwitch=_tcschr(szSwitches,szToken[1]);
               if(szSwitch)
               {
                    switch(*szSwitch)
                    {
                    case 'a':
                         if(szToken[2]=='o')
                         {
                              ChangeGlobalDWORD(szSeps,&g_dwAllowance);
                         }
                         else if(szToken[2]=='i')
                         {
                              ChangeGlobalDWORD(szSeps,&g_dwInAllowance);
                         }
                         else goto default2;
                         break;
                    case 'c':
                         ChangeGlobalTCHAR( szSeps, szToken );
                         if( g_pszString  )
                              szToken = _tcstok_s ( NULL, szSeps,&g_pszNext_token);
                         MakeGlobalCSVFileName( szToken  );
                         break;
                    case 'f': 
                         ChangeGlobalTCHAR( szSeps, szToken );
                         if( g_pszString )
                         {
                              size = _tcslen( g_pszString ) + 1;
                              szToken = _tcstok_s ( NULL, szSeps ,&g_pszNext_token);
                              g_pszWaveCharacteristics = new TCHAR[size];
                              hr = StringCchCopy(g_pszWaveCharacteristics, size, g_pszString);
                              if(FAILED(hr))
                              {
                                   LOG( TEXT("ERROR: Copying WAV Characteristics failed, Error code=%x\n" ), hr );
                                   break;
                              }
                         } // if if( g_pszString
                         else
                         {  // !if( g_pszString
                              size = _tcslen( pszDefaultWaveCharacteristics ) + 1; 
                              g_pszWaveCharacteristics = new TCHAR[size];
                              hr = StringCchCopy(g_pszWaveCharacteristics, size, pszDefaultWaveCharacteristics);
                              if(FAILED(hr))
                              {
                                   LOG( TEXT("ERROR: Copying default WAV Characteristics failed, Error code=%x\n" ), hr );
                                   break;
                              }
                         }  // else !g_pszString
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
                    case 't':
                         if( !ChangeGlobalDWORD( szSeps, &g_dwNoOfThreads ) || ( 0 == g_dwNoOfThreads ) ||( MAXNOTHREADS < g_dwNoOfThreads ) )
                         {
                              g_dwNoOfThreads = 13;
                              LOG( TEXT( "WARNING:  The number of threads specified for audio stability tests is invalid or out of range. The default value of 13 will be used.\n" ) );
                         } // if !ChangeGlobalDWORD
                         break;
                    case 'v':
                         g_bSave_WAV_file = TRUE;
                         ChangeGlobalTCHAR( szSeps, szToken );
                         if( g_pszString  )
                              szToken = _tcstok_s ( NULL, szSeps,&g_pszNext_token );
                         MakeGlobalWAVFileName( szToken  );
                         break;
                    case 'w':
                         if(szToken[2]=='o')
                         {
                              ChangeGlobalDWORD(szSeps,&g_dwOutDeviceID);
                         }
                         else if(szToken[2]=='i')
                         {
                              ChangeGlobalDWORD(szSeps,&g_dwInDeviceID);
                         }
                         else goto default2;
                         break;
                    case 'h':
                         g_headless=TRUE;
                         break;
                    case 'p':
                         g_powerTests=TRUE;
                         break;
                    
                    case '?':
                         LOG(TEXT(" "));
                         LOG(TEXT("usage:  s tux -o -d wavetest tux_parameters -c \"dll_parameters\""));
                         LOG(TEXT(" "));
                         LOG(TEXT("Tux parameters:  please refer to s tux -?"));
                         LOG(TEXT(" "));
                         LOG(TEXT("DLL parameters: [-c path\\filename] [-f duration_frequency_channels_bits] [-i] [-h] [-d duration] [-e]"));
                         LOG(TEXT("\t[-t number of threads] [-v path\\filename] [-wo outputDeviceID] [-wi inputDeviceID]"));
                         LOG(TEXT("\t[-ao allowance] [-ai allowance] [-s interval]"));
                         LOG(TEXT(" "));
                         LOG(TEXT("\t-ao\texpected playtime allowance (in ms,default=200)"));
                         LOG(TEXT("\t-ai\texpected capturetime allowance (in ms,default=200)"));
                         LOG(TEXT("\t-c path\\filename\tname of CSV file in which to output Latency test results"));
                         LOG(TEXT("\t\t\t(default file name = audio_latency_test_results.csv, default file extension = .csv)"));
                         LOG(TEXT("\t-f d_f_c_b\tlatency test duration and wave characteristics"));
                         LOG(TEXT("\t\t\td = duration in milliseconds (default = 1000)"));
                         LOG(TEXT("\t\t\tf = the sample frequency (default =11025)"));
                         LOG(TEXT("\t\t\tc = the number of channels (default=1)"));
                         LOG(TEXT("\t\t\tb = either 8 or 16, specifying the number of bits per sample (default=8)"));
                         LOG(TEXT("\t-i\tturn on interaction"));
                         LOG(TEXT("\t-h\theadless mode"));
                         LOG(TEXT("\t-p\tturn on power management tests"));
                         LOG(TEXT("\t-d\tduration (in seconds,default=1)"));
                         LOG(TEXT("\t-e\tturn off sound when capturing"));
                         LOG(TEXT("\t-s\tsleep interval for CALLBACK_NULL (in ms,default=50)"));
                         LOG(TEXT("\t-t\tset number of threads for audio stability tests. (default=13, maximum=40)"));
                         LOG(TEXT("\t-v path\\filename\tSave the audio file created by the Playback Interoperability test. path\\filename is optional."));
                         LOG(TEXT("\t\t\t(The defaults are: don't save, file name = \\release\\WaveInterOp.wav, and file extension = .wav)"));
                         LOG(TEXT("\t-wo\ttest specific WaveOut DeviceID (default=0, first output device)"));
                         LOG(TEXT("\t-wi\ttest specific WaveIn DeviceID (default=0, first input device)"));
                         LOG(TEXT("\t-?\tthis help message"));
                         g_bSkipIn=g_bSkipOut=TRUE;
                         return FALSE;
                         break;
                    default:
                    default2:
                         LOG(TEXT("ParseCommandLine:  Unknown Switch \"%s\"\n"),szToken);
                    }
               }
               else 
                    LOG(TEXT("ParseCommandLine:  Unknown Switch \"%s\"\n"),szToken);
          }
          else 
               LOG(TEXT("ParseCommandLine:  Unknown Parameter \"%s\"\n"),szToken);
          szToken=_tcstok_s (NULL,szSeps,&g_pszNext_token); 
     }
     return TRUE;
}
