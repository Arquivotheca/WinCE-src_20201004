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
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
// Copyright (c) 2004 Microsoft Corporation.  All rights reserved.
//
// --------------------------------------------------------------------

#include "StorageDetect.h"
/*
    this file contains the individual tests.  For each test you add to the FTE
    in StorageDetect.h, add the appropriate function to this file.

    Full documentation on the input parameters can be found in 
    tux.h, or on http://badmojo/tux.htm

    If you have additional questions, please email mojotool
*/


void printStoreInfo(){
    //TODO: output DSK, path, etc to allow command line generation
}


/*
MatchByVolume
    Searches for a storage device on a device using partition profile matching.

        Parameters:
        dwProfile       The index of the profile being searching for, 
                        as in gc_sProfileNames and gc_dwProfileMasks defined in StorageDetect.h
        
    Return value:
        1 if a match is found, 0 if not.  May be extended to return count of matches found.

    
*/
DWORD MatchByPart(DWORD dwProfile){
    DWORD dwNumberOfMatches = 0;
    TCHAR szPath[MAX_PATH * 2];

        
    if((NULL == CFSInfo::FindStorageWithProfile(gc_sProfileNames[dwProfile], szPath, MAX_PATH)) ||
        (szPath[0] == '\0')){
        DETAIL("Couldn't find matching partition");
        return 0;
    }
    else{
        g_pKato->Log(LOG_PASS, L"Found matching storage on path: %s", szPath);
        dwNumberOfMatches++;
    }
    g_pKato->Log(LOG_DETAIL, L"Completed search of stores for profile %s", gc_sProfileNames[dwProfile]);

    return dwNumberOfMatches;
}

/*
MatchByVolume
    Searches for a storage device on a device using lower level tactics then partition profile matching.

        Parameters:
        dwProfile       The index of the profile being searching for, 
                        as in gc_sProfileNames and gc_dwProfileMasks defined in StorageDetect.h
        
    Return value:
        TRUE if a match is found, FALSE if not

    
*/
BOOL MatchByVolume(DWORD dwProfile){
    g_pKato->Log(LOG_DETAIL, L"Looking for matches for device %s", gc_sProfileNames[dwProfile]);
    
    //SAKI: 
    //NOTE: This function could return a value indicating the number of matches found
    

    switch(gc_dwProfileMasks[dwProfile])
    {
        case SDMEMORY:
        break;
        case SDHCMEMORY:
        break;
        case MMC:
        break;
        //SAKI: if these are supported, AND them with gc_dwSupportedMatchByVolume in StorageDetect.h

        /*case RAMDISK:
        break;
        case MSFLASH:
        break;
        case HDPROFILE:
        break;
        case CDPROFILE:
        break;*/
        default:
        return FALSE;
    }
    

    return FALSE;
}

/*
StorageExists
    Searches for a storage device on a device using lower level tactics then partition profile matching.

        Parameters:
        lpFTE->dwUserData       Bitmask of all the profiles that, if found, should cause this test to pass
                                Bitmask values defined in StorageDetect.h, OR'd and passed in from function table
        
    Return value:
        TPR_PASS if a match is found, TPR_FAIL if not.

    
*/
TESTPROCAPI StorageExists(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure first
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    UNREFERENCED_PARAMETER(tpParam);

    BOOL bRetVal = TRUE;
    DWORD dwNumberOfMatches = 0;
    
    
    
    //iterate over each profile that a test case could be searching for.  The input parameter
    //is a binary mask.  If a bit is set, the test should pass if a corresponding 
    //profile on the device.
    for(DWORD currProfile = 0; currProfile < gc_dwProfileCount; currProfile++){
        if(lpFTE->dwUserData & gc_dwProfileMasks[currProfile]){
            //if user passed "volume" at command line and the profile has a special case volume match function
            if(!g_bSearchPartitions && (gc_dwProfileMasks[currProfile] & gc_dwSupportedMatchByVolume)){
                //g_pKato->Log(LOG_DETAIL, L"Matching by volume");
                bRetVal = MatchByVolume(currProfile);
                if(bRetVal){
                    dwNumberOfMatches++;
                }
            }
            else if(!g_bSearchPartitions){
                g_pKato->Log(LOG_DETAIL, L"Match by volume was specified, but is not supported for the desired profile.  Matching by partition");
                dwNumberOfMatches += MatchByPart(currProfile);
            }
            else{
                //g_pKato->Log(LOG_DETAIL, L"Matching by partition");
                dwNumberOfMatches += MatchByPart(currProfile);
            }
        }
    
    }
    
    if(dwNumberOfMatches > 0){
        if(dwNumberOfMatches > 1){
            WARN("More then one store with a matching profile was found. \nIf a profile is used to run tests they will only be run against 1 of these devices.\n  Consider specifying pathes.");
        }
        g_pKato->Log(LOG_DETAIL, L"%d stores were found with a matching profile, passing test", dwNumberOfMatches);
        return TPR_PASS;
    }
    else{
        FAIL("No stores were found with a matching profile, failing test");
        return TPR_FAIL;
    }

}

/*
    There's rarely much need to modify the remaining two functions
    (DllMain and ShellProc) unless you need to debug some very
    strange behavior, or if you are doing other customizations.
*/

BOOL WINAPI 
DllMain(
    HANDLE hInstance, 
    ULONG dwReason, 
    LPVOID lpReserved ) 
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);
    
    switch( dwReason )
    {
        case DLL_PROCESS_ATTACH:
            Debug(TEXT("%s: DLL_PROCESS_ATTACH\r\n"), MODULE_NAME);    
            return TRUE;

        case DLL_PROCESS_DETACH:
            Debug(TEXT("%s: DLL_PROCESS_DETACH\r\n"), MODULE_NAME);
            return TRUE;
    }
    return FALSE;
}


SHELLPROCAPI 
ShellProc(
    UINT uMsg, 
    SPPARAM spParam ) 
{
    LPSPS_BEGIN_TEST pBT = {0};
    LPSPS_END_TEST pET = {0};

    switch (uMsg) {
    
        // Message: SPM_LOAD_DLL
        case SPM_LOAD_DLL: 
            Debug(_T("ShellProc(SPM_LOAD_DLL, ...) called\r\n"));

            // If we are UNICODE, then tell Tux this by setting the following flag.
    #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
    #endif

            // turn on kato debugging
            KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);

            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();

            return SPR_HANDLED;        

        // Message: SPM_UNLOAD_DLL
        case SPM_UNLOAD_DLL: 
            Debug(_T("ShellProc(SPM_UNLOAD_DLL, ...) called"));

            return SPR_HANDLED;      

        // Message: SPM_SHELL_INFO
        case SPM_SHELL_INFO:
            Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));

            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;
        
            // handle command line parsing
            ParseCmdLine(g_pShellInfo->szDllCmdLine);

        return SPR_HANDLED;

        // Message: SPM_REGISTER
        case SPM_REGISTER:
            Debug(_T("ShellProc(SPM_REGISTER, ...) called\r\n"));
            
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
    #ifdef UNICODE
                return SPR_HANDLED | SPF_UNICODE;
    #else
                return SPR_HANDLED;
    #endif

        // Message: SPM_START_SCRIPT
        case SPM_START_SCRIPT:
            Debug(_T("ShellProc(SPM_START_SCRIPT, ...) called\r\n"));           
    
            return SPR_HANDLED;

        // Message: SPM_STOP_SCRIPT
        case SPM_STOP_SCRIPT:

            return SPR_HANDLED;

        // Message: SPM_BEGIN_GROUP
        case SPM_BEGIN_GROUP:
            Debug(_T("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n"));
            g_pKato->BeginLevel(0, _T("BEGIN GROUP:StorageDetect"));
            
            return SPR_HANDLED;

        // Message: SPM_END_GROUP
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called\r\n"));
            g_pKato->EndLevel(_T("END GROUP:StorageDetect"));
            
            return SPR_HANDLED;

        // Message: SPM_BEGIN_TEST
        case SPM_BEGIN_TEST:
            Debug(_T("ShellProc(SPM_BEGIN_TEST, ...) called\r\n"));

            // Start our logging level.
            pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                                _T("BEGIN TEST: %s, Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                pBT->dwRandomSeed);

            return SPR_HANDLED;

        // Message: SPM_END_TEST
        case SPM_END_TEST:
            Debug(_T("ShellProc(SPM_END_TEST, ...) called\r\n"));

            // End our logging level.
            pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(_T("END TEST: %s, %s, Time=%u.%03u"),
                            pET->lpFTE->lpDescription,
                            pET->dwResult == TPR_SKIP ? _T("SKIPPED") :
                            pET->dwResult == TPR_PASS ? _T("PASSED") :
                            pET->dwResult == TPR_FAIL ? _T("FAILED") : _T("ABORTED"),
                            pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

            return SPR_HANDLED;

        // Message: SPM_EXCEPTION
        case SPM_EXCEPTION:
            Debug(_T("ShellProc(SPM_EXCEPTION, ...) called\r\n"));
            g_pKato->Log(LOG_EXCEPTION, _T("Exception occurred!"));
            return SPR_HANDLED;

        default:
            //Debug(_T("ShellProc received bad message: 0x%X\r\n"), uMsg);
            //ASSERT(!"Default case reached in ShellProc!");
            return SPR_NOT_HANDLED;
    }
}

void Usage()
{
    Debug(_T("Usage:"));
    Debug(_T("======================="));
    Debug(_T("/volume"));
    Debug(_T("\tUse this option to match volumes rather then partitions"));
    Debug(_T("========================"));
}

void ParseCmdLine(LPCTSTR szCmdLine) 
{
    Usage();
    CClParse cmdLine(szCmdLine);

    if(cmdLine.GetOpt(L"volume")){
        g_bSearchPartitions = FALSE;
        Debug(_T("Volume specified, will attempt to find a matching volume rather then partition"));
    }
    else{
        g_bSearchPartitions = TRUE;
        Debug(_T("Volume not specified, will attempt to find a matching partition rather then volume"));
    }
}

