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
#pragma once

#include <sensor.h>
#include "accDefines.h"
#include "snsDebug.h"
#include "snsDefines.h"

#define ACC_EVENT_NAME _T("ACC_TEST_EVENT")


#define ACC_FUNC_LIB _T("acc_test_func.dll")
#define ACC_SCENARIO_LIB _T("acc_test_scenario.dll")

void acc_Callback( ACC_DATA* acc_data, DWORD dwContext );
void tst_Callback( ACC_DATA* acc_data, DWORD dwContext );
void tst_FakeCallback( BOOL bVal, DWORD dwVal );

BOOL UserPrompt(LPCTSTR szCaption, LPCTSTR szFormat, ...);

void AccDumpSample( ACC_DATA& accData );
void AccDumpOrientSample( ACC_ORIENT& accData );

DOUBLE CalculateDeviation( DOUBLE actual, DOUBLE expected );


BOOL AccGetHandle(HSENSOR &hAcc, LUID &sensorLuid);
BOOL AccAvailable( DEVMGR_DEVICE_INFORMATION& di );
BOOL AccGetDriverHandle( HANDLE& hHandle );
BOOL AccStartViaMdd( HANDLE& hDev, HANDLE& hQueue );
BOOL AccStopViaMdd( HANDLE& hDev );
BOOL AccSampleVerify(HANDLE& hQEvent, LUID &sensorLuid, BOOL bShouldPass = TRUE );
BOOL AccNominalScenarioCheck( );
BOOL EnableSimStubRotation(HSENSOR& hAcc );

BOOL AccSetSystemPowerState(DWORD dwFlags );
BOOL AccSetPowerRequirement(CEDEVICE_POWER_STATE pwrState );
BOOL AccReleasePowerRequirement();
void AccGetSystemPowerState();

void AccDumpSampleToFile( ACC_DATA* acc_data , BOOL bClose = FALSE );


BOOL AccStartSampling(HSENSOR &hAcc, HANDLE& hQHandle);
BOOL CreateNominalMessageQueue(HANDLE& hQEvent );
BOOL AccTestInitialization(LPCWSTR pszCmdLine);
BOOL AccTestCleanup();
BOOL GetAccRegistryEntry(AccRegEntry& accReg);
BOOL IsAccDriverRegistered();
BOOL GetAccDevMgrInfo(DEVMGR_DEVICE_INFORMATION *di);
BOOL UnloadDriver(DEVMGR_DEVICE_INFORMATION *di);
BOOL LoadDriver(DEVMGR_DEVICE_INFORMATION *di);
BOOL LoadStubAccPdd(LPCWSTR pszPddName);
BOOL ChangeDriverState( BOOL bLoadDriver, DEVMGR_DEVICE_INFORMATION *di);
void DumpDevMgrDevInfo( PDEVMGR_DEVICE_INFORMATION di );
void DumpSensorCaps( SENSOR_DEVICE_CAPS &sensorCaps );
void DumpPowerCaps( POWER_CAPABILITIES &powerCaps );
void DumpPowerState( CEDEVICE_POWER_STATE &powerState );
void DumpAccRegEntries( AccRegEntry& accReg );


BOOL IsModuleInstalled(LPCWSTR pszModName);


BOOL ProcessCmdLine(LPCWSTR pszCmdLine);
void Usage();
BOOL GetValueForParam( 
    const TCHAR* pszCmdLine, 
    const TCHAR* pszParam, 
    TCHAR* pszVal, 
    DWORD dwValSize );

void FillRandStr( TCHAR* pStr, DWORD dwSize );


void rotationCallback( ACC_ORIENT* acc_data );
void streamCallback( ACC_DATA* acc_data );

BOOL AccDataStream( DWORD dwSleepInterval = 10*ONE_SECOND, BOOL bLog = TRUE );
BOOL AccDetectRotation( DWORD dwSleepInterval, BOOL bLog = TRUE);
BOOL AccConvertToStaticAngles( ACC_DATA* accData, DOUBLE& alphaPitch, DOUBLE& alphaRoll );






