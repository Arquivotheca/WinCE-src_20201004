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

//------------------------------------------------------------------------------
// Global declarations:
// The following items exist here to enable the project header
// file to be included by multiple source files without
// causing multiple definition errors at link time
//
CKato *g_pKato = NULL;
SPS_SHELL_INFO *g_pShellInfo = NULL;
extern "C" {
    __declspec(dllexport) CKato* GetKato(void){return g_pKato;}
}

extern AccTestParam g_AccParam;
extern HANDLE g_hPwrReq;

HINSTANCE g_hInstDll = NULL;





#define PPARAM ((DWORD)(&sTestParam))

//  Description - Depth(UINT) - User Data(DWORD) - UniqueID(DWORD) - Test Proc
FUNCTION_TABLE_ENTRY g_lpFTE[] = 
{
    _T("---------------------------------------------------------------------"),0,  0,     0,   NULL,
    _T("                 ACCELEROMETER FUNCTIONAL TEST SUITE                 "),0,  0,     0,   NULL,
    _T("---------------------------------------------------------------------"),0,  0,     0,   NULL,
    _T(" "),                                                                    0,  0,     0,   NULL,
    _T(" "),                                                                    0,  0,     0,   NULL,
    _T("Accelerometer Native API (Pri 1)"),                                     0,  0,     0,   NULL,
    _T("OpenSensor() - Nominal"),                                               1,  1,  1001,   Tux_OpenSensor01,
    _T("AccelerometerStart() - Nominal"),                                       1,  1,  1002,   Tux_AccelerometerStart01,
    _T("AccelerometerStop() - Nominal"),                                        1,  1,  1003,   Tux_AccelerometerStop01,
    _T("AccelerometerCreateCallback() - Nominal"),                              1,  1,  1004,   Tux_AccelerometerCreateCallback01,
    _T("AccelerometerCancelCallback() - Nominal"),                              1,  1,  1005,   Tux_AccelerometerCancelCallback01,
    _T("AccelerometerSetMode() - Nominal"),                                     1,  1,  1006,   Tux_AccelerometerSetMode01,
    _T(" "),                                                                    0,  0,     0,   NULL,
    _T("Accelerometer Native API (Pri 2)"),                                     0,  0,     0,   NULL,
    _T("OpenSensor - Input Validation"),                                        1,  1,  1102,   Tux_OpenSensor02,
    _T("OpenSensor - Input Validation"),                                        1,  1,  1103,   Tux_OpenSensor03,
    _T("OpenSensor - Input Validation"),                                        1,  1,  1104,   Tux_OpenSensor04,
    _T("OpenSensor - Input Validation"),                                        1,  1,  1105,   Tux_OpenSensor05,
    _T("OpenSensor - Input Validation"),                                        1,  1,  1106,   Tux_OpenSensor06,
    _T("OpenSensor - Input Validation"),                                        1,  1,  1107,   Tux_OpenSensor07,
    _T("OpenSensor - Input Validation"),                                        1,  1,  1108,   Tux_OpenSensor08,
    _T("OpenSensor - Input Validation"),                                        1,  1,  1109,   Tux_OpenSensor09,
    _T("OpenSensor - Input Validation"),                                        1,  1,  1110,   Tux_OpenSensor10,   
    _T("AccelerometerStart - Input Validation"),                                1,  1,  1122,   Tux_AccelerometerStart02,
    _T("AccelerometerStart - Input Validation"),                                1,  1,  1123,   Tux_AccelerometerStart03,
    _T("AccelerometerStart - Input Validation"),                                1,  1,  1124,   Tux_AccelerometerStart04,
    _T("AccelerometerStart - Input Validation"),                                1,  1,  1125,   Tux_AccelerometerStart05,
    _T("AccelerometerStart - Input Validation"),                                1,  1,  1127,   Tux_AccelerometerStart07,
    _T("AccelerometerStart - Input Validation"),                                1,  1,  1128,   Tux_AccelerometerStart08,
    _T("AccelerometerStop - Input Validation"),                                 1,  1,  1142,   Tux_AccelerometerStop02,
    _T("AccelerometerStop - Input Validation"),                                 1,  1,  1143,   Tux_AccelerometerStop03,
    _T("AccelerometerStop - Input Validation"),                                 1,  1,  1144,   Tux_AccelerometerStop04,
    _T("AccelerometerCreateCallback - Input Validation"),                       1,  1,  1162,   Tux_AccelerometerCreateCallback02,
    _T("AccelerometerCreateCallback - Input Validation"),                       1,  1,  1163,   Tux_AccelerometerCreateCallback03,
    _T("AccelerometerCreateCallback - Input Validation"),                       1,  1,  1164,   Tux_AccelerometerCreateCallback04,
    _T("AccelerometerCreateCallback - Input Validation"),                       1,  1,  1165,   Tux_AccelerometerCreateCallback06,
    _T("AccelerometerCancelCallback - Input Validation"),                       1,  1,  1182,   Tux_AccelerometerCancelCallback02,
    _T("AccelerometerCancelCallback - Input Validation"),                       1,  1,  1183,   Tux_AccelerometerCancelCallback03,
    _T("AccelerometerCancelCallback - Input Validation"),                       1,  1,  1184,   Tux_AccelerometerCancelCallback04,
    _T("AccelerometerCancelCallback - Input Validation"),                       1,  1,  1185,   Tux_AccelerometerCancelCallback05,
    _T("AccelerometerSetMode() - Input Validation"),                            1,  1,  1202,   Tux_AccelerometerSetMode02,
    _T("AccelerometerSetMode() - Input Validation"),                            1,  1,  1203,   Tux_AccelerometerSetMode03,    
    _T("AccelerometerSetMode() - Input Validation"),                            1,  1,  1204,   Tux_AccelerometerSetMode04,
    _T("AccelerometerSetMode() - Input Validation"),                            1,  1,  1205,   Tux_AccelerometerSetMode05,    
    _T(" "),                                                                    0,  0,     0,   NULL,
    _T("Accelerometer MDD IOCTL (Pri 1)"),                                      0,  0,     0,   NULL,  
    _T("START IOCTL - Nominal"),                                                1,  1,  2001,   Tux_IOCTL_ACC_STARTSENSOR01,
    _T("STOP IOCTL - Nominal"),                                                 1,  1,  2002,   Tux_IOCTL_ACC_STOPSENSOR01,
    _T("Sensor Get Caps - Nominal"),                                            1,  1,  2003,   Tux_IOCTL_SENSOR_CSMDD_GET_CAPS01,
    _T("Power Caps - Nominal"),                                                 1,  1,  2004,   Tux_IOCTL_POWER_CAPABILITIES01,
    _T("Power Set - Nominal"),                                                  1,  1,  2005,   Tux_IOCTL_POWER_SET01,
    _T("Power Get - Nominal"),                                                  1,  1,  2006,   Tux_IOCTL_POWER_GET01,
    _T("Set Config IOCTL - Nominal"),                                           1,  1,  2007,   Tux_IOCTL_ACC_SETCONFIG_MODE01,
    _T(" "),                                                                    0,  0,     0,   NULL,
    _T("Accelerometer MDD IOCTL (Pri 2)"),                                      0,  0,     0,   NULL,  
    _T("START IOCTL - Input Validation"),                                       1,  1,  2102,   Tux_IOCTL_ACC_STARTSENSOR02,
    _T("START IOCTL - Input Validation"),                                       1,  1,  2104,   Tux_IOCTL_ACC_STARTSENSOR04,
    _T("START IOCTL - Input Validation"),                                       1,  1,  2106,   Tux_IOCTL_ACC_STARTSENSOR06,
    _T("START IOCTL - Input Validation"),                                       1,  1,  2108,   Tux_IOCTL_ACC_STARTSENSOR08,
    _T("START IOCTL - Input Validation"),                                       1,  1,  2109,   Tux_IOCTL_ACC_STARTSENSOR09,
    _T("STOP IOCTL - Input Validation"),                                        1,  1,  2122,   Tux_IOCTL_ACC_STOPSENSOR02,
    _T("STOP IOCTL - Input Validation"),                                        1,  1,  2123,   Tux_IOCTL_ACC_STOPSENSOR03,    
    _T("STOP IOCTL - Input Validation"),                                        1,  1,  2128,   Tux_IOCTL_ACC_STOPSENSOR08,
    _T("Sensor Get Caps - Input Validation"),                                   1,  1,  2142,   Tux_IOCTL_SENSOR_CSMDD_GET_CAPS02,
    _T("Sensor Get Caps - Input Validation"),                                   1,  1,  2143,   Tux_IOCTL_SENSOR_CSMDD_GET_CAPS03,
    _T("Sensor Get Caps - Input Validation"),                                   1,  1,  2144,   Tux_IOCTL_SENSOR_CSMDD_GET_CAPS04,    
    _T("Sensor Get Caps - Input Validation"),                                   1,  1,  2145,   Tux_IOCTL_SENSOR_CSMDD_GET_CAPS05,    
    _T("Power Caps - Input Validation"),                                        1,  1,  2162,   Tux_IOCTL_POWER_CAPABILITIES02,    
    _T("Power Set - Input Validation"),                                         1,  1,  2182,   Tux_IOCTL_POWER_SET02,
    _T("Power Get - Input Validation"),                                         1,  1,  2202,   Tux_IOCTL_POWER_GET02,    
    _T("Set Config IOCTL - Input Validation"),                                  1,  1,  2222,   Tux_IOCTL_ACC_SETCONFIG_MODE02,
    _T("Set Config IOCTL - Input Validation"),                                  1,  1,  2223,   Tux_IOCTL_ACC_SETCONFIG_MODE03,
    _T("Set Config IOCTL - Input Validation"),                                  1,  1,  2224,   Tux_IOCTL_ACC_SETCONFIG_MODE04,
    _T("Set Config IOCTL - Input Validation"),                                  1,  1,  2225,   Tux_IOCTL_ACC_SETCONFIG_MODE05,
    _T("Set Config IOCTL - Input Validation"),                                  1,  1,  2226,   Tux_IOCTL_ACC_SETCONFIG_MODE06,
    _T(" "),                                                                    0,  0,     0,   NULL,
    _T(" "),                                                                    0,  0,     0,   NULL,
    _T("---------------------------------------------------------------------"),0,  0,     0,   NULL,
    _T("                 ACCELEROMETER SCENARIO TEST SUITE                   "),0,  0,     0,   NULL,
    _T("---------------------------------------------------------------------"),0,  0,     0,   NULL,
    _T(" "),                                                                    0,  0,     0,   NULL,
    _T(" "),                                                                    0,  0,     0,   NULL,
    _T("Start Sensor Scenario - Multi-Start:Same Handle:Different Queue"),      1,  1,  3001,   Tux_Scenario01,
//  _T("Start Sensor Scenario - Multi-Start:Different Handle:Same Queue"),      1,  1,  3002,   Tux_Scenario02,
//  _T("Start Sensor Scenario - Multi-Start:Same Handle:Different Queue"),      1,  1,  3003,   Tux_Scenario03,
    _T("Start Sensor Scenario - Multi-Start:Different Handle:Same Queue"),      1,  1,  3004,   Tux_Scenario04,
    _T("Start Sensor Scenario - Multi-Start:Same Handle:Different Queue"),      1,  1,  3005,   Tux_Scenario05,
    _T("Start Sensor Scenario - Multi-Start:Different Handle:Same Queue"),      1,  1,  3006,   Tux_Scenario06,
    _T("Start Sensor Scenario - Multi-Start:Same Handle:Different Queue"),      1,  1,  3007,   Tux_Scenario07,
    _T("Start Sensor Scenario - Multi-Start:Different Handle:Same Queue"),      1,  1,  3008,   Tux_Scenario08,
    _T("Start Sensor Scenario - Multi-Start:Same Handle:Different Queue"),      1,  1,  3009,   Tux_Scenario09,
//  _T("Start Sensor Scenario - Multi-Start:Same Handle:Different Queue"),      1,  1,  3010,   Tux_Scenario10,
//  _T("Start Sensor Scenario - Multi-Start:Different Handle:Same Queue"),      1,  1,  3011,   Tux_Scenario11,
    _T("Start Sensor Scenario - Multi-Start:Same Handle:Different Queue"),      1,  1,  3012,   Tux_Scenario12, 
    _T("Idle Power State Scenario"),                                            1,  1,  3014,   Tux_Scenario14, 
    _T("Message Queue Saturation"),                                             1,  1,  3015,   Tux_Scenario15, 
    _T(" "),                                                                    0,  0,     0,   NULL,
    _T(" "),                                                                    0,  0,     0,   NULL,
    /*_T("---------------------------------------------------------------------"),0,  0,     0,   NULL,
    _T("                 ACCELEROMETER SCENARIO TEST SUITE                   "),0,  0,     0,   NULL,
    _T("---------------------------------------------------------------------"),0,  0,     0,   NULL,
    _T(" "),                                                                    0,  0,     0,   NULL,
    _T(" "),                                                                    0,  0,     0,   NULL,   
    _T(" "),                                                                    0,  0,     0,   NULL,
    _T(" "),                                                                    0,  0,     0,   NULL,*/
    _T("---------------------------------------------------------------------"),0,  0,     0,   NULL,
    NULL,                                                                       0,  0,     0,   NULL  
};
        
      



    
//------------------------------------------------------------------------------
// There's rarely much need to modify the remaining two functions
// (DllMain and ShellProc) unless you need to debug some very
// strange behavior, or if you are doing other customizations.
//
BOOL WINAPI DllMain( HANDLE hInstance, ULONG dwReason, LPVOID lpReserved ) 
{
    UNREFERENCED_PARAMETER(lpReserved);

    switch( dwReason )
    {
        case DLL_PROCESS_ATTACH:
            Debug(TEXT("%s: DLL_PROCESS_ATTACH\r\n"), MODULE_NAME);  
            g_hInstDll = (HINSTANCE)hInstance;
            return TRUE;

        case DLL_PROCESS_DETACH:
            Debug(TEXT("%s: DLL_PROCESS_DETACH\r\n"), MODULE_NAME);
            return TRUE;
    }
    return FALSE;
}//DllMain


//------------------------------------------------------------------------------
//
//
//
SHELLPROCAPI ShellProc( UINT uMsg, SPPARAM spParam ) 
{
    LPSPS_BEGIN_TEST pBT = {0};
    LPSPS_END_TEST pET = {0};
    int iReturn = SPR_HANDLED;
        
    switch (uMsg) 
    {    

        //----------------------------------------------------------------------
        // Message: SPM_LOAD_DLL
        // Sent once to the DLL immediately after it is loaded. The spParam
        // parameter will contain a pointer to a SPS_LOAD_DLL structure. The
        // DLL should set the fUnicode member of this structure to TRUE if the
        // DLL is built with the UNICODE flag set. If you set this flag, Tux
        // will ensure that all strings passed to your DLL will be in UNICODE
        // format, and all strings within your function table will be processed
        // by Tux as UNICODE. The DLL may return SPR_FAIL to prevent the DLL
        // from continuing to load.
        case SPM_LOAD_DLL: 
            Debug(_T("ShellProc(SPM_LOAD_DLL, ...) called\r\n"));

            // If we are UNICODE, then tell Tux this by setting the following flag.
            #ifdef UNICODE
                ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
            #endif

            // turn on kato debugging
            KatoDebug(1, KATO_MAX_VERBOSITY,KATO_MAX_VERBOSITY,KATO_MAX_LEVEL);
            g_pKato = (CKato*)KatoGetDefaultObject();

            break;  


        //----------------------------------------------------------------------
        // Message: SPM_UNLOAD_DLL
        case SPM_UNLOAD_DLL: 
            Debug(_T("ShellProc(SPM_UNLOAD_DLL, ...) called"));   
            
            ////////////////////////////////////////////////////////////////////
            // Test Specific Setup
            ////////////////////////////////////////////////////////////////////
            if( !AccTestCleanup() )
            {
                Debug(_T("### ERROR: Test Cleanup Failed. Refer to log for details" ));
                iReturn = SPR_FAIL;
            }
            break;     


        //----------------------------------------------------------------------
        // Message: SPM_SHELL_INFO
        // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
        // some useful information about its parent shell and environment. The
        // spParam parameter will contain a pointer to a SPS_SHELL_INFO
        // structure. The pointer to the structure may be stored for later use
        // as it will remain valid for the life of this Tux Dll. The DLL may
        // return SPR_FAIL to prevent the DLL from continuing to load.
        case SPM_SHELL_INFO:
            Debug(_T("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));

            // Store a pointer to our shell info for later use.
            g_pShellInfo = (LPSPS_SHELL_INFO)spParam;


            ////////////////////////////////////////////////////////////////////
            // Test Specific Setup
            ////////////////////////////////////////////////////////////////////
            if( !AccTestInitialization(g_pShellInfo->szDllCmdLine) )
            {
                Debug(_T("### ERROR: Test Setup Failed. Refer to log for details" ));
                iReturn = SPR_FAIL;
            }            
            break;

        //----------------------------------------------------------------------
        // Message: SPM_REGISTER
        case SPM_REGISTER:
            Debug(_T("ShellProc(SPM_REGISTER, ...) called\r\n"));
            
            ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;

            #ifdef UNICODE
                iReturn  = SPR_HANDLED | SPF_UNICODE;
            #endif
            break;

        //----------------------------------------------------------------------
        // Message: SPM_START_SCRIPT
        // Sent to the DLL immediately before a script is started. It is sent
        // to all Tux DLLs, including loaded Tux DLLs that are not in the
        // script. All DLLs will receive this message before the first
        // TestProc() in the script is called.
        case SPM_START_SCRIPT:
            Debug(_T("ShellProc(SPM_START_SCRIPT, ...) called\r\n"));   
            AccGetSystemPowerState();
            break;

        //----------------------------------------------------------------------
        // Message: SPM_STOP_SCRIPT
        case SPM_STOP_SCRIPT:
            break;

        //----------------------------------------------------------------------
        // Message: SPM_BEGIN_GROUP
        case SPM_BEGIN_GROUP:
            Debug(_T("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n"));
            g_pKato->BeginLevel(0, _T("BEGIN GROUP:DrvDetector"));            
            break;

        //----------------------------------------------------------------------
        // Message: SPM_END_GROUP
        case SPM_END_GROUP:
            Debug(_T("ShellProc(SPM_END_GROUP, ...) called\r\n"));
            g_pKato->EndLevel(_T("END GROUP:DrvDetector"));
            break;            

        //----------------------------------------------------------------------
        // Message: SPM_BEGIN_TEST
        case SPM_BEGIN_TEST:
            // Start our logging level.
            Sleep(1);
            pBT = (LPSPS_BEGIN_TEST)spParam;
            LOG( "SPM_BEGIN_TEST" );
            if( pBT->lpFTE->dwUniqueID != 3014 )
            {
                AccSetPowerRequirement(D0);
                AccGetSystemPowerState();
            }

            g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID, 
                                _T("BEGIN TEST: %s, Threads=%u, Seed=%u"),
                                pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                                pBT->dwRandomSeed);

            break;

        //----------------------------------------------------------------------
        // Message: SPM_END_TEST
        case SPM_END_TEST:
            // End our logging level.
            pET = (LPSPS_END_TEST)spParam;
            LOG( "SPM_END_TEST" );
            if( pET->lpFTE->dwUniqueID != 3014 )
            {
                AccGetSystemPowerState();
                AccReleasePowerRequirement();
            }
            g_pKato->EndLevel(_T("END TEST: %s, %s, Time=%u.%03u"),
                            pET->lpFTE->lpDescription,
                            pET->dwResult == TPR_SKIP ? _T("SKIPPED") :
                            pET->dwResult == TPR_PASS ? _T("PASSED") :
                            pET->dwResult == TPR_FAIL ? _T("FAILED") : _T("ABORTED"),
                            pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

            break;

        //----------------------------------------------------------------------
        // Message: SPM_EXCEPTION
        case SPM_EXCEPTION:
            Debug(_T("ShellProc(SPM_EXCEPTION, ...) called\r\n"));
            g_pKato->Log(LOG_EXCEPTION, _T("%s: Exception Thrown!"), MODULE_NAME);
            break;

            
        default:
            Debug(_T("ShellProc received message it is not handling: 0x%X\r\n"), uMsg);
            iReturn = SPR_NOT_HANDLED;
            break;
    }


    return iReturn;
}//ShellProc
