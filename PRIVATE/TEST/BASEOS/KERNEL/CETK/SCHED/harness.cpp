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

#include <windows.h>
#include <tchar.h>
#include "harness.h"

TCHAR *szTestMarker = TEXT("---------------------------------------------------------------\n");
static TCHAR szBuffer[ 1024 ] = { 0 };

TCHAR **szTestArgv=NULL;
int nTestArgc = 0;
DWORD dwStartTime = 0, dwEndTime = 0;
CRITICAL_SECTION cs = { 0 };

#ifdef __cplusplus
extern "C" {
#endif
	HINSTANCE   g_hInstance = { 0 };
#ifdef __cplusplus
}
#endif

#ifdef STANDALONE
DWORD dwFailTotal=0;
DWORD dwPassTotal=0;
DWORD dwTotalTests=0;
BOOL  *bRunTest= NULL;
DWORD dwRandomSeed=0;
#else
HKATO hKato;
#endif

#ifdef UNDER_CE
HANDLE hPPSH=NULL;
#endif

HANDLE hHeap=NULL;
//******************************************************************************
void CreateLog(LPTSTR szLogName)
{
	TCHAR szLogFile[MAX_PATH] = { 0 };

	InitHarness();
	hHeap = HeapCreate( HEAP_NO_SERIALIZE, 4096, 32768);
	wsprintf( szLogFile, TEXT("%s.LOG"), szLogName);
	
#ifdef STANDALONE
#else
	hKato = KatoCreate( szLogName);
#endif
#ifdef UNDER_CE
	hPPSH = PPSH_OpenStream( szLogFile, MODE_WRITE);
#endif

}

void CloseLog()
{
	HeapDestroy( hHeap);
#ifdef STANDALONE
#else
	KatoDestroy( hKato);
#endif
#ifdef UNDER_CE
	PPSH_CloseStream( hPPSH);
#endif
	DeinitHarness();
}

HANDLE StartTest( LPTSTR szText)
{
	PYGTESTDATA pTestData = NULL;
	MEMORYSTATUS ms = { 0 };


	pTestData = (PYGTESTDATA)HeapAlloc( hHeap, HEAP_ZERO_MEMORY, sizeof(YGTESTDATA));
	pTestData->bSuccess = TRUE;
	pTestData->szData = szText ? (LPTSTR)HeapAlloc( hHeap, 0, (_tcslen( szText)+1)*sizeof(TCHAR)) : NULL;
	ms.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus( &ms);
	pTestData->dwPhysStart = ms.dwAvailPhys;
	if (szText)  {
		_tcscpy( pTestData->szData, szText);
		LogPrint(  TEXT("START: %s\n"), szText);
//		KatoBeginLevel( hKato, 0, TEXT("BEGIN: %s"), szText);
	}
	return (HANDLE)pTestData;
}

BOOL FinishTest(HANDLE hTest)
{
	BOOL bSuccess = FALSE;
	PYGTESTDATA pTestData = NULL;
	MEMORYSTATUS ms = { 0 };

	pTestData = (PYGTESTDATA)hTest;
	bSuccess = pTestData->bSuccess;
#ifdef STANDALONE
	if (bSuccess) {
		dwPassTotal++;		
	} else {
		dwFailTotal++;
	}
	LogPrint( TEXT("\tNumber of steps = %ld "), pTestData->dwStepsStarted);
	LogPrint( TEXT("Steps Completed = %ld "), pTestData->dwStepsCompleted);
	LogPrint( TEXT("Steps Skipped   = %ld\r\n"), pTestData->dwStepsSkipped);
#endif

	if (((PYGTESTDATA)hTest)->szData) {
//	#ifdef STANDALONE
		LogPrint( TEXT("%-5s: %s\n"), bSuccess ? TEXT("PASS") : TEXT("FAIL"), ((PYGTESTDATA)hTest)->szData);
//	#else
//		KatoEndLevel( hKato, TEXT("%-5s: %s"), bSuccess ? TEXT("PASS") : TEXT("FAIL"), ((PYGTESTDATA)hTest)->szData);
//	#endif
		HeapFree( hHeap, 0, ((PYGTESTDATA)hTest)->szData);
	}
	ms.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus( &ms);
	LogPrint( TEXT("Memory Difference after running test = %ld\r\n"), pTestData->dwPhysStart-ms.dwAvailPhys);
	LogPrint( szTestMarker);
	HeapFree( hHeap, 0, (PYGTESTDATA)hTest);
	return bSuccess;
}

void FailTest( HANDLE hTest)
{
	((PYGTESTDATA)hTest)->bSuccess = FALSE;	
}

HANDLE BeginStep( HANDLE hTest, LPTSTR szText)  
{
	PYGSTEPDATA pStepData = NULL;

	((PYGTESTDATA)hTest)->dwStepsStarted++;
	pStepData = (PYGSTEPDATA)HeapAlloc( hHeap, 0, sizeof(YGSTEPDATA));
	pStepData->bSuccess = FALSE;
	pStepData->szData = szText ? (LPTSTR)HeapAlloc( hHeap, 0, (_tcslen( szText)+1)*sizeof(TCHAR)) : NULL;
	if (szText)  {
		_tcscpy( pStepData->szData, szText);
//#ifdef STANDALONE
		LogPrint(  TEXT("BEGIN: %s\r\n"), szText);
//#else
//		KatoBeginLevel( hKato, 0, TEXT("BEGIN: %s"), szText);
//#endif
	}
	return (HANDLE)pStepData;
}

void SkipStep( HANDLE hTest, HANDLE hStep)  
{							   
	
	((PYGTESTDATA)hTest)->dwStepsSkipped;


	if (((PYGSTEPDATA)hStep)->szData)  {
//#ifdef STANDALONE
		LogPrint( TEXT(" SKIP: %s\n"), ((PYGSTEPDATA)hStep)->szData);
//#else
//		KatoEndLevel( hKato, TEXT(" SKIP: %s"), ((PYGSTEPDATA)hStep)->szData);
//#endif
		HeapFree( hHeap, 0, ((PYGSTEPDATA)hStep)->szData);
	}
	HeapFree( hHeap, 0, (PYGSTEPDATA)hStep);
	return;
}

void PassStep( HANDLE hStep)  
{
  ((PYGSTEPDATA)hStep)->bSuccess = TRUE;	
}

void EndStep( HANDLE hTest, HANDLE hStep)
{
	BOOL bSuccess = FALSE;
 
 	((PYGTESTDATA)hTest)->dwStepsCompleted++;
	bSuccess = ((PYGSTEPDATA)hStep)->bSuccess;

	if (!bSuccess) FailTest( hTest);

	if (((PYGSTEPDATA)hStep)->szData)  {
//#ifdef STANDALONE
		LogPrint( TEXT("%-5s STEP: %s\r\n"), bSuccess ? TEXT("PASS") : TEXT("FAIL"), ((PYGSTEPDATA)hStep)->szData);
//#else
//		KatoEndLevel( hKato, TEXT("%-5s: %s"), bSuccess ? TEXT("PASS") : TEXT("FAIL"), ((PYGSTEPDATA)hStep)->szData);
//#endif
		HeapFree( hHeap, 0, ((PYGSTEPDATA)hStep)->szData);
	}
	HeapFree( hHeap, 0, (PYGSTEPDATA)hStep);
}	


void LogDetail( LPTSTR szFormat, ...)
{
	DWORD gle=GetLastError();
	va_list valist;

	EnterCriticalSection(& cs);
	va_start( valist, szFormat );
	wvsprintf( szBuffer, szFormat, valist );
	va_end( valist );


#ifndef STANDALONE
	KatoComment( hKato, 0, szBuffer);
#else
	PRINT( TEXT("\t%s\n"), szBuffer);
#endif
#ifdef UNDER_CE
	/* this is partially broken for high unicode chars */
	wcstombs( (char *)szBuffer, szBuffer, _tcslen(szBuffer));
	/* BUGBUG: if PPSH_WriteStream fails; GLE gets screwed up */
	PPSH_WriteStream( hPPSH, "\t", 1);
	PPSH_WriteStream( hPPSH, szBuffer, _tcslen(szBuffer));
	PPSH_WriteStream( hPPSH, "\n", 1);
#endif
	LeaveCriticalSection( &cs);
	SetLastError(gle);
}

void LogError( LPTSTR szText)
{
	DWORD dwErrorCode = GetLastError(); 	
	LogDetail( TEXT("%s: Failed with Errorcode %ld\r\n"), szText, dwErrorCode);
	SetLastError(dwErrorCode);
}

//******************************************************************************
void LogPrint(TCHAR *szFormat, ... )
{
	va_list valist;

	EnterCriticalSection( &cs);
	va_start( valist, szFormat );
	wvsprintf( szBuffer, szFormat, valist );
	va_end( valist );

	PRINT(szBuffer);
#ifdef UNDER_CE
	wcstombs( (char *)szBuffer, szBuffer, _tcslen(szBuffer));
	PPSH_WriteStream( hPPSH, szBuffer, _tcslen(szBuffer));
#endif
	LeaveCriticalSection( &cs);
}

void InitHarness()
{
	InitializeCriticalSection( &cs);
	
	dwStartTime = GetTickCount();
}

void DeinitHarness()
{
	DeleteCriticalSection( &cs);
	dwEndTime = GetTickCount();
}

#ifdef UNDER_CE
BOOL CopyFromPPSH( LPTSTR szSource, LPTSTR szDestination)
{
	HANDLE hSource = (HANDLE)HFILE_ERROR, hDestination = INVALID_HANDLE_VALUE;
	BOOL bSuccess = TRUE;
	LPSTR szLocalBuffer = NULL;
	DWORD dwWritten=0, dwBytesTotal=0;
	int nRead=0;

	hSource = PPSH_OpenStream( szSource, MODE_READ);

	if (hSource == (HANDLE)HFILE_ERROR)  {
		SetLastError( ERROR_FILE_NOT_FOUND);
		return(FALSE);
	}
	hDestination = CreateFile( szDestination, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hDestination == INVALID_HANDLE_VALUE)  {
		bSuccess = FALSE;
		goto exit;
	}

	szLocalBuffer = (LPSTR)HeapAlloc( GetProcessHeap(), 0, 1024);

	if (!szLocalBuffer)  {
		bSuccess = FALSE;
		goto exit;
	}
	do {
		nRead = PPSH_ReadStream( hSource, szLocalBuffer, 1024);
		if (nRead)	 {
			if (!WriteFile( hDestination, szLocalBuffer, nRead, &dwWritten, NULL))  {
				bSuccess = FALSE;
				goto exit;
			}
			dwBytesTotal += nRead;
		}
	} while( nRead == 1024);
exit:
	if (hDestination != INVALID_HANDLE_VALUE)  {
		CloseHandle( hDestination);
	}
	if (hSource != (HANDLE) HFILE_ERROR)  {
		PPSH_CloseStream( hSource);	
	}
	return(bSuccess);
}	
#endif


void LogMemory()
{
	MEMORYSTATUS ms = { 0 };
	
	ms.dwLength = sizeof( MEMORYSTATUS);
	GlobalMemoryStatus( &ms);
	
	LogDetail( TEXT("Total Physical = %ld Available Physical = %ld\r\n"), ms.dwTotalPhys, ms.dwAvailPhys);
}	

/***
*void Parse_Cmdline(cmdstart, argv, lpstr, numargs, numbytes)
*
*Purpose:
*       Parses the command line and sets up the Unicode argv[] array.
*       On entry, cmdstart should point to the command line,
*       argv should point to memory for the argv array, lpstr
*       points to memory to place the text of the arguments.
*       If these are NULL, then no storing (only counting)
*       is done.  On exit, *numargs has the number of
*       arguments (plus one for a final NULL argument),
*       and *numbytes has the number of bytes used in the buffer
*       pointed to by args.
*
*Entry:
*       LPWSTR cmdstart - pointer to command line of the form
*           <progname><nul><args><nul>
*       TCHAR **argv - where to build argv array; NULL means don't
*                      build array
*       LPWSTR lpstr - where to place argument text; NULL means don't
*                      store text
*
*Exit:
*       no return value
*       INT *numargs - returns number of argv entries created
*       INT *numbytes - number of bytes used in args buffer
*
*Exceptions:
*
*******************************************************************************/

void Parse_Cmdline (
    LPTSTR cmdstart,
    LPTSTR*argv,
    LPTSTR lpstr,
	INT lpstr_sz,
    INT *numargs,
    INT *numbytes
    )
{
    LPTSTR p = NULL;
	LPTSTR lpstr_o = lpstr;
    TCHAR c = 0;
    INT inquote = 0;                    /* 1 = inside quotes */
    INT copychar = 0;                   /* 1 = copy char to *args */
    WORD numslash = 0;                  /* num of backslashes seen */

    *numbytes = 0;
    *numargs = 1;                   /* the program name at least */

    /* first scan the program name, copy it, and count the bytes */
    p = cmdstart;
    if (argv)
        *argv++ = lpstr;

    /* A quoted program name is handled here. The handling is much
       simpler than for other arguments. Basically, whatever lies
       between the leading double-quote and next one, or a terminal null
       character is simply accepted. Fancier handling is not required
       because the program name must be a legal NTFS/HPFS file name.
       Note that the double-quote characters are not copied, nor do they
       contribute to numbytes. */
    if (*p == TEXT('\"'))
    {
        /* scan from just past the first double-quote through the next
           double-quote, or up to a null, whichever comes first */
        while ((*(++p) != TEXT('\"')) && (*p != TEXT('\0')))
        {
            *numbytes += sizeof(TCHAR);

            if (lpstr && (lpstr_o+lpstr_sz)<lpstr)
				PREFAST_SUPPRESS( 394, "false positive" )
                *lpstr++ = *p;
        }
        /* append the terminating null */
        *numbytes += sizeof(TCHAR);
        if (lpstr)
            *lpstr++ = TEXT('\0');

        /* if we stopped on a double-quote (usual case), skip over it */
        if (*p == TEXT('\"'))
            p++;
    }
    else
    {
        /* Not a quoted program name */
        do {
            *numbytes += sizeof(TCHAR);
            if (lpstr)
                *lpstr++ = *p;

            c = (TCHAR) *p++;

        } while (c > TEXT(' '));

        if (c == TEXT('\0'))
        {
            p--;
        }
        else
        {
            if (lpstr)
                *(lpstr - 1) = TEXT('\0');
        }
    }

    inquote = 0;

    /* loop on each argument */
    for ( ; ; )
    {
        if (*p)
        {
            while (*p == TEXT(' ') || *p == TEXT('\t'))
                ++p;
        }

        if (*p == TEXT('\0'))
            break;                  /* end of args */

        /* scan an argument */
        if (argv)
            *argv++ = lpstr;         /* store ptr to arg */
        ++*numargs;

        /* loop through scanning one argument */
        for ( ; ; )
        {
            copychar = 1;
            /* Rules: 2N backslashes + " ==> N backslashes and begin/end quote
                      2N+1 backslashes + " ==> N backslashes + literal "
                      N backslashes ==> N backslashes */
            numslash = 0;
            while (*p == TEXT('\\'))
            {
                /* count number of backslashes for use below */
                ++p;
                ++numslash;
            }
            if (*p == TEXT('\"'))
            {
                /* if 2N backslashes before, start/end quote, otherwise
                   copy literally */
                if (numslash % 2 == 0)
                {
                    if (inquote)
                        if (p[1] == TEXT('\"'))
                            p++;    /* Double quote inside quoted string */
                        else        /* skip first quote char and copy second */
                            copychar = 0;
                    else
                        copychar = 0;       /* don't copy quote */

                    inquote = !inquote;
                }
                numslash /= 2;          /* divide numslash by two */
            }

            /* copy slashes */
            while (numslash--)
            {
                if (lpstr)
                    *lpstr++ = TEXT('\\');
                *numbytes += sizeof(TCHAR);
            }

            /* if at end of arg, break loop */
            if (*p == TEXT('\0') || (!inquote && (*p == TEXT(' ') || *p == TEXT('\t'))))
                break;

            /* copy character into argument */
            if (copychar)
            {
                if (lpstr)
                        *lpstr++ = *p;
                *numbytes += sizeof(TCHAR);
            }
            ++p;
        }

        /* null-terminate the argument */

        if (lpstr)
            *lpstr++ = TEXT('\0');         /* terminate string */
        *numbytes += sizeof(TCHAR);
    }

}


/***
*CommandLineToArgvW - set up Unicode "argv" for C programs
*
*Purpose:
*       Read the command line and create the argv array for C
*       programs.
*
*Entry:
*       Arguments are retrieved from the program command line
*
*Exit:
*       "argv" points to a null-terminated list of pointers to UNICODE
*       strings, each of which is an argument from the command line.
*       The list of pointers is also located on the heap or stack.
*
*Exceptions:
*       Terminates with out of memory error if no memory to allocate.
*
*******************************************************************************/

LPTSTR* CommandLineToArgv(LPCTSTR lpCmdLine, int*pNumArgs)
{
    LPTSTR*argv_U = NULL;
    LPTSTR  cmdstart = NULL;                /* start of command line to parse */
    INT     numbytes = 0;
    //TCHAR   pgmname[MAX_PATH];
	DWORD	lpstr_sz = 0;

    if (pNumArgs == NULL) {
	SetLastError(ERROR_INVALID_PARAMETER);
	return NULL;
    }

	*pNumArgs = 0;

	if (*lpCmdLine == TEXT('\0')) {
		return NULL;
	}
    /* Get the program name pointer from Win32 Base */
    //GetModuleFileName (NULL, pgmname, sizeof(pgmname) / sizeof(TCHAR));

    /* if there's no command line at all (won't happen from cmd.exe, but
       possibly another program), then we use pgmname as the command line
       to parse, so that argv[0] is initialized to the program name */
//    cmdstart = (*lpCmdLine == TEXT('\0')) ? pgmname : (LPTSTR) lpCmdLine;
	cmdstart = (LPTSTR)lpCmdLine;

    /* first find out how much space is needed to store args */
    Parse_Cmdline (cmdstart, NULL, NULL, 0, pNumArgs, &numbytes);

    /* allocate space for argv[] vector and strings */
    argv_U = (LPTSTR*) HeapAlloc(GetProcessHeap(),
    							 0,
								*pNumArgs*sizeof(LPTSTR) + numbytes);
    if (!argv_U) {
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return (NULL);
    }

    /* store args and argv ptrs in just allocated block */
	lpstr_sz = *pNumArgs * sizeof(LPTSTR);
	ASSERT((((LPBYTE)argv_U) + lpstr_sz) >= (LPBYTE)argv_U );
    Parse_Cmdline (cmdstart, argv_U,
		   (LPTSTR) (((LPBYTE)argv_U) + lpstr_sz), numbytes,
                   pNumArgs, &numbytes);

    return (argv_U);
}

void PrintHelp() 
{
	LogPrint( TEXT("\r\nTestName <options>\r\n"));
	LogPrint( TEXT("-l\t\tList Tests\r\n"));
	LogPrint( TEXT("-r<seed>\t\tRandom Number Seed\r\n"));
	LogPrint( TEXT("-t<test>\t\tRun Test Number (Can have multiple number of these)\r\n"));
	LogPrint( TEXT("-?\t\tPrint this help\r\n"));
}

void ListTests(LPFUNCTION_TABLE_ENTRY lpfte)
{
	DWORD i=0;
	while ((++lpfte)->uDepth) {
		LogPrint(TEXT("\t%03ld) %s\r\n"), ++i, lpfte->lpDescription);
	}
}



#ifdef STANDALONE
void RunTests(LPFUNCTION_TABLE_ENTRY lpfte)
{
	DWORD i=0;
	LogPrint( TEXT("%4ld:\t%s\r\n"), i,lpfte->lpDescription);
	while ((++lpfte)->uDepth) {
		if (!bRunTest || bRunTest[i]) {
			LogPrint(TEXT("%4ld:\t%s\r\n"), i, lpfte->lpDescription);
			lpfte->lpTestProc( TPM_EXECUTE, (TPPARAM)(lpfte->dwUserData), lpfte);
		}
		i++;
	}
}

void CountTests(LPFUNCTION_TABLE_ENTRY lpfte)
{
	while ((++lpfte)->uDepth) {
		dwTotalTests++;
	}
}

#ifdef CONSOLE
int _tmain( int argc, TCHAR *argv[])
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nCmdShow)
#endif
{
	SPS_REGISTER spsr = { 0 };
	LPFUNCTION_TABLE_ENTRY lpfte = { 0 };
	DWORD dwElapsedTime = 0, dwTest = 0;
	SPS_SHELL_INFO ssi = { 0 };
	LPTSTR lpTestCmdLine = NULL;
	int i = 0;
#ifndef CONSOLE
	szTestArgv = CommandLineToArgv( lpCmdLine, &nTestArgc);
#else
	szTestArgv = argv;
	nTestArgc  = argc;
#endif
	
// Initialize the tests
	ShellProc( SPM_LOAD_DLL, NULL);
	ShellProc( SPM_REGISTER, (SPPARAM)&spsr);
#ifndef CONSOLE
	ssi.hInstance = hInstance;
#endif
	ssi.hWnd = NULL;
	ssi.hevmTerminate = lpTestCmdLine;
	ShellProc( SPM_SHELL_INFO, (SPPARAM) &ssi);
	lpfte = spsr.lpFunctionTable;

	CountTests(lpfte);

	LogPrint( TEXT("Test built on %s %s\r\n"), TEXT(__DATE__), TEXT(__TIME__));
	LogPrint( TEXT("Total Number of tests = %ld\r\n"), dwTotalTests);

// Parse Arguments
	i = 0;
	while (i < nTestArgc) {
		if (*szTestArgv[i] == TEXT('-'))  {
			switch (*(szTestArgv[i]+1)) {
			case TEXT('t'):
				if (!bRunTest) {
					bRunTest = (BOOL *)HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwTotalTests * sizeof(BOOL));
				}
				dwTest = _ttoi(szTestArgv[i]+2);
				if (dwTest && (dwTest <= dwTotalTests)) {
					bRunTest[dwTest-1] = TRUE;
				}
				break;
			case TEXT('l'):
				ListTests(lpfte);
				goto Done;
			case TEXT('?'):
				PrintHelp();
				goto Done;
			case TEXT('r'):
				dwRandomSeed = _ttoi(szTestArgv[i]+2);
				break;
			default:
				break;
			}
		}
		i++;
	}
	

// Run the tests
	RunTests(lpfte);

// Finish up and print status	
	if (dwEndTime >= dwStartTime) {
		dwElapsedTime = dwEndTime-dwStartTime;
	} else	{
		dwElapsedTime = dwEndTime + (0xFFFFFFFF-dwStartTime) + 1;
	}
	LogPrint( TEXT("Test completed at %02d:%02d:%02d.%02ld\r\n"),
		   (dwElapsedTime / (1000*60*60)) % 60,
		   (dwElapsedTime / (1000*60)) % 60,
		   (dwElapsedTime / 1000) % 60,
		    dwElapsedTime % 100);
	LogPrint( TEXT("Total number of tests  = %ld\r\n"), dwPassTotal + dwFailTotal);
	LogPrint( TEXT("Number of tests passed	= %ld\r\n"), dwPassTotal);
	LogPrint( TEXT("Number of tests failed = %ld\r\n"), dwFailTotal);
	LogPrint( TEXT(">>>>>>>>>>> TEST %s !!! <<<<<<<<<<<\r\n"), dwFailTotal ? TEXT("FAILED") : TEXT("PASSED"));
Done:
// Cleanup
	ShellProc( SPM_UNLOAD_DLL, NULL);
	if (bRunTest) {
		HeapFree( GetProcessHeap(), 0, bRunTest);
	}
	return(0);
}
#else

BOOL WINAPI DllMain(
	HANDLE	hModule, 
	DWORD	dwReason,
	LPVOID	lpRes
	)
{
	if(dwReason == DLL_PROCESS_ATTACH) 
		g_hInstance = (HINSTANCE)hModule;

    return TRUE;
}
#endif
