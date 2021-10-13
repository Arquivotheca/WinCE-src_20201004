/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@doc LIBRARY

Copyright (c) 1999-2000 Microsoft Corporation

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
// @const extern "C" LPTSTR | gszMainProgName | Name of the program using main.
//
// @comm:(LIBRARY)
//  You should fill in the name of your program with this variable, when using
//  the <f mymain> function and the NetMain library.
//
// @ex The following example shows how to use this |
//
// extern "C" LPTSTR gszMainProgName = _T("Program");
//
extern LPTSTR gszMainProgName;

// 
// @const extern "C" DWORD | gdwMainInitFlags | Initializations needed by the program.
// 
// @comm:(LIBRARY)
//  You should fill in the initializations needed by your program, when using
//  the <f mymain> function and the NetMain library.  If your program does not need
//  any initializations you should fill in a 0.  
//
//  This variable is used for automation support of tests, all tests that require
//  some sort of system initialization before running should fill in that value, or
//  the test is likely to fail in the automation lab.
// 
// @ex The following example shows a program that needs a network and machine name. |
// 
// extern "C" DWORD gdwMainInitFlags = INIT_NET_CONNECTION | INIT_MACHINE_NAME;
// 
extern DWORD gdwMainInitFlags;


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