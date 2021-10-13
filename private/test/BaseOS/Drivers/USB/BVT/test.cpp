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
//  Module: test.cpp
//          Contains the USB BVT test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include <devload.h>
#include <ceddk.h>
#include "USBHostBVT.h"
#define gcszThisFile    "test.cpp"

//Main test function that handles all cases

TESTPROCAPI TestProc(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
	UNREFERENCED_PARAMETER(tpParam);
    DWORD retVal = TPR_FAIL;
    USB_DRIVER_SETTINGS DriverSettings;
    DWORD dwClass=USB_NO_INFO;

    // The shell doesn't necessarily want us to execute the test. Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    g_pKato->Log(LOG_COMMENT, TEXT("USB BVT Tests:"));

    //open driver handle no matter what tests need to run!
    Util_LoadUSBD();

    //Switch according to base of test (e.g., test ID 601, base=600)
	switch ((lpFTE->dwUniqueID / 10)*10)
	{
	case BVT_FUNC:
		{
			switch (lpFTE->dwUniqueID)
			{
			case BVT_FUNC:
				{
					retVal=Test_GetUSBDVersion();
					break;
				}
			case BVT_FUNC+1:
				{
					retVal=Test_RegisterClient();
					break;
				}
			case BVT_FUNC+2:
				{
					retVal=Test_RegistrySettingsforClient();
					break;
				}
			case BVT_FUNC+3:
				{
					retVal=Test_OpenRegistryForClient();
					break;
				}
			case BVT_FUNC+4:
				{
					retVal=Test_GetRegistryPath();
					break;
				}
			case BVT_FUNC+5:
				{
					retVal=Test_UnRegisterClient();
					break;
				}
			case BVT_FUNC +6:
				{
					retVal=Test_RegistrySettingsforUnloadedClient();
					break;
				}
			case BVT_FUNC+7:
				{
					retVal=Test_OpenRegistryForUnloadedClient();
					break;
				}
			case BVT_FUNC+8:
				{
					retVal=Test_GetRegistryPathForUnloadedClient();
					break;
				}
			}//end switch
		}
	case BVT_PWR:
		{
			switch (lpFTE->dwUniqueID)
			{
			case BVT_PWR:
				retVal=Test_SuspendResumeGetVersion();
				break;
			case BVT_PWR+1:
				retVal=Test_SuspendResumeRegisteredClient();
				break;
			case BVT_PWR+2:
				retVal=Test_SuspendResumeUnRegisteredClient();
				break;
			}
		}
	case BVT_STRESS:
		{
			break;
		}
	default:
		{
			break;
		}
	}

	//clean up before exit
	Util_PrepareDriverSettings( &DriverSettings,dwClass);
	Util_UnRegisterClient(gcszDriverId,&DriverSettings);
	Util_UnLoadUSBD();

	return retVal;
}//end TestProc