//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

/*
      @doc LIBRARY

Module Name:

  netmain.cpp

 @module NetMain - Contains an entry point for starting functions consistently. |

  This file is meant as a template for making a main function with the parameters parsed correctly
  to pass into a standard C main, and for initializing the NetLogWrapper so functions like
  QAMessage work as expected.

  The parameters passed into the NetLogWrapper may change if you decide you want CE console
  or do not want some of the other entries passed in, but I suspect the defaults will 
  work correctly for most.


   <nl>Link:    netmain.lib
   <nl>Include: netmain.h

     @xref    <f _tmain>
     @xref    <f mymain>
*/

#include "netmain.h"
#include "netinit.h"

static void InitializeSystem(DWORD dwFlags);

#ifndef ASSERT
#include <assert.h>
#define ASSERT assert
#endif

#define MAX_ARGS	128			// Value chosen to make it easier to remove options from the command line
#define MAX_CMDLINE_CHARS	1024	// Maximum number of chars on the command line

//
// Global Variables
//
int g_argc;
LPTSTR *g_argv;

/*

@func extern "C" void | NetMainDumpParameters | 

    Library function to dump all of the valid NetMain parameters.

@comm:(LIBRARY)
    Dump the parameters that get used in the initial _tmain function
    and are never passed to the program.  See <f _tmain> for the current
    list of parameters.

*/
extern "C" void NetMainDumpParameters(void)
{
    NetLogMessage(_T("-mt     Use multiple logging files, one for each thread the test forks off."));
    NetLogMessage(_T("-z      Output test messages to the console."));
    NetLogMessage(_T("-fl     Output test messages to a file on the CE device."));
    NetLogMessage(_T("-fr     Output test messages to a PPSH file on the NT device.")); 
    NetLogMessage(_T("-v      Specifies the system to log with full verbosity. "));
    NetLogMessage(_T("-v[int] Specifies the system log with the given verbosity."));
    NetLogMessage(_T("-nowatt Specifies for the test to not output the 6 @ signs that indicate end of test."));
}

/*

@func extern "C" int | _tmain | 

  Entry point called by WinMain, calls the user defined <f mymain>.

@rdesc Returns the negative number of errors encounter by the test or
       if the test itself returns a negative value that is used.  If there
       were no errors then the function returns 0.

@parm   int    |  argc   | Contains the number of parameters
@parm   TCHAR* |  argv[] | Contains the command line parameters.

@comm:(LIBRARY)
    Parse the command line before the users mymain function gets 
    to it.  Look for specific parameters that effect LogWrap 
    functions.  This allows all programs to have common parameters
    for common functionality, without duplicating the effort.  <nl> 

    -mt     Use multiple logging files, one for each thread the 
            test forks off. <nl>
    -z      Output test messages to the console. <nl>
    -fl     Output test messages to a file on the CE device. <nl>
    -fr     Output test messages to a PPSH file on the NT device.  <nl>
    -v      Specifies the system to log with full verbosity. <nl>
    -v[int] Specifies the system log with the given verbosity.  Verbosity
            mappings can be found in netlog.h. <nl>
    -nowatt Specifies for the test to not output the 6 @ signs that indicate
            end of test.  This is to help automation support.
*/
extern "C"
int _cdecl _tmain(int argc, TCHAR* argv[])
{
    DWORD options_flag    = NETLOG_DEBUG_OP;
    DWORD level           = LOG_PASS;
    DWORD use_multithread = FALSE;
    BOOL  nowatt          = FALSE;
    BOOL  init            = FALSE;
    TCHAR szCmdLine[MAX_CMDLINE_CHARS] = _T("");
	BOOL bRemove[MAX_ARGS];
	int iNumRemoved		  = 0;
	int i, j;
	LPTSTR ptszFilterVars;

    //
    //  Recreate the original command line.  Do it here rather than just keeping the WinMain one
    //  since NT builds do not get WinMain.
    //

	// Make sure we have enough room to store the command line first
	for(j = 1, i = 0; i < argc; i++)
		j += _tcslen(argv[i]) + 1; // The extra char is for the space between arguments

	if(j > MAX_CMDLINE_CHARS)
	{
		OutputDebugString(TEXT("*** FATAL ERROR *** Command Line is too long!"));
		return 1;
	}

    for(i = 0; i < argc; i++)
    {
		_tcscat(szCmdLine, argv[i]);
		_tcscat(szCmdLine, _T(" "));
    }

	for(i = 0; i < MAX_ARGS; i++)
		bRemove[i] = FALSE;

	//
	//  Try to parse the command line for Logging options, and RAS options.
	//
	if((i = WasOption(argc, argv, _T("mt"))) >= 0)
	{
		use_multithread = TRUE;
		bRemove[i] = TRUE;
	}
	
	if((i = WasOption(argc, argv, _T("z"))) >= 0)
	{
		options_flag |= NETLOG_CON_OP;
		bRemove[i] = TRUE;
	}
	
	if((i = WasOption(argc, argv, _T("fl"))) >= 0)
	{
		options_flag |= NETLOG_FILE_OP;
		bRemove[i] = TRUE;
	}
	
	if((i = WasOption(argc, argv, _T("fr"))) >= 0)
	{
		options_flag |= NETLOG_PPSH_OP;
		bRemove[i] = TRUE;
	}
	
	if((i = WasOption(argc, argv, _T("v"))) >= 0)
	{
		// Check if a number is there, if not set to full.  Otherwise take the number.
		if(GetOptionAsInt(argc, argv, _T("v"), (int *)&level) < 0)
			level = LOG_COMMENT;
		bRemove[i] = TRUE;
	}
	
	if((i = WasOption(argc, argv, _T("nowatt"))) >= 0)
	{
		nowatt = TRUE;
		bRemove[i] = TRUE;
	}
	
	if((i = WasOption(argc, argv, _T("init"))) >= 0)
	{
		init = TRUE;
		bRemove[i] = TRUE;
	}

	// Get WATT variable filter, ex. VAR_1,VAR_2 would mean only WATT variables
	// with the name VAR_1 or VAR_2 would be output.  (case insensitive)
	if((i = WasOption(argc, argv, _T("wf"))) >= 0)
	{
		if(GetOption(argc, argv, _T("wf"), &ptszFilterVars) >= 0)
		{
			if(_tcslen(argv[i]) == _tcslen(_T("wf")))
				bRemove[i+1] = TRUE;
			RegisterWattFilter(ptszFilterVars);
		}
		bRemove[i] = TRUE;
	}
	
	//
	//  Remove our parameters off of the command line before the user gets it.
	//
	for(j = 1, i = 1; j < argc; j++)
	{
		if(bRemove[j])
			iNumRemoved++;
		else
			argv[i++] = argv[j];
	}
	
	argc -= iNumRemoved;

	// Set Globals
	g_argc = argc;
	g_argv = argv;

    //
    //  Initialize the Logwrapper
    //
    NetLogInitWrapper(argv[0], level, options_flag, use_multithread);

    //
    //  Disable the WATT output to allow tests to be batched.
    //
    if(nowatt)
        NetLogSetWATTOutput(FALSE);

    //
    //  Print out the command line.
    //
    NetLogMessage(_T("Running: %s"), szCmdLine);

    //
    // Initialize the system by calling the program 'NetInit' with the proper parameters.
    //
    if(init && (gdwMainInitFlags != 0))
		InitializeSystem(gdwMainInitFlags);

    int ret_val = -1;
    
    __try {

        // Call the user mymain function
        ret_val = mymain(argc, argv);
    
    } __except(1)
    {
        // Output an error so we know something went wrong.
        NetLogError(_T("The program '%s' main thread crashed, exiting"), gszMainProgName);
    }

    NetLogMessage(_T("User mymain function returned the value %d"), ret_val);
    
    // Finish up on the NetLogWrapper.
    NetLogCleanupWrapper();
    
    //
    //  If the user tries to output a positive or zero value, insert the error count to make sure 
    //  that we don't miss any errors.
    //
    if(ret_val >= 0)
    {
        ret_val = -((int)NetLogGetErrorCount());
    }
    
    return ret_val;    
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Function:
    WinMain

Description:
    This version of "WinMain" parses the command line into an array of
	parameters which is passed in classical C ("argc/argv") format to the
	user-defined function "_tmain".
	NOTE: The maximum number of arguments is currently hard-coded at 64.
          The user must provide their program name in the externed 
            LPTCSTR gszNetMainProgName.  This is filled in as argv[0]

-------------------------------------------------------------------*/

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
  LPTSTR lpCmdLine, int nCmdShow) 
{  
    // Parse command line into individual arguments - arg/argv style.
    LPTSTR argv[64] = { gszMainProgName };
    int argc = 64;

	argc--; // first arg is prog name
    CommandLineToArgs(lpCmdLine, &argc, &argv[1]);
	argc++; // add back the prog name

    // Call the standard main with our parsed arguments.
    return _tmain(argc, argv);
} // end _tWinMain


LPCTSTR szNetInitLib = _T("NetInit.dll");
#ifdef UNDER_CE
LPCWSTR szNetInitFunc = _T("NetInit");
LPCWSTR szLogInitFunc = _T("NetInitStartLogWrap");
#else
LPCSTR szNetInitFunc = "NetInit";
LPCSTR szLogInitFunc = "NetInitStartLogWrap";
#endif
typedef int (*NET_INIT_PF)(DWORD flags);
    
//
//  StartNetInit - runs the system init program with the given flags as the command line.
//
//  NetInit is a seperate DLL so that the user is not required to link with everything that
//  NetInit links with.
//
//  Load the DLL and call the function to initialize the system.  Pass into it the flags 
//  that the user provided in the executable.  Free the library when this is completed.
//
//  This function can error out which will change the error count for the application.
//  More thought should be put into this to make sure that erroring out is the correct 
//  thing to do.
//
static void InitializeSystem(DWORD dwFlags)
{
    HINSTANCE hLib = LoadLibrary(szNetInitLib);

    if(hLib == NULL)
    {
        NetLogWarning(_T(">>>>>>>> LoadLibrary FAILED on '%s' with '%d', the system cannot properly initialize"),
                    szNetInitLib, GetLastError());
        return;
    }
    else
    {
        NET_INIT_PF pfLogInit = (NET_INIT_PF)GetProcAddress(hLib, szLogInitFunc);
        NET_INIT_PF pfNetInit = (NET_INIT_PF)GetProcAddress(hLib, szNetInitFunc);

        // Get the handle associated with the current logging setup.
        HANDLE h = NetLogIsInitialized();
        ASSERT(h);

		if(pfLogInit)
		{
			if(!pfLogInit((DWORD)h))
				NetLogError(_T("Could not initialize the DLLs logging mechanism."));
		}
		else
			NetLogError(_T("Couldn't get pointer to function %s"), szLogInitFunc);

		if(pfNetInit)
		{
			if(pfNetInit(dwFlags) == -1)
				NetLogError(_T("NetInit() failed"));
		}
		else
			NetLogError(_T("Couldn't get pointer to function %s"), szNetInitFunc);

        if(!FreeLibrary(hLib))
        {
            NetLogError(_T("Could not free the library"));
        }
    }
}