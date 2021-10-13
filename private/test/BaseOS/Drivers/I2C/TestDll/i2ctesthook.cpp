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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//
//          Contains the Test Procedures
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////
#include "I2CTest.h"
#include "globals.h"

////////////////////////////////////////////////////////////////////////////////
// LoadI2CTestHook
// This function loads the I2C Test Hook DLL and gets the function pointers to the DLL APIs
// 
// Parameters - 
// None
//
// Return value:
//  TRUE if operation successful, FALSE otherwise 
///////////////////////////////////////////////////////////////////////////////////
BOOL LoadI2CTestHook()
{
    BOOL bRet = TRUE;

    g_hTestHook = LoadDriver(g_strTestHook);

    if( NULL == g_hTestHook )
    {
        g_pKato->Log( LOG_DETAIL, TEXT("Failed to load I2C Test Hook DLL : %s"), g_strTestHook );
        bRet = FALSE;
        goto Exit;
    }

    //
    // load the I2C Test Hook functions
    //
    g_pfnTestHookInit = 
        (PFN_I2C_TEST_HOOK_INIT)GetProcAddress(
            g_hTestHook, TEXT("I2CTestHookInit") );

    if(NULL == g_pfnTestHookInit)
    {
        g_pKato->Log( LOG_DETAIL, TEXT("Failed to get the function I2CTestHookInit") );
        bRet = FALSE;
        goto Exit;
    }

    g_pfnTestHookDeInit = 
        (PFN_I2C_TEST_HOOK_DEINIT)GetProcAddress(
            g_hTestHook, TEXT("I2CTestHookDeInit") );

    if(NULL == g_pfnTestHookDeInit)
    {
        g_pKato->Log( LOG_DETAIL, TEXT("Failed to get the function I2CTestHookDeInit") );
        bRet = FALSE;
        goto Exit;
    }

    Exit:

    return bRet;
}

////////////////////////////////////////////////////////////////////////////////
// LoadI2CTestHook
// This function unloads the I2C Test Hook DLL 
// 
// Parameters - 
// None
//
// Return value:
//  TRUE if operation successful, FALSE otherwise 
///////////////////////////////////////////////////////////////////////////////////

VOID UnloadI2CTestHook()
{
    if(NULL != g_hTestHook)
    {
        FreeLibrary(g_hTestHook);
    }
}

////////////////////////////////////////////////////////////////////////////////
// LoadI2CTestHook
// This function calls the I2CTestHookInit() function of test hook DLL
// 
// Parameters - 
// None
//
// Return value:
//  TRUE if operation successful, FALSE otherwise 
///////////////////////////////////////////////////////////////////////////////////

BOOL ApplyTestHook()
{
    BOOL bRet = TRUE;

    if(NULL != g_pfnTestHookInit)
    {
        bRet = (*g_pfnTestHookInit)();
    }

    return bRet;
}

////////////////////////////////////////////////////////////////////////////////
// ClearI2CTestHook
// This function calls the I2CTestHookDeInit() function of test hook DLL
// 
// Parameters - 
// None
//
// Return value:
//  TRUE if operation successful, FALSE otherwise 
///////////////////////////////////////////////////////////////////////////////////

BOOL ClearTestHook()
{
    BOOL bRet = TRUE;

    if(NULL != g_pfnTestHookDeInit)
    {
        bRet = (*g_pfnTestHookDeInit)();
    }

    return bRet;
}

 

////////////////////////////////////////////////////////////////////////////////
