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
/*
 * tuxStandard.cpp
 */

/* need the debug functions */
#include "commonUtils.h"

#include <katoex.h>
#include <tux.h>

/*
 * tux.h expects to find the global function table.  This is common
 * code that is linked by each separate test dll.  We define extern so
 * that tux.h finds the function table and we can still keep this code
 * common.
 */
extern FUNCTION_TABLE_ENTRY g_lpFTE[];

#include <stdio.h>
#include <stdlib.h>


// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION g_csProcess;

/******* for command line processing */
/* handle high level command line args.  These aren't the test specific args. */
BOOL handleCmdLineUnload();
BOOL handleCmdLine ();

#define CMD_FILE_NAME_PATH (_T("\\release\\"))
#define MAX_FILENAME (256)
#define ARG_STR_CMD_FILE_NAME (_T("-cmdf"))
#define MAX_COMMAND_LENGTH (1024)

// Max size for Platform Name
#define SIZE_PLAT_NAME 50

/* if we read from a file we need a global pointer to track the memory
 * allocated to the command line.  If the correct cmd line options are
 * provided this is initialized on dll load and freed on dll exit
 */
TCHAR * g_szFileCmdLine = NULL;

//******************************************************************************
//***** Windows CE specific code
//******************************************************************************
#ifdef UNDER_CE

#ifndef STARTF_USESIZE
#define STARTF_USESIZE     0x00000002
#endif

#ifndef STARTF_USEPOSITION
#define STARTF_USEPOSITION 0x00000004
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif

#ifndef _vsntprintf
#define _vsntprintf(d,c,f,a) wvsprintf(d,f,a)
#endif

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
   return TRUE;
}
#endif

//******************************************************************************
//***** ShellProc()
//******************************************************************************

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) {
//extern "C" __declspec(dllexport) INT ShellProc(UINT uMsg, SPPARAM spParam) {

    switch (uMsg) {

        //------------------------------------------------------------------------
        // Message: SPM_LOAD_DLL
        //
        // Sent once to the DLL immediately after it is loaded.  The spParam
        // parameter will contain a pointer to a SPS_LOAD_DLL structure.  The DLL
        // should set the fUnicode member of this structre to TRUE if the DLL is
        // built with the UNICODE flag set.  By setting this flag, Tux will ensure
        // that all strings passed to your DLL will be in UNICODE format, and all
        // strings within your function table will be processed by Tux as UNICODE.
        // The DLL may return SPR_FAIL to prevent the DLL from continuing to load.
        //------------------------------------------------------------------------

        case SPM_LOAD_DLL: {
            Debug(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));
            Debug(TEXT("If you aren't seeing the test debug output and you aren't"));
            Debug(TEXT("running through cetk then you probably aren't passing tux"));
            Debug(TEXT("the -o option on the command line."));

            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif

            // Get/Create our global logging object.
            g_pKato = (CKato*)KatoGetDefaultObject();

            // Initialize our global critical section.
            InitializeCriticalSection(&g_csProcess);

            return SPR_HANDLED;
        }

        //------------------------------------------------------------------------
        // Message: SPM_UNLOAD_DLL
        //
        // Sent once to the DLL immediately before it is unloaded.
        //------------------------------------------------------------------------

        case SPM_UNLOAD_DLL: {
            Debug(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));

            /* remove the mem that might have been allocated for the command line */
            if (!handleCmdLineUnload())
            {
                Debug (_T("Error on dll unload."));
            }

            // This is a good place to delete our global critical section.
            DeleteCriticalSection(&g_csProcess);

            return SPR_HANDLED;
        }

        //------------------------------------------------------------------------
        // Message: SPM_SHELL_INFO
        //
        // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
        // some useful information about its parent shell and environment.  The
        // spParam parameter will contain a pointer to a SPS_SHELL_INFO structure.
        // The pointer to the structure may be stored for later use as it will
        // remain valid for the life of this Tux Dll.  The DLL may return SPR_FAIL
        // to prevent the DLL from continuing to load.
        //------------------------------------------------------------------------

        case SPM_SHELL_INFO: {
            Debug(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));

            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

            /* look for any special options, etc. */
            if (!handleCmdLine())
            {
                /* set the global command line for all of the tests */
                g_szDllCmdLine = g_pShellInfo->szDllCmdLine;
            }

            Debug(TEXT("Command Line: %s"), g_szDllCmdLine);

#ifndef UNICODE
            Debug (_T("This test was given a command line and was not "));
            Debug (_T("compiled with UNICODE.  The command line parsing"));
            Debug (_T("routines assume UNICODE and therefore might not"));
            Debug (_T("work correctly.  This should never happen on CE."));
#endif /* UNICODE */

            return SPR_HANDLED;
        }

        //------------------------------------------------------------------------
        // Message: SPM_REGISTER
        //
        // This is the only ShellProc() message that a DLL is required to handle
        // (except for SPM_LOAD_DLL if you are UNICODE).  This message is sent
        // once to the DLL immediately after the SPM_SHELL_INFO message to query
        // the DLL for it's function table.  The spParam will contain a pointer to
        // a SPS_REGISTER structure.  The DLL should store its function table in
        // the lpFunctionTable member of the SPS_REGISTER structure.  The DLL may
        // return SPR_FAIL to prevent the DLL from continuing to load.
        //------------------------------------------------------------------------

        case SPM_REGISTER: {
            Debug(TEXT("ShellProc(SPM_REGISTER, ...) called"));
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
            #ifdef UNICODE
                return SPR_HANDLED | SPF_UNICODE;
            #else
                return SPR_HANDLED;
            #endif
        }

        //------------------------------------------------------------------------
        // Message: SPM_START_SCRIPT
        //
        // Sent to the DLL immediately before a script is started.  It is sent to
        // all Tux DLLs, including loaded Tux DLLs that are not in the script.
        // All DLLs will receive this message before the first TestProc() in the
        // script is called.
        //------------------------------------------------------------------------

        case SPM_START_SCRIPT: {
            Debug(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
            return SPR_HANDLED;
        }

        //------------------------------------------------------------------------
        // Message: SPM_STOP_SCRIPT
        //
        // Sent to the DLL when the script has stopped.  This message is sent when
        // the script reaches its end, or because the user pressed stopped prior
        // to the end of the script.  This message is sent to all Tux DLLs,
        // including loaded Tux DLLs that are not in the script.
        //------------------------------------------------------------------------

        case SPM_STOP_SCRIPT: {
            Debug(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
            return SPR_HANDLED;
        }

        //------------------------------------------------------------------------
        // Message: SPM_BEGIN_GROUP
        //
        // Sent to the DLL before a group of tests from that DLL is about to be
        // executed.  This gives the DLL a time to initialize or allocate data for
        // the tests to follow.  Only the DLL that is next to run receives this
        // message.  The prior DLL, if any, will first receive a SPM_END_GROUP
        // message.  For global initialization and de-initialization, the DLL
        // should probably use SPM_START_SCRIPT and SPM_STOP_SCRIPT, or even
        // SPM_LOAD_DLL and SPM_UNLOAD_DLL.
        //------------------------------------------------------------------------

        case SPM_BEGIN_GROUP: {
            Debug(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called"));
            g_pKato->BeginLevel(0, TEXT("BEGIN GROUP"));
            return SPR_HANDLED;
        }

        //------------------------------------------------------------------------
        // Message: SPM_END_GROUP
        //
        // Sent to the DLL after a group of tests from that DLL has completed
        // running.  This gives the DLL a time to cleanup after it has been run.
        // This message does not mean that the DLL will not be called again to run
        // tests; it just means that the next test to run belongs to a different
        // DLL.  SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL to track when it
        // is active and when it is not active.
        //------------------------------------------------------------------------

        case SPM_END_GROUP: {
            Debug(TEXT("ShellProc(SPM_END_GROUP, ...) called"));
            g_pKato->EndLevel(TEXT("END GROUP"));
            return SPR_HANDLED;
        }

        //------------------------------------------------------------------------
        // Message: SPM_BEGIN_TEST
        //
        // Sent to the DLL immediately before a test executes.  This gives the DLL
        // a chance to perform any common action that occurs at the beginning of
        // each test, such as entering a new logging level.  The spParam parameter
        // will contain a pointer to a SPS_BEGIN_TEST structure, which contains
        // the function table entry and some other useful information for the next
        // test to execute.  If the ShellProc function returns SPR_SKIP, then the
        // test case will not execute.
        //------------------------------------------------------------------------

        case SPM_BEGIN_TEST: {
            Debug(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));

            // Start our logging level.
            LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID,
                             TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                             pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                             pBT->dwRandomSeed);

            return SPR_HANDLED;
        }

        //------------------------------------------------------------------------
        // Message: SPM_END_TEST
        //
        // Sent to the DLL after a single test executes from the DLL.  This gives
        // the DLL a time perform any common action that occurs at the completion
        // of each test, such as exiting the current logging level.  The spParam
        // parameter will contain a pointer to a SPS_END_TEST structure, which
        // contains the function table entry and some other useful information for
        // the test that just completed.
        //------------------------------------------------------------------------

        case SPM_END_TEST: {
            Debug(TEXT("ShellProc(SPM_END_TEST, ...) called"));

            // End our logging level.
            LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;
            g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                           pET->lpFTE->lpDescription,
                           pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                           pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                           pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                           pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

            return SPR_HANDLED;
        }

        //------------------------------------------------------------------------
        // Message: SPM_EXCEPTION
        //
        // Sent to the DLL whenever code execution in the DLL causes and exception
        // fault.  By default, Tux traps all exceptions that occur while executing
        // code inside a Tux DLL.
        //------------------------------------------------------------------------

        case SPM_EXCEPTION: {
            Debug(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
            g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
            return SPR_HANDLED;
        }
   }

   return SPR_NOT_HANDLED;
}


/*
 * kato isn't initialized at this point, so don't use Error or Info
 */

/* Helper function for handleCmdLine() */
BOOL GetCommandLine(const TCHAR *szPlatName, FILE *fCmdFile, __out_ecount(dwCmdLineLen) TCHAR *szCmdLine, DWORD dwCmdLineLen);

/*******************************************************************************
 *                                handleCmdLine
 ******************************************************************************/

// This function gets the file name from the command line and parses the file
// to get the command line argument for the given device.
//
// Arguments:
//      Input: None
//      Output: None
//
// Return value:
//      TRUE if successful, FALSE otherwise

BOOL handleCmdLine ()
{
    BOOL returnVal = FALSE;

    cTokenControl tokens;
    FILE * fCmdFile = NULL;
    TCHAR * szPlatName = NULL;
    TCHAR *szCmdLine = NULL;

    // First check if command line exists
    if (!(g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine))
    {
        goto cleanAndReturn;
    }

    // Link into command line global
    g_szDllCmdLine = g_pShellInfo->szDllCmdLine;

    // Tokenize the command line
    if (!tokenize ((TCHAR *) g_szDllCmdLine, &tokens))
    {
        Debug (_T("Something is wrong with command line."));
        goto cleanAndReturn;
    }

    // Look for the cmd from file option
    TCHAR * szFname;

    if (!getOptionString (&tokens, ARG_STR_CMD_FILE_NAME, &szFname))
    {
        //Doesn't exist, which isn't an error.  Silent pass through.
        goto cleanAndReturn;
    }

    // The file name exists, so get the real path of the file
    TCHAR szRealName[MAX_FILENAME] = CMD_FILE_NAME_PATH;

    if (FAILED(StringCchCat(szRealName, MAX_FILENAME, szFname)))
    {
        Debug (_T("Path to filename '%s' could not be constructed."), szFname);
        goto cleanAndReturn;
    }
    

    // szRealName should now have complete path to file
    // Open the file in binary read mode
    errno_t err = _tfopen_s (&fCmdFile, szRealName, _T("rb"));

    if (err || !fCmdFile)
    {
        Debug (_T("Couldn't open file  %s."), szRealName);
        goto cleanAndReturn;
    }

    // Now we need to read the correct cmd line out of the file.
    // For this get the Platform Name and check if it matches the first word
    // of one of the cmd lines in the file.

    // Get the Platform Name
    szPlatName = (TCHAR*) malloc (SIZE_PLAT_NAME * sizeof(TCHAR));

    if (!szPlatName)
    {
        Debug (_T("Error: Couldn't allocate mem."));
        goto cleanAndReturn;
    }

    if (!GetPlatformName(szPlatName, SIZE_PLAT_NAME))
    {
        Debug (_T("Error: Could not get the platform name."));
        goto cleanAndReturn;
    }

    // Allocate memory to get the cmd line from the file.
    szCmdLine = (TCHAR*) malloc (MAX_COMMAND_LENGTH * sizeof(TCHAR));

    if (!szCmdLine)
    {
        Debug (_T("Error: Couldn't allocate mem."));
        goto cleanAndReturn;
    }

    // The file is open and ready for reading.
    // Call the function that reads the file and returns the
    // command line with matching Platform Name at its beginning.

    if(!GetCommandLine(szPlatName, fCmdFile, szCmdLine, MAX_COMMAND_LENGTH))
    {
        Debug (_T("Error: Could not get the cmd line."));
        goto cleanAndReturn;
    }
    Debug (_T("Command line successfully obtained from file: %s"), szCmdLine);

    // We found the correct cmd line with matching platform name
    // Parse the cmd line and obtain only the cmd line without the platform name
    // at the beginning.

    // Parse through the platform name(first word) and white spaces
    // until you reach the beginning of the command line (second word).

    const TCHAR *szPos = szCmdLine;

    while((*szPos != _T('\0')) && (!_istspace(*szPos)))
    {
        szPos++;
    }
    while((*szPos != _T('\0')) && (_istspace(*szPos)))
    {
        szPos++;
    }

    // Check if there was no command line and we reached the end of string.
    if(*szPos == _T('\0'))
    {
        Debug (_T("The command line is empty."));
        goto cleanAndReturn;
    }

    // Allocate memory for the global variable that holds the final cmd line
    g_szFileCmdLine = (TCHAR*)malloc(MAX_COMMAND_LENGTH * sizeof(TCHAR));
    if (!g_szFileCmdLine)
    {
        Debug (_T("Couldn't allocate memory."));
        goto cleanAndReturn;
    }

    // Copy the rest of the cmd line from this position to the global
    _tcsncpy_s(g_szFileCmdLine, MAX_COMMAND_LENGTH, (szPos), _TRUNCATE);

    // Link into command line global
    g_szDllCmdLine = g_szFileCmdLine;

    // We are done
    returnVal = TRUE;

 cleanAndReturn:

    // If file open, close it
    if(fCmdFile)
    {
        if(fclose(fCmdFile))
        {
            Debug (_T("Error closing file."));
        }
    }

    // If memory for cmd line allocated but we could not successfully read
    // the cmd line from file, free the allocated memory.
    if (g_szFileCmdLine && !returnVal)
    {
        free (g_szFileCmdLine);
        g_szFileCmdLine = NULL;
    }

    // Free szDevType and szDevTypeCmdLine if allocated.
    if(szPlatName)
    free(szPlatName);

    if(szCmdLine)
    free(szCmdLine);

    return (returnVal);

}

/*******************************************************************************
 *                                Get Command Line
 ******************************************************************************/

// This function reads the command lines from the given file. It then checks if the
// platform name matches the beginning of the command line to determine if it is
// the correct command line for the given platform.
//
// Arguments:
//      Input: Pointer to platform name string, pointer to file to be read,
//                       pointer to output buffer
//      Output: Command line in the output buffer
//
// Return value:
//      TRUE if successful, FALSE otherwise

BOOL GetCommandLine(const TCHAR *szPlatName, FILE *fCmdFile, __out_ecount(dwCmdLineLen) TCHAR *szCmdLine, DWORD dwCmdLineLen)
{
    BOOL returnVal = FALSE;

    // Check inputs
    if((!szPlatName) || (!fCmdFile) || (!szCmdLine) || (!dwCmdLineLen))
    {
        Debug (_T("One or more of the inputs are Null."));
        goto cleanAndReturn;
    }

    // The file is open and ready for reading.
    // Call the function that reads the file and returns the
    // command line with matching Platform Name at its beginning.

    char szCmd [MAX_COMMAND_LENGTH] = { 0 };
    TCHAR tc_szCmd[MAX_COMMAND_LENGTH] = { 0 };
    TCHAR szStoreCmdLine[MAX_COMMAND_LENGTH] = { 0 };

    do
    {
        if (!fgets(szCmd, _countof (szCmd), fCmdFile))
        {
            Debug (_T("Couldn't read from file.  GetLastError is %u."),
            GetLastError ());
            goto cleanAndReturn;
        }

        // The string obtained from the file could be terminated by \n, \r, \0
        // or a combination of them. We want to strip these and make sure
        // it is null terminated.
        char *endPtr = szCmd;
        while((*endPtr != '\r') && (*endPtr != '\n') && (*endPtr != '\0'))
        {
            endPtr++;
        }

        *endPtr = '\0';

        // The string read from the file is in ASCII, we need to convert it to unicode.
        // Get the length of the string needed for the unicode string
        size_t unicodeLength = 0;
        mbstowcs_s(&unicodeLength, NULL, 0, szCmd, _countof(szCmd));

        if (unicodeLength <= 0)
        {
            Debug (_T("Error getting needed unicode length."));
            goto cleanAndReturn;
        }

        // Check if given buffer is sufficient for the unicode string
        if(unicodeLength > _countof(tc_szCmd))
        {
            Debug (_T("Insufficient buffer for unicode string."));
        }

        // Now convert the ASCII string to unicode
        unicodeLength = 0;
        PREFAST_SUPPRESS( 26000, TEXT("Prefast is confused about the value of _TRUNCATE -1(=4294967295) and comes to the wrong conclusion\r\n"));
        if(mbstowcs_s (&unicodeLength, tc_szCmd, _countof(tc_szCmd), szCmd, _TRUNCATE) || (unicodeLength <= 0))
        {
            Debug (_T("Error converting to unicode"));
            goto cleanAndReturn;
        }

        // We now have the unicode cmd line
        // Debug (_T("Command line successfully read from file: %s"), tc_szCmd);

        // Find out if this is the cmd line we want.
        // Check if the Platform Name matches the beginning of the cmd line.

        // First store the cmd line in a temporary string
        _tcsncpy_s(szStoreCmdLine, _countof(szStoreCmdLine), tc_szCmd, _TRUNCATE);


        // To get the platform name from the beginning of the cmd line
        // get the first token using a white space separator (space, tab)

        TCHAR * szContext = NULL;
        const TCHAR * tok = NULL;
        const TCHAR *const szSeps = _T(" \t");

        tok = _tcstok_s (tc_szCmd, szSeps, &szContext);

        // Compare this with the actual platform name.
        // If equal, we found the correct cmd line, else continue.
    } while((_tcsnicmp(tc_szCmd, szPlatName, _countof(tc_szCmd))) != 0);

    // We now have the correct cmd line stored in szStoreCmdLine.
    // Copy this to the output string

    // Check if the output buffer is sufficient.
    if(_countof(szStoreCmdLine) > dwCmdLineLen)
    {
        Debug (_T("Error: Insufficient buffer for cmd line."));
        goto cleanAndReturn;
    }

    _tcsncpy_s(szCmdLine, dwCmdLineLen, szStoreCmdLine, _TRUNCATE);

    returnVal = TRUE;

cleanAndReturn:
    return returnVal;

}


/*******************************************************************************
 *                                handleCmdLineUnload
 ******************************************************************************/

// This function frees the memory allocated in handleCmdLine when the dll
// is unloaded.
//
// Arguments:
//      Input: None
//      Output: None
//
// Return value:
//      TRUE if successful, FALSE otherwise

BOOL handleCmdLineUnload()
{
    if (g_szFileCmdLine)
    {
        free (g_szFileCmdLine);
        g_szFileCmdLine = NULL;
    }

    return (TRUE);
}
