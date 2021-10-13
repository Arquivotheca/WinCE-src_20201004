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
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@doc LIBRARY


Module Name:
    netmain.h

Abstract:
    Declaration of a helper entry point to perform common functions.

Revision History:
     3-Jan-2000		Created
	20-Jan-2000		INIT_RAS_PHONEBOOK_ENTRY flag added

-------------------------------------------------------------------*/
#ifndef _NET_MAIN_H_
#define _NET_MAIN_H_

#include <windows.h>
#include <tchar.h>
#include "netall.h"
#include "cmnwrap.h"

#ifdef __cplusplus
extern "C" {
#endif

int mymain(int argc, TCHAR* argv[]);

//
// @const LPTSTR | gszMainProgName | Name of the program using main.
//
// @comm:(LIBRARY)
//  You should fill in the name of your program with this variable, when using
//  the <f mymain> function and the NetMain library.
//
// @ex The following example shows how to use this |
//
//     LPTSTR gszMainProgName = _T("Program");
//
extern LPTSTR gszMainProgName;

// 
// @const DWORD | gdwMainInitFlags | Initializations needed by the program.
// 
// @comm:(LIBRARY)
//  You should fill in the initializations needed by your program, when using
//  the <f mymain> function and the NetMain library.  If your program does not need
//  any initializations you should fill in a 0.    <nl>
//
//  This variable is used for automation support of tests, all tests that require
//  some sort of system initialization before running should fill in that value, or
//  the test is likely to fail in the automation lab.
// 
// @ex The following example shows a program that needs a network and machine name. |
// 
//     DWORD gdwMainInitFlags = INIT_NET_CONNECTION | INIT_MACHINE_NAME;
// 
extern DWORD gdwMainInitFlags;

// 
// @const int | g_argc | Count of command-line arguments.
// 
// @comm:(LIBRARY)
//  Use this and g_argv to retrieve the filtered command-line arguments.
// 
// @ex The following example shows one way to parse command-line arguments. |
// 
//     ParseLocalArguments(g_argc, g_argv);
// 
extern int g_argc;

// 
// @const LPTSTR | g_argv | List of command-line arguments.
// 
// @comm:(LIBRARY)
//  Use this and g_argc to retrieve the filtered command-line arguments.
// 
// @ex The following example shows one way to parse command-line arguments. |
// 
//     ParseLocalArguments(g_argc, g_argv);
// 
extern LPTSTR *g_argv;

// 
// @func extern "C" void | NetMainDumpParameters |
// 
//     Library function to dump all of the valid NetMain parameters.
// 
// @comm:(LIBRARY)
//     Dumps the parameters that get used in the initial _tmain function
//     and are never passed to the program.  <nl>
//
//     Current options:  <nl>
// 
//     -mt      Use multiple logging files, one for each thread the test forks off.  <nl>
//     -z       Output test messages to the console.  <nl>
//     -fl      Output test messages to a file on the CE device.  <nl>
//     -nowatt  Specifies for the test to not output the 6 @ signs that indicate end of test.  <nl>
//     -wf[fil] Sets the specified WATT filter(s).  <nl>
//     -v       Sets the system logging to full verbosity.  <nl>
//     -v[lib]  Sets the given library's logging to full verbosity.  <nl>
//     -v[verb] Sets the system logging to the given verbosity.  <nl>
//     -v[lib][=verb] Sets the given library's logging to the given verbosity.  <nl>
//         "lib" is a library's logging name.  <nl>
//             For example, "BC" is backchannel.  <nl>
//         "verb" is a verbosity level. Options:  <nl>
//             0-15 Kato log levels:  <nl>
//               0-2   Errors  <nl>
//               3-9   Warnings  <nl>
//               10-11 Status  <nl>
//               12-15 Debug/Comments  <nl>
//             "err"  - only log errors  <nl>
//             "wrn"  - log errors and warnings  <nl>
//             "stat" - log status, error and warnings  <nl>
//             "all"  - log all messages - same as "15"      
//
void NetMainDumpParameters(void);

#ifdef __cplusplus
}
#endif

//
// @flag INIT_NET_CONNECTION | 0x1 Initialize the network connection.
//
#define INIT_NET_CONNECTION  0x1

//
// @flag INIT_MACHINE_NAME   | 0x2 Initialize the machine name of the CE device to a predictable name.
//
#define INIT_MACHINE_NAME    0x2

//
// @flag INIT_RANDOM_MACHINE_NAME   | 0x4 Initialize the machine name of the CE device to a random, unique name.
//
#define INIT_RANDOM_MACHINE_NAME    0x4

//
// @flag INIT_USER_CREDENTIALS | 0x8 Initialize the user credentials to default values.
//
#define INIT_USER_CREDENTIALS 0x8

//
// @flag INIT_MSMQ           | 0x10 Initialize MSMQ to default values.
//
#define INIT_MSMQ            0x10

//
// @flag INIT_RAS_PHONEBOOK_ENTRY | 0x20 Create a default entry in the RAS phonebook.
//
#define INIT_RAS_PHONEBOOK_ENTRY  0x20

#endif
