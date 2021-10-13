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
 * tuxIltiming.h
 */


#ifndef __TUX_ILTIMING_H__
#define __TUX_ILTIMING_H__

/**********************************  Headers  *********************************/
#include <windows.h>
#include <tux.h>


/**************************************************************************
 * 
 *                               Test Functions
 *
 **************************************************************************/

/*
 * Prints out the usage message for the iltiming tests. It tells the user 
 * what the tests do and also specifies the input that the user needs to 
 * provide to run the tests. 
 */
TESTPROCAPI UsageMessage(
                         UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE);

/*
 * Verifies if the ILTIMING Ioctl is supported by the platform and if the
 * Ioctl is able to handle the necessary inputs.
 */
TESTPROCAPI testIfIltimingIoctlSupported(
               UINT uMsg,  
               TPPARAM tpParam,
               LPFUNCTION_TABLE_ENTRY lpFTE);

/*
 * This test checks if enabling and disabling ILTIMING multiple times
 * does not cause a problem.
 */
TESTPROCAPI testEnableDisableSequence(
               UINT uMsg,  
               TPPARAM tpParam,
               LPFUNCTION_TABLE_ENTRY lpFTE);

/*
 * This test verifies calling the Ioctl with incorrect inbound and outbound parameters.
 * The Ioctl should handle them correctly. 
 */
TESTPROCAPI
testIltimingInOutParam(
       UINT uMsg,  
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE);


#endif // __TUX_ILTIMING_H__
