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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Implementation of the NetMain_t class.
//
// ----------------------------------------------------------------------------

#include "NetMain_t.hpp"

#include <assert.h>
#include <inc/bldver.h>
#include <cmnprint.h>
#include <netcmn.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Constructor.
//
NetMain_t::
NetMain_t(void)
    : m_StartTime(GetTickCount()),
      m_ProgInstance((HINSTANCE)GetModuleHandle(NULL)),
      m_InterruptHandle(NULL),
      m_NumberWinsockStartups(0),
      m_CommandLineAssembled(false)
{
    // Initialize cmnprint.
    CmnPrint_Initialize();
	CmnPrint_RegisterPrintFunctionEx(PT_LOG,     NetLogDebug,   FALSE);
 	CmnPrint_RegisterPrintFunctionEx(PT_WARN1,   NetLogWarning, FALSE);
 	CmnPrint_RegisterPrintFunctionEx(PT_WARN2,   NetLogWarning, FALSE);
	CmnPrint_RegisterPrintFunctionEx(PT_FAIL,    NetLogError,   FALSE);
	CmnPrint_RegisterPrintFunctionEx(PT_VERBOSE, NetLogMessage, FALSE);
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
NetMain_t::
~NetMain_t(void)
{
    // Close the interrupt event.
    if (m_InterruptHandle)
        CloseHandle(m_InterruptHandle);
    
#if (CE_MAJOR_VER >= 6)
    // Clean ip cmnprint.
    CmnPrint_Cleanup();
#endif

    // Clean up winsock.
    while (m_NumberWinsockStartups-- > 0)
        CleanupWinsock();
}

// ----------------------------------------------------------------------------
//
// Reassembles and/or retrieves the original command-line.
//
extern int     g_argc;  // from netmain
extern LPTSTR *g_argv;  // from netmain
const TCHAR *
NetMain_t::
GetCommandLine(void) const
{
    if (!m_CommandLineAssembled)
    {
        for (int ax = 0 ; ax < g_argc ; ++ax)
        {
            if (g_argv[ax] && g_argv[ax][0])
            {
                if (m_CommandLine.length() > 0)
                    m_CommandLine.append(TEXT(" "));
                m_CommandLine.append(g_argv[ax]);
            }
        }
        m_CommandLineAssembled = true;
    }
    return m_CommandLine;
}

// ----------------------------------------------------------------------------
//
// Initializes the program.
// Returns ERROR_ALREADY_EXISTS if the program is already running.
//
DWORD
NetMain_t::
Init(
    IN int    argc,
    IN TCHAR *argv[])
{
    DWORD result;

    // Parse the non-netmain command-line arguments.
    // Also look for a "help" argument.
    result = DoParseCommandLine(argc, argv);
    if (NO_ERROR != result
     || WasOption(argc, argv, TEXT("?")) >= 0
     || WasOption(argc, argv, TEXT("help")) >= 0)
    {
        DoPrintUsage();
        return ERROR_INVALID_PARAMETER;
    }

    // Make sure only one instance exists.
    result = DoFindPreviousInstance();
    if (NO_ERROR != result)
    {
        // If there's already an existing instance, ask the user
        // whether they want to kill it.
        if (ERROR_ALREADY_EXISTS == result)
        {
            DoKillPreviousInstance();
        }
        else
        {
            CmnPrint(PT_FAIL, 
                     TEXT("Failed finding last instance: error=%u (0x%X:%s)"), 
                     result, result, ErrorToText(result));
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Interrupts the program by setting the termination event.
//
void
NetMain_t::
Interrupt(void)
{
    if (NULL != m_InterruptHandle)
        SetEvent(m_InterruptHandle);
}

// ----------------------------------------------------------------------------
//    
// Determines whether the program has received a termination event.
//
bool
NetMain_t::
IsInterrupted(void) const
{
    return (NULL == m_InterruptHandle)
        || (WAIT_TIMEOUT != WaitForSingleObject(m_InterruptHandle, 0L));
}

// ----------------------------------------------------------------------------
//
// Clears the termination event.
//
void
NetMain_t::
ClearInterrupt(void)
{
    if (NULL != m_InterruptHandle)
        ResetEvent(m_InterruptHandle);
}

// ----------------------------------------------------------------------------
//
// Initializes or cleans up winsock.
// Produces the appropriate error messages on failure.
//
DWORD
NetMain_t::
StartupWinsock(
    IN int MinorVersion,
    IN int MajorVersion)
{
    WSADATA wsaData;
    DWORD result = WSAStartup(MAKEWORD(MinorVersion,MajorVersion), &wsaData);
    if (NO_ERROR == result)
    {
        m_NumberWinsockStartups++;
    }
    else
    {
        CmnPrint(PT_FAIL, 
                 TEXT("Could not initialize Winsock: wsa error=%u (0x%X:%s)"), 
                 result, result, ErrorToText(result));
    }
    return result;
}

DWORD
NetMain_t::
CleanupWinsock(void)
{
    DWORD result = WSACleanup();
    if (NO_ERROR == result)
    {
        m_NumberWinsockStartups--;
    }
    else
    {
        result = WSAGetLastError();
        CmnPrint(PT_FAIL, 
                 TEXT("Could not initialize Winsock: wsa error=%u (0x%X:%s)"), 
                 result, result, ErrorToText(result));
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Displays the program's command-line arguments.
// This default implementation dumps a list of the standard netmain
// arguments.
//
extern "C" void NetMainDumpParameters(void);
void
NetMain_t::
DoPrintUsage(void) const
{
    NetMainDumpParameters();
}

// ----------------------------------------------------------------------------
//
// Parses the program's command-line arguments.
// This default implementation does nothing.
//
DWORD
NetMain_t::
DoParseCommandLine(
    IN int    argc,
    IN TCHAR *argv[])
{
    // nothing to do
    return NO_ERROR;
}
    
// ----------------------------------------------------------------------------
//
// Determines if an instance of the program is already running.
// Returns ERROR_ALREADY_EXISTS if so.
// If necessary, creates the program-termination event.
//
DWORD
NetMain_t::
DoFindPreviousInstance(void)
{
    if (NULL != m_InterruptHandle)
        CloseHandle(m_InterruptHandle);
    
    m_InterruptHandle = CreateEvent(NULL, TRUE, FALSE, gszMainProgName);
   
    DWORD result = GetLastError();
    if (NULL == m_InterruptHandle && NO_ERROR == result)
    {
        result = ERROR_INVALID_HANDLE;
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// If there's already an existing instance, asks the user whether
// they want to kill it and, if so, signals an interrupt:
// If the optional "question" string is supplied, asks that rather
// than the standard "Already running. Kill it?".
//
void
NetMain_t::
DoKillPreviousInstance(
    IN const TCHAR *pKillPreviousQuestion)
{
    if (NULL       == pKillPreviousQuestion
     || TEXT('\0') == pKillPreviousQuestion[0])
    {
        pKillPreviousQuestion = TEXT("The program is already running.")
                                TEXT(" Do you want to kill it?");
    }
    if (IDOK == MessageBox(NULL, pKillPreviousQuestion,
                           gszMainProgName, MB_OKCANCEL|MB_ICONQUESTION))
    {
        Interrupt();
    }
}

// ----------------------------------------------------------------------------
