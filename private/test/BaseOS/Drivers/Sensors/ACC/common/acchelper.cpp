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

#define _USE_MATH_DEFINES

#include "clparse.h"
#include <auto_xxx.hxx>
#include <math.h>

// This should really be defined in math.h
#define M_PI       3.14159265358979323846


#include "AccHelper.h"
#include "AccApiFuncWrap.h"
#include "acc_ioctl.h"
#include "sensor.h"
#include <accelerometer.h>  // Mock ACC PDD (ex. IOCTL_ACC_SIMULATE_ROTATE)
#include <tlhelp32.h>
#include <toolhelp.h>

AccTestParam g_AccParam;
HINSTANCE g_hFuncDLLInst;
HINSTANCE g_hScenarioDLLInst;

HANDLE g_hPwrReq;

GUID g_guid3dAccelerometer2 = {0xC2FB0F5F, 0xE2D2, 0x4C78, { 0xBC, 0xD0, 0x35, 0x2A, 0x95, 0x82, 0x81, 0x9D}};
ACC_DATA acc_last = {0};
ACC_DATA acc_delta = {0};


//------------------------------------------------------------------------------
// acc_Callback
//
void acc_Callback( ACC_DATA* acc_data, DWORD dwContext )
{
    static DWORD dwCount = 0;
    acc_delta.x = acc_data->x - acc_last.x;
    acc_delta.y = acc_data->y - acc_last.y;
    acc_delta.z = acc_data->z - acc_last.z;
    acc_delta.hdr.dwTimeStampMs = acc_data->hdr.dwTimeStampMs - acc_last.hdr.dwTimeStampMs;

    memcpy_s( &acc_last, sizeof(acc_last), acc_data, sizeof(*acc_data) );

    LOG( "[%05d] <%d> CS(%07d: %02.03f, %02.03f, %02.03f) DL(%07d: %02.03f, %02.03f, %02.03f)", 
        dwCount++,
        acc_data->hdr.sensorLuid,
        acc_data->hdr.dwTimeStampMs,
        acc_data->x,        
        acc_data->y,
        acc_data->z,
        acc_delta.hdr.dwTimeStampMs,
        acc_delta.x,        
        acc_delta.y,
        acc_delta.z );    
 
}//acc_Callback


//------------------------------------------------------------------------------
// tst_Callback
//
void tst_Callback( ACC_DATA* acc_data, DWORD dwContext )
{
    LOG_START();

    LOG( "tst_Callback( 0x%x, 0x%x)", (DWORD)acc_data, dwContext );

    AccDumpSample(*acc_data);

    HANDLE hEvent = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME );
    if( hEvent != NULL )
    {
        SetEventData( hEvent, dwContext );
        SetEvent( hEvent );
        Sleep(0);
        CloseHandle ( hEvent );
    }
    else
    {
        LOG_WARN( "Could not open event" );
    }
    LOG_END();
}//tst_Callback


//------------------------------------------------------------------------------
// tst_FakeCallback
//
void tst_FakeCallback( BOOL bVal, DWORD dwVal )
{
    LOG_START();

    LOG( "tst_FakeCallback( 0x%x, 0x%x)", bVal, dwVal );

    HANDLE hEvent = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME );
    if( hEvent != NULL )
    {
        SetEvent( hEvent );
        CloseHandle ( hEvent );
    }
    else
    {
        LOG_WARN( "Could not open event" );
    }
    LOG_END();
}//tst_FakeCallback


//------------------------------------------------------------------------------
// CalculateDeviation
//
DOUBLE CalculateDeviation( DOUBLE actual, DOUBLE expected )
{
    LOG_START();
    DOUBLE idResult = 100;

    if( expected == 0 && actual == 0)
    {
        idResult = 0;
    }
    else if( expected != 0 )
    {
        idResult = 100.0*(fabs((actual - expected )/(expected)));
    }
    else if( actual != 0 )
    {
        idResult = 100.0*(fabs((expected - actual )/(actual)));
    }
    
    LOG_END();    
    return idResult;        
}//CalculateDeviation


//------------------------------------------------------------------------------
// UserPrompt
//
BOOL UserPrompt(LPCTSTR szCaption, LPCTSTR szFormat, ...)
{
    LOG_START();
    TCHAR   szBuffer[1024];
    va_list pArgs;
    va_start(pArgs, szFormat);
    StringCchVPrintfEx(szBuffer,_countof(szBuffer),NULL,NULL,STRSAFE_IGNORE_NULLS, szFormat, pArgs);
    va_end(pArgs);
 
    int iRes = MessageBox(
        NULL, 
        szBuffer,
        szCaption, 
        MB_OKCANCEL|MB_SETFOREGROUND);
    LOG_END();
    return (iRes==IDOK)?TRUE:FALSE;

}//UserPrompt



//------------------------------------------------------------------------------
// LoadAccTestLibraries
//
BOOL LoadAccTestLibraries()
{
    LOG_START();
    BOOL bResult = TRUE;
           
    g_hFuncDLLInst = LoadLibrary(ACC_FUNC_LIB);

    if( g_hFuncDLLInst == NULL )
    {   
        LOG_ERROR( "Could not load library: (%s)", ACC_FUNC_LIB );
    }

    g_hScenarioDLLInst = LoadLibrary( ACC_SCENARIO_LIB );
    if( g_hScenarioDLLInst == NULL )
    {   
        LOG_ERROR( "Could not load library: (%s)", ACC_SCENARIO_LIB );
    }

DONE:
    LOG_END();
    return bResult;
}//LoadAccTestLibraries    




//------------------------------------------------------------------------------
// AccSetSystemPowerState 
//
BOOL AccSetSystemPowerState(DWORD dwFlags )
{
    LOG_START();
    BOOL bResult = TRUE;
    DWORD dwRes = 0;
    LOG( "Requesting Power State 0x%02X ", dwFlags );
    
    dwRes = SetSystemPowerState(NULL, dwFlags, 0);

    if( dwRes != ERROR_SUCCESS )
    {
        LOG_ERROR("SetSystemPowerState failed: (0x%08X", dwRes);
    }

    AccGetSystemPowerState();
DONE:
    LOG_END();
    return bResult;
}//AccSetSystemPowerState



//------------------------------------------------------------------------------
// AccGetSystemPowerState
//
void AccGetSystemPowerState()
{
    TCHAR szGetPowerState[MAX_PATH];
    DWORD dwGetFlags = 0;

    if(ERROR_SUCCESS != GetSystemPowerState(szGetPowerState, _countof(szGetPowerState), &dwGetFlags))
    {
        LOG_WARN("GetSystemPowerState returned FALSE" );
    }
    else
    {
        LOG("Current Power State: (%s)  Flags: (0x%08X) Global Power Requirement Handler: (0x%08X)", szGetPowerState, dwGetFlags , g_hPwrReq);
    }

}//AccGetSystemPowerState
    


//------------------------------------------------------------------------------
// AccSetPowerRequirement
//
BOOL AccSetPowerRequirement(CEDEVICE_POWER_STATE pwrState )
{
    LOG_START();
    BOOL bResult = TRUE;

    LOG( "Requesting Power State 0x%02X for default Accelerometer", pwrState );

    if(g_hPwrReq != NULL)
    {
        LOG( "Exisitng Power Requirement Detected - Releasing...." );
        AccReleasePowerRequirement();
    }

    
    g_hPwrReq = SetPowerRequirement(L"ACC1:", pwrState, POWER_NAME|POWER_FORCE, NULL, 0);
    if(g_hPwrReq == NULL)
    {
        LOG_ERROR("SetPowerRequirement failed");
    }
    else
    {
        LOG("Power Requirement Successfully Set with Handle: 0x%08X", g_hPwrReq );
    }

    AccGetSystemPowerState();

DONE:
    LOG_END();
    return bResult;
}//AccSetPowerRequirement


//------------------------------------------------------------------------------
// AccReleasePowerRequirement
//
BOOL AccReleasePowerRequirement()
{
    LOG_START();
    BOOL bResult = TRUE;
    DWORD dwRes =  0;
    

    AccGetSystemPowerState();

    LOG("Attmepting to release Power Requirement with Handle: 0x%08X", g_hPwrReq );

    if(g_hPwrReq != NULL)
    {
        dwRes = ReleasePowerRequirement(g_hPwrReq);
        if( ERROR_SUCCESS != dwRes )
        {
            LOG_ERROR( "ReleasePowerRequirement Failed: (0x%08X)", dwRes );
        }
        g_hPwrReq = NULL;
    }
    else
    {
        LOG_WARN( "ReleasePowerRequirement called with invalid handle" );
    }
  
DONE:    
    LOG_END();
    return bResult;
}//AccReleasePowerRequirement


//------------------------------------------------------------------------------
// AccTestInitialization
//
BOOL AccTestInitialization(LPCWSTR pszCmdLine)
{
    LOG_START();
    BOOL bResult = TRUE;


    //1. Do we even have the registry entries for this driver
    if(( !GetAccRegistryEntry( g_AccParam.accReg ) ) ||
       ( !ProcessCmdLine(pszCmdLine) ) ||
       ( !LoadAccTestLibraries() ))
    {
        LOG_ERROR("Test Initialization Failed");
    }
        
DONE:
    LOG_END();
    return bResult;
}//AccTestInitialization


//------------------------------------------------------------------------------
// AccTestCleanup
//
BOOL AccTestCleanup()
{
    LOG_START();
    BOOL bResult = TRUE;
    
    if( g_hFuncDLLInst != NULL )
    {   
       if( !FreeLibrary( g_hFuncDLLInst ) )
       {
           LOG_WARN( "Could not free library: (%s)", ACC_FUNC_LIB );
       }

       g_hFuncDLLInst = NULL;
    }

    if( g_hScenarioDLLInst != NULL )
    {   
       if( !FreeLibrary( g_hScenarioDLLInst ) )
       {
           LOG_WARN( "Could not free library: (%s)", ACC_SCENARIO_LIB );
       }

       g_hScenarioDLLInst = NULL;
    }
 
    LOG_END();
    return bResult;
}//AccTestCleanup


//------------------------------------------------------------------------------
// AccSampleVerify
//
BOOL AccSampleVerify(HANDLE& hQEvent, LUID &sensorLuid, BOOL bShouldPass /*TRUE*/ )
{
    LOG_START();
    BOOL bResult = TRUE;
    BOOL bRead = FALSE;
    DWORD dwWait = 0;
    DWORD dwStartTick = 0;
    DWORD dwTickDelta = 0;
    DWORD dwIterationCount = 0;
    DWORD dwRxSampleCount = 0;
    DWORD dwValidSampleCount = 0;
    DWORD dwBytesRead = 0;
    DWORD dwFlags = 0;
    DWORD dwFailureCount = 0;
    ACC_DATA accData = {0};
    DWORD dwFlushed = 0;

    dwStartTick = GetTickCount();

    while( dwIterationCount < 10 && dwFailureCount < 5 )
    {
        bRead = ReadMsgQueue(hQEvent,
            (LPVOID) &accData,
            sizeof(accData),
            &dwBytesRead,
            500,
            &dwFlags);

        if( bRead )
        {               
            if( dwStartTick > accData.hdr.dwTimeStampMs )
            {
                LOG( "Stale Sample (%d/%d)",  dwStartTick, accData.hdr.dwTimeStampMs );
                continue;
            }

            dwRxSampleCount++;            
        }

        dwIterationCount++;

        //No Samples Received - But we were expecting samples...
        if( !bRead && bShouldPass)
        {
            dwFailureCount++;            
            LOG_WARN( "Message Queue Notification: Message Read Failed"  );
                
        }
        //Samples Received - But we weren't expecting samples...
        else if( bRead && !bShouldPass )
        {
            AccGetSystemPowerState();
            LOG_WARN( "Unexpected Sample Received" );
        }
        //Sample Received and we were expecting it....
        else if( bRead&& bShouldPass )
        {
            if( memcmp(&accData.hdr.sensorLuid, &sensorLuid, sizeof(LUID)) != 0)
            {
                dwFailureCount++;            
                LOG_WARN( "Data Sample Sensor ID Mismatch" );
                        
            }
            else
            {          
                dwValidSampleCount++;
                LOG( "----------------------------------------------" );
                LOG( "ACCELEROMETER SAMPLE INFORMATION" );
                LOG( "Data Vector:.................(%f,%f,%f)", accData.x, accData.y, accData.z );
                LOG( "Sample Timestamp:............(%d)", accData.hdr.dwTimeStampMs );
                LOG( "Sensor Id:...................(0x%08X%08X)", accData.hdr.sensorLuid.HighPart, accData.hdr.sensorLuid.LowPart );
                LOG( "Sample Size..................(%d)", accData.hdr.cbSize );
                LOG( "Sample Read Interval.........(%d)", dwTickDelta );
                LOG( "Sample Count.................(%d of %d)", dwValidSampleCount, dwRxSampleCount );
                LOG( "Current Failure Count........(%d)", dwFailureCount );
                LOG( "----------------------------------------------\n\n" );
            }
        }
        //No Samples Received - No Samples Expected
        else if( !bRead && !bShouldPass )
        {
            LOG( "No Unexpected Samples Received (Iteration: %d/10)", dwIterationCount );
        }
        //Ooops
        else
        {
            LOG_WARN( "No Sample Read and no handling condition set" );
        }
    }


    if( dwFailureCount != 0 && bShouldPass )
    {
        LOG_ERROR( "Sample Validation Failed" );
    }
        
DONE:
    LOG( "Received: %d samples Flushed: %d", dwRxSampleCount, dwFlushed );
    LOG_END();   
    return bResult;    
}//AccSampleVerify

//------------------------------------------------------------------------------
// AccNominalScenarioCheck
//
BOOL AccNominalScenarioCheck( )
{
    LOG_START();
    BOOL bResult = TRUE;
    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    HANDLE hQEvent = INVALID_HANDLE_VALUE;

    if( !AccGetHandle(hAcc, sensorLuid))
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    if( !CreateNominalMessageQueue(hQEvent) )
    {
        LOG_ERROR( "Could not create sensor message queue" );
    }

    if( ERROR_SUCCESS != f_AccelerometerStart(hAcc, hQEvent) )
    {
        LOG_ERROR( "Could not Start Accelerometer" );
    }

    if( !AccSampleVerify( hQEvent, sensorLuid ) )
    {
        LOG_ERROR( "Could not verify samples. Check log for details." );
    }
        
    if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
    {
        LOG_ERROR( "Could not Stop Accelerometer" );
    }

DONE:

    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQEvent);
    LOG_END();
    return bResult;
    
}//AccNominalScenarioCheck


//------------------------------------------------------------------------------
// CreateNominalMessageQueue
//
BOOL CreateNominalMessageQueue(HANDLE& hQEvent )
{
    LOG_START();
    
    static DWORD dwIdx = 0;
    TCHAR qName[MAX_PATH];
    BOOL bResult = TRUE;

    //Lets give it a unique name
    StringCchPrintfEx( 
        qName,
        _countof(qName),
        NULL,
        NULL,
        STRSAFE_NULL_ON_FAILURE,
        _T("%Q_NOM_%d"),
        dwIdx++ );
   
   //Create it (or try to)
   LOG( "Creating Queue: %s", qName );
   hQEvent = INVALID_HANDLE_VALUE;
   hQEvent = CreateMsgQueue( qName, &nominalQOpt );
   DWORD dwLastErr = GetLastError();
   
   if( dwLastErr!=ERROR_SUCCESS )
   {
       LOG_ERROR( "Queue Creation has Failed with an unknown error" );
   }

DONE:

    LOG_END();
    return bResult;
}//CreateNominalMessageQueue


//------------------------------------------------------------------------------
// FillRandStr
//
void FillRandStr( TCHAR* pStr, DWORD dwSize )
{

    if( pStr && dwSize > 1 )
    {
        for( DWORD i = 0; i < dwSize-1; i++ )
        {
            pStr[i] = (TCHAR)(RANGE_RAND(0, 255));
        }
        pStr[i] = '\0';
    }    
}//FillRandStr


//------------------------------------------------------------------------------
// DumpDevMgrDevInfo
//
void DumpDevMgrDevInfo( PDEVMGR_DEVICE_INFORMATION di )
{
    LOG("#### Device Manager Device Info");
    LOG("...Device Handle........(%s)", (di->hDevice!=INVALID_HANDLE_VALUE)?_T("VALID"):_T("INVALID"));
    LOG("...Parent Handle........(%s)", (di->hParentDevice!=INVALID_HANDLE_VALUE)?_T("VALID"):_T("INVALID"));
    LOG("...Legacy Name..........(%s)", di->szLegacyName);
    LOG("...Device Key...........(%s)", di->szDeviceName);
    LOG("...Device Name..........(%s)", di->szDeviceName);
    LOG("...Bus Name.............(%s)", di->szBusName);
    LOG("...Structure Size.......(%d)\n\n", di->dwSize);
}//DumpDevMgrDevInfo


//------------------------------------------------------------------------------
// DumpAccRegEntries
//
void DumpAccRegEntries( AccRegEntry& accReg )
{
    LOG("#### Accelerometer Register Info");
    LOG("...MDD Key..............(%s)", pszAccKeyMdd);
    LOG("...\\Dll................(%s)", accReg.mdd.szDll);
    LOG("...\\Order..............(%d)", accReg.mdd.dwOrder);
    LOG("...\\Prefix.............(%s)", accReg.mdd.szPrefix);
    LOG("...PDD Key..............(%s)", pszAccKeyPdd);
    LOG("...\\PDD................(%s)", accReg.pdd.szDll);
}//DumpAccRegEntries




//------------------------------------------------------------------------------
// DumpSensorCaps
//
void DumpSensorCaps( SENSOR_DEVICE_CAPS &sensorCaps )
{
    LOG("#### Sensor Device Capabilities");

    switch( sensorCaps.type )
    {
        case SENSOR_ACCELEROMETER_3D:
            LOG("...Sensor Type..........SENSOR_ACCELEROMETER_3D(%d)", sensorCaps.type);
            break;

        default:
            LOG("...Sensor Type..........Unknown Type (%d)", sensorCaps.type);
            break;
    }
    LOG("...cbSize...............(%d)", sensorCaps.cbSize);
    LOG("...dwFlags..............(0x%08X)", sensorCaps.dwFlags);
    LOG("...sensorLuid...........(0x%08X%08X)", sensorCaps.sensorLuid.HighPart, sensorCaps.sensorLuid.LowPart);
    LOG("...dwMinSwIntervalUs....(%d)", sensorCaps.dwMinSwIntervalUs);
}//DumpSensorCaps



//------------------------------------------------------------------------------
// DumpPowerCaps
//
void DumpPowerCaps( POWER_CAPABILITIES &powerCaps )
{
    LOG("#### Power Capabilities");

    LOG("...DeviceDx ............(0x%08X)", powerCaps.DeviceDx );
    LOG("...WakeFromDx ..........(0x%08X)", powerCaps.WakeFromDx );
    LOG("...InrushDx  ...........(0x%08X)", powerCaps.InrushDx  );
    LOG("...Flags................(0x%08X)", powerCaps.Flags);
    for( DWORD i = 0; i < 5; i++ )
    {
        LOG("...Level D%d:", i );
        LOG("......Power.............(%d)mW", powerCaps.Power[i]);
        LOG("......Latency...........(%d)ms", powerCaps.Latency[i]);
    }
    
}//DumpPowerCaps


//------------------------------------------------------------------------------
// DumpPowerState
//
void DumpPowerState( CEDEVICE_POWER_STATE &powerState )
{
    LOG("#### Power State");
    switch( powerState )
    {
        case PwrDeviceUnspecified:
            LOG("...Power Device State...(PwrDeviceUnspecified)");
            break;
            
        case D0:
            LOG("...Power Device State...(D0)");
                break;

        case D1:
            LOG("...Power Device State...(D1)");
            break;
            
        case D2:
            LOG("...Power Device State...(D2)");
            break;
            
        case D3:
            LOG("...Power Device State...(D3)");
            break;

        case D4:
            LOG("...Power Device State...(D4)");
            break;
   
        case PwrDeviceMaximum:
            LOG("...Power Device State...(PwrDeviceMaximum)");
            break;

        default:           
            LOG("...UNKOWN POWER DEVICE STATE (%d)", powerState);
            break;
            
    }    
}//DumpPowerState

//------------------------------------------------------------------------------
// EnableSimStubRotation
//
BOOL EnableSimStubRotation(HSENSOR& hAcc )
{
    LOG_START();
    BOOL bResult = TRUE;
    if( !DeviceIoControl(hAcc,IOCTL_ACC_SIMULATE_ROTATE,0,0,0,0,0,NULL) )
    {
        LOG_ERROR( "IOCTL_ACC_SIMULATE_ROTATE Failed!" );
    }
DONE:
    LOG_END();
    return bResult;
}//EnableSimStubRotation


//------------------------------------------------------------------------------
// LoadStubAccPdd
//
BOOL LoadStubAccPdd(LPCWSTR pszPddName)
{
    LOG_START();
    BOOL bResult = TRUE;    
    HANDLE hFind = INVALID_HANDLE_VALUE;
    HKEY        hAccKey = NULL;
    DWORD       dwType  = 0;
    DWORD       dwSize = 0; 
    
    LOG( "Searching for Accelerometer device handle..." );
    DEVMGR_DEVICE_INFORMATION di = {0};   
    di.dwSize = sizeof( di );
    hFind = FindFirstDevice( DeviceSearchByGuid, (LPCVOID)&g_guid3dAccelerometer2, &di);   
    if( INVALID_HANDLE_VALUE == hFind) 
    {
        LOG_ERROR( "No Accelerometer Device Found" );
    }

    LOG( "Deactivating Current Device: %s", di.szLegacyName);
    if( !DeactivateDevice( di.hDevice ) )
    {
        LOG_ERROR( "Could not deactivate device" )
    }

    LOG( "Opening Registry Key: %s", pszAccKeyMdd );
    if( RegOpenKeyEx(
        HKEY_LOCAL_MACHINE, 
        pszAccKeyMdd, 
        0, 
        0,
        &hAccKey) != ERROR_SUCCESS)
      {
          LOG_ERROR( "Could not open HKLM\\%s Result:(x)", pszAccKeyMdd );
      }


    LOG( "Modifying PDD Registry Entry with (%s)", pszPddName);
    dwSize = wcslen( pszPddName );
    if( RegSetValueEx(
        hAccKey,
        _T("PDD"),
        0,
        REG_SZ,
        (LPBYTE)pszPddName,
        (dwSize  + 1) * sizeof(TCHAR))!= ERROR_SUCCESS )
    {
        LOG_ERROR( "Could not read HKLM\\%s\\%s Result:(x)", pszAccKeyPdd, DEVLOAD_DLLNAME_VALNAME );
    }    

    LOG( "Re-Activating the device" );
    if( !ActivateDeviceEx( di.szDeviceKey, NULL, 0, NULL) )
    {
        LOG_ERROR( "Could not re-activate device" );
    }
    
DONE:
    FindClose( hFind );
    LOG_END();
    return bResult;
}//LoadStubAccPdd


//------------------------------------------------------------------------------
// IsAccDriverRegistered
//
BOOL IsAccDriverRegistered( )
{
    LOG_START();
    BOOL bResult = TRUE; 
    AccRegEntry accReg;

    if( !GetAccRegistryEntry( accReg ) )
    {
        LOG_ERROR( "Could not retrieve Accelerometer Entries" );
    }
    
    DumpAccRegEntries( accReg );

DONE:

    LOG_END();
    return bResult;
}//IsAccDriverRegistered


//------------------------------------------------------------------------------
// GetAccDevMgrInfo
//
BOOL GetAccDevMgrInfo(DEVMGR_DEVICE_INFORMATION *di)
{
    static const TCHAR* pszSearchParam = _T("ACC1:\0");
    HANDLE hSearchDevice = INVALID_HANDLE_VALUE;
    BOOL bResult = TRUE;

    RETAILMSG( TRUE, (_T("Searching for (%s) for driver"), pszSearchParam));

    //Zero our structure for the coming adventure
    memset(di, 0, sizeof(*di));
    di->dwSize = sizeof(*di);

    // See if we have any devices by this name
    hSearchDevice = FindFirstDevice(DeviceSearchByLegacyName, pszSearchParam,  di);
    VERIFY_STEP( "Finding First Device", ((hSearchDevice != NULL)&(hSearchDevice != INVALID_HANDLE_VALUE)), TRUE);

DONE:
    CHECK_CLOSE_FIND_HANDLE(hSearchDevice);
    return bResult;
}//GetAccDevMgrInfo

//------------------------------------------------------------------------------
// AccGetHandle
//
BOOL AccGetHandle(HSENSOR &hAcc, LUID &sensorLuid)
{
    LOG_START();
    BOOL bResult = TRUE;    
    HANDLE hFind = INVALID_HANDLE_VALUE;

    DEVMGR_DEVICE_INFORMATION di = {0};   
    di.dwSize = sizeof( di );

    hAcc = ACC_INVALID_HANDLE_VALUE;
   
    hFind = FindFirstDevice( DeviceSearchByGuid, (LPCVOID)&g_guid3dAccelerometer2, &di);
    if( INVALID_HANDLE_VALUE == hFind) 
    {
        LOG_ERROR( "No Accelerometer Device Found" );
    }

    FindClose( hFind );
     
    hAcc = f_OpenSensor(di.szLegacyName, &sensorLuid);
    if( hAcc == ACC_INVALID_HANDLE_VALUE )
    {
        LOG_ERROR( "Could not open handle to accelerometer" );    
    }
    
DONE:

    LOG_END();
    return bResult;
}//AccGetHandle


//------------------------------------------------------------------------------
// AccStartSampling
//
BOOL AccStartSampling(HSENSOR &hAcc, HANDLE& hQHandle)
{
    LOG_START();
    BOOL bResult = TRUE; 
    LUID sensorLuid = {0};


    if( hQHandle == INVALID_HANDLE_VALUE )
    {
        LOG_ERROR( "Invalid Queue Handle" );
    }  

    if( hAcc == ACC_INVALID_HANDLE_VALUE )
    {
        LOG( "Accelerometer Handle not specified. Acquiring Handle" );
        if( !AccGetHandle( hAcc, sensorLuid ) )
        {
            LOG_ERROR( "Could not acquire accelerometer handle" );
        }
    }
       
    if( ERROR_SUCCESS != f_AccelerometerStart(hAcc, hQHandle))
    {
        LOG_ERROR( "Sampling start failed" );    
    }

    //Verify we're getting something
    DWORD dwWait = WaitForSingleObject(hQHandle, 30 * ONE_SECOND ); 
    if( dwWait == WAIT_FAILED || dwWait == WAIT_TIMEOUT )
    {
        LOG_ERROR( "Test Sampling Failed");
    }
    else
    {
        LOG( "Test Sample Received (%d)", dwWait ); 
        LOG( "Accelerometer Sampling Started.........");
    }
    
DONE:
    if(!bResult)
    {
        CHECK_CLOSE_ACC_HANDLE(hAcc);
    }

    LOG_END();
    return bResult;
}//AccStartSampling

//------------------------------------------------------------------------------
// AccDumpSample
//
void AccDumpSample( ACC_DATA& accData )
{
    LOG( "----------------------------------------------" );
    LOG( "ACCELEROMETER SAMPLE INFORMATION" );
    LOG( "Data Vector:.................(%f,%f,%f)", accData.x, accData.y, accData.z );
    LOG( "Sample Timestamp:............(%d)", accData.hdr.dwTimeStampMs );
    LOG( "Sensor Luid:.................(0x%08X%08X)", accData.hdr.sensorLuid.HighPart, accData.hdr.sensorLuid.LowPart );
    LOG( "Sample Size..................(%d)", accData.hdr.cbSize );
    LOG( "----------------------------------------------\n\n" )
}//AccDumpSample


//------------------------------------------------------------------------------
// Name:        PrettyPrintDevOrient
// Description: Attribute pretty print
LPSTR PrettyPrintDevOrient(DWORD dwMask)
{
    switch(dwMask)
    {
        case DEVICE_ORIENTATION_UNKNOWN:
            return ("DEVICE_ORIENTATION_UNKNOWN");
            break;
        case DEVICE_ORIENTATION_PORTRAIT_UP:
            return ("DEVICE_ORIENTATION_PORTRAIT_UP");
            break;
        case DEVICE_ORIENTATION_LANDSCAPE_LEFT:
            return ("DEVICE_ORIENTATION_LANDSCAPE_LEFT");
            break;
        case DEVICE_ORIENTATION_PORTRAIT_DOWN:
            return ("DEVICE_ORIENTATION_PORTRAIT_DOWN");
            break;
        case DEVICE_ORIENTATION_LANDSCAPE_RIGHT:
            return ("DEVICE_ORIENTATION_LANDSCAPE_RIGHT");
            break;
        default:
            return ("UNKNOWN ORIENTATION");
            break;
    }
}//PrettyPrintDevOrient


//------------------------------------------------------------------------------
// AccDumpOrientSample
//
void AccDumpOrientSample( ACC_ORIENT& accData )
{
    LOG( "----------------------------------------------" );
    LOG( "ACCELEROMETER ORIENTATION SAMPLE INFORMATION" );
    LOG( "Orientation..................(%s)",PrettyPrintDevOrient(accData.devOrientation) );
    LOG( "Sample Timestamp:............(%d)", accData.hdr.dwTimeStampMs );
    LOG( "Sensor Luid:.................(0x%08X%08X)", accData.hdr.sensorLuid.HighPart, accData.hdr.sensorLuid.LowPart );
    LOG( "Sample Size..................(%d)", accData.hdr.cbSize );   
    LOG( "----------------------------------------------\n\n" )
}//AccDumpOrientSample





//------------------------------------------------------------------------------
// AccDumpSampleToFile
//
void AccDumpSampleToFile( ACC_DATA* acc_data , BOOL bClose/*FALSE*/ )
{
    LOG_START();   


    static FILE * pOutFile = NULL;

    static DOUBLE dwAvgTime = 0;
    static DWORD dwLastTime = 0;
    static DWORD dwDeltaTime = 0;
    static DWORD dwSampleCount = 0;
    
    static ACC_VECTOR acc_AvgData = {0};
    static ACC_VECTOR acc_LastData = {0};
    static ACC_VECTOR acc_DeltaData = {0};


    static TCHAR szFileName[MAX_PATH] = _T("");

    HRESULT hResult;

    if( bClose )
    {
        if( pOutFile )
        {
            fclose( pOutFile );
        }

        pOutFile = NULL;

        goto DONE;
    }

    if( !pOutFile )
    {
    
        //Starting new log, clear all variables   
        dwAvgTime = 0;
        dwLastTime = 0;
        dwDeltaTime = 0;
        dwSampleCount = 0;
        acc_AvgData;
        acc_LastData;
        acc_DeltaData;

        // Close the old one
        if( pOutFile != NULL )
        {        
              fclose( pOutFile );
        }

        // Generate new log file name
        hResult = StringCchPrintf(szFileName, _countof(szFileName), _T("%\\Release\\_acc_data_file_%08d.log"), GetTickCount());
        if (FAILED(hResult))
        {
            LOG_WARN("Failed to create output file name.");
            goto DONE;
        }

        //Open it...
        if (0 != _wfopen_s(&pOutFile, szFileName, L"w"))
        {
            LOG_WARN("Failed to open output file %s", szFileName);
            goto DONE;
        }
        else
        {
            // Write out unicode stamp into the file
              _ftprintf(pOutFile, _T("\nLAST_X\tLAST_Y\tLAST_Z\tLAST_TS\tAVG_X\tAVG_Y\tAVG_Z\tDEL_X\tDEL_Y\tDEL_Z\tDEL_TS\r\n"));
        }        
    }
  
    if( acc_data && pOutFile )
    {


        if( dwSampleCount == 0 )
        {
            //First Sample...          
            acc_LastData.x = acc_data->x; 
            acc_LastData.y = acc_data->y;
            acc_LastData.z = acc_data->z;
            dwLastTime = acc_data->hdr.dwTimeStampMs;   

        }
        else
        {
            //Running Average
            acc_AvgData.x =  (acc_AvgData.x + acc_LastData.x ) * .5;
            acc_AvgData.y =  (acc_AvgData.y + acc_LastData.y ) * .5;
            acc_AvgData.z =  (acc_AvgData.z + acc_LastData.z ) * .5;
            dwAvgTime = ( dwAvgTime + dwLastTime ) * .5;

            //Delta
            acc_DeltaData.x = acc_data->x - acc_LastData.x;
            acc_DeltaData.y = acc_data->y - acc_LastData.y;
            acc_DeltaData.z = acc_data->z - acc_LastData.z;
            dwDeltaTime = acc_data->hdr.dwTimeStampMs - dwLastTime;

            //Log it
            _ftprintf(pOutFile,_T("%02.6f\t%02.6f\t%02.6f\t%08d\t%02.6f\t%02.6f\t%02.6f\t%02.6f\t%02.6f\t%02.6f\t%08d\r\n"),
                acc_LastData.x,   acc_LastData.y, acc_LastData.z, dwLastTime, 
                acc_AvgData.x,    acc_AvgData.y,  acc_AvgData.z, 
                acc_DeltaData.x,  acc_DeltaData.y,acc_DeltaData.z,dwDeltaTime );

        }
        
            
        dwSampleCount++;

    }

DONE:

    LOG_END();
}//AccDumpSampleToFile


//------------------------------------------------------------------------------
// AccAvailable
//
BOOL AccAvailable( DEVMGR_DEVICE_INFORMATION& di )
{
    LOG_START();
        
    BOOL bResult = TRUE;
    HANDLE hDev = FindFirstDevice( DeviceSearchByGuid,(LPCVOID)&g_guid3dAccelerometer2 , &di);
    if( hDev == INVALID_HANDLE_VALUE )
    {
        LOG_ERROR( "No Accelerometer Device Found" );
    }

    //Got what we needed
    FindClose( hDev );
    
DONE:
    LOG_END();
    return bResult;
}//AccAvailable

//------------------------------------------------------------------------------
// AccGetDriverHandle
//
BOOL AccGetDriverHandle( HANDLE& hHandle )
{
    BOOL bResult = TRUE;
    hHandle = CreateFile(
        _T("ACC1:"),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if( hHandle == INVALID_HANDLE_VALUE )
    {
        LOG_ERROR( "Failed to Accelerometer Driver" );
    }

DONE:
    return bResult;
}//AccGetDriverHandle


//------------------------------------------------------------------------------
// AccStartViaMdd
//
BOOL AccStartViaMdd( HANDLE& hDev, HANDLE& hQueue )
{
    LOG_START();
    BOOL bResult = TRUE;
    DWORD dwResult = 0;

    if( !AccGetDriverHandle( hDev ) )
    {
        LOG_ERROR("Can't Access Driver" );
    }

    if( !CreateNominalMessageQueue(hQueue) )
    {
        LOG_ERROR( "Queue Creation Failed" );
    }

    if( !DeviceIoControl(
        hDev,
        IOCTL_ACC_STARTSENSOR,        
        hQueue,
        sizeof(HANDLE),
        &dwResult,
        sizeof(DWORD),
        NULL,
        NULL) )
    {
        LOG_ERROR( "START Command Failed" );
    }

DONE:

    LOG_END();
    return bResult;
}//AccStartViaMdd


//------------------------------------------------------------------------------
// AccStopViaMdd
//
BOOL AccStopViaMdd( HANDLE& hDev )
{
    LOG_START();
    BOOL bResult = TRUE;
    DWORD dwResult = 0;

    if( !DeviceIoControl(
        hDev,
        IOCTL_ACC_STOPSENSOR,        
        NULL,
        0,
        &dwResult,
        sizeof(DWORD),
        NULL,
        NULL) )
    {
        LOG_ERROR( "STOP Command Failed" );
    }

DONE:

    LOG_END();
    return bResult;
}//AccStopViaMdd



//------------------------------------------------------------------------------
// UnloadDriver
//
BOOL UnloadDriver(DEVMGR_DEVICE_INFORMATION *di)
{
    LOG_START();
    BOOL bResult = ChangeDriverState(FALSE, di);
    LOG_END(); 
    return bResult;
}//UnloadDriver



//------------------------------------------------------------------------------
// LoadDriver
//
BOOL LoadDriver(DEVMGR_DEVICE_INFORMATION *di)
{
    LOG_START();
    BOOL bResult = ChangeDriverState(TRUE, di);
    LOG_END(); 
    return bResult;

}//LoadDriver


//------------------------------------------------------------------------------
// ChangeDriverState
//
BOOL ChangeDriverState( BOOL bLoadDriver, DEVMGR_DEVICE_INFORMATION *di)
{
    LOG_START();
    HANDLE hParentDevice = INVALID_HANDLE_VALUE;
    ce::auto_hfile hfParent = INVALID_HANDLE_VALUE;
    DEVMGR_DEVICE_INFORMATION parent_di;
    BOOL bResult = TRUE;

    RETAILMSG( TRUE, (_T("...Preparing to %s driver:"), bLoadDriver?_T("LOAD"):_T("UNLOAD")));

    // What are we trying to change now?
    VERIFY_STEP( "Verifying Driver Information", (di != NULL), TRUE );
    DumpDevMgrDevInfo(di);

    // Trust but verify.....
    hParentDevice = di->hParentDevice;
    VERIFY_STEP( "Verifying Parent Device Handle", ((hParentDevice != NULL)&(hParentDevice != INVALID_HANDLE_VALUE)), TRUE );

    // Out, damn'd bits! out, I say!
    memset(&parent_di, 0, sizeof(parent_di));
    parent_di.dwSize = sizeof(parent_di);
    bResult = GetDeviceInformationByDeviceHandle(hParentDevice,&parent_di);
    VERIFY_STEP( "Querying Parent Information", bResult, TRUE );

    // Deja Vu & make life easier
    RETAILMSG( TRUE, (_T("...Displaying Parent Driver Information:")));
    DumpDevMgrDevInfo(&parent_di);
    LPCTSTR pszBusName = di->szBusName;
    LPCTSTR pszParentBusName = parent_di.szBusName;

    // Need a handle to the parent device    
    RETAILMSG( TRUE, (_T("...Opening Handle For (%s):"), pszParentBusName));
    hfParent = CreateFile(pszParentBusName, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
    VERIFY_STEP( "Verifying Open Handle", hfParent.valid(), TRUE );
    
    // Do the deed... unload the child
    bResult = DeviceIoControl(
        hfParent, 
        ((bLoadDriver)?(IOCTL_BUS_ACTIVATE_CHILD):(IOCTL_BUS_DEACTIVATE_CHILD)),
        (PVOID)pszBusName, 
        (_tcslen(pszBusName) + 1) * sizeof(*pszBusName), 
        NULL, 
        0, 
        NULL, 
        NULL);    
    VERIFY_STEP( "Child Device State Change", bResult, TRUE );

DONE:
    LOG_END();
    return bResult;
}//ChangeDriverState

//------------------------------------------------------------------------------
// GetValueForParam
//
BOOL GetValueForParam( const TCHAR* pszCmdLine, const TCHAR* pszParam, TCHAR* pszVal, DWORD dwValSize )
{

    if( pszCmdLine == NULL || pszParam == NULL || pszVal == NULL || dwValSize == 0 )
        return FALSE;
    
    TCHAR szCmdLineCpy[MAX_PATH];
    TCHAR* cmdTokens[MAX_PATH] = {NULL};
    TCHAR* pszCurToken;
    TCHAR *token=NULL;
    DWORD dwIdx = 0;
    BOOL bFound = FALSE;
    HRESULT hResult;

    //We're going to kill the cmd line..so copy it
    hResult = StringCchCopyEx(szCmdLineCpy, MAX_PATH, pszCmdLine, NULL, NULL, STRSAFE_NULL_ON_FAILURE);

    if( !SUCCEEDED(hResult) )
        return FALSE;
    
    //Look for our parameter 
    cmdTokens[dwIdx] = wcstok_s (szCmdLineCpy,_T(" "), &token);
    while ( cmdTokens[dwIdx] != NULL && !bFound && dwIdx < MAX_PATH)
    { 
        pszCurToken = cmdTokens[dwIdx];
        if( _tcscmp( pszParam, pszCurToken+1) == 0 )
        {
            bFound = TRUE;
        }

        dwIdx++;
        cmdTokens[dwIdx] = wcstok_s(NULL, _T(" "), &token);
        
        if( bFound )
        {
            hResult = StringCchCopyEx(pszVal, dwValSize, cmdTokens[dwIdx], NULL, NULL, STRSAFE_NULL_ON_FAILURE);
            
            if( !SUCCEEDED(hResult) )
                return FALSE;
        }
    }

    return bFound;
}


//------------------------------------------------------------------------------------
// GetAccRegistryEntry
// CE7 makes no restrictions on the name of the registry key entry associated
// with this driver. Therefore, this function attempts to provide this info
// by looking at a likely place in the registry, and fails silently if not found.
//
// Return Value: TRUE (always)
//-----------------------------------------------------------------------------------
BOOL GetAccRegistryEntry(AccRegEntry& accReg)
{
    LOG_START();

    BOOL        bResult = TRUE;
    HANDLE      hFind = INVALID_HANDLE_VALUE;
    HKEY        hAccKey = NULL;
    DWORD       dwType  = 0;
    DWORD       dwSize = 0; 
    LONG        lResult = ERROR_SUCCESS;

    memset(&accReg.mdd, 0, sizeof(accReg.mdd));
    //If possible open, read and all ACC registry keys
    LOG( "Opening HKLM\\%s", pszAccKeyMdd );
    lResult = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE, 
        pszAccKeyMdd, 
        0, 
        0,
        &hAccKey);
    if( lResult != ERROR_SUCCESS )
    {
        LOG( "Could not open HKLM\\%s Result:(%d)", pszAccKeyMdd, lResult );
        goto DONE;
    }

         
    LOG( "Querying HKLM\\%s\\%s", pszAccKeyMdd, DEVLOAD_DLLNAME_VALNAME);
    dwSize = _countof( accReg.mdd.szDll );
    lResult = RegQueryValueEx(
        hAccKey,
        DEVLOAD_DLLNAME_VALNAME,
        NULL,
        &dwType,
        (LPBYTE)&accReg.mdd.szDll,
        &dwSize);
    if( lResult != ERROR_SUCCESS )
    {
        LOG( "Could not read HKLM\\%s\\%s Result:(%d)", pszAccKeyMdd, DEVLOAD_DLLNAME_VALNAME, lResult );
    }
        
    LOG( "Querying HKLM\\%s\\%s", pszAccKeyMdd, DEVLOAD_PREFIX_VALNAME);
    dwSize = _countof( accReg.mdd.szPrefix );
    lResult = RegQueryValueEx(
        hAccKey,
        DEVLOAD_PREFIX_VALNAME,
        NULL,
        &dwType,
        (LPBYTE)&accReg.mdd.szPrefix,
        (LPDWORD)&dwSize);
    if( lResult != ERROR_SUCCESS )
    {
        LOG( "Could not read HKLM\\%s\\%s Result:(%d)", pszAccKeyMdd, DEVLOAD_PREFIX_VALNAME, lResult );
    }
          
    LOG( "Querying HKLM\\%s\\%s", pszAccKeyMdd, DEVLOAD_ICLASS_VALNAME);
    dwSize = _countof( accReg.mdd.szIClass );
    lResult = RegQueryValueEx(
        hAccKey,
        DEVLOAD_ICLASS_VALNAME,
        NULL,
        &dwType,
        (LPBYTE)&accReg.mdd.szIClass,
        (LPDWORD)&dwSize);
    if( lResult != ERROR_SUCCESS )    
    {
        LOG( "Could not read HKLM\\%s\\%s Result:(%d)", pszAccKeyMdd, DEVLOAD_ICLASS_VALNAME, lResult );
    }              

    LOG( "Querying HKLM\\%s\\%s", pszAccKeyMdd,DEVLOAD_LOADORDER_VALNAME);
    dwSize = sizeof( accReg.mdd.dwOrder );
    lResult = RegQueryValueEx(
        hAccKey,
        DEVLOAD_LOADORDER_VALNAME,
        NULL,
        &dwType,
        (LPBYTE)&accReg.mdd.dwOrder,
        (LPDWORD)&dwSize);
    if( lResult != ERROR_SUCCESS )
    {
        LOG( "Could not read HKLM\\%s\\%s Result:(%d)", pszAccKeyMdd, DEVLOAD_LOADORDER_VALNAME, lResult );
    }         

DONE:
    DumpAccRegEntries( accReg );
    CHECK_CLOSE_KEY( hAccKey );
    
    LOG_END();
    return bResult;
}//GetAccRegistryEntry


//------------------------------------------------------------------------------
// Name: ProcessCmdLine
//
BOOL ProcessCmdLine(LPCWSTR pszCmdLine)
{
    LOG_START();
    CClParse* pCmdLine = NULL;
    BOOL bResult = TRUE;
    
    g_AccParam.bUserMode = FALSE;
    g_AccParam.bFakePdd = FALSE;
    g_AccParam.dwNumberOfClients = CMD_CLIENTS_DEF;
    g_AccParam.accHome.x = 0;
    g_AccParam.accHome.y = 0;
    g_AccParam.accHome.z = 0;
    g_AccParam.dwNumberOfSamples = CMD_SAMPLES_DEF;
    g_AccParam.dwTimeErrorMargin = CMD_TIME_MARGIN_DEF; //ms
    g_AccParam.dwDataErrorMargin = CMD_DATA_MARGIN_DEF; //mg
    
    // Read in the commandline
    if (pszCmdLine)
    {
        pCmdLine = new CClParse(pszCmdLine);
        if (!pCmdLine)
        {
            LOG_ERROR( "Cannot Allocate Memeory" );
        }
    }
    else
    {
        LOG_ERROR( "Argument Error: No Arguments" );
    }


    //----------------------------------------------
    //Parse Help
    if( pCmdLine->GetOpt(CMD_HELP) )
    {
        //Usage();
        goto DONE;
    }

    
    //----------------------------------------------
    //Parse Number of Clients 
    //
    if( pCmdLine->GetOpt(CMD_CLIENTS) )
    {
        if( pCmdLine->GetOptVal(CMD_CLIENTS, &(g_AccParam.dwNumberOfClients)))
        {
            if( g_AccParam.dwNumberOfClients < CMD_CLIENTS_MIN || 
                g_AccParam.dwNumberOfClients > CMD_CLIENTS_MAX )
            {
                g_AccParam.dwNumberOfClients = CMD_CLIENTS_DEF;
                LOG_WARN( "WARNING: Parameter (%s) out of range [%d,%d]. Setting to %d",
                    CMD_CLIENTS, CMD_CLIENTS_MIN, CMD_CLIENTS_MAX, CMD_CLIENTS_DEF);
            }               
        }     
    }

    //----------------------------------------------
    //Parse Number of Samples 
    //
    if( pCmdLine->GetOpt(CMD_SAMPLES) )
    {
        if( pCmdLine->GetOptVal(CMD_SAMPLES, &(g_AccParam.dwNumberOfSamples)))
        {
            if( g_AccParam.dwNumberOfClients < CMD_SAMPLES_MIN || 
                g_AccParam.dwNumberOfClients > CMD_SAMPLES_MAX )
            {
                g_AccParam.dwNumberOfClients = CMD_SAMPLES_DEF;
                LOG_WARN( "WARNING: Parameter (%s) out of range [%d,%d]. Setting to %d",
                    CMD_SAMPLES, CMD_SAMPLES_MIN, CMD_SAMPLES_MAX, CMD_SAMPLES_DEF);
            }               
        }     
    }

    //----------------------------------------------
    //Parse Time Error Margin 
    //
    if( pCmdLine->GetOpt(CMD_TIME_MARGIN) )
    {
        if( pCmdLine->GetOptVal(CMD_TIME_MARGIN, &(g_AccParam.dwTimeErrorMargin)))
        {
            if( g_AccParam.dwTimeErrorMargin < CMD_TIME_MARGIN_MIN || 
                g_AccParam.dwTimeErrorMargin > CMD_TIME_MARGIN_MAX )
            {
                g_AccParam.dwTimeErrorMargin = CMD_TIME_MARGIN_DEF;
                LOG_WARN( "WARNING: Parameter (%s) out of range [%d,%d]. Setting to %d",
                    CMD_TIME_MARGIN, CMD_TIME_MARGIN_MIN, CMD_TIME_MARGIN_MAX, CMD_TIME_MARGIN_DEF);
            }               
        }     
    }

    //----------------------------------------------
    //Parse Data Error Margin 
    //
    if( pCmdLine->GetOpt(CMD_DATA_MARGIN) )
    {
        if( !pCmdLine->GetOptVal(CMD_DATA_MARGIN, &(g_AccParam.dwDataErrorMargin)))
        {
            LOG_WARN("WARNING: Value for parameter (%s) could not be parsed. Setting to 0",
                CMD_DATA_MARGIN); 
            g_AccParam.dwDataErrorMargin = 0;        
        }     
    }

    //----------------------------------------------
    //Parse Home Vector - X-Coordinate
    //
    if( pCmdLine->GetOpt(CMD_SET_X) )
    {
        TCHAR szParam[10];
        if( !GetValueForParam( pszCmdLine, CMD_SET_X, szParam, 10 ) )
        {
            LOG_WARN("WARNING: Value for parameter (%s) could not be parsed. Setting to 0",
                CMD_SET_X); 
            g_AccParam.accHome.x = 0;
        }
        else
        {
            g_AccParam.accHome.x = _ttoi(szParam);
        }
    }


    //----------------------------------------------
    //Parse Home Vector - Y-Coordinate
    //
    if( pCmdLine->GetOpt(CMD_SET_Y) )
    {
        TCHAR szParam[10];
        if( !GetValueForParam( pszCmdLine, CMD_SET_Y, szParam, 10 ) )
        {
                LOG_WARN("WARNING: Value for parameter (%s) could not be parsed. Setting to 0",
                CMD_SET_Y); 
            g_AccParam.accHome.y = 0;
        }
        else
        {
            g_AccParam.accHome.y = _ttoi(szParam);
        }
    }


    //----------------------------------------------
    //Parse Home Vector - Z-Coordinate
    //
    if( pCmdLine->GetOpt(CMD_SET_Z) )
    {
        TCHAR szParam[10];
        if( !GetValueForParam( pszCmdLine, CMD_SET_Z, szParam, 10 ) )
        {
            LOG_WARN("WARNING: Value for parameter (%s) could not be parsed. Setting to 0",
                CMD_SET_Z); 
            g_AccParam.accHome.z = 0;
        }
        else
        {
            g_AccParam.accHome.z = _ttoi(szParam);
        }
    }


    //Figure out if we are going to be using our fake pdd 
    //For now base this on whether or not the module name is loaded
    g_AccParam.bFakePdd = IsModuleInstalled( _T("fakeaccpdd.dll"));    


    LOG("#### Accelerometer Test Suite Parameters");
    LOG("...User Mode...........(%s)",g_AccParam.bUserMode?(_T("USER")):(_T("KERNEL")));
    LOG("...Fake PDD............(%s)",g_AccParam.bFakePdd?(_T("STUB PDD")):(_T("DEVICE PDD")));
    LOG("...Number of Clients...(%d)",g_AccParam.dwNumberOfClients);
    LOG("...X-Coordinate Home...(%d)",g_AccParam.accHome.x);
    LOG("...Y-Coordinate Home...(%d)",g_AccParam.accHome.y);
    LOG("...Z-Coordinate Home...(%d)",g_AccParam.accHome.z);
    LOG("...Number of Samples...(%d)",g_AccParam.dwNumberOfSamples);
    LOG("...Time Error Margin...(%d)",g_AccParam.dwTimeErrorMargin); 
    LOG("...Data Error Margin...(%d)",g_AccParam.dwDataErrorMargin);
    LOG("...");

    LOG("#### Generating Shared Map Area for Parameters");

   
            
DONE:
    if(pCmdLine)        
        delete pCmdLine;

    LOG_END();
    return bResult;
}//ProcessCmdLine


//------------------------------------------------------------------------------
// Name:        IsModuleInstalled
//
BOOL IsModuleInstalled(LPCWSTR pszModName)
{
    BOOL bResult = TRUE;
    BOOL bFound = FALSE;
    MODULEENTRY32 me32 = {0};
    me32.dwSize = sizeof (MODULEENTRY32);

    HANDLE hModuleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_GETALLMODS, 0);
    if( hModuleSnapshot != INVALID_HANDLE_VALUE )
    {
        Module32First (hModuleSnapshot, &me32);
        do
        {
            if( _tcsstr( me32.szModule, pszModName ))
            {
                bFound = TRUE;
                LOG("Found Module(%s) @ (%s)", me32.szModule, me32.szExePath );
                break;
            }

        }
        while (Module32Next (hModuleSnapshot, & me32));
    }
    else
    {
        LOG_ERROR( "Could Not Access Module List" );
    }
DONE:
    CloseToolhelp32Snapshot( hModuleSnapshot );
    return bResult&&bFound;
}//IsModuleInstalled

//------------------------------------------------------------------------------
// Name: AccConvertToStaticAngles
//
BOOL AccConvertToStaticAngles( ACC_DATA* accData, DOUBLE& alphaPitch, DOUBLE& alphaRoll )
{
    LOG_START();
    BOOL bResult = TRUE;
    double fX = 0;
    double fY = 0;
    double fZ = 0 ;
    
    if( accData == NULL )
    {
        LOG_ERROR( "Null Data Pointer Provided");
    }
    
    fX = accData->x ;
    fY = accData->y ;
    fZ = accData->z ;

    //------------------------------------------------------------------
    //Pitch
    if( fY == 0 )
    {
        // Special case for disconnect between Quadrants I && IV and
        // Quadrants II & III 
        if( fZ > 0 )
        {
            alphaPitch = 180;
        }
        else
        {
            alphaPitch = 0;            
        }
    }
    else
    {   
        alphaPitch = ( -1 * atan( fZ/fY) ) * ( 180 / M_PI );

        if( fY < 0  )
        {
            //Quadrant I & II
            alphaPitch += 90;
        }
         else if ( fY > 0 )
        {
            //Quadrant III & IV
            alphaPitch += 270;
        }
    }

    //------------------------------------------------------------------
    //Roll
    if( fX == 0 )
    {
        // Special case for disconnect between Quadrants I && IV and
        // Quadrants II & III 
        if( fZ > 0 )
        {
            alphaRoll = 180;
        }
        else
        {
            alphaRoll = 0;            
        }
    }
    else
    {    
        alphaRoll = ( -1 * atan( fZ/fX) ) * ( 180 / M_PI );

        if( fX < 0  )
        {
            //Quadrant I & II
            alphaRoll += 90;
        }
        else if ( fX > 0 )
        {
            //Quadrant III & IV
            alphaRoll += 270;
        }
    }
DONE:    
    LOG_END();
    return bResult;
}//AccConvertToStaticAngles



//------------------------------------------------------------------------------
// Name: rotationLightCallback
//
void rotationLightCallback(ACC_ORIENT* acc_data )
{
    //Empty for performance measurement
}//rotationLightCallback


//------------------------------------------------------------------------------
// Name: streamLightCallback
//
void streamLightCallback(ACC_DATA* acc_data )
{
    //Empty for performance measurement
}//streamLightCallback

//------------------------------------------------------------------------------
// Name: rotationCallback
//
void rotationCallback( ACC_ORIENT* acc_data )
{
    static int cnt = 0;
    cnt++;
    LOG( "Detected Orientation Change: %d", cnt );
    AccDumpOrientSample( *acc_data );
}//rotationCallback

//------------------------------------------------------------------------------
// Name: streamCallback
//
void streamCallback( ACC_DATA* acc_data )
{
    static int cnt = 0;
    static DOUBLE alphaPitch = 0;
    static DOUBLE alphaRoll = 0;
    AccConvertToStaticAngles( acc_data, alphaPitch, alphaRoll );
    LOG( "%d. [%d]: (%3.2f, %3.2f, %3.2f) (P:%3.2f  R:%3.2f)", 
        cnt, 
        acc_data->hdr.dwTimeStampMs, 
        acc_data->x, 
        acc_data->y, 
        acc_data->z, 
        alphaPitch, 
        alphaRoll );
}//streamCallback


//------------------------------------------------------------------------------
// Name: AccDetectRotation
//
BOOL AccDetectRotation( DWORD dwSleepInterval/* 10 SECONDS */, BOOL bLog /*TRUE*/)
{
    LOG_START();    
    BOOL bResult = TRUE;
    DWORD dwResult = ERROR_SUCCESS;
    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    
    if( !AccGetHandle(hAcc, sensorLuid) )
    {
        LOG_ERROR( "Could not acquire handle" );
    }
 
    AccelerometerSetMode(hAcc, ACC_CONFIG_ORIENTATION_MODE, 0);                  
    
    if( bLog )
    {
        dwResult = f_AccelerometerCreateCallback(hAcc,(ACCELEROMETER_CALLBACK*)rotationCallback, NULL );
    }
    else
    {
        dwResult = f_AccelerometerCreateCallback(hAcc,(ACCELEROMETER_CALLBACK*)rotationLightCallback, NULL );
    }

    if( ERROR_SUCCESS != dwResult)
    {
        LOG_ERROR( "Could not attach callback" );    
    }
    
    Sleep( dwSleepInterval );

    if( ERROR_SUCCESS != f_AccelerometerCancelCallback(hAcc) )
    {
        LOG_ERROR( "Could not cancel callback" );
    }
   
DONE:   
    f_AccelerometerStop( hAcc );
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult;
}//AccDetectRotation

//------------------------------------------------------------------------------
// Name: AccDataStream
//
BOOL AccDataStream( DWORD dwSleepInterval /* 10 SECONDS */, BOOL bLog /*TRUE*/ )
{
    LOG_START();    
    BOOL bResult = TRUE;
    DWORD dwResult = ERROR_SUCCESS;
    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    
    if( !AccGetHandle(hAcc, sensorLuid) )
    {
        LOG_ERROR( "Could not acquire handle" );
    }
 
    AccelerometerSetMode(hAcc, ACC_CONFIG_STREAMING_DATA_MODE, 0);                  
    
    if( bLog )
    {
        dwResult = f_AccelerometerCreateCallback(hAcc,(ACCELEROMETER_CALLBACK*)streamCallback, NULL );
    }
    else
    {
        dwResult = f_AccelerometerCreateCallback(hAcc,(ACCELEROMETER_CALLBACK*)streamLightCallback, NULL );
    }

    if( ERROR_SUCCESS != dwResult)
    {
        LOG_ERROR( "Could not attach callback" );    
    }
    
    Sleep( dwSleepInterval );

    if( ERROR_SUCCESS != f_AccelerometerCancelCallback(hAcc) )
    {
        LOG_ERROR( "Could not cancel callback" );
    }
   
DONE:   
    f_AccelerometerStop( hAcc );
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult;
}//AccDataStream;