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
//******************************************************************************
//***** Include files
//******************************************************************************


//
// Created by:  Dollys
// Modified by:  shivss
//


#include    <windows.h>
#include    <stdio.h>
#include    <katoex.h>
#include    <usbdi.h>

#define _TRY_IGNORE_COMMENTS
#include    "USBHostBVT.h"
#include    "main.h"
#include    "globals.h"

// ----------------------------------------------------------------------------
//
// Debugger
//
// ----------------------------------------------------------------------------
#ifdef DEBUG


DBGPARAM dpCurSettings =
{
    TEXT("USB HOST BVT"),
    NULL,
    0
};

#endif  // DEBUG


DWORD  g_Suspend = SUSPEND_RESUME_DISABLED;

//******************************************************************************
//***** Test Helper Functions
//******************************************************************************
//fix me: change setting values.
//******************************************************************************
//***** Helper function Util_PrepareDriverSettings
//***** Purpose: fills up driversettings struct with driver class protocol vender etc values
//***** Params: takes class id to fill either usb_no_info for all values or
//*****            keeps incrementing subclass n protocol to create multiple different test class settings
//***** Returns: void
//******************************************************************************
void    Util_PrepareDriverSettings( LPUSB_DRIVER_SETTINGS lpDriverSettings, DWORD dwClass)
{
    static DWORD dwSubClass=0;
    static DWORD dwProtocol=0;
    lpDriverSettings->dwCount             = sizeof(USB_DRIVER_SETTINGS);

    if(USB_NO_INFO == dwClass)
    {
        lpDriverSettings->dwVendorId          = 0x045E;
        lpDriverSettings->dwProductId         = 0x930A;
        lpDriverSettings->dwReleaseNumber     = USB_NO_INFO;

        lpDriverSettings->dwDeviceClass       =    USB_NO_INFO ;
        lpDriverSettings->dwDeviceSubClass    = USB_NO_INFO;
        lpDriverSettings->dwDeviceProtocol    = USB_NO_INFO;

        lpDriverSettings->dwInterfaceClass    = USB_NO_INFO;
        lpDriverSettings->dwInterfaceSubClass = USB_NO_INFO;
        lpDriverSettings->dwInterfaceProtocol = USB_NO_INFO;
    }
    //the below is for stress testing, to generate large number of clients with different subclass and protocol values.
    else if(0xFF == dwClass)
    {
        //fix me, change the vendor and product ids below.
        lpDriverSettings->dwVendorId          = 0x045E;
        lpDriverSettings->dwProductId         = 0x930A;
        lpDriverSettings->dwReleaseNumber     = USB_NO_INFO;

        lpDriverSettings->dwDeviceClass       = 0xFF;
        lpDriverSettings->dwDeviceSubClass    = dwSubClass++;
        lpDriverSettings->dwDeviceProtocol    = dwProtocol++;

        lpDriverSettings->dwInterfaceClass    = 0xFF;
        lpDriverSettings->dwInterfaceSubClass = dwSubClass++;
        lpDriverSettings->dwInterfaceProtocol = dwProtocol++;
    }

}


//******************************************************************************
//***** Helper function Util_LoadUSBD
//***** Purpose: Loads USBD if not already loaded
//***** Params: None
//***** Returns: TRUE if successfully loaded, FALSE otherwise.
//******************************************************************************

BOOL Util_LoadUSBD()
{
    if (!g_fUSBDLoaded)
    {
        if(LoadDllGetAddress ( TEXT("USBD.DLL"),
            USBDEntryPointsText,
            (LPDLL_ENTRY_POINTS) & USBDEntryPoints ))
        {
            g_fUSBDLoaded = TRUE;
            DETAIL(IDS_TEST_USBD_LOAD);
        }
        else
        {
            ERRFAIL(IDS_TEST_USBD_LOAD);
        }
    }

    return g_fUSBDLoaded;
}
//******************************************************************************
//***** Helper function h_LoadUSBD
//***** Purpose: UnLoads USBD if not already loaded
//***** Params: None
//***** Returns: TRUE if successfully unloaded, FALSE otherwise.
//******************************************************************************
BOOL Util_UnLoadUSBD()
{
    if (g_fUSBDLoaded)
    {
        if( UnloadDll( (LPDLL_ENTRY_POINTS) & USBDEntryPoints ))
        {
            g_fUSBDLoaded = FALSE;
            DETAIL(IDS_TEST_USBD_UNLOAD);
        }
        else
        {
            ERRFAIL(IDS_TEST_USBD_UNLOAD);
        }
    }
    return !g_fUSBDLoaded;

}

//******************************************************************************
//***** Helper function Util_VerifyStrValueRegistry
//***** Purpose: Reads REG_SZ value from any registry path under HKLM
//***** Params: Registry path, Reg name to read, reg value to match it to
//***** Returns: TRUE if successfully matched, FALSE otherwise.
//******************************************************************************
DWORD Util_VerifyStrValueRegistry(TCHAR *pszRegKey,TCHAR *pszRegValueName,TCHAR *pszValue)
{
    DWORD dwStatus = TPR_FAIL,cbValue,type;
    TCHAR pszRegValue[MAX_PATH];
    HKEY hKey = NULL;
    if(!pszRegKey || !pszRegValueName || !pszValue)
        goto endVerifyStrValueRegistry;

	dwStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pszRegKey, 0, NULL, &hKey);
    if(ERROR_SUCCESS != dwStatus || NULL == hKey)
	{
        dwStatus = TPR_FAIL;
		DETAIL(IDS_TEST_OPEN_REGISTRY);
        goto endVerifyStrValueRegistry;
    }

    // query the value
    Log(TEXT(IDS_READ_REGISTRY_VALUE),pszRegValueName, pszRegKey);
    type = REG_SZ;
    cbValue = MAX_PATH-1;
    dwStatus = RegQueryValueEx(hKey, pszRegValueName, NULL, &type, (BYTE*)pszRegValue, &cbValue);
    if(ERROR_SUCCESS != dwStatus)
	{
        dwStatus = TPR_FAIL;
        DETAIL(IDS_TEST_QUERY_REGISTRY);
        goto endVerifyStrValueRegistry;
    }

	//match value
    Log(TEXT(IDS_MATCH_REGISTRY_VALUE), pszRegValue, pszValue);
    if(0 != _tcscmp(pszRegValue, pszValue))
    {
        DETAIL(IDS_TEST_VERIFY_REGISTRY);
        dwStatus=TPR_FAIL;
    }
    else
    {
        DETAIL(IDS_TEST_VERIFY_REGISTRY);
        dwStatus=TPR_PASS;
    }

endVerifyStrValueRegistry:
    if(hKey)
        RegCloseKey(hKey);

    return dwStatus;
}

//******************************************************************************
//***** Helper function Util_OpenKey
//***** Purpose: Opens client driver registry key using USBD OpenClientRegistryKey function
//***** Params: Client Driver Id
//***** Returns: Reg Key if successfully Opened, NULL otherwise.
//******************************************************************************
HKEY Util_OpenKey(TCHAR *pszDriverId)
{
    HKEY hkDriverKey=NULL;
    hkDriverKey = ((*USBDEntryPoints.lpOpenClientRegistryKey)(pszDriverId));
    if(NULL != hkDriverKey)
    {
        Log( TEXT(IDS_TEST_OPEN_REGISTRY));
    }
    return hkDriverKey;
}

//******************************************************************************
//***** Helper function Util_GetUSBDVersion()
//***** Purpose: Gets USBD version info using USBD functions
//***** Params: none
//***** Returns: TRUE if succesful, false otherwise
//******************************************************************************
DWORD Util_GetUSBDVersion()
{
    DWORD dwMajorVersion = 0;
    DWORD dwMinorVersion = 0;
    DWORD dwStatus = TPR_FAIL;

	// USBD version info
	(*USBDEntryPoints.lpGetUSBDVersion)(&dwMajorVersion,&dwMinorVersion);

	if((dwMajorVersion && dwMinorVersion))
	{
		Log( TEXT(IDS_TEST_GET_USBD_VERSION_STR),
			dwMajorVersion,dwMinorVersion);
		dwStatus = TPR_PASS;
	}
	else
	{
		Fail( TEXT(IDS_TEST_GET_USBD_VERSION));
		dwStatus = TPR_FAIL;
	}

	return dwStatus;
}
//******************************************************************************
//***** Helper function Util_RegisterClient
//***** Purpose: Registers client driver using USBD functions
//***** Params: Client Driver Id, Driver Lib File, Settings
//***** Returns: DWORD return value from USBD driver
//******************************************************************************
 DWORD Util_RegisterClient(TCHAR *pszDriverId, LPCWSTR szDriverLibFile, LPUSB_DRIVER_SETTINGS lpDriverSettings)
 {
    DWORD fRet = TRUE,fRetTemp;
    if(!g_fUSBHostClientRegistered)
    {
        if((*USBDEntryPoints.lpRegisterClientDriverID)(pszDriverId))
        {
            Log( TEXT(IDS_TEST_REGISTER_CLIENT),
                pszDriverId
                );

            fRetTemp = (*USBDEntryPoints.lpRegisterClientSettings)(szDriverLibFile,\
                    pszDriverId, NULL, lpDriverSettings);
            if(!fRetTemp)
            {
                Fail( TEXT(IDS_TEST_REGISTER_CLIENT_SETTINGS),
                    pszDriverId
                    );
                (*USBDEntryPoints.lpUnRegisterClientDriverID)(pszDriverId);
                g_fUSBHostClientRegistered=FALSE;
                fRet = FALSE;
            }
            else
            {
            Log( TEXT(IDS_TEST_REGISTER_CLIENT_SETTINGS),
                pszDriverId
                );
            g_fUSBHostClientRegistered=TRUE;
            fRet = TRUE;
            }
        }
        else
        {
        Fail( TEXT(IDS_TEST_REGISTER_CLIENT),
            pszDriverId
            );
        g_fUSBHostClientRegistered=FALSE;
        fRet = FALSE;
        }

    }
    return fRet;

 }

 //******************************************************************************
//***** Helper function Util_UnRegisterClient
//***** Purpose: UnRegisters client driver using USBD functions
//***** Params: Client Driver Id, Settings
//***** Returns: DWORD return value from USBD driver
//******************************************************************************
 DWORD Util_UnRegisterClient(TCHAR *pszDriverId, LPUSB_DRIVER_SETTINGS lpDriverSettings)
 {

    DWORD fRet=TRUE,fRetTemp;
    if(g_fUSBHostClientRegistered)
    {

        if(!((*USBDEntryPoints.lpUnRegisterClientSettings)(pszDriverId, NULL, lpDriverSettings)))
            {
                Fail( TEXT(IDS_TEST_UNREGISTER_CLIENT_SETTINGS),
                     pszDriverId
                    );
            }
        else
            {
            Log( TEXT(IDS_TEST_UNREGISTER_CLIENT_SETTINGS),
                 pszDriverId
                );
             fRetTemp = (*USBDEntryPoints.lpUnRegisterClientDriverID)(pszDriverId);
             if(fRetTemp)
             {
                Log( TEXT(IDS_TEST_UNREGISTER_CLIENT), pszDriverId);
                g_fUSBHostClientRegistered=FALSE;
                fRet = TRUE;
              }
            else
            {
                Fail( TEXT(IDS_TEST_UNREGISTER_CLIENT), pszDriverId);
                fRet = FALSE;
                g_fUSBHostClientRegistered=TRUE;
            }
        }
    }
    return fRet;
 }

//******************************************************************************
//***** Helper Function Util_RegistryPathTest
//***** Purpose: Gets the USB driver Registry Path using USBD function GetClientRegistryPath
//***** Params: DriverId
//***** Returns: TRUE if successfull, FALSE otherwise
//******************************************************************************
DWORD Util_RegistryPathTest(TCHAR *pszDriverId)
{
    TCHAR szRegistryPath[MAX_PATH];
    DWORD dwStatus = TPR_FAIL,dwRet;
	HKEY hKey = NULL;
    if(Util_LoadUSBD())
    {
        if((*USBDEntryPoints.lpGetClientRegistryPath)(szRegistryPath,MAX_PATH,pszDriverId))
        {
			if(!szRegistryPath)
			{
				goto endRegistryPathTest;
			}

			//also make sure its a valid registry, try opening it.
			dwRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szRegistryPath, 0, NULL, &hKey);
			if(ERROR_SUCCESS != dwRet || NULL == hKey) {
				dwStatus = TPR_FAIL;
				Log( TEXT(IDS_TEST_OPEN_REGISTRY));
				goto endRegistryPathTest;
			}
			else
			{
				Log( TEXT(IDS_TEST_READ_REGISTRY_PATH_STR),
                szRegistryPath
                );
				dwStatus = TPR_PASS;
			}
        }
        else
        {
            Log( TEXT(IDS_TEST_READ_REGISTRY_PATH));
            dwStatus = TPR_FAIL;
        }
    }

endRegistryPathTest:
	if(hKey)
		RegCloseKey(hKey);
	return dwStatus;
}

//******************************************************************************
//***** Test Functions return TPR_PASS/TPR_FAIL
//******************************************************************************

#if 0

//******************************************************************************
//***** Test Function Test_ClientRegistryKey
//***** Purpose: Tests the USB driver Registry Settings by using Util_OpenKey Helper func
//***** Params: DriverId, Driver Settings
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_ClientRegistryKey(TCHAR *pszDriverId,LPUSB_DRIVER_SETTINGS lpDriverSettings)
{
   DWORD dwRet = TPR_FAIL;
   DWORD dwStatus = TPR_FAIL;
   HKEY hkDriverKey;
   hkDriverKey = Util_OpenKey(pszDriverId);
   if (hkDriverKey)
   {
	   dwRet = RegCloseKey(hkDriverKey);
	   if (ERROR_SUCCESS == dwRet)
	   {
		   Log(TEXT(IDS_TEST_CLOSE_REGISTRY));
		   dwStatus = TPR_PASS;
	   }
	   else
	   {
		   ERRFAIL(IDS_TEST_CLOSE_REGISTRY);
		   dwStatus = TPR_FAIL;
	   }
   }
   else
   {
	   ERRFAIL(IDS_TEST_OPEN_REGISTRY);
	   dwStatus = TPR_FAIL;
   }
   if(hkDriverKey)
   {
	   RegCloseKey(hkDriverKey);
   }

   return dwStatus;
}

#endif

//******************************************************************************
//***** Test Function Test_GetUSBDVersion
//***** Purpose: Gets the USB driver version using helper function
//***** Params: None
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_GetUSBDVersion()
{
    DWORD dwStatus=TPR_FAIL;

    if( Util_LoadUSBD() )
    {
        if( Util_GetUSBDVersion() )
        {
            dwStatus = TPR_PASS;
        }
        else
        {
            ERRFAIL(IDS_TEST_GET_USBD_VERSION);
            dwStatus = TPR_FAIL;
        }
    }

	return dwStatus;
}

//******************************************************************************
//***** Test Function Test_RegisterClient
//***** Purpose: Registers a client driver using helper func
//***** Params: none
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_RegisterClient()
{
    DWORD dwRet = TPR_FAIL;
	DWORD dwStatus = TPR_FAIL;

    if( Util_LoadUSBD())
    {
        USB_DRIVER_SETTINGS DriverSettings;
        DWORD dwClass = USB_NO_INFO;
        Util_PrepareDriverSettings(&DriverSettings,dwClass);
        dwRet = Util_RegisterClient(gcszDriverId, gcszDriverName,&DriverSettings);
    }
    dwStatus=(dwRet?TPR_PASS:TPR_FAIL);

    return dwStatus;
}

//******************************************************************************
//***** Test Function Test_UnRegisterClient
//***** Purpose: UnRegisters a client driver using helper func
//***** Params: none
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_UnRegisterClient()
{
    DWORD dwRet = TPR_FAIL;
	DWORD dwStatus = TPR_FAIL;

    if( Util_LoadUSBD() )
    {
        USB_DRIVER_SETTINGS DriverSettings;
        DWORD dwClass = USB_NO_INFO;
        Util_PrepareDriverSettings( &DriverSettings,dwClass);

		// first make sure client driver is registered before unregistering it
        if( Util_RegisterClient(gcszDriverId, gcszDriverName,&DriverSettings) )
        {
            dwRet = Util_UnRegisterClient(gcszDriverId,&DriverSettings);
            dwStatus = (dwRet?TPR_PASS:TPR_FAIL);
        }
    }

    return dwStatus;
}
//******************************************************************************
//***** Test Function Test_RegistrySettingsforClient
//***** Purpose: Tests Registry of a client driver using helper func
//***** Params: none
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_RegistrySettingsforClient()
{
    DWORD dwRet = TPR_FAIL;
	// DWORD dwStatus = TPR_FAIL;

    if( Util_LoadUSBD() )
    {
        USB_DRIVER_SETTINGS DriverSettings;
        DWORD dwClass = USB_NO_INFO;
        Util_PrepareDriverSettings( &DriverSettings,dwClass);
        if( Util_RegisterClient(gcszDriverId, gcszDriverName,&DriverSettings) )
        {
            LPTSTR pszRegKey,pszRegValueName,pszRegValue;
            pszRegKey = new TCHAR[MAX_PATH];
            pszRegValueName = new TCHAR[MAX_PATH] ;
            pszRegValue = new TCHAR[MAX_PATH];

			//check for entry under \\Drivers\\USB\\LoadClients
            StringCchPrintf(pszRegKey, MAX_PATH, TEXT(TEST_REG_PATH));
            StringCchPrintf(pszRegValueName, MAX_PATH, TEXT(TEST_REG_NAME));
            StringCchPrintf(pszRegValue, MAX_PATH, TEXT(TEST_REG_VALUE));
            dwRet = Util_VerifyStrValueRegistry(pszRegKey,pszRegValueName,pszRegValue);
        }
    }

	return dwRet;
}

//******************************************************************************
//***** Test Function Test_RegistryForUnloadedClient
//***** Purpose: Tests Registry of a unloaded/non existing client driver using helper func
//***** Params: none
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_RegistrySettingsforUnloadedClient()
{
    DWORD dwRet = TPR_FAIL;
    DWORD dwStatus = TPR_FAIL;

	if(Util_LoadUSBD())
    {
        USB_DRIVER_SETTINGS DriverSettings;
        DWORD dwClass = USB_NO_INFO;
        Util_PrepareDriverSettings( &DriverSettings,dwClass);

		// first make sure client driver is registered before unregistering it
        if( Util_RegisterClient(gcszDriverId, gcszDriverName,&DriverSettings) )
        {
			if( Util_UnRegisterClient(gcszDriverId,&DriverSettings) )
			{
				// now check to see if registry settings still remain
				LPTSTR pszRegKey,pszRegValueName,pszRegValue;
				pszRegKey = new TCHAR[MAX_PATH];
				pszRegValueName = new TCHAR[MAX_PATH] ;
				pszRegValue = new TCHAR[MAX_PATH];
				StringCchPrintf(pszRegKey, MAX_PATH, TEXT(TEST_REG_PATH));
				StringCchPrintf(pszRegValueName, MAX_PATH, TEXT(TEST_REG_NAME));
				StringCchPrintf(pszRegValue, MAX_PATH, TEXT(TEST_REG_VALUE));
				dwRet = Util_VerifyStrValueRegistry(pszRegKey,pszRegValueName,pszRegValue);

				//should fail any read on the registry settings for unloaded client
				if(TPR_PASS == dwRet)
				{
					dwStatus = TPR_FAIL;
				}
				else
				{
					dwStatus = TPR_PASS;
				}
			}
		}
    }

	return dwStatus;
}
//******************************************************************************
//***** Test Function Test_OpenRegistryForClient
//***** Purpose: Tests Opening Registry of client driver using helper func
//***** Params: none
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_OpenRegistryForClient()
{
    DWORD dwRet = TPR_FAIL;
    HKEY hKey = NULL;

    if( Util_LoadUSBD() )
    {
        USB_DRIVER_SETTINGS DriverSettings;
        DWORD dwClass = USB_NO_INFO;
        Util_PrepareDriverSettings( &DriverSettings,dwClass);
        if( Util_RegisterClient(gcszDriverId, gcszDriverName,&DriverSettings) )
        {
            hKey = Util_OpenKey(gcszDriverId);
            dwRet = (NULL == hKey)?TPR_FAIL:TPR_PASS;
        }
    }
    if(hKey)
    {
        RegCloseKey(hKey);
    }
return dwRet;
}

//******************************************************************************
//***** Test Function Test_OpenRegistryForUnloadedClient
//***** Purpose: Tests Registry of a unloaded/non existing client driver using helper func
//***** Params: none
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_OpenRegistryForUnloadedClient()
{
    DWORD dwRet = TPR_FAIL;
    HKEY hkey = NULL;

    if(Util_LoadUSBD())
    {
        USB_DRIVER_SETTINGS DriverSettings;
        DWORD dwClass = USB_NO_INFO;
        Util_PrepareDriverSettings( &DriverSettings,dwClass);

		// first make sure client driver is registered before unregistering it
        if( Util_RegisterClient(gcszDriverId, gcszDriverName,&DriverSettings) )
        {
			if( Util_UnRegisterClient(gcszDriverId,&DriverSettings) )
			{
				// should fail gracefully registry tests once the client has been unregistered.
				hkey = Util_OpenKey(gcszDriverId);
				if(NULL == hkey)
				{
					dwRet = TPR_PASS;
				}
			}
		}
	}

	if(hkey)
    {
        RegCloseKey(hkey);
    }

	return dwRet;
}

//******************************************************************************
//***** Test Function Test_GetRegistryPath
//***** Purpose: Tests Opening Registry of client driver using helper func
//***** Params: none
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_GetRegistryPath()
{
    DWORD dwRet = TPR_FAIL;

    if( Util_LoadUSBD() )
    {
        USB_DRIVER_SETTINGS DriverSettings;
        DWORD dwClass=USB_NO_INFO;
        Util_PrepareDriverSettings( &DriverSettings,dwClass);

		// first register client
        if(Util_RegisterClient(gcszDriverId, gcszDriverName,&DriverSettings))
        {
            // now try to check registry path
			dwRet = Util_RegistryPathTest(gcszDriverId);
        }

    }

	return dwRet;
}

//******************************************************************************
//***** Test Function Test_GetRegistryPathForUnloadedClient
//***** Purpose: Tests Registry of a unloaded/non existing client driver using helper func
//***** Params: none
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_GetRegistryPathForUnloadedClient()
{
    DWORD dwRet = TPR_FAIL;
	DWORD dwStatus = TPR_FAIL;

    if( Util_LoadUSBD() )
    {
        USB_DRIVER_SETTINGS DriverSettings;
        DWORD dwClass = USB_NO_INFO;
        Util_PrepareDriverSettings( &DriverSettings,dwClass);

		// first make sure client driver is registered before unregistering it
        if( Util_RegisterClient(gcszDriverId, gcszDriverName,&DriverSettings) )
        {
			if( Util_UnRegisterClient(gcszDriverId,&DriverSettings) )
			{
				// now try to check registry path. Should not be there.
				dwRet = Util_RegistryPathTest(gcszDriverId);
				dwStatus = (TPR_PASS == dwRet)?TPR_FAIL:TPR_PASS;
				if(TPR_PASS != dwStatus)
				{
					Log(TEXT(IDS_SHOULD_FAIL_UNREGISTERED_CLIENT));
				}
			}
		}
    }
    return dwStatus;
}
//******************************************************************************
//***** Test Function Test_SuspendResumeGetVersion
//***** Purpose: Tests VERSION post suspend resume
//***** Params: none
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_SuspendResumeGetVersion()
{

      if(g_Suspend == SUSPEND_RESUME_DISABLED )
   {
   	  Log(TEXT("Suspend /Resume Not Enabled through Command Line"));
         return TPR_SKIP;
   }



    DWORD dwRet = TPR_FAIL;
	DWORD dwStatus = TPR_FAIL;

    if( Util_LoadUSBD() )
    {
        // first get USBD version info
		if( Util_GetUSBDVersion() == TPR_PASS )
		{
			// try suspend and resume
			if( Util_SuspendAndResume() == ERROR_SUCCESS )
			{
				// try to get USBD version again
				dwRet = Util_GetUSBDVersion();
				dwStatus = (TPR_PASS==dwRet)?TPR_PASS:TPR_FAIL;
			}
			else
			{
				// suspend resume wasn't supported so skip test
				Log( TEXT(IDS_SKIP_SUSPEND_RESUME_VERSION));
				dwStatus = TPR_SKIP;
			}
		}
	}

    return dwStatus;
}
//******************************************************************************
//***** Test Function Test_SuspendResumeRegisteredClient
//***** Purpose: Tests registered client settings post suspend resume
//***** Params: none
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_SuspendResumeRegisteredClient()
{

   if(g_Suspend == SUSPEND_RESUME_DISABLED)
   {
   	  Log(TEXT("Suspend /Resume Not Enabled through Command Line"));
         return TPR_SKIP;
   }
    DWORD dwRet = TPR_FAIL;
	DWORD dwStatus = TPR_FAIL;
    HKEY hKey = NULL;
	USB_DRIVER_SETTINGS DriverSettings;

	if( Util_LoadUSBD() )
    {

        DWORD dwClass = USB_NO_INFO;
        Util_PrepareDriverSettings( &DriverSettings,dwClass);

		// register the client
		if( Util_RegisterClient(gcszDriverId, gcszDriverName,&DriverSettings) )
		{
			// try suspend resume
			if( Util_SuspendAndResume() == ERROR_SUCCESS )
			{
				// now check if registry psth vslues were preserved
				LPTSTR pszRegKey,pszRegValueName,pszRegValue;
				pszRegKey = new TCHAR[MAX_PATH];
				pszRegValueName = new TCHAR[MAX_PATH] ;
				pszRegValue = new TCHAR[MAX_PATH];
				StringCchPrintf(pszRegKey, MAX_PATH, TEXT(TEST_REG_PATH));
				StringCchPrintf(pszRegValueName, MAX_PATH, TEXT(TEST_REG_NAME));
				StringCchPrintf(pszRegValue, MAX_PATH, TEXT(TEST_REG_VALUE));
				dwRet = Util_VerifyStrValueRegistry(pszRegKey,pszRegValueName,pszRegValue);

				// registry path values not preserved so fail the test
				if(TPR_FAIL == dwRet)
				{
					dwStatus = TPR_FAIL;
					Log(TEXT(IDS_SHOULD_PASS_REGISTERED_CLIENT));
					goto endTestSuspendResumeRegisteredClient;
				}

				// check if USB driver registry path was preserved
				dwRet = Util_RegistryPathTest(gcszDriverId);

				// driver registry path was not preserved so fail the test
				if(TPR_FAIL == dwRet)
				{
					dwStatus = TPR_FAIL;
					Log(TEXT(IDS_SHOULD_PASS_REGISTERED_CLIENT));
					goto endTestSuspendResumeRegisteredClient;
				}

				// try to open the client driver registry key
				hKey = Util_OpenKey(gcszDriverId);

				// couldn't open the key so fail the test
				if(NULL == hKey)
				{
					dwStatus = TPR_FAIL;
					Log(TEXT(IDS_SHOULD_PASS_REGISTERED_CLIENT));
					goto endTestSuspendResumeRegisteredClient;
				}
				// key opened so pass the test
				else
				{
					dwStatus = TPR_PASS;
				}
			}
			// suspend resume wasn't supported so skip test
			else
			{
				Log( TEXT(IDS_SKIP_SUSPEND_RESUME_REGISTERED_CLIENT));
			}
		}
	}

endTestSuspendResumeRegisteredClient:
    if(hKey)
    {
        RegCloseKey(hKey);
    }

	Util_UnRegisterClient(gcszDriverId,&DriverSettings);

    return dwStatus;
}
//******************************************************************************
//***** Test Function Test_SuspendResumeUnRegisteredClient
//***** Purpose: Tests unregistered client settings post suspend resume
//***** Params: none
//***** Returns: TPR_PASS if successfull, TPR_FAIL otherwise
//******************************************************************************
DWORD Test_SuspendResumeUnRegisteredClient()
{

   if(g_Suspend == SUSPEND_RESUME_DISABLED)
  {
   	  Log(TEXT("Suspend /Resume Not Enabled through Command Line"));
         return TPR_SKIP;
   }

    DWORD dwStatus = TPR_FAIL;
    DWORD dwRet = TPR_FAIL;
    HKEY hKey = NULL;
	USB_DRIVER_SETTINGS DriverSettings;

    if(Util_LoadUSBD())
    {
        DWORD dwClass = USB_NO_INFO;
        Util_PrepareDriverSettings( &DriverSettings,dwClass);

		// first make sure client driver is registered before unregistering it
        if( Util_RegisterClient(gcszDriverId, gcszDriverName,&DriverSettings) )
        {
			//unregister the client
			if( Util_UnRegisterClient(gcszDriverId,&DriverSettings))
			{
				// try suspend resume
				if( Util_SuspendAndResume() == ERROR_SUCCESS )
				{
					// now check if registry psth values were preserved
					LPTSTR pszRegKey,pszRegValueName,pszRegValue;
					pszRegKey = new TCHAR[MAX_PATH];
					pszRegValueName = new TCHAR[MAX_PATH] ;
					pszRegValue = new TCHAR[MAX_PATH];
					StringCchPrintf(pszRegKey, MAX_PATH, TEXT(TEST_REG_PATH));
					StringCchPrintf(pszRegValueName, MAX_PATH, TEXT(TEST_REG_NAME));
					StringCchPrintf(pszRegValue, MAX_PATH, TEXT(TEST_REG_VALUE));
					dwRet = Util_VerifyStrValueRegistry(pszRegKey,pszRegValueName,pszRegValue);

					// registry path values for unregistered client were preserved: fail the test
					if(TPR_PASS == dwRet)
					{
						dwStatus = TPR_FAIL;
						Log(TEXT(IDS_SHOULD_FAIL_UNREGISTERED_CLIENT));
						goto endTestSuspendResumeUnRegisteredClient;
					}

					// check if USB driver registry path was preserved
					dwRet = Util_RegistryPathTest(gcszDriverId);

					// unregistered USB driver registry path was preserved: fail the test
					if(TPR_PASS == dwRet)
					{
						dwStatus = TPR_FAIL;
						Log(TEXT(IDS_SHOULD_FAIL_UNREGISTERED_CLIENT));
						goto endTestSuspendResumeUnRegisteredClient;
					}

					// try to open the client driver registry key
					hKey = Util_OpenKey(gcszDriverId);

					// couldn't open the key for unregisterd driver: pass the test
					if(NULL==hKey)
					{
						dwStatus=TPR_PASS;
					}
					else
					{
						// key opened for unregistered driver: fail the test
						dwStatus = TPR_FAIL;
						Log(TEXT(IDS_SHOULD_FAIL_UNREGISTERED_CLIENT));
						goto endTestSuspendResumeUnRegisteredClient;
					}
				}

				// suspend resume wasn't supported so skip test
				else
				{
					Log( TEXT(IDS_SKIP_SUSPEND_RESUME_UNREGISTERED_CLIENT));
				}
			}
		}
	}

endTestSuspendResumeUnRegisteredClient:
    if(hKey)
    {
        RegCloseKey(hKey);
    }
    return dwStatus;
}
