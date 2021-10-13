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
#include "sensordetect.h"
#include "AccHelper.h"

//Expected sensor list....
LPTSTR sensorPrefixTable[] =
{

    TEXT("ACC*"),
};

static const int sensorCount = _countof( sensorPrefixTable );


static const TCHAR* pszAccEntry = _T("Drivers\\BuiltIn\\ACC");


//------------------------------------------------------------------------------
// DumpAllAvailableSensorInformation
//
void DumpAllAvailableSensorInformation()
{
    LOG_START();

    ListAllSensors();

    AccRegistryEntry  accInfo;

    GetAccRegistryInfo( accInfo );
  
    DumpAccRegistryInfo( accInfo );

    LOG_END();
}//DumpAllAvailableSensorInformation




//------------------------------------------------------------------------------
// isSensorPresent
//
BOOL isSensorPresent(LPCTSTR pszPrefix)
{
    LOG_START();

    LOG( "Detecting.........(%s)", pszPrefix );
    
    BOOL bSensorPresent = TRUE;
    DEVMGR_DEVICE_INFORMATION di ={0};
    di.dwSize = sizeof(di);

    HANDLE hSearchDevice = NULL;   

    hSearchDevice = FindFirstDevice(DeviceSearchByLegacyName, pszPrefix, &di);

    if( hSearchDevice == INVALID_HANDLE_VALUE )
    {
        LOG_WARN( "Could not find device...." );
        bSensorPresent = false;
    }
    else
    {
        LOG( "Found an instance of: (%s)", pszPrefix );
    }

    LOG_END();
    return bSensorPresent;
}//isSensorPresent

//------------------------------------------------------------------------------
// DumpDevMgrDevInfo
//
void DumpDevMgrDevInfo( PDEVMGR_DEVICE_INFORMATION di, LPCTSTR pszPrefix, DWORD dwCount )
{
    LOG("%s[%d] - Device Information", pszPrefix, dwCount );
    LOG("...Device Handle........(%s)", (di->hDevice!=INVALID_HANDLE_VALUE)?_T("VALID"):_T("INVALID"));
    LOG("...Parent Handle........(%s)", (di->hParentDevice!=INVALID_HANDLE_VALUE)?_T("VALID"):_T("INVALID"));
    LOG("...Legacy Name..........(%s)", di->szLegacyName);
    LOG("...Device Key...........(%s)", di->szDeviceName);
    LOG("...Device Name..........(%s)", di->szDeviceName);
    LOG("...Bus Name.............(%s)", di->szBusName);
    LOG("...Structure Size.......(%d)\n\n", di->dwSize);
}//DumpDevMgrDevInfo


//------------------------------------------------------------------------------
// ListAllSensors
//
void ListAllSensors()
{
    LOG_START();
    DWORD dwCount = 0;

    DEVMGR_DEVICE_INFORMATION di;

    for( int i = 0; i < sensorCount; i++ )
    {

        LOG( "Detecting................(%s)", sensorPrefixTable[i] );
        HANDLE hSearchDevice = NULL;    
        memset(&di, 0, sizeof(di));
        di.dwSize = sizeof(di);

        hSearchDevice = FindFirstDevice(DeviceSearchByLegacyName, sensorPrefixTable[i], &di);

        if( hSearchDevice == INVALID_HANDLE_VALUE )
        {
            LOG("%s......................(NOT PRESENT)", sensorPrefixTable[i] );                
        }
        else
        {
            dwCount++;
            DumpDevMgrDevInfo(&di, sensorPrefixTable[i], dwCount );
        }
        
        while( FindNextDevice(hSearchDevice, &di) )
        {
            dwCount++;
            DumpDevMgrDevInfo(&di, sensorPrefixTable[i], dwCount );                
        }       

        dwCount = 0;
        LOG( "\n\n");
    }
    LOG_END();
}//ListAllSensors


//------------------------------------------------------------------------------
// GetSnsRegistryInfo
//
void GetSnsRegistryInfo( SnsRegistryEntry & snsInfo, LPCTSTR pszKeyEntry )
{
    LOG_START();
    BOOL        bResult = TRUE;
    HANDLE      hFind = INVALID_HANDLE_VALUE;
    HKEY        hKey = NULL;
    DWORD       dwType  = 0;
    DWORD       dwSize = 0; 
    LONG        lResult = ERROR_SUCCESS;
    
    //Open, read and verify all ACC registry keys
    LOG( "Opening HKLM\\%s", pszKeyEntry );
    lResult = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE, 
        pszKeyEntry, 
        0, 
        0,
        &hKey);
    if( lResult != ERROR_SUCCESS )
    {
        LOG( "Could not open HKLM\\%s Result:(%d)", pszKeyEntry, lResult );
        goto DONE;
    }
    
             
    LOG( "Querying HKLM\\%s\\%s", pszKeyEntry, DEVLOAD_DLLNAME_VALNAME);
    dwSize = _countof( snsInfo.szMddDll );
    lResult = RegQueryValueEx(
    hKey,
        DEVLOAD_DLLNAME_VALNAME,
        NULL,
        &dwType,
        (LPBYTE)&snsInfo.szMddDll,
        &dwSize);
    if( lResult != ERROR_SUCCESS )
    {
        LOG( "Failed to Open(%d)...............(\\%s\\%s)", lResult,  pszKeyEntry, DEVLOAD_DLLNAME_VALNAME);
    }
        
    LOG( "Querying HKLM\\%s\\%s", pszKeyEntry, DEVLOAD_PREFIX_VALNAME);
    dwSize = _countof( snsInfo.szPrefix );
    lResult = RegQueryValueEx(
        hKey,
        DEVLOAD_PREFIX_VALNAME,
        NULL,
        &dwType,
        (LPBYTE)&snsInfo.szPrefix,
        (LPDWORD)&dwSize);
    if( lResult != ERROR_SUCCESS )
    {
         LOG( "Failed to Open(%d)...............(\\%s\\%s)", lResult,  pszKeyEntry, DEVLOAD_PREFIX_VALNAME);
    }
          
    LOG( "Querying HKLM\\%s\\%s", pszKeyEntry, DEVLOAD_ICLASS_VALNAME);
    dwSize = _countof( snsInfo.szIClass );
    lResult = RegQueryValueEx(
        hKey,
        DEVLOAD_ICLASS_VALNAME,
        NULL,
        &dwType,
        (LPBYTE)&snsInfo.szIClass,
        (LPDWORD)&dwSize);
    if( lResult != ERROR_SUCCESS )    
    {
         LOG( "Failed to Open(%d)...............(\\%s\\%s)", lResult,  pszKeyEntry, DEVLOAD_ICLASS_VALNAME);
    }              

    LOG( "Querying HKLM\\%s\\%s", pszKeyEntry,DEVLOAD_LOADORDER_VALNAME);
    dwSize = sizeof( snsInfo.dwOrder );
    lResult = RegQueryValueEx(
        hKey,
        DEVLOAD_LOADORDER_VALNAME,
        NULL,
        &dwType,
        (LPBYTE)&snsInfo.dwOrder,
        (LPDWORD)&dwSize);
    if( lResult != ERROR_SUCCESS )
    {
         LOG( "Failed to Open(%d)...............(\\%s\\%s)", lResult,  pszKeyEntry, DEVLOAD_LOADORDER_VALNAME);
    }         
        
    LOG( "Querying HKLM\\%s\\%s", L"PDD", DEVLOAD_DLLNAME_VALNAME);
    dwSize = _countof(snsInfo.szPddDll);
    lResult = RegQueryValueEx(
        hKey,
        _T("PDD"),
        NULL,
        &dwType,
        (LPBYTE)&snsInfo.szPddDll,
        &dwSize);
    if( lResult != ERROR_SUCCESS )
    {
         LOG( "Failed to Open(%d)...............(\\%s\\%s)", lResult,  pszKeyEntry, L"PDD");
    }               

DONE:
    CHECK_CLOSE_KEY( hKey );   
    LOG_END();
}//GetSnsRegistryInfo


//------------------------------------------------------------------------------
// GetAccRegistryInfo
//
void GetAccRegistryInfo( AccRegistryEntry & accInfo )
{
    LOG_START();
    GetSnsRegistryInfo(accInfo, pszAccEntry);

    BOOL        bResult = TRUE;
    HANDLE      hFind = INVALID_HANDLE_VALUE;
    HKEY        hKey = NULL;
    DWORD       dwType  = 0;
    DWORD       dwSize = 0; 
    LONG        lResult = ERROR_SUCCESS;
    
    //Open, read and verify all ACC registry keys
    LOG( "Opening HKLM\\%s", pszAccEntry );
    lResult = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE, 
        pszAccEntry, 
        0, 
        0,
        &hKey);
    if( lResult != ERROR_SUCCESS )
    {
        LOG( "Could not open HKLM\\%s Result:(%d)", pszAccEntry, lResult );
        goto DONE;
    }


    LOG( "Querying HKLM\\%s\\%s", L"I2CDriver", DEVLOAD_DLLNAME_VALNAME);
    dwSize = _countof(accInfo.szI2CDriver);
    lResult = RegQueryValueEx(
        hKey,
        _T("I2CDriver"),
        NULL,
        &dwType,
        (LPBYTE)&accInfo.szI2CDriver,
        &dwSize);
    if( lResult != ERROR_SUCCESS )
    {
         LOG( "Failed to Open(%d)...............(\\%s\\%s)", lResult,  pszAccEntry, L"I2CDriver");
    }               

    LOG( "Querying HKLM\\%s\\%s", pszAccEntry,L"I2CAddr");
    dwSize = sizeof( accInfo.dwI2CAddr );
    lResult = RegQueryValueEx(
        hKey,
        L"I2CAddr",
        NULL,
        &dwType,
        (LPBYTE)&accInfo.dwI2CAddr,
        (LPDWORD)&dwSize);
    if( lResult != ERROR_SUCCESS )
    {
        LOG( "Failed to Open(%d)...............(\\%s\\%s)", lResult,  pszAccEntry, L"I2CAddr");
    }   

    LOG( "Querying HKLM\\%s\\%s", pszAccEntry,L"I2CBus");
    dwSize = sizeof( accInfo.dwI2CBus);
    lResult = RegQueryValueEx(
        hKey,
        L"I2CBus",
        NULL,
        &dwType,
        (LPBYTE)&accInfo.dwI2CBus,
        (LPDWORD)&dwSize);
    if( lResult != ERROR_SUCCESS )
    {
         LOG( "Failed to Open(%d)...............(\\%s\\%s)", lResult,  pszAccEntry, L"I2CBus");
    }

    LOG( "Querying HKLM\\%s\\%s", pszAccEntry,L"I2CSpeed");
    dwSize = sizeof( accInfo.dwI2CSpeed);
    lResult = RegQueryValueEx(
        hKey,
        L"I2CSpeed",
        NULL,
        &dwType,
        (LPBYTE)&accInfo.dwI2CSpeed,
        (LPDWORD)&dwSize);
    if( lResult != ERROR_SUCCESS )
    {
         LOG( "Failed to Open(%d)...............(\\%s\\%s)", lResult,  pszAccEntry, L"I2CSpeed");
    }


    LOG( "Querying HKLM\\%s\\%s", pszAccEntry,L"I2COptions");
    dwSize = sizeof( accInfo.dwI2COptions);
    lResult = RegQueryValueEx(
        hKey,
        L"I2COptions",
        NULL,
        &dwType,
        (LPBYTE)&accInfo.dwI2COptions,
        (LPDWORD)&dwSize);
    if( lResult != ERROR_SUCCESS )
    {
         LOG( "Failed to Open(%d)...............(\\%s\\%s)", lResult,  pszAccEntry, L"I2COptions");
    }


DONE:
    CHECK_CLOSE_KEY( hKey );   
    LOG_END();
}//GetAccRegistryInfo


//------------------------------------------------------------------------------
// DumpSnsRegistryInfo
//
void DumpSnsRegistryInfo( SnsRegistryEntry & snsInfo )
{
    LOG("...MDD Library...................(%s)", snsInfo.szMddDll);    
    LOG("...PDD Library...................(%s)", snsInfo.szPddDll);    
    LOG("...Prefix........................(%s)", snsInfo.szPrefix);        
    LOG("...IClass........................(%s)", snsInfo.szIClass);        
    LOG("...Order.........................(%d)", snsInfo.dwOrder);    
}//DumpSnsRegistryInfo

//------------------------------------------------------------------------------
// DumpAccRegistryInfo
//
void DumpAccRegistryInfo( AccRegistryEntry & accInfo )
{
    LOG("#### Accelerometer Register Info");
    LOG("...Registry Entry................(%s)", pszAccEntry);
    DumpSnsRegistryInfo( accInfo );
    LOG("...I2C Driver....................(%s)", accInfo.szI2CDriver);    
    LOG("...I2C Address...................(0x%08X)", accInfo.dwI2CAddr);    
    LOG("...I2C Bus.......................(0x%08X)", accInfo.dwI2CBus);        
    LOG("...I2C Speeed....................(0x%08X)", accInfo.dwI2CSpeed);    
    LOG("...I2C Options...................(0x%08X)", accInfo.dwI2COptions);    
}//DumpAccRegistryInfo

