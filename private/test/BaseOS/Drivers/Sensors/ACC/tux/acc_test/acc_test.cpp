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

#include "acc_test.h"

#include "CClientManager.h"
#include "AccApiFunctional.h"
#include "AccApiScenario.h"

#include "acc_ioctl.h"

extern AccTestParam g_AccParam;
extern HINSTANCE g_hFuncDLLInst;
extern HINSTANCE g_hScenarioDLLInst;
extern HANDLE g_hPwrReq;


 GUID g_guid3dAccelerometer =  {0xC2FB0F5F, 0xE2D2, 0x4C78, { 0xBC, 0xD0, 0x35, 0x2A, 0x95, 0x82, 0x81, 0x9D}};



//------------------------------------------------------------------------------
//call_Test: Single client funnel for now
//
BOOL call_Test( PVOID pData, DWORD dwDataSize, LPCTSTR pszFunctionName )
{
    LOG_START();
    BOOL bResult = TRUE;

    //Setup our client bypass
    CClientManager::SNS_CLIENT_FNPTR fncClientEntry = NULL;
    CClientManager::ClientPayload payload = {
        pData, 
        dwDataSize, 
        TRUE };
    
    //Load appropriate test        
    fncClientEntry =(CClientManager::SNS_CLIENT_FNPTR) GetProcAddress(g_hFuncDLLInst,pszFunctionName);
    
    if( !fncClientEntry )
    {
        LOG_ERROR( "Could not access test function (%s)", pszFunctionName);
    }

    fncClientEntry( &payload );    
    
DONE:
    LOG_END();
    return (bResult&&payload.dwOutVal);
}//call_Test



//------------------------------------------------------------------------------
//call_Test: Single client funnel for now
//
BOOL call_Test2( PVOID pData, DWORD dwDataSize, LPCTSTR pszFunctionName )
{
    LOG_START();
    BOOL bResult = TRUE;

    //Setup our client bypass
    CClientManager::SNS_CLIENT_FNPTR fncClientEntry = NULL;
    CClientManager::ClientPayload payload = {
        pData, 
        dwDataSize, 
        TRUE };
    
    //Load appropriate test        
    fncClientEntry =(CClientManager::SNS_CLIENT_FNPTR) GetProcAddress(g_hScenarioDLLInst,pszFunctionName);
    
    if( !fncClientEntry )
    {
        LOG_ERROR( "Could not access test function (%s)", pszFunctionName);
    }

    fncClientEntry( &payload );    
    
DONE:
    LOG_END();
    return (bResult&&payload.dwOutVal);
}//call_Test



//------------------------------------------------------------------------------
// Tux_OpenSensor01
//
TESTPROCAPI Tux_OpenSensor01(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;
    LUID sensorLuid = {0};
    AccOpenSensorTstData_t data = {0};    
        
    //Good case
    DEVMGR_DEVICE_INFORMATION di = {0};   
    di.dwSize = sizeof( di );
    HANDLE hDev = FindFirstDevice( DeviceSearchByGuid,(LPCVOID)&g_guid3dAccelerometer, &di);
     
    data.in_name = di.szLegacyName;
    data.in_pSensorLuid = &sensorLuid;
    data.expError = ERROR_SUCCESS;
    data.expHandle = 0;
    
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenSensorTstData_t), L"tst_OpenSensor" ))
    {
        LOG_ERROR( "Test Failed" );
    }

DONE:
    LOG_END();
    CHECK_CLOSE_FIND_HANDLE( hDev );
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_OpenSensor01  

//------------------------------------------------------------------------------
// Tux_OpenSensor02
//
TESTPROCAPI Tux_OpenSensor02(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;
    LUID sensorLuid = {0};
    AccOpenSensorTstData_t data = {0};    
    
    data.in_name = _T('\0');
    data.in_pSensorLuid = &sensorLuid;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expHandle = ACC_INVALID_HANDLE_VALUE;
    
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenSensorTstData_t), L"tst_OpenSensor" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_OpenSensor02

//------------------------------------------------------------------------------
// Tux_OpenSensor03
//
TESTPROCAPI Tux_OpenSensor03(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;
    LUID sensorLuid = {0};
    AccOpenSensorTstData_t data = {0};    
    
    data.in_name = NULL;
    data.in_pSensorLuid = &sensorLuid;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expHandle = ACC_INVALID_HANDLE_VALUE;
    
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenSensorTstData_t), L"tst_OpenSensor" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_OpenSensor03

//------------------------------------------------------------------------------
// Tux_OpenSensor04
//
TESTPROCAPI Tux_OpenSensor04(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;
    LUID sensorLuid = {0};
    AccOpenSensorTstData_t data = {0};    
    
    data.in_name = _T("ACC1:ACC");
    data.in_pSensorLuid = &sensorLuid;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expHandle = ACC_INVALID_HANDLE_VALUE;
    
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenSensorTstData_t), L"tst_OpenSensor" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_OpenSensor04


//------------------------------------------------------------------------------
// Tux_OpenSensor05
//
TESTPROCAPI Tux_OpenSensor05(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;
    LUID sensorLuid = {0};
    TCHAR szStr[MAX_PATH];
    AccOpenSensorTstData_t data = {0};    
    
    //Random string fun
    FillRandStr( szStr, MAX_PATH );

    data.in_name = szStr;
    data.in_pSensorLuid = &sensorLuid;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expHandle = ACC_INVALID_HANDLE_VALUE;
    
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenSensorTstData_t), L"tst_OpenSensor" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_OpenSensor05

//------------------------------------------------------------------------------
// Tux_OpenSensor06
//
TESTPROCAPI Tux_OpenSensor06(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;
    LUID sensorLuid = {0};
    AccOpenSensorTstData_t data = {0};    
    
    //Find the wrong device.
    DEVMGR_DEVICE_INFORMATION di = {0};   
    di.dwSize = sizeof( di );

    HANDLE hDev = FindFirstDevice( DeviceSearchByGuid,(LPCVOID)&guidAccelerometer3dType_BAD , &di);
 
    data.in_name = di.szLegacyName;
    data.in_pSensorLuid = &sensorLuid;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expHandle = ACC_INVALID_HANDLE_VALUE;
    
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenSensorTstData_t), L"tst_OpenSensor" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    CHECK_CLOSE_FIND_HANDLE( hDev );
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_OpenSensor06



//------------------------------------------------------------------------------
// Tux_OpenSensor07
//
TESTPROCAPI Tux_OpenSensor07(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;
    LUID sensorLuid = {0};
    AccOpenSensorTstData_t data = {0};    
    
    //Find the right device, use wrong data
    DEVMGR_DEVICE_INFORMATION di = {0};   
    di.dwSize = sizeof( di );
    HANDLE hDev = FindFirstDevice( DeviceSearchByGuid,(LPCVOID)&g_guid3dAccelerometer, &di);
 
    data.in_name = di.szDeviceKey;
    data.in_pSensorLuid = &sensorLuid;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expHandle = ACC_INVALID_HANDLE_VALUE;
    
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenSensorTstData_t), L"tst_OpenSensor" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    CHECK_CLOSE_FIND_HANDLE( hDev );
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_OpenSensor07

//------------------------------------------------------------------------------
// Tux_OpenSensor08
//
TESTPROCAPI Tux_OpenSensor08(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;
    LUID sensorLuid = {0};
    AccOpenSensorTstData_t data = {0};    
    
    //Find the right device, use wrong data
    DEVMGR_DEVICE_INFORMATION di = {0};   
    di.dwSize = sizeof( di );
    HANDLE hDev = FindFirstDevice( DeviceSearchByGuid,(LPCVOID)&g_guid3dAccelerometer, &di);
 
    data.in_name = di.szDeviceName;
    data.in_pSensorLuid = &sensorLuid;
    data.expError = ERROR_SUCCESS;
    data.expHandle = 0;
    
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenSensorTstData_t), L"tst_OpenSensor" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    CHECK_CLOSE_FIND_HANDLE( hDev );
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_OpenSensor08

//------------------------------------------------------------------------------
// Tux_OpenSensor09
//
TESTPROCAPI Tux_OpenSensor09(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;
    LUID sensorLuid = {0};
    AccOpenSensorTstData_t data = {0};    
    
    //Find the right device, use wrong data
    DEVMGR_DEVICE_INFORMATION di = {0};   
    di.dwSize = sizeof( di );
    HANDLE hDev = FindFirstDevice( DeviceSearchByGuid,(LPCVOID)&g_guid3dAccelerometer, &di);
 
    data.in_name = di.szBusName;
    data.in_pSensorLuid = &sensorLuid;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expHandle = ACC_INVALID_HANDLE_VALUE;
    
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenSensorTstData_t), L"tst_OpenSensor" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    CHECK_CLOSE_FIND_HANDLE( hDev );
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_OpenSensor09

//------------------------------------------------------------------------------
// Tux_OpenSensor10
//
TESTPROCAPI Tux_OpenSensor10(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START(); 
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;
    LUID sensorLuid = {0};
    AccOpenSensorTstData_t data = {0};    
    
    data.in_name = _T("ACC1");
    data.in_pSensorLuid = &sensorLuid;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expHandle = ACC_INVALID_HANDLE_VALUE;
    
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenSensorTstData_t), L"tst_OpenSensor" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_OpenSensor10


//------------------------------------------------------------------------------
// Tux_OpenSensor11
//
TESTPROCAPI Tux_OpenSensor11(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;
    AccOpenSensorTstData_t data = {0};    
    
    //Bad LUID
    DEVMGR_DEVICE_INFORMATION di = {0};   
    di.dwSize = sizeof( di );
    HANDLE hDev = FindFirstDevice( DeviceSearchByGuid,(LPCVOID)&g_guid3dAccelerometer, &di);
 
    data.in_name = di.szLegacyName;
    data.in_pSensorLuid = NULL;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expHandle = ACC_INVALID_HANDLE_VALUE;
    
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenSensorTstData_t), L"tst_OpenSensor" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    CHECK_CLOSE_FIND_HANDLE( hDev );
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_OpenSensor11



//------------------------------------------------------------------------------
// Tux_AccelerometerStart01
//
TESTPROCAPI Tux_AccelerometerStart01(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) 
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg);        
   
    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE; 
    AccOpenAccStartTstData_t data = {0};
    LUID sensorLuid = {0};
    
    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }       

    CreateNominalMessageQueue(hQNominal);   
    
    data.hAcc = hAcc;
    data.hQueue = hQNominal;
    data.expError = ERROR_SUCCESS; 
    data.expSamples = TRUE;
          
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenAccStartTstData_t), L"tst_AccelerometerStart"))
    {
        LOG_ERROR( "Test Failed" );
    }

    if( f_AccelerometerStop(hAcc) != ERROR_SUCCESS)
    {
        LOG_ERROR( "Test Cleanup Failed" );
    }
    
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQNominal);        
        
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerStart01


//------------------------------------------------------------------------------
// Tux_AccelerometerStart02
//
TESTPROCAPI Tux_AccelerometerStart02(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg);        
    
    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccOpenAccStartTstData_t data = {0};
    LUID sensorLuid = {0};
    
    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }       

    CreateNominalMessageQueue(hQNominal);   
    
    data.hAcc = hAcc;
    data.hQueue = INVALID_HANDLE_VALUE;
    data.expError = ERROR_INVALID_PARAMETER; 
    data.expSamples = FALSE;
       
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenAccStartTstData_t), L"tst_AccelerometerStart"))
    {
        LOG_ERROR( "Test Failed" );
    }

    
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQNominal);        
        
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerStart02

//------------------------------------------------------------------------------
// Tux_AccelerometerStart03
//
TESTPROCAPI Tux_AccelerometerStart03(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg);        
    
    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccOpenAccStartTstData_t data = {0};
    LUID sensorLuid = {0};

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }       

    CreateNominalMessageQueue(hQNominal);   
    
    data.hAcc = NULL;
    data.hQueue = hQNominal;
    data.expError = ERROR_INVALID_PARAMETER; 
    data.expSamples = FALSE;
          
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenAccStartTstData_t), L"tst_AccelerometerStart"))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQNominal);        
        
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerStart03


//------------------------------------------------------------------------------
// Tux_AccelerometerStart04
//
TESTPROCAPI Tux_AccelerometerStart04(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg);        
    
    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccOpenAccStartTstData_t data = {0};
    LUID sensorLuid = {0};
    
    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }       

    CreateNominalMessageQueue(hQNominal);   
    
    data.hAcc = hAcc;
    data.hQueue = NULL;
    data.expError = ERROR_INVALID_PARAMETER; 
    data.expSamples = FALSE;
         
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenAccStartTstData_t), L"tst_AccelerometerStart"))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQNominal);        
        
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerStart04

//------------------------------------------------------------------------------
// Tux_AccelerometerStart05
//
TESTPROCAPI Tux_AccelerometerStart05(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg);        
    
    BOOL bResult = TRUE;    
    HANDLE hQTest = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccOpenAccStartTstData_t data = {0};
    LUID sensorLuid = {0};

    MSGQUEUEOPTIONS testQOpt = 
    {
        (Q_NOM_SIZE - 1),
        Q_NOM_FLAG,
        Q_NOM_MAXMSG, 
        Q_NOM_MAXMSG_SIZE, 
        Q_NOM_ACCESS,
    };

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }       

    //Special Queue case
    hQTest = CreateMsgQueue( L"TSTQ", &testQOpt );

   
    data.hAcc = hAcc;
    data.hQueue = hQTest;
    data.expError = ERROR_INVALID_PARAMETER; 
    data.expSamples = FALSE;
          
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenAccStartTstData_t), L"tst_AccelerometerStart"))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQTest);        
        
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerStart05

//------------------------------------------------------------------------------
// Tux_AccelerometerStart06
//
TESTPROCAPI Tux_AccelerometerStart06(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg);        
     
    BOOL bResult = TRUE;    
    HANDLE hQTest = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccOpenAccStartTstData_t data = {0};
    LUID sensorLuid = {0};

    //Special Queue case
    MSGQUEUEOPTIONS testQOpt = 
    {
        (Q_NOM_SIZE + 100),
        Q_NOM_FLAG,
        Q_NOM_MAXMSG, 
        Q_NOM_MAXMSG_SIZE, 
        Q_NOM_ACCESS,
    };

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }       

    hQTest = CreateMsgQueue( L"TSTQ", &testQOpt );
    
    data.hAcc = hAcc;
    data.hQueue = hQTest;
    data.expError = ERROR_INVALID_PARAMETER; 
    data.expSamples = FALSE;
          
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenAccStartTstData_t), L"tst_AccelerometerStart"))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQTest);        
         
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerStart06

//------------------------------------------------------------------------------
// Tux_AccelerometerStart07
//
TESTPROCAPI Tux_AccelerometerStart07(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg);        
    
    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccOpenAccStartTstData_t data = {0};
    LUID sensorLuid = {0};
    
    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }       

    CreateNominalMessageQueue(hQNominal);   
    
    data.hAcc = hAcc;
    data.hQueue = NULL;
    data.expError = ERROR_INVALID_PARAMETER; 
    data.expSamples = FALSE;
          
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenAccStartTstData_t), L"tst_AccelerometerStart"))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQNominal);        
        
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerStart07

//------------------------------------------------------------------------------
// Tux_AccelerometerStart08
//
TESTPROCAPI Tux_AccelerometerStart08(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg);        

    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccOpenAccStartTstData_t data = {0};
    LUID sensorLuid = {0};
    
    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }       

    CreateNominalMessageQueue(hQNominal);   

    data.hAcc = ACC_INVALID_HANDLE_VALUE;
    data.hQueue = hQNominal;
    data.expError = ERROR_INVALID_PARAMETER; 
    data.expSamples = FALSE;
   
    if( !call_Test( (PVOID)(&data), sizeof(AccOpenAccStartTstData_t), L"tst_AccelerometerStart"))
    {
        LOG_ERROR( "Test Failed" );
    }

DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQNominal);        
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerStart08



//------------------------------------------------------------------------------
// Tux_AccelerometerStop01
//
TESTPROCAPI Tux_AccelerometerStop01(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccOpenAccStopTstData_t stopData = {0};    

    if( !CreateNominalMessageQueue(hQNominal))
    {
        LOG_ERROR( "Message Queue Creation Failed" );
    }    

    if( !AccStartSampling(hAcc,hQNominal))
    {
        LOG_ERROR( "Accelerometer Sampling Failed" );           
    }      

    //Ensure we have a good stop
    stopData.hAcc = hAcc;
    stopData.hQueue = hQNominal;
    stopData.expError = ERROR_SUCCESS; 
    stopData.expSamples = FALSE;

    if( !call_Test( (PVOID)(&stopData), sizeof(AccOpenAccStopTstData_t), L"tst_AccelerometerStop"))
    {
        LOG_ERROR( "Test Failed" );
    }
                
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQNominal);               

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerStop01


//------------------------------------------------------------------------------
// Tux_AccelerometerStop02
//
TESTPROCAPI Tux_AccelerometerStop02(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
  
    AccOpenAccStopTstData_t stopData = {0};    

    if( !CreateNominalMessageQueue(hQNominal))
    {
        LOG_ERROR( "Message Queue Creation Failed" );
    }    

    if( !AccStartSampling(hAcc,hQNominal))
    {
        LOG_ERROR( "Accelerometer Sampling Failed" );           
    }      

    //Ensure we have a bad stop
    stopData.hAcc = NULL;
    stopData.hQueue = hQNominal;
    stopData.expError = ERROR_INVALID_PARAMETER; 
    stopData.expSamples = TRUE;

    if( !call_Test( (PVOID)(&stopData), sizeof(AccOpenAccStopTstData_t), L"tst_AccelerometerStop"))
    {
        LOG_ERROR( "Test Failed" );
    }
                
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQNominal);               

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerStop01 


//------------------------------------------------------------------------------
// Tux_AccelerometerStop03
//
TESTPROCAPI Tux_AccelerometerStop03(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
  
    AccOpenAccStopTstData_t stopData = {0};    

    if( !CreateNominalMessageQueue(hQNominal))
    {
        LOG_ERROR( "Message Queue Creation Failed" );
    }    

    if( !AccStartSampling(hAcc,hQNominal))
    {
        LOG_ERROR( "Accelerometer Sampling Failed" );           
    }      

    //Ensure we have a bad stop
    stopData.hAcc = ACC_INVALID_HANDLE_VALUE;
    stopData.hQueue = hQNominal;
    stopData.expError = ERROR_INVALID_PARAMETER; 
    stopData.expSamples = TRUE;

    if( !call_Test( (PVOID)(&stopData), sizeof(AccOpenAccStopTstData_t), L"tst_AccelerometerStop"))
    {
        LOG_ERROR( "Test Failed" );
    }
                
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQNominal);               

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerStop03


//------------------------------------------------------------------------------
// Tux_AccelerometerStop04
//
TESTPROCAPI Tux_AccelerometerStop04(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;    
    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccOpenAccStopTstData_t stopData = {0};    

    AccGetHandle(hAcc, sensorLuid);

    //Ensure we have a good stop
    stopData.hAcc = hAcc;
    stopData.hQueue = NULL;
    stopData.expError = ERROR_INVALID_HANDLE; 
    stopData.expSamples = FALSE;

    if( !call_Test( (PVOID)(&stopData), sizeof(AccOpenAccStopTstData_t), L"tst_AccelerometerStop"))
    {
        LOG_ERROR( "Test Failed" );
    }
                
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);


    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerStop04




//------------------------------------------------------------------------------
// Tux_AccelerometerCreateCallback01
//
TESTPROCAPI Tux_AccelerometerCreateCallback01(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccCreateCallTstData_t data = {0};
    LUID sensorLuid = {0};

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    data.bValidCallback = TRUE;
    data.hAcc = hAcc;
    data.expError = ERROR_SUCCESS;
    data.expSamples = TRUE;
    data.bFakeCallback = FALSE;
    data.bHasContext = FALSE;
    data.lpvContext = NULL;

    if( !call_Test( (PVOID)(&data), sizeof(AccCreateCallTstData_t), L"tst_AccelerometerCreateCallback"))
    {
        LOG_ERROR( "Test Failed" );
    } 
       
DONE:
    //Since this was a good case....
    f_AccelerometerCancelCallback(hAcc);
    
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerCreateCallback01


//------------------------------------------------------------------------------
// Tux_AccelerometerCreateCallback02
//
TESTPROCAPI Tux_AccelerometerCreateCallback02(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccCreateCallTstData_t data = {0};
    LUID sensorLuid = {0};

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    data.bValidCallback = TRUE;
    data.hAcc = ACC_INVALID_HANDLE_VALUE;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expSamples = FALSE;
    data.bFakeCallback = FALSE;
    data.bHasContext = FALSE;
    data.lpvContext = NULL;

    if( !call_Test( (PVOID)(&data), sizeof(AccCreateCallTstData_t), L"tst_AccelerometerCreateCallback"))
    {
        LOG_ERROR( "Test Failed" );
    }
       
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerCreateCallback02

//------------------------------------------------------------------------------
// Tux_AccelerometerCreateCallback03
//
TESTPROCAPI Tux_AccelerometerCreateCallback03(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccCreateCallTstData_t data = {0};
    LUID sensorLuid = {0};

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    data.bValidCallback = FALSE;
    data.hAcc = hAcc;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expSamples = FALSE;
    data.bFakeCallback = FALSE;
    data.bHasContext = FALSE;
    data.lpvContext = NULL;
 
    if( !call_Test( (PVOID)(&data), sizeof(AccCreateCallTstData_t), L"tst_AccelerometerCreateCallback"))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerCreateCallback03


//------------------------------------------------------------------------------
// Tux_AccelerometerCreateCallback04
//
TESTPROCAPI Tux_AccelerometerCreateCallback04(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccCreateCallTstData_t data = {0};
    LUID sensorLuid = {0};

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    data.bValidCallback = TRUE;
    data.hAcc = ACC_INVALID_HANDLE_VALUE;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expSamples = FALSE;
    data.bFakeCallback = TRUE;
    data.bHasContext = FALSE;
    data.lpvContext = NULL;

    if( !call_Test( (PVOID)(&data), sizeof(AccCreateCallTstData_t), L"tst_AccelerometerCreateCallback"))
    {
        LOG_ERROR( "Test Failed" );
    } 
       
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerCreateCallback04


//------------------------------------------------------------------------------
// Tux_AccelerometerCreateCallback05
//
TESTPROCAPI Tux_AccelerometerCreateCallback05(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccCreateCallTstData_t data = {0};
    LUID sensorLuid = {0};

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    data.bValidCallback = TRUE;
    data.hAcc = hAcc;
    data.expError = ERROR_INVALID_PARAMETER;
    data.expSamples = FALSE;
    data.bFakeCallback = TRUE;
    data.bHasContext = FALSE;
    data.lpvContext = NULL;

    if( !call_Test( (PVOID)(&data), sizeof(AccCreateCallTstData_t), L"tst_AccelerometerCreateCallback"))
    {
        LOG_ERROR( "Test Failed" );
    }
       
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerCreateCallback05



//------------------------------------------------------------------------------
// Tux_AccelerometerCreateCallback05
//
TESTPROCAPI Tux_AccelerometerCreateCallback06(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccCreateCallTstData_t data = {0};
    LUID sensorLuid = {0};

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    data.bValidCallback = TRUE;
    data.hAcc = hAcc;
    data.expError = ERROR_SUCCESS;
    data.expSamples = TRUE;
    data.bFakeCallback = FALSE;
    data.bHasContext = TRUE;
    data.lpvContext = CALLBACK_CONTEXT_SIG;

    if( !call_Test( (PVOID)(&data), sizeof(AccCreateCallTstData_t), L"tst_AccelerometerCreateCallback"))
    {
        LOG_ERROR( "Test Failed" );
    }

DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerCreateCallback06




//------------------------------------------------------------------------------
// Tux_AccelerometerCancelCallback01
//
TESTPROCAPI Tux_AccelerometerCancelCallback01(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccCancelCallTstData_t cancelData = {0};
    LUID sensorLuid = {0};

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    //Create our callback we will want to cancel
    f_AccelerometerCreateCallback(hAcc, (ACCELEROMETER_CALLBACK *)&tst_Callback, NULL);

    //Setup a successful cancel 
    cancelData.bValidCallback = TRUE;
    cancelData.hAcc = hAcc;
    cancelData.expError = ERROR_SUCCESS;
    cancelData.expSamples = FALSE;

    if( !call_Test( (PVOID)(&cancelData), sizeof(AccCancelCallTstData_t), L"tst_AccelerometerCancelCallback"))
    {
        LOG_ERROR( "Test Failed" );
    }
       
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerCancelCallback01


//------------------------------------------------------------------------------
// Tux_AccelerometerCancelCallback02
//
TESTPROCAPI Tux_AccelerometerCancelCallback02(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccCancelCallTstData_t cancelData = {0};
    LUID sensorLuid = {0};

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    //Create our callback we will want to cancel
    f_AccelerometerCreateCallback(hAcc, (ACCELEROMETER_CALLBACK *)&tst_Callback, NULL);

    cancelData.bValidCallback = TRUE;
    cancelData.hAcc = NULL;
    cancelData.expError = ERROR_GEN_FAILURE;
    cancelData.expSamples = TRUE;

    if( !call_Test( (PVOID)(&cancelData), sizeof(AccCancelCallTstData_t), L"tst_AccelerometerCancelCallback"))
    {
        LOG_ERROR( "Test Failed" );
    }

    //Cancel it for good this time
    if( f_AccelerometerCancelCallback(hAcc) != ERROR_SUCCESS )
    {
        LOG_ERROR( "Could not cleanup test" );
    }
       
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerCancelCallback02

//------------------------------------------------------------------------------
// Tux_AccelerometerCancelCallback03
//
TESTPROCAPI Tux_AccelerometerCancelCallback03(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccCancelCallTstData_t cancelData = {0};
    LUID sensorLuid = {0};

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    //Create our callback we will want to cancel
    f_AccelerometerCreateCallback(hAcc, (ACCELEROMETER_CALLBACK *)&tst_Callback, NULL);

    cancelData.bValidCallback = TRUE;
    cancelData.hAcc = ACC_INVALID_HANDLE_VALUE;
    cancelData.expError = ERROR_GEN_FAILURE;
    cancelData.expSamples = TRUE;

    if( !call_Test( (PVOID)(&cancelData), sizeof(AccCancelCallTstData_t), L"tst_AccelerometerCancelCallback"))
    {
        LOG_ERROR( "Test Failed" );
    }

    //Cancel it for good this time
    if( f_AccelerometerCancelCallback(hAcc) != ERROR_SUCCESS )
    {
        LOG_ERROR( "Could not cleanup test" );
    }
       
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerCancelCallback03

//------------------------------------------------------------------------------
// Tux_AccelerometerCancelCallback04
//
TESTPROCAPI Tux_AccelerometerCancelCallback04(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccCancelCallTstData_t cancelData = {0};
    LUID sensorLuid = {0};

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    //No CreateCallback called, try to cancel it. 
    cancelData.bValidCallback = FALSE;
    cancelData.hAcc = hAcc;
    cancelData.expError = ERROR_GEN_FAILURE;
    cancelData.expSamples = TRUE;

    if( !call_Test( (PVOID)(&cancelData), sizeof(AccCancelCallTstData_t), L"tst_AccelerometerCancelCallback"))
    {
        LOG_ERROR( "Test Failed" );
    }
       
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerCancelCallback04

//------------------------------------------------------------------------------
// Tux_AccelerometerCancelCallback05
//
TESTPROCAPI Tux_AccelerometerCancelCallback05(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    AccCancelCallTstData_t cancelData = {0};
    LUID sensorLuid = {0};

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    cancelData.bValidCallback = FALSE;
    cancelData.hAcc = NULL;
    cancelData.expError = ERROR_GEN_FAILURE;
    cancelData.expSamples = TRUE;

    if( !call_Test( (PVOID)(&cancelData), sizeof(AccCancelCallTstData_t), L"tst_AccelerometerCancelCallback"))
    {
        LOG_ERROR( "Test Failed" );
    }

DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);

    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerCancelCallback05


//------------------------------------------------------------------------------
// Tux_AccelerometerSetMode01
//
TESTPROCAPI Tux_AccelerometerSetMode01(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE; 
    AccSetMode_t tstData = {0};
    LUID sensorLuid = {0};

    //We need a fake PDD to run this test in automation.... 
    if( !g_AccParam.bFakePdd )
    {
        LOG( "No Stub PDD Loaded" );
        goto DONE;
    }

    CreateNominalMessageQueue(hQNominal);   

    if( !AccStartSampling(hAcc,hQNominal))
    {
        LOG_ERROR( "Accelerometer Sampling Failed" );           
    }  

    //Good Case
    tstData.hAcc = hAcc;
    tstData.hQueue = hQNominal;
    tstData.configMode = ACC_CONFIG_ORIENTATION_MODE;
    tstData.reserved = NULL;
    tstData.expError = ERROR_SUCCESS;
    tstData.expSSamples = TRUE;
    tstData.expOSamples = TRUE;

    if( !call_Test( (PVOID)(&tstData), sizeof(AccSetMode_t), L"tst_AccelerometerSetMode"))
    {
        LOG_ERROR( "Test Failed" );
    }

    if( f_AccelerometerStop(hAcc) != ERROR_SUCCESS)
    {
        LOG_ERROR( "Test Cleanup Failed" );
    }
       
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQNominal);        
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerSetMode01


//------------------------------------------------------------------------------
// Tux_AccelerometerSetMode02
//
TESTPROCAPI Tux_AccelerometerSetMode02(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE; 
    AccSetMode_t tstData = {0};
    LUID sensorLuid = {0};

    //We need a fake PDD to run this test in automation.... 
    if( !g_AccParam.bFakePdd )
    {
        LOG( "No Stub PDD Loaded" );
        goto DONE;
    }

    CreateNominalMessageQueue(hQNominal);   

    if( !AccStartSampling(hAcc,hQNominal))
    {
        LOG_ERROR( "Accelerometer Sampling Failed" );           
    }  

    //Invalid Config Mode - No OSamples
    tstData.hAcc = hAcc;
    tstData.hQueue = hQNominal;
    tstData.configMode = (ACC_CONFIG_MODE)0xFFFF;
    tstData.reserved = NULL;
    tstData.expError = ERROR_INVALID_PARAMETER;
    tstData.expSSamples = TRUE;
    tstData.expOSamples = FALSE;

    if( !call_Test( (PVOID)(&tstData), sizeof(AccSetMode_t), L"tst_AccelerometerSetMode"))
    {
        LOG_ERROR( "Test Failed" );
    }

    if( f_AccelerometerStop(hAcc) != ERROR_SUCCESS)
    {
        LOG_ERROR( "Test Cleanup Failed" );
    }
       
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQNominal);        
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerSetMode02


//------------------------------------------------------------------------------
// Tux_AccelerometerSetMode03
//
TESTPROCAPI Tux_AccelerometerSetMode03(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE; 
    AccSetMode_t tstData = {0};
    LUID sensorLuid = {0};

    //We need a fake PDD to run this test in automation.... 
    if( !g_AccParam.bFakePdd )
    {
        LOG( "No Stub PDD Loaded" );
        goto DONE;
    }

    CreateNominalMessageQueue(hQNominal);   


    //Invalid Acc Handle - no samples
    tstData.hAcc = NULL;
    tstData.hQueue = hQNominal;
    tstData.configMode = ACC_CONFIG_ORIENTATION_MODE;
    tstData.reserved = NULL;
    tstData.expError = ERROR_INVALID_PARAMETER;
    tstData.expSSamples = FALSE;
    tstData.expOSamples = FALSE;

    if( !call_Test( (PVOID)(&tstData), sizeof(AccSetMode_t), L"tst_AccelerometerSetMode" ))
    {
        LOG_ERROR( "Test Failed" );
    }


       
DONE:

    CHECK_CLOSE_Q_HANDLE(hQNominal);        
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerSetMode03


//------------------------------------------------------------------------------
// Tux_AccelerometerSetMode04
//
TESTPROCAPI Tux_AccelerometerSetMode04(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;    
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE; 
    AccSetMode_t tstData = {0};
    LUID sensorLuid = {0};

    //We need a fake PDD to run this test in automation.... 
    if( !g_AccParam.bFakePdd )
    {
        LOG( "No Stub PDD Loaded" );
        goto DONE;
    }

    CreateNominalMessageQueue(hQNominal);   

    if( !AccStartSampling(hAcc,hQNominal))
    {
        LOG_ERROR( "Accelerometer Sampling Failed" );           
    }  

    //Invalid Q Handle - no samples
    tstData.hAcc = hAcc;
    tstData.hQueue = NULL;
    tstData.configMode = ACC_CONFIG_ORIENTATION_MODE;
    tstData.reserved = NULL;
    tstData.expError = ERROR_SUCCESS;
    tstData.expSSamples = FALSE;
    tstData.expOSamples = FALSE;

    if( !call_Test( (PVOID)(&tstData), sizeof(AccSetMode_t), L"tst_AccelerometerSetMode"))
    {
        LOG_ERROR( "Test Failed" );
    }

    if( f_AccelerometerStop(hAcc) != ERROR_SUCCESS)
    {
        LOG_ERROR( "Test Cleanup Failed" );
    }
       
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQNominal);        
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerSetMode04

//------------------------------------------------------------------------------
// Tux_AccelerometerSetMode05
//
TESTPROCAPI Tux_AccelerometerSetMode05(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 

    BOOL bResult = TRUE;    
    HANDLE hQTest = INVALID_HANDLE_VALUE;
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE; 
    AccSetMode_t tstData = {0};
    LUID sensorLuid = {0};

    //Special Queue case
    MSGQUEUEOPTIONS testQOpt = 
    {
        (Q_NOM_SIZE + 100),
        Q_NOM_FLAG,
        Q_NOM_MAXMSG, 
        Q_NOM_MAXMSG_SIZE, 
        Q_NOM_ACCESS,
    };

    
    //We need a fake PDD to run this test in automation.... 
    if( !g_AccParam.bFakePdd )
    {
        LOG( "No Stub PDD Loaded" );
        goto DONE;
    }

    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }       

    hQTest = CreateMsgQueue( _T("TSTQ"), &testQOpt ); 

    //Invalid Huge Queue case
    tstData.hAcc = hAcc;
    tstData.hQueue = hQTest;
    tstData.configMode = ACC_CONFIG_ORIENTATION_MODE;
    tstData.reserved = NULL;
    tstData.expError = ERROR_SUCCESS;
    tstData.expSSamples = TRUE;
    tstData.expOSamples = TRUE;

    if( !AccStartSampling(hAcc,hQTest))
    {
        LOG_ERROR( "Accelerometer Sampling Failed" );           
    } 


    if( !call_Test( (PVOID)(&tstData), sizeof(AccSetMode_t), L"tst_AccelerometerSetMode"))
    {
        LOG_ERROR( "Test Failed" );
    }

    if( f_AccelerometerStop(hAcc) != ERROR_SUCCESS)
    {
        LOG_ERROR( "Test Cleanup Failed" );
    }
       
DONE:
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQTest);        
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_AccelerometerSetMode05

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STARTSENSOR01
//
TESTPROCAPI Tux_IOCTL_ACC_STARTSENSOR01(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    //Assuming the DLL is already loaded (we've got more serious problems otherwise)
    if( !AccGetDriverHandle( hMdd ) )
    {
        LOG_ERROR( "Could not retrieve MDD driver handle" );
    }
    
    if( !CreateNominalMessageQueue(hQNominal) )
    {
        LOG_ERROR( "Could not create Message Queue" );
    }

    //Good case
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_STARTSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = TRUE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutError == 0xFFFFFFFF || dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }
    
    //Stop?

DONE:
    CHECK_CLOSE_HANDLE(hMdd);   
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STARTSENSOR01  

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STARTSENSOR02
//
TESTPROCAPI Tux_IOCTL_ACC_STARTSENSOR02(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    //Assuming the DLL is already loaded (we've got more serious problems otherwise)
    if( !AccGetDriverHandle( hMdd ) )
    {
        LOG_ERROR( "Could not retrieve MDD driver handle" );
    }

    //Invalid Queue Handle
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_STARTSENSOR;
    data.lpInBuffer_in = NULL;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutError == 0xFFFFFFFF || dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }


DONE:
    CHECK_CLOSE_HANDLE(hMdd);   
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STARTSENSOR02

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STARTSENSOR03
//
TESTPROCAPI Tux_IOCTL_ACC_STARTSENSOR03(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    //Assuming the DLL is already loaded (we've got more serious problems otherwise)
    if( !AccGetDriverHandle( hMdd ) )
    {
        LOG_ERROR( "Could not retrieve MDD driver handle" );
    }

    //Invalid Queue Handle
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_STARTSENSOR;
    data.lpInBuffer_in = INVALID_HANDLE_VALUE;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutError == 0xFFFFFFFF || dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }


DONE:
    CHECK_CLOSE_HANDLE(hMdd);   
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STARTSENSOR03


//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STARTSENSOR04
//
TESTPROCAPI Tux_IOCTL_ACC_STARTSENSOR04(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    //Assuming the DLL is already loaded (we've got more serious problems otherwise)
    if( !AccGetDriverHandle( hMdd ) )
    {
        LOG_ERROR( "Could not retrieve MDD driver handle" );
    }

    if( !CreateNominalMessageQueue(hQNominal) )
    {
        LOG_ERROR( "Could not create Message Queue" );
    }

    //Invalid buffer size 
    data.hDevice_in = hMdd;    
    data.dwIOCTL_in = IOCTL_ACC_STARTSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = 0;
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutError == 0xFFFFFFFF || dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }


DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);   
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STARTSENSOR04



//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STARTSENSOR05
//
TESTPROCAPI Tux_IOCTL_ACC_STARTSENSOR05(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    //Assuming the DLL is already loaded (we've got more serious problems otherwise)
    if( !AccGetDriverHandle( hMdd ) )
    {
        LOG_ERROR( "Could not retrieve MDD driver handle" );
    }

    if( !CreateNominalMessageQueue(hQNominal) )
    {
        LOG_ERROR( "Could not create Message Queue" );
    }

    //Invalid buffer size
    data.hDevice_in = hMdd;    
    data.dwIOCTL_in = IOCTL_ACC_STARTSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = 0;   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutError == 0xFFFFFFFF || dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }


DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);   
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STARTSENSOR05

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STARTSENSOR06
//
TESTPROCAPI Tux_IOCTL_ACC_STARTSENSOR06(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    //Assuming the DLL is already loaded (we've got more serious problems otherwise)
    if( !AccGetDriverHandle( hMdd ) )
    {
        LOG_ERROR( "Could not retrieve MDD driver handle" );
    }

    if( !CreateNominalMessageQueue(hQNominal) )
    {
        LOG_ERROR( "Could not create Message Queue" );
    }

    //Invalid output buffer 
    data.hDevice_in = hMdd;    
    data.dwIOCTL_in = IOCTL_ACC_STARTSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = NULL;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);   
        
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STARTSENSOR06

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STARTSENSOR07
//
TESTPROCAPI Tux_IOCTL_ACC_STARTSENSOR07(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    //Assuming the DLL is already loaded (we've got more serious problems otherwise)
    if( !AccGetDriverHandle( hMdd ) )
    {
        LOG_ERROR( "Could not retrieve MDD driver handle" );
    }

    if( !CreateNominalMessageQueue(hQNominal) )
    {
        LOG_ERROR( "Could not create Message Queue" );
    }

    //Invalid output buffer 
    data.hDevice_in = hMdd;    
    data.dwIOCTL_in = IOCTL_ACC_STARTSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = NULL;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutError == 0xFFFFFFFF || dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }


DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);   
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STARTSENSOR07


//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STARTSENSOR08
//
TESTPROCAPI Tux_IOCTL_ACC_STARTSENSOR08(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};

    if( !CreateNominalMessageQueue(hQNominal) )
    {
        LOG_ERROR( "Could not create Message Queue" );
    }

    //Invalid Device Handle
    data.hDevice_in = INVALID_HANDLE_VALUE;
    data.dwIOCTL_in = IOCTL_ACC_STARTSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_HANDLE;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STARTSENSOR08  

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STARTSENSOR09
//
TESTPROCAPI Tux_IOCTL_ACC_STARTSENSOR09(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    HANDLE hQNominal = INVALID_HANDLE_VALUE;
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};

    if( !CreateNominalMessageQueue(hQNominal) )
    {
        LOG_ERROR( "Could not create Message Queue" );
    }

    //Invalid Device Handle
    data.hDevice_in = NULL;
    data.dwIOCTL_in = IOCTL_ACC_STARTSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_HANDLE;


    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STARTSENSOR09


//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STOPSENSOR01
//
TESTPROCAPI Tux_IOCTL_ACC_STOPSENSOR01(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;
    HANDLE hQNominal = INVALID_HANDLE_VALUE;

    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Good Case
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_STOPSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = TRUE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutError == 0xFFFFFFFF || dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }
    
DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STOPSENSOR01

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STOPSENSOR02
//
TESTPROCAPI Tux_IOCTL_ACC_STOPSENSOR02(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;
    HANDLE hQNominal = INVALID_HANDLE_VALUE;

    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Invalid device handle
    data.hDevice_in = INVALID_HANDLE_VALUE;
    data.dwIOCTL_in = IOCTL_ACC_STOPSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;
    
    data.bytesReturned_exp = dwOutSize; //Invalid context,shouldn't write to the outbuffer
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_HANDLE;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( !AccStopViaMdd( hMdd ) )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }
    
DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STOPSENSOR02

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STOPSENSOR03
//
TESTPROCAPI Tux_IOCTL_ACC_STOPSENSOR03(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;
    HANDLE hQNominal = INVALID_HANDLE_VALUE;

    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Invalid device handle
    data.hDevice_in = NULL;
    data.dwIOCTL_in = IOCTL_ACC_STOPSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = dwOutSize; //Invalid context,shouldn't write to the outbuffer
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_HANDLE;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( !AccStopViaMdd( hMdd ) )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }
    
DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STOPSENSOR03

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STOPSENSOR04
//
TESTPROCAPI Tux_IOCTL_ACC_STOPSENSOR04(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;
    HANDLE hQNominal = INVALID_HANDLE_VALUE;

    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Invalid queue handle
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_STOPSENSOR;
    data.lpInBuffer_in = INVALID_HANDLE_VALUE;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutError == 0xFFFFFFFF || dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

    if( !AccStopViaMdd( hMdd ) )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }
    
DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STOPSENSOR04

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STOPSENSOR05
//
TESTPROCAPI Tux_IOCTL_ACC_STOPSENSOR05(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;
    HANDLE hQNominal = INVALID_HANDLE_VALUE;

    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Invalid queue handle
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_STOPSENSOR;
    data.lpInBuffer_in = NULL;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutError == 0xFFFFFFFF || dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }


    if( !AccStopViaMdd( hMdd ) )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }
    
DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STOPSENSOR05


//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STOPSENSOR06
//
TESTPROCAPI Tux_IOCTL_ACC_STOPSENSOR06(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;
    HANDLE hQNominal = INVALID_HANDLE_VALUE;

    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Invalid buffer size
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_STOPSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = 0;
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutError == 0xFFFFFFFF || dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }


    if( !AccStopViaMdd( hMdd ) )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }
    
DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STOPSENSOR06

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STOPSENSOR07
//
TESTPROCAPI Tux_IOCTL_ACC_STOPSENSOR07(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;
    HANDLE hQNominal = INVALID_HANDLE_VALUE;

    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Invalid buffer size
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_STOPSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = 0;   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INSUFFICIENT_BUFFER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutError == 0xFFFFFFFF || dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }


    if( !AccStopViaMdd( hMdd ) )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }
    
DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STOPSENSOR07

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STOPSENSOR08
//
TESTPROCAPI Tux_IOCTL_ACC_STOPSENSOR08(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;
    HANDLE hQNominal = INVALID_HANDLE_VALUE;

    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Invalid buffer 
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_STOPSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = NULL;
    data.lpBytesReturned_out = 0;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }


    if( !AccStopViaMdd( hMdd ) )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }
    
DONE:
    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STOPSENSOR08

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_STOPSENSOR09
//
TESTPROCAPI Tux_IOCTL_ACC_STOPSENSOR09(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;
    HANDLE hQNominal = INVALID_HANDLE_VALUE;

    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Invalid buffer 
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_STOPSENSOR;
    data.lpInBuffer_in = hQNominal;
    data.nInBufferSize_in = sizeof(HANDLE);
    data.nOutBufferSize_in = sizeof(DWORD);   

    data.lpOutBuffer_out = &dwOutError;
    data.lpBytesReturned_out = NULL;

    data.bytesReturned_exp = sizeof(DWORD);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_ACC_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutError == 0xFFFFFFFF || dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

DONE:

    if( !AccStopViaMdd( hMdd ) )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }
    

    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_STOPSENSOR09


//------------------------------------------------------------------------------
// Tux_IOCTL_SENSOR_CSMDD_GET_CAPS01
//
TESTPROCAPI Tux_IOCTL_SENSOR_CSMDD_GET_CAPS01(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;
    HANDLE hQNominal = INVALID_HANDLE_VALUE;

    SENSOR_DEVICE_CAPS sensorCaps = {0};
   
    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Good Case
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_SENSOR_CSMDD_GET_CAPS;
    data.lpInBuffer_in = NULL;                      //Optional Parameter
    data.nInBufferSize_in = NULL;                   //Optional Parameter
    data.nOutBufferSize_in = sizeof(SENSOR_DEVICE_CAPS);     

    data.lpOutBuffer_out = &sensorCaps;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(SENSOR_DEVICE_CAPS);
    data.bIoReturn_exp = TRUE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }


    //What have we got
    DumpSensorCaps(sensorCaps);
    

    if( sensorCaps.type != SENSOR_ACCELEROMETER_3D )
    {
        LOG_ERROR( "Invalid Sensor Type Returned for this feature" );
    }

DONE:
    //Cleanup
    if( !AccStopViaMdd( hMdd ) )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }
    

    CHECK_CLOSE_Q_HANDLE(hQNominal);   
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_SENSOR_CSMDD_GET_CAPS01


//------------------------------------------------------------------------------
// Tux_IOCTL_SENSOR_CSMDD_GET_CAPS02
//
TESTPROCAPI Tux_IOCTL_SENSOR_CSMDD_GET_CAPS02(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;
    SENSOR_DEVICE_CAPS sensorCaps = {0};

   if( !AccGetDriverHandle( hMdd ) )
   {
       LOG_ERROR("Can't Access Driver" );
   }

    // Good Case - without starting accelerometer
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_SENSOR_CSMDD_GET_CAPS;
    data.lpInBuffer_in = NULL;                      //Optional Parameter
    data.nInBufferSize_in = NULL;                   //Optional Parameter
    data.nOutBufferSize_in = sizeof(SENSOR_DEVICE_CAPS);     

    data.lpOutBuffer_out = &sensorCaps;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(SENSOR_DEVICE_CAPS);
    data.bIoReturn_exp = TRUE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }


    //What have we got
    DumpSensorCaps(sensorCaps);
    

    if( sensorCaps.type != SENSOR_ACCELEROMETER_3D )
    {
        LOG_ERROR( "Invalid Sensor Type Returned for this feature" );
    }

DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_SENSOR_CSMDD_GET_CAPS02


//------------------------------------------------------------------------------
// Tux_IOCTL_SENSOR_CSMDD_GET_CAPS03
//
TESTPROCAPI Tux_IOCTL_SENSOR_CSMDD_GET_CAPS03(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;


   if( !AccGetDriverHandle( hMdd ) )
   {
       LOG_ERROR("Can't Access Driver" );
   }

    // Bad Case - without starting accelerometer - invalid buffer
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_SENSOR_CSMDD_GET_CAPS;
    data.lpInBuffer_in = NULL;                      //Optional Parameter
    data.nInBufferSize_in = NULL;                   //Optional Parameter
    data.nOutBufferSize_in = sizeof(SENSOR_DEVICE_CAPS);     

    data.lpOutBuffer_out = NULL;                    //No output buffer (Req)
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(SENSOR_DEVICE_CAPS);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_SENSOR_CSMDD_GET_CAPS03

//------------------------------------------------------------------------------
// Tux_IOCTL_SENSOR_CSMDD_GET_CAPS04
//
TESTPROCAPI Tux_IOCTL_SENSOR_CSMDD_GET_CAPS04(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    DWORD fakeSensorCaps[2] = {0};
    DWORD fakeSize = 2 * sizeof(DWORD);

   if( !AccGetDriverHandle( hMdd ) )
   {
       LOG_ERROR("Can't Access Driver" );
   }

    // Bad Case - without starting accelerometer - wrong buffer and size
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_SENSOR_CSMDD_GET_CAPS;
    data.lpInBuffer_in = NULL;                      //Optional Parameter
    data.nInBufferSize_in = NULL;                   //Optional Parameter
    data.nOutBufferSize_in = fakeSize;     

    data.lpOutBuffer_out = &fakeSensorCaps;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = 0;
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_INVALID_PARAMETER;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_SENSOR_CSMDD_GET_CAPS04


//------------------------------------------------------------------------------
// Tux_IOCTL_SENSOR_CSMDD_GET_CAPS05
//
TESTPROCAPI Tux_IOCTL_SENSOR_CSMDD_GET_CAPS05(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutError = 0xFFFFFFFF;
    DWORD  dwOutSize = 0xFFFFFFFF;

    BOOL bResult = TRUE;

    AccIoControlTstData_t data = {0};
    HANDLE hMdd1 = INVALID_HANDLE_VALUE;
    HANDLE hMdd2 = INVALID_HANDLE_VALUE;    


    SENSOR_DEVICE_CAPS sensorCaps1 = {0};
    SENSOR_DEVICE_CAPS sensorCaps2 = {0};


    //Get two handles
    if( !AccGetDriverHandle( hMdd1 ) )
    {
        LOG_ERROR("Can't Access Driver" );
    }

    if( !AccGetDriverHandle( hMdd2 ) )
    {
        LOG_ERROR("Can't Access Driver" );
    }

    // Good Case - Handle 1
    data.hDevice_in = hMdd1;
    data.dwIOCTL_in = IOCTL_SENSOR_CSMDD_GET_CAPS;
    data.lpInBuffer_in = NULL;                      //Optional Parameter
    data.nInBufferSize_in = NULL;                   //Optional Parameter
    data.nOutBufferSize_in = sizeof(SENSOR_DEVICE_CAPS);     
    data.lpOutBuffer_out = &sensorCaps1;
    data.lpBytesReturned_out = &dwOutSize;
    data.bytesReturned_exp = sizeof(SENSOR_DEVICE_CAPS);
    data.bIoReturn_exp = TRUE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

    //Repeat for handle 2
    dwOutSize = 0xFFFFFFFF;
    data.hDevice_in = hMdd2;
    data.lpInBuffer_in = NULL;                      //Optional Parameter
    data.nInBufferSize_in = NULL;                   //Optional Parameter
    data.lpOutBuffer_out = &sensorCaps2;
    data.lpBytesReturned_out = &dwOutSize;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

    //What have we got
    LOG( "Dumping handle 1 sensor capability information...." );
    DumpSensorCaps(sensorCaps1);
    
    LOG( "Dumping handle 2 sensor capability information...." );
    DumpSensorCaps(sensorCaps2);    

    if( sensorCaps1.type != SENSOR_ACCELEROMETER_3D  ||
        sensorCaps2.type != SENSOR_ACCELEROMETER_3D )
    {
        LOG_ERROR( "Invalid Sensor Type Returned for this feature" );
    }

    //if( RtlEqualLuid(sensorCaps1.sensorLuid, sensorCaps2.sensorLuid) )
    if( memcmp(&sensorCaps1.sensorLuid, &sensorCaps2.sensorLuid, sizeof(LUID)) == 0 )
    {
        LOG_ERROR( "Same sensor ID returned for different handles" );
    }

DONE:

    CHECK_CLOSE_HANDLE(hMdd1);
    CHECK_CLOSE_HANDLE(hMdd2);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_SENSOR_CSMDD_GET_CAPS05




//------------------------------------------------------------------------------
// Tux_IOCTL_POWER_CAPABILITIES01
//
TESTPROCAPI Tux_IOCTL_POWER_CAPABILITIES01(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    POWER_CAPABILITIES powerCapabilities = {0};

   if( !AccGetDriverHandle( hMdd ) )
   {
       LOG_ERROR("Can't Access Driver" );
   }

    // Good Case - without starting accelerometer
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_POWER_CAPABILITIES;
    data.lpInBuffer_in = NULL; 
    data.nInBufferSize_in = 0;
    data.nOutBufferSize_in = sizeof(POWER_CAPABILITIES);

    data.lpOutBuffer_out = &powerCapabilities;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(POWER_CAPABILITIES);
    data.bIoReturn_exp = TRUE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

    //What do we have...
    DumpPowerCaps( powerCapabilities );

DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_POWER_CAPABILITIES01


//------------------------------------------------------------------------------
// Tux_IOCTL_POWER_CAPABILITIES02
//
TESTPROCAPI Tux_IOCTL_POWER_CAPABILITIES02(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    if( !AccGetDriverHandle( hMdd ) )
    {
        LOG_ERROR("Can't Access Driver" );
    }

    // Bad Case - invalid buffer
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_POWER_CAPABILITIES;
    data.lpInBuffer_in = NULL; 
    data.nInBufferSize_in = 0;
    data.nOutBufferSize_in = sizeof(POWER_CAPABILITIES);

    data.lpOutBuffer_out = NULL;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(POWER_CAPABILITIES);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_POWER_CAPABILITIES02





//------------------------------------------------------------------------------
// Tux_IOCTL_POWER_SET01
//
TESTPROCAPI Tux_IOCTL_POWER_SET01(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    CEDEVICE_POWER_STATE  powerState = D0;

   if( !AccGetDriverHandle( hMdd ) )
   {
       LOG_ERROR("Can't Access Driver" );
   }

    // good Case - without starting accelerometer
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_POWER_SET;
    data.lpInBuffer_in = NULL; 
    data.nInBufferSize_in = 0;
    data.nOutBufferSize_in = sizeof(CEDEVICE_POWER_STATE);

    data.lpOutBuffer_out = &powerState;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(CEDEVICE_POWER_STATE);
    data.bIoReturn_exp = TRUE;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

    //What do we have...
    LOG( "Current Power State...." );
    DumpPowerState( powerState );
    
DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_POWER_SET01


//------------------------------------------------------------------------------
// Tux_IOCTL_POWER_SET02
//
TESTPROCAPI Tux_IOCTL_POWER_SET02(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    CEDEVICE_POWER_STATE  powerState = PwrDeviceUnspecified ;
    
    if( !AccGetDriverHandle( hMdd ) )
    {
        LOG_ERROR("Can't Access Driver" );
    }

    // Bad Case - Garbage Power State
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_POWER_SET;
    data.lpInBuffer_in = NULL; 
    data.nInBufferSize_in = 0;
    data.nOutBufferSize_in = sizeof(CEDEVICE_POWER_STATE);

    data.lpOutBuffer_out = &powerState;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(CEDEVICE_POWER_STATE);
    data.bIoReturn_exp = FALSE;
    //data.dwLastErr_exp = ERROR_SUCCESS;
    
    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }
    
DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_POWER_SET02

//------------------------------------------------------------------------------
// Tux_IOCTL_POWER_GET01
//
TESTPROCAPI Tux_IOCTL_POWER_GET01(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    CEDEVICE_POWER_STATE  powerState = D0;

    HANDLE hQNominal = INVALID_HANDLE_VALUE;
      
    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Good Case - without starting accelerometer
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_POWER_GET;
    data.lpInBuffer_in = NULL; 
    data.nInBufferSize_in = 0;
    data.nOutBufferSize_in = sizeof(CEDEVICE_POWER_STATE);

    data.lpOutBuffer_out = &powerState;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(CEDEVICE_POWER_STATE);
    data.bIoReturn_exp = TRUE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

    LOG( "Current Power State...." );
    DumpPowerState( powerState );

DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    CHECK_CLOSE_Q_HANDLE(hQNominal);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_POWER_GET01

//------------------------------------------------------------------------------
// Tux_IOCTL_POWER_GET02
//
TESTPROCAPI Tux_IOCTL_POWER_GET02(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)   
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    HANDLE hQNominal = INVALID_HANDLE_VALUE;
      
    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Bad Case - invalid buffer
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_POWER_GET;
    data.lpInBuffer_in = NULL; 
    data.nInBufferSize_in = 0;
    data.nOutBufferSize_in = sizeof(CEDEVICE_POWER_STATE);

    data.lpOutBuffer_out = NULL;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(CEDEVICE_POWER_STATE);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }


DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    CHECK_CLOSE_Q_HANDLE(hQNominal);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_POWER_GET02

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_SETCONFIG_MODE01
//
TESTPROCAPI Tux_IOCTL_ACC_SETCONFIG_MODE01(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    ACC_CONFIG_MODE configMode = ACC_CONFIG_ORIENTATION_MODE;

    HANDLE hQNominal = INVALID_HANDLE_VALUE;
      
    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Good Case - without starting accelerometer
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_SETCONFIG_MODE;
    data.lpInBuffer_in = &configMode; 
    data.nInBufferSize_in = sizeof( ACC_CONFIG_MODE);
    data.nOutBufferSize_in = 0;

    data.lpOutBuffer_out = NULL;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(ACC_CONFIG_MODE);
    data.bIoReturn_exp = TRUE;
    data.dwLastErr_exp = ERROR_SUCCESS;

Sleep( 10000 );

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    CHECK_CLOSE_Q_HANDLE(hQNominal);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_SETCONFIG_MODE01

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_SETCONFIG_MODE02
//
TESTPROCAPI Tux_IOCTL_ACC_SETCONFIG_MODE02(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    ACC_CONFIG_MODE configMode = (ACC_CONFIG_MODE)0xFFFFFFFF;

    HANDLE hQNominal = INVALID_HANDLE_VALUE;
      
    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Bad Case - Invalid Config Mode
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_SETCONFIG_MODE;
    data.lpInBuffer_in = &configMode; 
    data.nInBufferSize_in = sizeof( ACC_CONFIG_MODE);
    data.nOutBufferSize_in = 0;

    data.lpOutBuffer_out = NULL;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(ACC_CONFIG_MODE);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    CHECK_CLOSE_Q_HANDLE(hQNominal);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_SETCONFIG_MODE02

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_SETCONFIG_MODE03
//
TESTPROCAPI Tux_IOCTL_ACC_SETCONFIG_MODE03(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    ACC_CONFIG_MODE configMode = ACC_CONFIG_ORIENTATION_MODE;

    HANDLE hQNominal = INVALID_HANDLE_VALUE;
      
    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Bad Case - Invalid Input Size
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_SETCONFIG_MODE;
    data.lpInBuffer_in = &configMode; 
    data.nInBufferSize_in = 0;
    data.nOutBufferSize_in = 0;

    data.lpOutBuffer_out = NULL;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(ACC_CONFIG_MODE);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    CHECK_CLOSE_Q_HANDLE(hQNominal);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_SETCONFIG_MODE03



//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_SETCONFIG_MODE04
//
TESTPROCAPI Tux_IOCTL_ACC_SETCONFIG_MODE04(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    ACC_CONFIG_MODE configMode = ACC_CONFIG_ORIENTATION_MODE;

    HANDLE hQNominal = INVALID_HANDLE_VALUE;
      
    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Bad Case - Invalid Input Size
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_SETCONFIG_MODE;
    data.lpInBuffer_in = &configMode; 
    data.nInBufferSize_in = 10*sizeof( ACC_CONFIG_MODE);
    data.nOutBufferSize_in = 0;

    data.lpOutBuffer_out = NULL;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(ACC_CONFIG_MODE);
    data.bIoReturn_exp = FALSE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    CHECK_CLOSE_Q_HANDLE(hQNominal);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_SETCONFIG_MODE04


//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_SETCONFIG_MODE05
//
TESTPROCAPI Tux_IOCTL_ACC_SETCONFIG_MODE05(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    ACC_CONFIG_MODE configMode = ACC_CONFIG_ORIENTATION_MODE;

    HANDLE hQNominal = INVALID_HANDLE_VALUE;
      
    if( !AccStartViaMdd( hMdd, hQNominal) )
    {
        LOG_ERROR( "Cannot Start Accelerometer" );
    }

    // Good Case Multi
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_SETCONFIG_MODE;
    data.lpInBuffer_in = &configMode; 
    data.nInBufferSize_in = sizeof( ACC_CONFIG_MODE);
    data.nOutBufferSize_in = 0;

    data.lpOutBuffer_out = NULL;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(ACC_CONFIG_MODE);
    data.bIoReturn_exp = TRUE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

    configMode = ACC_CONFIG_ORIENTATION_MODE;
    dwOutSize = 0xFFFFFFFF;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

    configMode = ACC_CONFIG_STREAMING_DATA_MODE;
    dwOutSize = 0xFFFFFFFF;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

    configMode = ACC_CONFIG_STREAMING_DATA_MODE;
    dwOutSize = 0xFFFFFFFF;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    CHECK_CLOSE_Q_HANDLE(hQNominal);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_SETCONFIG_MODE05

//------------------------------------------------------------------------------
// Tux_IOCTL_ACC_SETCONFIG_MODE03
//
TESTPROCAPI Tux_IOCTL_ACC_SETCONFIG_MODE06(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    DWORD  dwOutSize = 0xFFFFFFFF;
    BOOL bResult = TRUE;
    AccIoControlTstData_t data = {0};
    HANDLE hMdd = INVALID_HANDLE_VALUE;

    ACC_CONFIG_MODE configMode = ACC_CONFIG_ORIENTATION_MODE;

    HANDLE hQNominal = INVALID_HANDLE_VALUE;
      
    if( !AccGetDriverHandle( hMdd ) )
    {
         LOG_ERROR("Can't Access Driver" );
    }

    // Bad Case - No Start
    data.hDevice_in = hMdd;
    data.dwIOCTL_in = IOCTL_ACC_SETCONFIG_MODE;
    data.lpInBuffer_in = &configMode; 
    data.nInBufferSize_in = sizeof( ACC_CONFIG_MODE);
    data.nOutBufferSize_in = 0;

    data.lpOutBuffer_out = NULL;
    data.lpBytesReturned_out = &dwOutSize;

    data.bytesReturned_exp = sizeof(ACC_CONFIG_MODE);
    data.bIoReturn_exp = TRUE;
    data.dwLastErr_exp = ERROR_SUCCESS;

    if(!call_Test( (PVOID)(&data), sizeof(AccIoControlTstData_t), L"tst_SNS_MDD_IOCTL"))
    {
        LOG_ERROR( "Test Failed!" );
    }

    if( dwOutSize == 0xFFFFFFFF )
    {
        LOG_ERROR( "Post-conditions Verify - Test Fail!" );
    }

DONE:
    CHECK_CLOSE_HANDLE(hMdd);
    CHECK_CLOSE_Q_HANDLE(hQNominal);
    
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;
}//Tux_IOCTL_ACC_SETCONFIG_MODE03


//------------------------------------------------------------------------------
// Tux_Scenario01
//
TESTPROCAPI Tux_Scenario01(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_StartScenario01" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario01  

//------------------------------------------------------------------------------
// Tux_Scenario02
//
TESTPROCAPI Tux_Scenario02(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_StartScenario02" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario02


//------------------------------------------------------------------------------
// Tux_Scenario03
//
TESTPROCAPI Tux_Scenario03(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_QueueScenario01" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario03

//------------------------------------------------------------------------------
// Tux_Scenario04
//
TESTPROCAPI Tux_Scenario04(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_QueueScenario02" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario04




//------------------------------------------------------------------------------
// Tux_Scenario05
//
TESTPROCAPI Tux_Scenario05(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_EndScenario01" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario05

//------------------------------------------------------------------------------
// Tux_Scenario06
//
TESTPROCAPI Tux_Scenario06(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_EndScenario02" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario06


//------------------------------------------------------------------------------
// Tux_Scenario07
//
TESTPROCAPI Tux_Scenario07(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_EndScenario03" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario07

//------------------------------------------------------------------------------
// Tux_Scenario08
//
TESTPROCAPI Tux_Scenario08(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_EndScenario04" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario08


//------------------------------------------------------------------------------
// Tux_Scenario09
//
TESTPROCAPI Tux_Scenario09(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_CallbackScenario01" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario09

//------------------------------------------------------------------------------
// Tux_Scenario10
//
TESTPROCAPI Tux_Scenario10(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;
                              
    if( !call_Test2( NULL, 0, L"tst_CallbackScenario02" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario10

//------------------------------------------------------------------------------
// Tux_Scenario11
//
TESTPROCAPI Tux_Scenario11(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_CallbackScenario03" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario11

//------------------------------------------------------------------------------
// Tux_Scenario12
//
TESTPROCAPI Tux_Scenario12(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_CallbackScenario04" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario12


//------------------------------------------------------------------------------
// Tux_Scenario14
//
TESTPROCAPI Tux_Scenario14(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_PowerScenario01" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario14

//------------------------------------------------------------------------------
// Tux_Scenario15
//
TESTPROCAPI Tux_Scenario15(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_QueueScenario03" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_Scenario15



//------------------------------------------------------------------------------
// Tux_UtilityScenario01
//
TESTPROCAPI Tux_UtilityScenario01(UINT uMsg, TPPARAM /*tpParam*/, LPFUNCTION_TABLE_ENTRY /*lpFTE*/)
{
    LOG_START();
    IGNORE_NONEXECUTE_CMDS(uMsg); 
    BOOL bResult = TRUE;

    if( !call_Test2( NULL, 0, L"tst_UtilityScenario01" ))
    {
        LOG_ERROR( "Test Failed" );
    }
    
DONE:
    LOG_END();
    return bResult?TPR_PASS:TPR_FAIL;

}//Tux_UtilityScenario01

