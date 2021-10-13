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
//
// bvt.cpp:  BVT test functions for the Touch PQD driver
//
// Test Functions: TouchVerifyRegistryTest()
//                 TchProxyVerifyRegistryTest()
//                 TchCalVerifyRegistryTest()
//                 TouchVerifyDriversLoadedTest()
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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------

#include "..\globals.h"
#include "..\winmain.h"

#define __FILE_NAME__   TEXT( "BVT.CPP" )  //This filename for kato logs
#define nPREFIXLEN 3
#define nDFTBUFLEN 50
#define TOUCH_DRIVER_GUID TEXT( "{25121442-08CA-48dd-91CC-BFC9F790027C}" )
#define POWER_MNGR_GUID   TEXT( "{A32942B7-920C-486b-B0E6-92A702A99B35}" )

static BOOL GetTouchProxyDriverName( WCHAR *, int ); 

extern CKato*                                   g_pKato;
//
// TouchVerifyRegistryTest:  Verfies mandatory touch driver registry elements are loaded
//
TESTPROCAPI TouchVerifyRegistryTest( 
            UINT                   uMsg, 
            TPPARAM                tpParam, 
            LPFUNCTION_TABLE_ENTRY lpFTE )
{
	INT TP_Status = TPR_FAIL;

	LONG   lStatus;
	HKEY   hKey;
	WCHAR  wszPrefix[ nPREFIXLEN + 1 ];
	WCHAR  wszBuffer[ nDFTBUFLEN + 1 ];
	WCHAR *pwszBuffer = NULL, *pwszEntry;
	DWORD  dwType;
	DWORD  dwValueLen;
	WCHAR *pwszGuids[] = { TOUCH_DRIVER_GUID, POWER_MNGR_GUID, NULL };

	int n, nError;

	if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

	NKDbgPrintfW(L"g_pKato = 0x%x\r\n", g_pKato);
    
	// Get handle for registry key [HKEY_LOCAL_MACHINE\Drivers\BuiltIn\Touch]
	lStatus = RegOpenKeyEx( HKEY_LOCAL_MACHINE               ,
	                        TEXT( "Drivers\\BuiltIn\\Touch" ),
							0                                ,
	                        KEY_QUERY_VALUE                  ,
							&hKey                            );

	if ( lStatus != ERROR_SUCCESS )
	{
		FAIL( "Error opening registry key [HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch]" );
		g_pKato->Log( LOG_DETAIL, TEXT( "Return code = %ld" ), lStatus );
		goto Exit;		
	}

	// Check the Prefix value and ensure it is "TCH"
	dwValueLen = ( nPREFIXLEN + 1 ) * sizeof( WCHAR );

	lStatus = RegQueryValueEx( hKey               , 
		                       TEXT( "Prefix" )   ,
	                           NULL               ,
							   NULL               ,
							   ( LPBYTE )wszPrefix,
							   &dwValueLen        );

	if ( lStatus != ERROR_SUCCESS )
	{
		FAIL( "Error reading Prefix entry in [HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch]" );
		goto Close_Key_Exit;
	}

	if ( wcscmp( wszPrefix, TEXT( "TCH" ) ) )
	{
        FAIL( "Prefix registry value not set to expected value" );
		g_pKato->Log( LOG_DETAIL, TEXT( "[HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch] Prefix = %s" ), wszPrefix );

		goto Close_Key_Exit;
	}
    //Check for existence of DLL name
	if ( GetTouchDiverName( wszBuffer, nDFTBUFLEN ) != TRUE )
	{
		FAIL( "Error checking for registry [HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch] Dll" );
		goto Close_Key_Exit;
	}

	/*
	 * Check for existence of SYSTR or IRQ registry entry.  If SYSTR does not exist,
	 * there should be an IRQ entry so a SYSTR can be dynamically allocated
	 */

	lStatus = RegQueryValueEx( hKey             , 
		                       TEXT( "SYSINTR" ),
	                           NULL             ,
							   NULL             ,
							   NULL             ,
							   NULL            );


	if ( lStatus != ERROR_SUCCESS )
		lStatus = RegQueryValueEx( hKey         , 
	                               TEXT( "IRQ" ),
		                           NULL         ,
								   NULL         ,
								   NULL         ,
								   NULL         );

	if ( lStatus != ERROR_SUCCESS )
	{
		FAIL( "Error checking for either SYSTR or IRQ in [HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch]" );
		goto Close_Key_Exit;
	}	
	
	//Check for existence of Priority256 registry entry
	lStatus = RegQueryValueEx( hKey                 , 
		                       TEXT( "Priority256" ),
	                           NULL                 ,
							   NULL                 ,
							   NULL                 ,
							   NULL                 );

	if ( lStatus != ERROR_SUCCESS )
	{
		FAIL( "Error checking for Priority256 entry in [HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch]" );
		goto Close_Key_Exit;
	}	
	/*
	 * Check IClass entries for the touch driver and power management driver.
	 * 
	 * First get the buffer length needed to hold all GUIDs and make sure this 
	 * registry type is a multiple string type
	 */

	lStatus = RegQueryValueEx( hKey            , 
		                       TEXT( "IClass" ),
	                           NULL            ,
							   &dwType         ,
							   NULL            ,
							   &dwValueLen     );

	if ( lStatus != ERROR_SUCCESS )
	{
		FAIL( "Error checking for IClass entry in [HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch]" );
		goto Close_Key_Exit;
	}	
    //Execting this registry entry to be a multi string data type
	if ( dwType != REG_MULTI_SZ )
	{
		FAIL( "Expecting Multi String type for IClass entry in [HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch]" );
		g_pKato->Log( LOG_DETAIL, TEXT( "[HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch] IClass type is %x" ), dwType );
		goto Close_Key_Exit;
	}	
   
	//Create buffer to store IClass GUIDs and retrieve the GUIDs
	if ( ( pwszBuffer = ( WCHAR * ) new BYTE[ dwValueLen ] ) == NULL )
	{
		nError = GetLastError();
		FAIL( "Error allocating memory for IClass GUIDs" );
		g_pKato->Log( LOG_DETAIL, TEXT( "Error = %x" ), nError );
		goto Close_Key_Exit;
	}

	lStatus = RegQueryValueEx( hKey                , 
		                       TEXT( "IClass" )    ,
	                           NULL                ,
							   &dwType             ,
							   ( LPBYTE) pwszBuffer,  
							   &dwValueLen         );

	if ( lStatus != ERROR_SUCCESS )
	{
		FAIL( "Error retrieving IClass entries in [HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch]" );
		goto Close_Key_Exit;
	}	
	
	// Look for the touch and power management driver GUIDs in IClass
	for ( n = 0; pwszGuids[ n ] != NULL; n++ ) 
	{	
		for ( pwszEntry  = pwszBuffer; 
			 *pwszEntry != NULL; 
			  pwszEntry += wcslen( pwszEntry ) + 1 )
			  if ( _wcsicmp( pwszGuids[ n ], pwszEntry ) )
				  break;
		
		if ( *pwszEntry == NULL )  //NULL indicates no match was found
		{
			FAIL( "Could not find all the expected IClass GUIDs in [HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch]" );
			g_pKato->Log( LOG_DETAIL, 
				          TEXT( "Could not find %s Driver GUID: %s" ), 
						  n == 0 ? TEXT( "Touch" ) : TEXT( "Power Management" ),
						  pwszGuids[ n ]                                      );
			goto Close_Key_Exit;
		}	

	}

	TP_Status = TPR_PASS;

Close_Key_Exit:

	RegCloseKey( hKey );
Exit:

	if ( pwszBuffer != NULL )
		delete pwszBuffer;

    return TP_Status;
}


//
// IsThereTchCalibDll:  Verfies touch proxy driver registry elements indicate
//                      a touch calibration dll exists
//                           
// ARGUMENTS: pwszTchCalibDll: [out] Dll driver name. 
//            nLen          :  [in]  Maximum character length for pwszDriverName
//
// RETURNS:
//	TRUE:  If successful
//	FALSE: On error
//	EOF:   Registry entry not found
//
INT IsThereTchCalibDll( WCHAR *pwszTchCalibDll, INT nLen )
{
	INT   nResult = FALSE;

	LONG   lStatus;
	HKEY   hKey;
	DWORD  dwValueLen;

	/*
	 * Get handle for registry key [HKEY_LOCAL_MACHINE\System\GWE\TouchProxy]
	 */ 

	lStatus = RegOpenKeyEx( HKEY_LOCAL_MACHINE               ,
	                        TEXT( "System\\GWE\\TouchProxy" ),
							0                                ,
	                        KEY_QUERY_VALUE                  ,
							&hKey                            );


	if ( lStatus != ERROR_SUCCESS )
	{
		FAIL( "Error opening registry key [HKEY_LOCAL_MACHINE\\System\\GWE\\TouchProxy]" );
		g_pKato->Log( LOG_DETAIL, TEXT( "Return code = %ld" ), lStatus );
		goto Exit;		
	}

	/*
	 * Check for existence of Priority256 registry entry
	 */

	dwValueLen = ( nLen + 1 ) * sizeof( WCHAR );

    lStatus = RegQueryValueEx( hKey                      , 
		                       TEXT( "TchCalDll" )       ,
	                           NULL                      ,
							   NULL                      ,
							   ( LPBYTE ) pwszTchCalibDll,
							   &dwValueLen               );

	switch ( lStatus )
	{
		case ERROR_SUCCESS:

			if ( *pwszTchCalibDll == '\0' )
				nResult = EOF;
			else
				nResult = TRUE;
			break;

		case ERROR_FILE_NOT_FOUND:

			nResult = EOF;
			break;
	
		default:

			FAIL( "Error checking for TchCalDll entry in [HKEY_LOCAL_MACHINE\\System\\GWE\\TouchProxy]" );
			g_pKato->Log( LOG_DETAIL, TEXT( "Return code = %ld" ), lStatus );

	}	


	RegCloseKey( hKey );
Exit:


    return nResult;


}



//
// GetTouchDiverName:  Extracts DLL name from the registry at HKLM\Drivers\BuiltInTouch 
//
// ARGUMENTS: pwszDriverName: [out] Dll driver name. 
//            nLen          : [in]  Maximum character length for pwszDriverName
//
// RETURNS:
//	TRUE:  If successful
//	FALSE: If otherwise
//
static BOOL GetTouchDiverName( WCHAR *pwszDriverName, int nLen ) 
{
	BOOL   nResult = FALSE;

	LONG   lStatus;
	HKEY   hKey;
	DWORD  dwValueLen;

	/*
	 * Get handle for registry key [HKEY_LOCAL_MACHINE\Drivers\BuiltIn\Touch]
	 */ 

	lStatus = RegOpenKeyEx( HKEY_LOCAL_MACHINE               ,
	                        TEXT( "Drivers\\BuiltIn\\Touch" ),
							0                                ,
	                        KEY_QUERY_VALUE                  ,
							&hKey                            );

	if ( lStatus != ERROR_SUCCESS )
	{
		FAIL( "Error opening registry key [HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch]" );
		g_pKato->Log( LOG_DETAIL, TEXT( "Return code = %ld" ), lStatus );
		goto Exit;		
	}

	/*
	 * Lookup DLL name
	 */

	dwValueLen = ( nLen + 1 ) * sizeof( WCHAR );


	lStatus = RegQueryValueEx( hKey                     , 
		                       TEXT( "Dll" )            ,
	                           NULL                     ,
							   NULL                     ,
							   ( LPBYTE ) pwszDriverName,
							   &dwValueLen              );

	if ( lStatus != ERROR_SUCCESS || *pwszDriverName == '\0' )
	{
		FAIL( "Error extracting registry value for [HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn\\Touch] Dll" );
		goto Close_Key_Exit;
	}

	nResult = TRUE;

Close_Key_Exit:

	RegCloseKey( hKey );
Exit:

    return nResult;

}

//
// GetTouchProxyDriverName:  Extracts DLL name for the "proxy touch" driver.  If one is 
//                           is specified in the registry at
//
//                                [HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\TOUCH] DriverName
//
//                           this will returned in pwszDriverName.  Otherwise, it is assume
//                           the driver name is "touch.dll"
//
// ARGUMENTS: pwszDriverName: [out] Dll driver name. 
//            nLen          : [in]  Maximum character length for pwszDriverName
//
// RETURNS:
//	TRUE:  If successful
//	FALSE: If otherwise
//
static BOOL GetTouchProxyDriverName( WCHAR *pwszDriverName, int nLen ) 
{
	BOOL   nResult = FALSE;

	LONG   lStatus;
	HKEY   hKey;
	DWORD  dwValueLen;

	/*
	 * Get handle for registry key [HKEY_LOCAL_MACHINE\Drivers\BuiltIn\Touch]
	 */ 

	lStatus = RegOpenKeyEx( HKEY_LOCAL_MACHINE               ,
	                        TEXT( "HARDWARE\\DEVICEMAP\\TOUCH" ),
							0                                ,
	                        KEY_QUERY_VALUE                  ,
							&hKey                            );

	if ( lStatus != ERROR_SUCCESS )
	{
		FAIL( "Error opening registry key [HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\TOUCH]" );
		g_pKato->Log( LOG_DETAIL, TEXT( "Return code = %ld" ), lStatus );
		goto Exit;		
	}

	/*
	 * Lookup DriverName
	 */

	dwValueLen = ( nLen + 1 ) * sizeof( WCHAR );


	lStatus = RegQueryValueEx( hKey                     , 
		                       TEXT( "DriverName" )     ,
	                           NULL                     ,
							   NULL                     ,
							   ( LPBYTE ) pwszDriverName,
							   &dwValueLen              );

	switch ( lStatus )
	{
		case ERROR_SUCCESS:

			if ( *pwszDriverName != '\0' )               
				break;
		/*
		 * Driver not sspecified.  Assume it is "touch.dll"
		 */
		case ERROR_FILE_NOT_FOUND:

			wcscpy_s( pwszDriverName, ( size_t ) nLen, TEXT( "touch.dll" ) );  //Assume it is "touch.dll"
			break;
	
		default:

			FAIL( "Error checking for DriverName entry in [HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\TOUCH]" );
			g_pKato->Log( LOG_DETAIL, TEXT( "Return code = %ld" ), lStatus );
			goto Close_Key_Exit;

	}	

	nResult = TRUE;

Close_Key_Exit:

	RegCloseKey( hKey );

Exit:

    return nResult;


}

//
// TouchVerifyDriversLoadedTest:  Verifies if the touch driver, proxy touch driver
//                                and calibration driver (if specified in the registry)
//                                are loaded.
//
TESTPROCAPI TouchVerifyDriversLoadedTest( 
            UINT                   uMsg, 
            TPPARAM                tpParam, 
            LPFUNCTION_TABLE_ENTRY lpFTE )
{
	INT TP_Status = TPR_FAIL;

	WCHAR wszDriverName[ MAX_PATH ];

	HMODULE hMod;

	int nError;

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

	//Get touch driver DLL from registry
	if ( GetTouchDiverName( wszDriverName, MAX_PATH ) != TRUE )
	{
		FAIL( "Error extracting touch driver name from registry" );
		goto Exit;		
	}
    // Test if DLL is loaded
	if ( ( hMod = GetModuleHandle( wszDriverName ) ) == NULL )
	{
		nError = GetLastError();
		FAIL( "Error checking if touch driver is loaded" );
		g_pKato->Log( LOG_DETAIL, TEXT( "Driver = %s Error = %d" ), wszDriverName, nError );
	}
	//CloseHandle( hMod );

	//Get proxy touch driver DLL from registry
	if ( GetTouchProxyDriverName( wszDriverName, MAX_PATH ) != TRUE )
	{
		FAIL( "Error extracting proxy touch driver name from registry" );
		goto Exit;		
	}
	// Test if DLL is loaded
	if ( ( hMod = GetModuleHandle( wszDriverName ) ) == NULL )
	{
		nError = GetLastError();
		FAIL( "Error checking if proxy touch driver is loaded" );
		g_pKato->Log( LOG_DETAIL, TEXT( "Driver = %s Error = %d" ), wszDriverName, nError );

		goto Exit;		
	}

	//CloseHandle( hMod );

	// Check if a calibration DLL has been specifed
	switch ( IsThereTchCalibDll( wszDriverName, MAX_PATH ) )
	{
		case EOF:				//No.  Return with PASSing value
			return TPR_PASS;

		case TRUE:
			break;

		default:				//Error occurred
			FAIL( "Error looking for callibration driver entry in the registry" );
			goto Exit;
	}

	//Test if calibrartion DLL is loaded
	
	if ( ( hMod = GetModuleHandle( wszDriverName ) ) == NULL )
	{
		nError = GetLastError();
		FAIL( "Error checking if touch calibration driver is loaded" );
		g_pKato->Log( LOG_DETAIL, TEXT( "Driver = %s Error = %d" ), wszDriverName, nError );

		goto Exit;		
	}

	// CloseHandle( hMod );

	TP_Status = TPR_PASS;

Exit:

    return TP_Status;



}

//
// TouchVerifyTouchPanelActiveTest:  Verifies if the touch driver has been
//                                   registered by the Device Manager
//
TESTPROCAPI TouchVerifyTouchPanelActiveTest( 
            UINT                   uMsg, 
            TPPARAM                tpParam, 
            LPFUNCTION_TABLE_ENTRY lpFTE )
{
	INT    TP_Status = TPR_FAIL;
	HANDLE hndTch;
	INT    nError;

	DEVMGR_DEVICE_INFORMATION DMInfo;

    if( uMsg != TPM_EXECUTE )
    {
        return TPR_NOT_HANDLED;
    }

	memset( &DMInfo, 0, sizeof( DEVMGR_DEVICE_INFORMATION ) );

	DMInfo.dwSize = sizeof( DEVMGR_DEVICE_INFORMATION );

	hndTch = FindFirstDevice( DeviceSearchByLegacyName,
	                          L"TCH?:",
							  &DMInfo );

	if ( hndTch == INVALID_HANDLE_VALUE )
	{
		nError = GetLastError();
		FAIL( "Error retrieving device activation data" );
		g_pKato->Log( LOG_DETAIL, TEXT( "Error = %d" ), nError );

		goto Exit;		
	}

	CloseHandle( hndTch );

	TP_Status = TPR_PASS;

Exit:

    return TP_Status;

}