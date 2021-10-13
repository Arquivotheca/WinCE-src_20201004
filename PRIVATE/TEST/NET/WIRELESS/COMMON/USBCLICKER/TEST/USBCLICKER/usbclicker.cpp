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
// This program provides a run-time interface for the USB clicker facility.
//
// ----------------------------------------------------------------------------

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <winsock2.h>
#include <windows.h>
#include <netmain.h>
#include <cmnprint.h>

#include <USBClicker_t.hpp>
#include <WiFUtils.hpp>

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Run-time arguments:
//

// Configuration registry key:
static TCHAR *DefaultRegistryKey = NULL;
static TCHAR *    s_pRegistryKey = DefaultRegistryKey;

// Board type and number:
static int DefaultBoardType = (int)USBClicker_t::NormalClickerType;
static int      s_BoardType = DefaultBoardType;
static int DefaultBoardNumber = 1;
static int      s_BoardNumber = DefaultBoardNumber;

// Comm port-number and speed:
static int DefaultCommPort = 1;
static int      s_CommPort = DefaultCommPort;
static int DefaultCommSpeed = 9600;
static int      s_CommSpeed = DefaultCommSpeed;

// Remote APControl server host-name and port-number:
static TCHAR *DefaultServerHost = NULL;
static TCHAR *    s_pServerHost = DefaultServerHost;
static TCHAR *DefaultServerPort = TEXT("33331");
static TCHAR *    s_pServerPort = DefaultServerPort;

// ----------------------------------------------------------------------------
//
// External variables for netmain:
//
WCHAR *gszMainProgName = L"USBClicker";
DWORD  gdwMainInitFlags = 0;

// ----------------------------------------------------------------------------
//
// Prints the program usage.
//
void
PrintUsage()
{
	LogAlways(TEXT("++USB \"Clicker\" program++"));
	LogAlways(TEXT("%ls [options] -o operation"), gszMainProgName);
	LogAlways(TEXT("options:"));
    LogAlways(TEXT("   -k   configuration registry key (default none)"));
    LogAlways(TEXT("   -t   board type (0 or 1) (default %d)"), DefaultBoardType);
    LogAlways(TEXT("   -b   board number (default %d)"), DefaultBoardNumber);
    LogAlways(TEXT("   -c   COM port number (default %d)"), DefaultCommPort);
    LogAlways(TEXT("   -r   COM baud-rate (default %d)"), DefaultCommSpeed);
    LogAlways(TEXT("   -S   name/address of APControl server (default none"));
    LogAlways(TEXT("   -p   server's port-number (default: %s)"), DefaultServerPort);
    LogAlways(TEXT("   -o   operation - one of:"));
    LogAlways(TEXT("          insert"));
    LogAlways(TEXT("          remove"));
    LogAlways(TEXT("   -?   show this information"));
}

// ----------------------------------------------------------------------------
//
int 
mymain(int argc, WCHAR **argv) 
{
    bool wsaInitialized = false;
    USBClicker_t clicker;
    TCHAR *pOperation = NULL;
    DWORD result;
    
    CmnPrint_Initialize();
	CmnPrint_RegisterPrintFunctionEx(PT_LOG,     NetLogDebug,   FALSE);
 	CmnPrint_RegisterPrintFunctionEx(PT_WARN1,   NetLogWarning, FALSE);
 	CmnPrint_RegisterPrintFunctionEx(PT_WARN2,   NetLogWarning, FALSE);
	CmnPrint_RegisterPrintFunctionEx(PT_FAIL,    NetLogError,   FALSE);
	CmnPrint_RegisterPrintFunctionEx(PT_VERBOSE, NetLogMessage, FALSE);

	// Parse the run-time arguments.    
	if (QAWasOption(TEXT("?")) || QAWasOption(TEXT("help")))
	{
		PrintUsage();
		return 0;
	}
    QAGetOptionAsInt(TEXT("b"), &s_BoardNumber);
    QAGetOptionAsInt(TEXT("c"), &s_CommPort);
    QAGetOption     (TEXT("k"), &s_pRegistryKey);
    QAGetOption     (TEXT("o"), &pOperation);
    QAGetOption     (TEXT("p"), &s_pServerPort);
    QAGetOptionAsInt(TEXT("r"), &s_CommSpeed);
    QAGetOption     (TEXT("s"), &s_pServerHost);
    QAGetOptionAsInt(TEXT("t"), &s_BoardType);

    // If there was a registry key, use it to load the configuration.
    if (NULL != s_pRegistryKey && TEXT('\0') != s_pRegistryKey[0])
    {
        result = clicker.LoadFromRegistry(s_pRegistryKey);
        if (ERROR_SUCCESS != result)
        {
            LogError(TEXT("Can't load USB Clicker from registry \"%s\""),
                     s_pRegistryKey);
            goto Cleanup;
        }
    }

    // Otherwise, configure the board from the command-line arguments.
    else
    {
        clicker.SetBoardType((USBClicker_t::ClickerType_e)s_BoardType);
        clicker.SetBoardNumber(s_BoardNumber);
        clicker.SetCommPort(s_CommPort);
        clicker.SetCommSpeed(s_CommSpeed);
        if (NULL != s_pServerHost && TEXT('\0') != s_pServerHost[0])
        {
            result = clicker.SetRemoteServer(s_pServerHost, s_pServerPort);
            if (ERROR_SUCCESS != result)
            {
                LogError(TEXT("Can't set remote APControl server to \"%s:%s\""),
                         s_pServerHost, s_pServerPort);
                goto Cleanup;
            }
        }
    }

    // If necessary, initialize WinSock.
    if (NULL != clicker.GetServerHost())
    {
        WSADATA wsaData;
	    result = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (0 != result)
        {
            LogError(TEXT("WSAStartup failed: %s"), Win32ErrorText(result));
            goto Cleanup;
        }
        wsaInitialized = true;
    }

    // Perform the requested operation.
    if (NULL == pOperation)
    {
        LogError(TEXT("Missing operation"));
        PrintUsage();
        result = ERROR_INVALID_PARAMETER;
        goto Cleanup;
    }
    else
    if (_tcsicmp(pOperation, TEXT("insert")) == 0)
    {
        result = clicker.Insert();
    }
    else
    if (_tcsicmp(pOperation, TEXT("remove")) == 0)
    {
        result = clicker.Remove();
    }
    else
    {
        LogError(TEXT("Operation \"%s\" is unrecognized"), pOperation);
        PrintUsage();
        result = ERROR_INVALID_PARAMETER;
        goto Cleanup;
    }

    if (ERROR_SUCCESS == result)
         LogDebug(TEXT("Operation succeeded"));
    else LogDebug(TEXT("Operation failed: %d"), Win32ErrorText(result));

Cleanup:
    
    // If necessary, shut down WinSock.
    if (wsaInitialized)
    {
	    WSACleanup();
    }
    
	return (ERROR_SUCCESS == result)? 0 : -1;
}

// ----------------------------------------------------------------------------
