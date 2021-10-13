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
//  Copyright (c) Microsoft Corporation
//
//  Module:
//          Contains the test functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"
#include <pm.h>
#include <nkintr.h>


// We have 16 combinations of device power states to test.
const UCHAR g_DeviceDx[] = {0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F,
                            0x11, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1F};


#define NUM_DEVICE_STATES 16
#define MAX_DEVICE_NAME   32


//DEFAULT Power Model Timeouts
typedef struct _SYS_TIMEOUTS{
    DWORD  dwACUserIdleTimeout;
    DWORD  dwACSysIdleTimeout;
    DWORD  dwACSuspendTimeout;
}SYS_TIMEOUTS, *PSYS_TIMEOUTS;

SYS_TIMEOUTS    g_sysTimeouts;

//PDA Power Model Timeouts
DWORD g_dwUserIdleTimeout;
DWORD g_dwBacklightTimeout;


// Need lengthy timeouts for the test so we don't see a lot of state transitions
// DEFAULT Power Model
#define TST_SYS_ACUSERIDLETIMEOUT 120 // 120 sec for user idle timeout
#define TST_SYS_ACSYSIDLETIMEOUT  120 // 120 sec for system idle timeout
#define TST_SYS_ACSUSPENDTIMOUT   120 // 120 sec for system suspend timeout

// PDA Power Model
#define TST_PDA_USERIDLETIMEOUT   120
#define TST_PDA_BACKLIGHTTIMEOUT  120


// Helper functions
BOOL SetTimeOuts();
BOOL RestoreTimeOuts();

TESTPROCAPI LowerCeilingRaiseFloor(UCHAR DeviceDx);
TESTPROCAPI SetDevicePowerPassive(UCHAR DeviceDx);
TESTPROCAPI SetDevicePowerActive(UCHAR DeviceDx);
TESTPROCAPI DevicePowerNotifyTest(UCHAR DeviceDx);
TESTPROCAPI RaiseCeilingPassive(UCHAR DeviceDx);
TESTPROCAPI RaiseCeilingActive(UCHAR DeviceDx);
TESTPROCAPI LowerFloorPassive(UCHAR DeviceDx);
TESTPROCAPI LowerFloorActive(UCHAR DeviceDx);
TESTPROCAPI RequestUnsupportedStates(UCHAR DeviceDx);

BOOL ValidatePowerChange(BOOL fChanged, LPCTSTR pszChange, LPTSTR pszDevName,
    CEDEVICE_POWER_STATE dxActual, CEDEVICE_POWER_STATE dxExpected, CEDEVICE_POWER_STATE dxPrevious);

BOOL ValidatePowerChangeCeiling(BOOL fChanged, LPCTSTR pszChange, LPTSTR pszDevName,
    CEDEVICE_POWER_STATE dxActual, CEDEVICE_POWER_STATE dxExpected, CEDEVICE_POWER_STATE dxPrevious);


// Test Procs

////////////////////////////////////////////////////////////////////////////////
// Tst_LowerCeilingRaiseFloor
//
//  for each ceiling value D0 through D4 :
//
//      Test setting the ceiling:
//
//          Call SetSystemPowerState and verify success
//          Determine the expected new power state based on what the driver supports
//          if the previous power state is equal to the expected new state
//              Verify that the driver receives no power change notification
//          otherwise,
//              Verify that the driver receives power change notification
//              Verify that the expected state is requested
//
//          for each floor value D4 through D0:
//
//              Test setting the floor:
//
//                  Call SetPowerRequirement and verify success
//                  Determine the expected new power state based on what the driver supports
//                  if the expected value is lower than the current power setting of the device
//                      Verify that the driver receives no power change notification
//                  Otherwise,
//                      Verify that the driver receives power change notification
//                      Verify that the expected state is requested
//
//              Test removing the floor:
//
//                  Call ReleasePowerRequirement and verify success
//                  if the previously expected value was lower than the current power setting
//                      Verify that the driver receives no power change notification
//                  Otherwise
//                      Verify that the driver receives power change notficication
//                      Verify that the previous power state is requested
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_LowerCeilingRaiseFloor(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    // The shell doesn't necessarily want us to execute the test. Make sure first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwPass = 0;
    DWORD dwFail = 0;

    // Lengthen the Timeout values so that state transitions don't occur when running this test.
    // If power states change during this test, this will skew the results.
    if (!SetTimeOuts())
    {
        LOG(_T("FAILED to set timeout values"));
    }

    //Test with each variation of device power states
    for(DWORD dwIndex = 0; dwIndex < NUM_DEVICE_STATES; dwIndex++)
    {
        LOG (_T("")); //blank line
        LOG(_T("**** Test for DeviceDx=0x%02X ****"), g_DeviceDx[dwIndex]);

        if(TPR_PASS != LowerCeilingRaiseFloor(g_DeviceDx[dwIndex]))
        {
               LOG(_T("**** Test for DeviceDx=0x%02X FAILED ****"), g_DeviceDx[dwIndex]);
               dwFail++;
               returnVal = TPR_FAIL;
        }
        else
        {
               LOG(_T("**** Test for DeviceDx=0x%02X PASSED ****"), g_DeviceDx[dwIndex]);
               dwPass++;
        }
    }

    if (!RestoreTimeOuts())
    {
        LOG(_T("FAILED to restore timeout values"));
    }

    LOG(_T("Number of subtests that PASSED = %u, Number of subtests that FAILED = %u"), dwPass, dwFail);

    return returnVal;
}


////////////////////////////////////////////////////////////////////////////////
// Tst_SetDevicePower
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_SetDevicePowerPassive(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwPass = 0;
    DWORD dwFail = 0;

    // Lengthen the Timeout values so that state transitions don't occur when running this test.
    // If power states change during this test, this will skew the results.
    if (!SetTimeOuts())
    {
        LOG(_T("FAILED to set timeout values"));
    }


    // Test with each variation of device power states
    for(DWORD dwIndex = 0; dwIndex < NUM_DEVICE_STATES; dwIndex++)
    {
        LOG (_T("")); //blank line
        LOG(_T("**** Test for DeviceDx=0x%02X ****"), g_DeviceDx[dwIndex]);

        if(TPR_PASS != SetDevicePowerPassive(g_DeviceDx[dwIndex]))
        {
               LOG(_T("**** Test for DeviceDx=0x%02X FAILED ****"), g_DeviceDx[dwIndex]);
               dwFail++;
               returnVal = TPR_FAIL;
        }
        else
        {
               LOG(_T("**** Test for DeviceDx=0x%02X PASSED ****"), g_DeviceDx[dwIndex]);
               dwPass++;
        }
    }

    if (!RestoreTimeOuts())
    {
        LOG(_T("FAILED to restore timeout values"));
    }

    LOG(_T("Number of subtests that PASSED = %u, Number of subtests that FAILED = %u"), dwPass, dwFail);

    return returnVal;
}


////////////////////////////////////////////////////////////////////////////////
// Tst_SetDeviceActive
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_SetDevicePowerActive(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwPass = 0;
    DWORD dwFail = 0;

    // Lengthen the Timeout values so that state transitions don't occur when running this test.
    // If power states change during this test, this will skew the results.
    if (!SetTimeOuts())
    {
        LOG(_T("FAILED to set timeout values"));
    }


    // Test with each variation of device power states
    for(DWORD dwIndex = 0; dwIndex < NUM_DEVICE_STATES; dwIndex++)
    {
        LOG (_T("")); //blank line
        LOG(_T("**** Test for DeviceDx=0x%02X ****"), g_DeviceDx[dwIndex]);

        if(TPR_PASS != SetDevicePowerActive(g_DeviceDx[dwIndex]))
        {
               LOG(_T("**** Test for DeviceDx=0x%02X FAILED ****"), g_DeviceDx[dwIndex]);
               dwFail++;
               returnVal = TPR_FAIL;
        }
        else
        {
               LOG(_T("**** Test for DeviceDx=0x%02X PASSED ****"), g_DeviceDx[dwIndex]);
               dwPass++;
        }
    }

    if (!RestoreTimeOuts())
    {
    LOG(_T("FAILED to restore timeout values"));
    }

    LOG(_T("Number of subtests that PASSED = %u, Number of subtests that FAILED = %u"), dwPass, dwFail);

    return returnVal;
}


////////////////////////////////////////////////////////////////////////////////
// Tst_DevicePowerNotify
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_DevicePowerNotify(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwPass = 0;
    DWORD dwFail = 0;

    // Lengthen the Timeout values so that state transitions don't occur when running this test.
    // If power states change during this test, this will skew the results.
    if (!SetTimeOuts())
    {
        LOG(_T("FAILED to set timeout values"));
    }

    // Test with each variation of device power states
    for(DWORD dwIndex = 0; dwIndex < NUM_DEVICE_STATES; dwIndex++)
    {
        LOG (_T("")); //blank line
        LOG(_T("**** Test for DeviceDx=0x%02X ****"), g_DeviceDx[dwIndex]);

        if(TPR_PASS != DevicePowerNotifyTest(g_DeviceDx[dwIndex]))
        {
               LOG(_T("**** Test for DeviceDx=0x%02X FAILED ****"), g_DeviceDx[dwIndex]);
               dwFail++;
               returnVal = TPR_FAIL;
        }
        else
        {
               LOG(_T("**** Test for DeviceDx=0x%02X PASSED ****"), g_DeviceDx[dwIndex]);
               dwPass++;
        }
    }

    if (!RestoreTimeOuts())
    {
        LOG(_T("FAILED to restore timeout values"));
    }

    LOG(_T("Number of subtests that PASSED = %u, Number of subtests that FAILED = %u"), dwPass, dwFail);

    return returnVal;
}



////////////////////////////////////////////////////////////////////////////////
// Tst_RaiseCeilingPassive
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RaiseCeilingPassive(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwPass = 0;
    DWORD dwFail = 0;

    // Lengthen the Timeout values so that state transitions don't occur when running this test.
    // If power states change during this test, this will skew the results.
    if (!SetTimeOuts())
    {
        LOG(_T("FAILED to set timeout values"));
    }

    // Test with each variation of device power states
    for(DWORD dwIndex = 0; dwIndex < NUM_DEVICE_STATES; dwIndex++)
    {
        LOG (_T("")); //blank line
        LOG(_T("**** Test for DeviceDx=0x%02X ****"), g_DeviceDx[dwIndex]);

        if(TPR_PASS != RaiseCeilingPassive(g_DeviceDx[dwIndex]))
        {
               LOG(_T("**** Test for DeviceDx=0x%02X FAILED ****"), g_DeviceDx[dwIndex]);
               dwFail++;
               returnVal = TPR_FAIL;
        }
        else
        {
               LOG(_T("**** Test for DeviceDx=0x%02X PASSED ****"), g_DeviceDx[dwIndex]);
               dwPass++;
        }
    }

    if (!RestoreTimeOuts())
    {
        LOG(_T("FAILED to restore timeout values"));
    }

    LOG(_T("Number of subtests that PASSED = %u, Number of subtests that FAILED = %u"), dwPass, dwFail);

    return returnVal;
}


////////////////////////////////////////////////////////////////////////////////
// Tst_RaiseCeilingActive
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RaiseCeilingActive(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwPass = 0;
    DWORD dwFail = 0;

    // Lengthen the Timeout values so that state transitions don't occur when running this test.
    // If power states change during this test, this will skew the results.
    if (!SetTimeOuts())
    {
        LOG(_T("FAILED to set timeout values"));
    }

    // Test with each variation of device power states
    for(DWORD dwIndex = 0; dwIndex < NUM_DEVICE_STATES; dwIndex++)
    {
        LOG (_T("")); //blank line
        LOG(_T("**** Test for DeviceDx=0x%02X ****"), g_DeviceDx[dwIndex]);

        if(TPR_PASS != RaiseCeilingActive(g_DeviceDx[dwIndex]))
        {
               LOG(_T("**** Test for DeviceDx=0x%02X FAILED ****"), g_DeviceDx[dwIndex]);
               dwFail++;
               returnVal = TPR_FAIL;
        }
        else
        {
               LOG(_T("**** Test for DeviceDx=0x%02X PASSED ****"), g_DeviceDx[dwIndex]);
               dwPass++;
        }
    }

    if (!RestoreTimeOuts())
    {
        LOG(_T("FAILED to restore timeout values"));
    }

    LOG(_T("Number of subtests that PASSED = %u, Number of subtests that FAILED = %u"), dwPass, dwFail);

    return returnVal;
}


////////////////////////////////////////////////////////////////////////////////
// Tst_LowerFloorPassive
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_LowerFloorPassive(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwPass = 0;
    DWORD dwFail = 0;

    // Lengthen the Timeout values so that state transitions don't occur when running this test.
    // If power states change during this test, this will skew the results.
    if (!SetTimeOuts())
    {
        LOG(_T("FAILED to set timeout values"));
    }

    // Test with each variation of device power states
    for(DWORD dwIndex = 0; dwIndex < NUM_DEVICE_STATES; dwIndex++)
    {
        LOG (_T("")); //blank line
        LOG(_T("**** Test for DeviceDx=0x%02X ****"), g_DeviceDx[dwIndex]);

        if(TPR_PASS != LowerFloorPassive(g_DeviceDx[dwIndex]))
        {
               LOG(_T("**** Test for DeviceDx=0x%02X FAILED ****"), g_DeviceDx[dwIndex]);
               dwFail++;
               returnVal = TPR_FAIL;
        }
        else
        {
               LOG(_T("**** Test for DeviceDx=0x%02X PASSED ****"), g_DeviceDx[dwIndex]);
               dwPass++;
        }
    }

    if (!RestoreTimeOuts())
    {
        LOG(_T("FAILED to restore timeout values"));
    }

    LOG(_T("Number of subtests that PASSED = %u, Number of subtests that FAILED = %u"), dwPass, dwFail);

    return returnVal;
}


////////////////////////////////////////////////////////////////////////////////
// Tst_LowerFloorActive
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_LowerFloorActive(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwPass = 0;
    DWORD dwFail = 0;

    // Lengthen the Timeout values so that state transitions don't occur when running this test.
    // If power states change during this test, this will skew the results.
    if (!SetTimeOuts())
    {
        LOG(_T("FAILED to set timeout values"));
    }

    // Test with each variation of device power states
    for(DWORD dwIndex = 0; dwIndex < NUM_DEVICE_STATES; dwIndex++)
    {
        LOG (_T("")); //blank line
        LOG(_T("**** Test for DeviceDx=0x%02X ****"), g_DeviceDx[dwIndex]);

        if(TPR_PASS != LowerFloorActive(g_DeviceDx[dwIndex]))
        {
               LOG(_T("**** Test for DeviceDx=0x%02X FAILED ****"), g_DeviceDx[dwIndex]);
               dwFail++;
               returnVal = TPR_FAIL;
        }
        else
        {
               LOG(_T("**** Test for DeviceDx=0x%02X PASSED ****"), g_DeviceDx[dwIndex]);
               dwPass++;
        }
    }

    if (!RestoreTimeOuts())
    {
        LOG(_T("FAILED to restore timeout values"));
    }

    LOG(_T("Number of subtests that PASSED = %u, Number of subtests that FAILED = %u"), dwPass, dwFail);

    return returnVal;
}


////////////////////////////////////////////////////////////////////////////////
// Tst_RequestUnsupportedStates
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_RequestUnsupportedStates(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwPass = 0;
    DWORD dwFail = 0;

    // Lengthen the Timeout values so that state transitions don't occur when running this test.
    // If power states change during this test, this will skew the results.
    if (!SetTimeOuts())
        LOG(_T("FAILED to set timeout values"));

    // Test with each variation of device power states
    for(DWORD dwIndex = 0; dwIndex < NUM_DEVICE_STATES; dwIndex++)
    {
        LOG (_T("")); //blank line
        LOG(_T("**** Test for DeviceDx=0x%02X ****"), g_DeviceDx[dwIndex]);

        if(TPR_PASS != RequestUnsupportedStates(g_DeviceDx[dwIndex]))
        {
               LOG(_T("**** Test for DeviceDx=0x%02X FAILED ****"), g_DeviceDx[dwIndex]);
               dwFail++;
               returnVal = TPR_FAIL;
        }
        else
        {
               LOG(_T("**** Test for DeviceDx=0x%02X PASSED ****"), g_DeviceDx[dwIndex]);
               dwPass++;
        }
    }

    if (!RestoreTimeOuts())
    {
        LOG(_T("FAILED to restore timeout values"));
    }

    LOG(_T("Number of subtests that PASSED = %u, Number of subtests that FAILED = %u"), dwPass, dwFail);

    return returnVal;
}


////////////////////////////////////////////////////////////////////////////////
// LowerCeilingRaiseFloor
//
// Parameters:
//  DeviceDx  Bitmask indicating the device states to be supported by the 
//            test driver
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI LowerCeilingRaiseFloor(UCHAR DeviceDx)
{
    BOOL fChange;
    TCHAR szDevName[MAX_DEVICE_NAME] = _T("");
    HANDLE hPwr = NULL;
    DWORD lastErr = ERROR_SUCCESS;
    CEDEVICE_POWER_STATE dxCeiling = D0;
    CEDEVICE_POWER_STATE dxFloor = D4;
    CEDEVICE_POWER_STATE dxPrevious = PwrDeviceUnspecified;
    CEDEVICE_POWER_STATE dxActual = PwrDeviceUnspecified;
    CEDEVICE_POWER_STATE dxExpected = PwrDeviceUnspecified;
    CEDEVICE_POWER_STATE dxCurrent = PwrDeviceUnspecified;

    // device test object
    CPDX pdx;

    if(!pdx.StartTestDriver(DeviceDx))
    {
        FAIL("StartTestDriver()");
        goto Error;
    }

    LOG(_T("Created device named \"%s\" with DeviceDx=0x%02X"),
        pdx.GetDeviceName(szDevName, MAX_DEVICE_NAME), DeviceDx);

    dxActual = pdx.GetCurrentPowerState();
    dxPrevious = dxActual;

    // iterate through all the ceiling values from D0 to D4
    for(dxCeiling = D0; dxCeiling < PwrDeviceMaximum;
        dxCeiling = (CEDEVICE_POWER_STATE)((UINT)dxCeiling + 1))
    {
        dxFloor = D4; // previous loop left dxFloor as PwrDeviceUnspecified, but did not set it
        LOG(_T("")); //Blank line
        LOG(_T("*** TEST: \"SET THE CEILING\" (device=\"%s\", dxFloor=D%u, dxCeiling=D%u, ")
            _T("dxActual=D%u, DeviceDx=0x%02X)"), szDevName, dxFloor, dxCeiling, dxActual, DeviceDx);

        // determine the results we're going to expect if the device does not support
        // the power state we're setting the ceiling to
        if(DeviceDx & DX_MASK(dxCeiling))
        {
            LOG(_T("Device \"%s\" supports device state D%u"), szDevName, dxCeiling);
            dxExpected = dxCeiling;
        }
        else
        {
            LOG(_T("Device \"%s\" DOES NOT support device state D%u"), szDevName, dxCeiling);
            dxExpected = RemapDx(dxCeiling, DeviceDx);

        }

        LOG(_T("Expecting \"%s\" to go to state D%u"), szDevName, dxExpected);
        if(!pdx.SetDeviceCeiling(dxCeiling))
        {
            SetLastError(lastErr);
            ERRFAIL("SetDeviceCeiling()");
            goto Error;
        }
        else
        {
            // Wait for device power state to be updated.
            LOG(_T("Waiting for \"%s\" to receive set power request..."), szDevName);
            fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);

            if(!ValidatePowerChangeCeiling(fChange, _T("SetDeviceCeiling()"), szDevName,
                dxActual, dxExpected, dxPrevious))
            {
                goto Error;
            }

            // success setting the correct ceiling value for the device, now raise the floor from
            // D4 to D0 and verify behavior
            for(dxFloor = D4; dxFloor >= D0;
                dxFloor = (CEDEVICE_POWER_STATE)((UINT)dxFloor - 1))
            {
                dxCurrent = dxActual;

                LOG(_T("*** TEST: \"SET THE FLOOR\" (device=\"%s\", dxFloor=D%u, dxCeiling=D%u, ")
                    _T("dxActual, DeviceDx=0x%02X)"), szDevName, dxFloor, dxCeiling, dxActual, DeviceDx);

                // set a power requirement on the device to adjust the floor
                LOG(_T("Calling SetPowerRequirement(%s, D%u, POWER_NAME) to adjust the floor"),
                    szDevName, dxFloor);

                // it is possible the device will not support this power state, we must determine
                // what we expect it to do
                if(DeviceDx & DX_MASK(dxFloor))
                {
                    // devuce supports this floor value
                    //
                    LOG(_T("Device \"%s\" supports device state D%u"), szDevName, dxFloor);
                    dxExpected = dxFloor;
                }
                else
                {
                    // device does not support this floor value
                    LOG(_T("Device \"%s\" DOES NOT support device state D%u"), szDevName, dxFloor);
                    dxExpected = RemapDx(dxFloor, DeviceDx);
                }
                LOG(_T("Expecting \"%s\" floor to be set at state D%u"), szDevName, dxExpected);

                // if the current power state is higher than the new floor we expect
                // the driver to remain at the current state
                if(dxExpected > dxCurrent)
                {
                    LOG(_T("Device \"%s\" should remain at D%u because it is above the new floor D%u"),
                        szDevName, dxCurrent, dxExpected);
                    dxExpected = dxCurrent;
                }
                else if(dxExpected == dxCurrent)
                {
                    LOG(_T("Device \"%s\" should remain at D%u because it is equal to the new floor D%u"),
                        szDevName, dxCurrent, dxExpected);
                    dxExpected = dxCurrent;
                }

                // call SetPowerRequirement
                hPwr = SetPowerRequirement(szDevName, dxFloor, POWER_NAME, NULL, 0);
                if(NULL == hPwr)
                {
                    ERRFAIL("SetPowerRequirement()");
                    goto Error;
                }

                // wait for possible notificaitons and verify results
                fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);
                if(!ValidatePowerChange(fChange, _T("SetPowerRequirement()"), szDevName,
                    dxActual, dxExpected, dxCurrent))
                {
                    goto Error;
                }

                LOG(_T("*** TEST: \"RELEASE THE FLOOR\" (device=\"%s\", dxFloor=D%u, dxCeiling=D%u, ")
                    _T("DeviceDx=0x%02X)"), szDevName, dxFloor, dxCeiling, DeviceDx);

                // dxCurrent is equal to the value before the floor was raised. no matter
                // what happened after the SetPowerRequirement call, we should return
                // to dxCurrent after ther requirement was released (even if we never
                // moved from dxCurrent
                dxExpected = dxCurrent;

                // dxActual is the current power state of the device
                dxCurrent = dxActual;

                // release the power requirement on the device to put the floor back to D4
                LOG(_T("Calling ReleasePowerRequirement() on \"%s\""), szDevName);
                lastErr = ReleasePowerRequirement(hPwr);
                if(ERROR_SUCCESS != lastErr)
                {
                    SetLastError(lastErr);
                    ERRFAIL("ReleasePowerRequirement()");
                    goto Error;
                }
                
                hPwr = NULL;

                fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);
                if(!ValidatePowerChange(fChange, _T("ReleasePowerRequirement()"), szDevName,
                    dxActual, dxExpected, dxCurrent))
                {
                    goto Error;
                }
            }

            // at this point, dxActual is once again the current power state of the device
            dxPrevious = dxActual;
        }
    }

Error:
    if(hPwr)
    {
        lastErr = ReleasePowerRequirement(hPwr);
        if(ERROR_SUCCESS != lastErr)
        {
            SetLastError(lastErr);
            ERRFAIL("ReleasePowerRequirement()");
        }
        hPwr = NULL;
    }

    pdx.StopTestDriver();
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// SetDevicePowerPassive
//
// Parameters:
//  DeviceDx  Bitmask indicating the device states to be supported by the 
//            test driver
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI SetDevicePowerPassive(UCHAR DeviceDx)
{
    BOOL fChange;
    TCHAR szDevName[MAX_DEVICE_NAME] = _T("");
    DWORD lastErr = ERROR_SUCCESS;
    HANDLE hPwr = NULL;
    CEDEVICE_POWER_STATE dxSet, dxActual;
    CEDEVICE_POWER_STATE dxExpected, dxCurrent;
    CEDEVICE_POWER_STATE dxFloor, dxFloorActual;
    CEDEVICE_POWER_STATE dxCeiling, dxCeilingActual;

    // device test object
    CPDX pdx;

    if(!pdx.StartTestDriver(DeviceDx))
    {
        FAIL("StartTestDriver()");
        goto Error;
    }

    LOG(_T("Created device named \"%s\" with DeviceDx=0x%02X"),
        pdx.GetDeviceName(szDevName, MAX_DEVICE_NAME), DeviceDx);

    // default floor values for the device
    dxFloor = D4;
    dxFloorActual = RemapDx(dxFloor, DeviceDx);

    for(dxCeiling = D0; dxCeiling <= D4;
        dxCeiling = (CEDEVICE_POWER_STATE)((UINT)dxCeiling + 1))
    {
        dxCurrent = pdx.GetCurrentPowerState();

        // set the ceiling value

        // what level do we expect the ceiling to go to?
        dxCeilingActual = RemapDx(dxCeiling, DeviceDx);
        LOG(_T("Expecting ceiling for \"%s\" to be D%u"), szDevName, dxCeilingActual);

        if(!pdx.SetDeviceCeiling(dxCeiling))
        {
            SetLastError(lastErr);
            ERRFAIL("SetDeviceCeiling()");
            goto Error;
        }

        fChange = pdx.WaitForPowerChange(g_tNotifyWait, NULL);

        // iterate through all possible floor values for the device
        for(dxFloor = D4; dxFloor >= D0;
            dxFloor = (CEDEVICE_POWER_STATE)((UINT)dxFloor - 1))
        {
            // release previous power requirements
            if(hPwr)
            {
                lastErr = ReleasePowerRequirement(hPwr);
                if(ERROR_SUCCESS != lastErr)
                {
                    SetLastError(lastErr);
                    ERRFAIL("ReleasePowerRequirement()");
                    goto Error;
                }
                
                fChange = pdx.WaitForPowerChange(g_tNotifyWait, NULL);
            }
            dxCurrent = pdx.GetCurrentPowerState();

            LOG(_T("Calling SetPowerRequirement(%s, D%u, POWER_NAME, NULL, 0)"), szDevName, dxFloor);

            dxFloorActual = RemapDx(dxFloor, DeviceDx);
            LOG(_T("Expecting floor for \"%s\" to be D%u"), szDevName, dxFloorActual);

            hPwr = SetPowerRequirement(szDevName, dxFloor, POWER_NAME, NULL, 0);
            if(NULL == hPwr)
            {
                ERRFAIL("SetPowerRequirement()");
                goto Error;
            }

            fChange = pdx.WaitForPowerChange(g_tNotifyWait, NULL);

            // iterate through all possible power state values to request
            for(dxSet = D4; dxSet >= D0;
                dxSet = (CEDEVICE_POWER_STATE)((UINT)dxSet - 1))
            {
                dxCurrent = pdx.GetCurrentPowerState();

                LOG(_T("Calling SetDevicePower(%s, POWER_NAME, D%u)"), szDevName, dxSet);

                if(DeviceDx & DX_MASK(dxSet))
                {
                    LOG(_T("\"%s\" supports power state D%u"), szDevName, dxSet);
                    dxExpected = dxSet;
                }
                else
                {
                    LOG(_T("\"%s\" does not support state D%u"), szDevName, dxSet);
                    dxExpected = RemapDx(dxSet, DeviceDx);
                }

                LOG(_T("\"%s\" expecting power state D%u to map to D%u"), szDevName, dxSet,
                    dxExpected);

                lastErr = SetDevicePower(szDevName, POWER_NAME, dxSet);
                if(ERROR_SUCCESS != lastErr)
                {
                    SetLastError(lastErr);
                    ERRFAIL("SetDevicePower()");
                    goto Error;
                }

                fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);
                if(!ValidatePowerChange(fChange, _T("SetDevicePower()"), szDevName,
                    dxActual, dxExpected, dxCurrent))
                {
                    goto Error;
                }
            }

            // after requesting all legal power states, request PwrDeviceUnspecified
            // so that the device will transition back to the proper power state
            // given our imposed ceiling and floor values
            LOG(_T("Calling SetDevicePower(%s, POWER_NAME, PwrDeviceUnspecified)"),
                szDevName);

            if(dxFloorActual == dxCeilingActual)
            {
                dxExpected = dxCeilingActual;
                LOG(_T("\"%s\" floor and ceiling are both D%u, expecting device to ")
                    _T("be in state D%u"), szDevName, dxFloorActual, dxExpected);
            }
            else if(dxFloorActual < dxCeilingActual)
            {
                dxExpected = dxFloorActual;
                LOG(_T("\"%s\" floor is above ceiling, expecting device to ")
                    _T("be at floor D%u"), szDevName, dxFloorActual, dxExpected);
            }
            else
            {
                dxExpected = dxCeilingActual;
                LOG(_T("\"%s\" floor is below ceiling, expecting device to ")
                    _T("be at ceiling D%u"), szDevName, dxCeilingActual, dxExpected);
            }

            dxCurrent = pdx.GetCurrentPowerState();

            lastErr = SetDevicePower(szDevName, POWER_NAME, PwrDeviceUnspecified);
            if(ERROR_SUCCESS != lastErr)
            {
                SetLastError(lastErr);
                ERRFAIL("SetDevicePower(PwrDeviceUnspecified)");
                goto Error;
            }

            fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);
            if(!ValidatePowerChange(fChange, _T("SetDevicePower(PwrDeviceUnspecified)"),
                szDevName, dxActual, dxExpected, dxCurrent))
            {
                goto Error;
            }

        }
    }

Error:
    if(hPwr)
    {
        lastErr = ReleasePowerRequirement(hPwr);
        if(ERROR_SUCCESS != lastErr)
        {
            SetLastError(lastErr);
            ERRFAIL("ReleasePowerRequirement()");
            hPwr = NULL;
        }
    }

    pdx.StopTestDriver();
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// SetDevicePowerActive
//
// Parameters:
//  DeviceDx  Bitmask indicating the device states to be supported by the 
//            test driver
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI SetDevicePowerActive(UCHAR DeviceDx)
{
    BOOL fChange;
    TCHAR szDevName[MAX_DEVICE_NAME] = _T("");
    DWORD lastErr = ERROR_SUCCESS;
    HANDLE hPwr = NULL;
    CEDEVICE_POWER_STATE dxSet, dxActual, dxRequest;
    CEDEVICE_POWER_STATE dxExpected, dxCurrent;
    CEDEVICE_POWER_STATE dxFloor, dxFloorActual;
    CEDEVICE_POWER_STATE dxCeiling, dxCeilingActual;

    // device test object
    CPDX pdx;

    if(!pdx.StartTestDriver(DeviceDx))
    {
        FAIL("StartTestDriver()");
        goto Error;
    }

    LOG(_T("Created device named \"%s\" with DeviceDx=0x%02X"),
        pdx.GetDeviceName(szDevName, MAX_DEVICE_NAME), DeviceDx);

    // default floor values for the device
    dxFloor = D4;
    dxFloorActual = RemapDx(dxFloor, DeviceDx);

    for(dxCeiling = D0; dxCeiling <= D4;
        dxCeiling = (CEDEVICE_POWER_STATE)((UINT)dxCeiling + 1))
    {
        dxCurrent = pdx.GetCurrentPowerState();

        // set the ceiling value

        // what level do we expect the ceiling to go to?
        dxCeilingActual = RemapDx(dxCeiling, DeviceDx);
        LOG(_T("Expecting ceiling for \"%s\" to be D%u"), szDevName, dxCeilingActual);

        if(!pdx.SetDeviceCeiling(dxCeiling))
        {
            SetLastError(lastErr);
            ERRFAIL("SetDeviceCeiling()");
            goto Error;
        }

        fChange = pdx.WaitForPowerChange(g_tNotifyWait, NULL);

        // iterate through all possible floor values for the device
        for(dxFloor = D4; dxFloor >= D0;
            dxFloor = (CEDEVICE_POWER_STATE)((UINT)dxFloor - 1))
        {
            // release previous power requirements
            if(hPwr)
            {
                lastErr = ReleasePowerRequirement(hPwr);
                if(ERROR_SUCCESS != lastErr)
                {
                    SetLastError(lastErr);
                    ERRFAIL("ReleasePowerRequirement()");
                    goto Error;
                }
                
                fChange = pdx.WaitForPowerChange(g_tNotifyWait, NULL);
            }
            dxCurrent = pdx.GetCurrentPowerState();

            LOG(_T("Calling SetPowerRequirement(%s, D%u, POWER_NAME, NULL, 0)"), szDevName, dxFloor);

            dxFloorActual = RemapDx(dxFloor, DeviceDx);
            LOG(_T("Expecting floor for \"%s\" to be D%u"), szDevName, dxFloorActual);

            hPwr = SetPowerRequirement(szDevName, dxFloor, POWER_NAME, NULL, 0);
            if(NULL == hPwr)
            {
                ERRFAIL("SetPowerRequirement()");
                goto Error;
            }

            fChange = pdx.WaitForPowerChange(g_tNotifyWait, NULL);


            for(dxRequest = D0; dxRequest <= D4;
                dxRequest = (CEDEVICE_POWER_STATE)((UINT)dxRequest + 1))
            {
                if(!(DeviceDx & DX_MASK(dxRequest)))
                {
                    LOG(_T("\"%s\" does not support power state D%u, skipping request"),
                        szDevName, dxRequest);
                    continue;
                }

                // request the device state
                if(!pdx.SetDesiredPowerState(dxRequest))
                {
                    FAIL("SetDesiredPowerState()");
                    goto Error;
                }

                fChange = pdx.WaitForPowerChange(g_tNotifyWait, NULL);

                // iterate through all possible power state values to request
                for(dxSet = D4; dxSet >= D0;
                    dxSet = (CEDEVICE_POWER_STATE)((UINT)dxSet - 1))
                {
                    dxCurrent = pdx.GetCurrentPowerState();

                    LOG(_T("Calling SetDevicePower(%s, POWER_NAME, D%u)"), szDevName, dxSet);

                    if(DeviceDx & DX_MASK(dxSet))
                    {
                        LOG(_T("\"%s\" supports power state D%u"), szDevName, dxSet);
                        dxExpected = dxSet;
                    }
                    else
                    {
                        LOG(_T("\"%s\" does not support state D%u"), szDevName, dxSet);
                        dxExpected = RemapDx(dxSet, DeviceDx);
                    }

                    LOG(_T("\"%s\" expecting power state D%u to map to D%u"), szDevName, dxSet,
                        dxExpected);

                    lastErr = SetDevicePower(szDevName, POWER_NAME, dxSet);
                    if(ERROR_SUCCESS != lastErr)
                    {
                        SetLastError(lastErr);
                        ERRFAIL("SetDevicePower()");
                        goto Error;
                    }

                    fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);
                    if(!ValidatePowerChange(fChange, _T("SetDevicePower()"), szDevName,
                        dxActual, dxExpected, dxCurrent))
                    {
                        goto Error;
                    }
                }

                // after requesting all legal power states, request PwrDeviceUnspecified
                // so that the device will transition back to the proper power state
                // given our imposed ceiling and floor values
                LOG(_T("Calling SetDevicePower(%s, POWER_NAME, PwrDeviceUnspecified)"),
                    szDevName);

                if(dxFloorActual == dxCeilingActual)
                {
                    dxExpected = dxCeilingActual;
                    LOG(_T("\"%s\" floor and ceiling are both D%u, expecting device to ")
                        _T("be in state D%u"), szDevName, dxFloorActual, dxExpected);
                }
                else if(dxFloorActual < dxCeilingActual)
                {
                    dxExpected = dxFloorActual;
                    LOG(_T("\"%s\" floor is above ceiling, expecting device to ")
                        _T("be at floor D%u"), szDevName, dxFloorActual, dxExpected);
                }
                else if(dxRequest >= dxFloorActual)
                {
                    dxExpected = dxFloorActual;
                    LOG(_T("\"%s\" request is below floor, expecting device to ")
                        _T("go to floor D%u"), szDevName, dxExpected);
                }
                else if(dxRequest <= dxCeilingActual)
                {
                    dxExpected = dxCeilingActual;
                    LOG(_T("\"%s\" request is above ceiling, expecting device to ")
                        _T("go to ceiling D%u"), szDevName, dxExpected);
                }
                else
                {
                    dxExpected = dxRequest;
                    LOG(_T("\"%s\" request is between ceiling and floor, expecting device to ")
                        _T("go to requested state D%u"), szDevName, dxExpected);
                }

                dxCurrent = pdx.GetCurrentPowerState();

                lastErr = SetDevicePower(szDevName, POWER_NAME, PwrDeviceUnspecified);
                if(ERROR_SUCCESS != lastErr)
                {
                    SetLastError(lastErr);
                    ERRFAIL("SetDevicePower(PwrDeviceUnspecified)");
                    goto Error;
                }

                fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);
                if(!ValidatePowerChange(fChange, _T("SetDevicePower(PwrDeviceUnspecified)"),
                    szDevName, dxActual, dxExpected, dxCurrent))
                {
                    goto Error;
                }
            }
        }
    }

Error:
    if(hPwr)
    {
        lastErr = ReleasePowerRequirement(hPwr);
        if(ERROR_SUCCESS != lastErr)
        {
            SetLastError(lastErr);
            ERRFAIL("ReleasePowerRequirement()");
        }
        hPwr = NULL;
    }

    pdx.StopTestDriver();
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// DevicePowerNotifyTest
//
// Parameters:
//  DeviceDx  Bitmask indicating the device states to be supported by the 
//            test driver
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI DevicePowerNotifyTest(UCHAR DeviceDx)
{
    BOOL fChange;
    TCHAR szDevName[MAX_DEVICE_NAME] = _T("");
    DWORD lastErr = ERROR_SUCCESS;
    HANDLE hPwr = NULL;
    CEDEVICE_POWER_STATE dxFloor, dxFloorActual;
    CEDEVICE_POWER_STATE dxCeiling, dxCeilingActual;
    CEDEVICE_POWER_STATE dxRequest, dxRequestActual;
    CEDEVICE_POWER_STATE dxExpected, dxCurrent;
    CEDEVICE_POWER_STATE dxActual;

    // device test object
    CPDX pdx;

    if(!pdx.StartTestDriver(DeviceDx))
    {
        FAIL("StartTestDriver()");
        goto Error;
    }

    LOG(_T("Created device named \"%s\" with DeviceDx=0x%02X"),
        pdx.GetDeviceName(szDevName, MAX_DEVICE_NAME), DeviceDx);


    // iterate through every possible ceiling and floor combination
    for(dxCeiling = D0; dxCeiling < PwrDeviceMaximum;
        dxCeiling = (CEDEVICE_POWER_STATE)((UINT)dxCeiling + 1))
    {
        // set the ceiling
        if(!pdx.SetDeviceCeiling(dxCeiling))
        {
            SetLastError(lastErr);
            ERRFAIL("SetDeviceCeiling()");
            goto Error;
        }

        // This ceiling change might notify the driver, so wait just in case.
        // We don't care if this succeeds or fails because we're not testing the
        // ceiling setting behavior in this test. The state is validated in the
        // inner loop so any misbehavior will eventually be caught.
        fChange = pdx.WaitForPowerChange(g_tNotifyWait, NULL);

        // the device might not physically support this state as a ceiling, so remap it
        // so we know what should be supported
        dxCeilingActual = RemapDx(dxCeiling, DeviceDx);
        for(dxFloor = D4; dxFloor >= D0;
            dxFloor = (CEDEVICE_POWER_STATE)((UINT)dxFloor - 1))
        {
            // release previous power requirements
            if(hPwr)
            {
                lastErr = ReleasePowerRequirement(hPwr);
                if(ERROR_SUCCESS != lastErr)
                {
                    SetLastError(lastErr);
                    ERRFAIL("ReleasePowerRequirement()");
                    goto Error;
                }
                fChange = pdx.WaitForPowerChange(g_tNotifyWait, NULL);
            }
            dxCurrent = pdx.GetCurrentPowerState();

            // the device might not physically support this state as a floor, so remap it
            // so we know what should be supported
            dxFloorActual = RemapDx(dxFloor, DeviceDx);
            LOG(_T("Configuring \"%s\" with dxCeiling=D%u, dxFloor=D%u (actual ceiling=D%u, ")
                _T("actual floor=D%u)"), szDevName, dxCeiling, dxFloor, dxCeilingActual, dxFloorActual);

            // set the floor
            hPwr = SetPowerRequirement(szDevName, dxFloor, POWER_NAME, NULL, 0);
            if(NULL == hPwr)
            {
                ERRFAIL("SetPowerRequirement()");
                goto Error;
            }

            // This floor change might notify the driver, so wait just in case.
            // We don't care if this succeeds or fails because we're not testing the
            // floor setting behavior in this test. The state is validated in the
            // inner loop so any misbehavior will eventually be caught.
            fChange = pdx.WaitForPowerChange(g_tNotifyWait, NULL);

            // now iterate through each possible request the device can make and verify
            // the results
            for(dxRequest = D0; dxRequest < PwrDeviceMaximum;
                dxRequest = (CEDEVICE_POWER_STATE)((UINT)dxRequest + 1))
            {
                // the device might not physically support this state, so remap it
                dxRequestActual = RemapDx(dxRequest, DeviceDx);

                // save the current power state before making request
                dxCurrent = pdx.GetCurrentPowerState();

                // some validation of the current power state, as we did not validate
                // state in the outer loops
                if(dxCurrent > dxFloorActual)
                {
                    LOG(_T("\"%s\" current power state, D%u, is below the floor D%u"),
                        szDevName, dxCurrent, dxFloorActual);
                    FAIL("Fatal Error; power state below the floor");
                    goto Error;
                }

                if(dxCurrent < dxCeilingActual && dxCurrent != dxFloorActual)
                {
                    // current power state is above the ceiling and not equal
                    // to the floor (in case the floor is above the ceiling
                    LOG(_T("\"%s\" current power state, D%u, is abofe the ceiling D%u"),
                        szDevName, dxCurrent, dxCeilingActual);
                    FAIL("Fatal Error; power state above the ceiling");
                    goto Error;
                }

                LOG(_T("Calling DevicePowerNotify(%s, D%u, POWER_NAME)"), szDevName, dxRequest);

                // determine our expected state value
                if(dxFloorActual == dxCeilingActual)
                {
                    dxExpected = dxCeilingActual;
                    LOG(_T("\"%s\" floor and ceiling are both D%u, expecting device to ")
                        _T("be in state D%u"), szDevName, dxFloorActual, dxExpected);
                }
                else if(dxFloorActual < dxCeilingActual)
                {
                    dxExpected = dxFloorActual;
                    LOG(_T("\"%s\" floor is above ceiling at D%u, expecting device to ")
                        _T("be in state D%u"), szDevName, dxFloorActual, dxExpected);
                }
                else if(dxRequestActual >= dxFloorActual)
                {
                    dxExpected = dxFloorActual;
                    LOG(_T("\"%s\" request is below floor, expecting device to ")
                        _T("go to floor D%u"), szDevName, dxExpected);
                }
                else if(dxRequestActual <= dxCeilingActual)
                {
                    dxExpected = dxCeilingActual;
                    LOG(_T("\"%s\" request is above ceiling, expecting device to ")
                        _T("go to ceiling D%u"), szDevName, dxExpected);
                }
                else
                {
                    dxExpected = dxRequestActual;
                    LOG(_T("\"%s\" request is between ceiling and floor, expecting device to ")
                        _T("go to requested state D%u"), szDevName, dxExpected);
                }

                if(!pdx.SetDesiredPowerState(dxRequest))
                {
                    FAIL("SetDesiredPowerState()");
                    goto Error;
                }

                fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);
                if(!ValidatePowerChange(fChange, _T("DevicePowerNotify()"), szDevName,
                    dxActual, dxExpected, dxCurrent))
                {
                    goto Error;
                }
            }
        }
    }

Error:
    if(hPwr)
    {
        lastErr = ReleasePowerRequirement(hPwr);
        if(ERROR_SUCCESS != lastErr)
        {
            SetLastError(lastErr);
            ERRFAIL("ReleasePowerRequirement()");
        }
        hPwr = NULL;
    }

    pdx.StopTestDriver();
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// RaiseCeilingPassive
//
// Parameters:
//  DeviceDx  Bitmask indicating the device states to be supported by the 
//            test driver
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI RaiseCeilingPassive(UCHAR DeviceDx)
{
    BOOL fChange;
    TCHAR szDevName[MAX_DEVICE_NAME] = _T("");
    DWORD lastErr = ERROR_SUCCESS;
    CEDEVICE_POWER_STATE dxCurrent, dxActual;
    CEDEVICE_POWER_STATE dxCeiling, dxExpected;

    // device test object
    CPDX pdx;

    if(!pdx.StartTestDriver(DeviceDx))
    {
        FAIL("StartTestDriver()");
        goto Error;
    }

    LOG(_T("Created device named \"%s\" with DeviceDx=0x%02X"),
        pdx.GetDeviceName(szDevName, MAX_DEVICE_NAME), DeviceDx);

    // iterate through states D4-D0
    for(dxCeiling = D4; dxCeiling >= D0;
        dxCeiling = (CEDEVICE_POWER_STATE)((UINT)dxCeiling - 1))
    {
        dxCurrent = pdx.GetCurrentPowerState();

        // set the ceiling value

        // what level do we expect the ceiling to go to?
        dxExpected = RemapDx(dxCeiling, DeviceDx);
        LOG(_T("Expecting ceiling for \"%s\" to be D%u"), szDevName, dxExpected);

        if(!pdx.SetDeviceCeiling(dxCeiling))
        {
            SetLastError(lastErr);
            ERRFAIL("SetDeviceCeiling()");
            goto Error;
        }

        // verify the device is notified accordingly
        fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);
        if(!ValidatePowerChangeCeiling(fChange, _T("SetDeviceCeiling()"), szDevName,
            dxActual, dxExpected, dxCurrent))
        {
            goto Error;
        }
    }

Error:
    pdx.StopTestDriver();
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// RaiseCeilingActive
//
// Parameters:
//  DeviceDx  Bitmask indicating the device states to be supported by the 
//            test driver
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI RaiseCeilingActive(UCHAR DeviceDx)
{
    BOOL fChange;
    TCHAR szDevName[MAX_DEVICE_NAME] = _T("");
    DWORD lastErr = ERROR_SUCCESS;
    CEDEVICE_POWER_STATE dxRequest, dxActual;
    CEDEVICE_POWER_STATE dxCurrent, dxCeilingActual;
    CEDEVICE_POWER_STATE dxCeiling, dxExpected;

    // device test object
    CPDX pdx;

    if(!pdx.StartTestDriver(DeviceDx))
    {
        FAIL("StartTestDriver()");
        goto Error;
    }

    LOG(_T("Created device named \"%s\" with DeviceDx=0x%02X"),
        pdx.GetDeviceName(szDevName, MAX_DEVICE_NAME), DeviceDx);

    dxCeiling = D0;

    // driver will request all states that it supports
    for(dxRequest = D4; dxRequest >= D0;
        dxRequest = (CEDEVICE_POWER_STATE)((UINT)dxRequest - 1))
    {
        // if the driver does not support this state, it should not request to
        // transition to it
        if(!(DeviceDx & DX_MASK(dxRequest)))
        {
            LOG(_T("\"%s\" does not support power state D%u... skipping request"),
                szDevName, dxRequest);
            continue;
        }

        dxCurrent = pdx.GetCurrentPowerState();

        // request the supported state
        LOG(_T("Calling DevicePowerNotify(%s, D%u, POWER_NAME)"), szDevName, dxRequest);
        if(!pdx.SetDesiredPowerState(dxRequest))
        {
            FAIL("SetDesiredPowerState()");
            goto Error;
        }

        // determine the result we expect to transition to
        if(dxRequest < dxCeiling)
        {
            // expect it to transition to the ceiling
            dxExpected = RemapDx(dxCeiling, DeviceDx);
            LOG(_T("\"%s\" requesting D%u (above the current ceiling) expecting D%u"),
                szDevName, dxRequest, dxExpected);
        }
        else
        {
            // expect it to transition to the requested value
            dxExpected = dxRequest;
            LOG(_T("\"%s\" requesting D%u (below the current ceiling) expecting D%u"),
                szDevName, dxRequest, dxExpected);
        }

        fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);
        if(!ValidatePowerChange(fChange, _T("DevicePowerNotify()"), szDevName,
            dxActual, dxExpected, dxCurrent))
        {
            goto Error;
        }

        // iterate through states D4-D0
        for(dxCeiling = D4; dxCeiling >= D0;
            dxCeiling = (CEDEVICE_POWER_STATE)((UINT)dxCeiling - 1))
        {
            dxCurrent = pdx.GetCurrentPowerState();
            dxCeilingActual = RemapDx(dxCeiling, DeviceDx);

            // set the ceiling value
            LOG(_T("\"%s\" expecting actual ceiling to be D%u"), szDevName, dxCeilingActual);
            if(dxRequest > dxCeilingActual)
            {
                if(dxRequest != dxCurrent)
                {
                    // this situation should never happen...
                    FAIL("Fatal error; requested state is within allowed range");
                    goto Error;
                }
                // requested power state is already the current power state
                dxExpected = dxCurrent;
                LOG(_T("\"%s\" requested power state is below the ceiling, expecting device to ")
                    _T("remain at D%u"), szDevName, dxExpected);
            }
            else if(dxRequest == dxCeilingActual)
            {
                dxExpected = dxCeilingActual;
                LOG(_T("\"%s\" requested power state equal to the new ceiling, expecting device ")
                    _T("to go to D%u"), szDevName, dxExpected);
            }
            else
            {
                dxExpected = dxCeilingActual;
                LOG(_T("\"%s\" requested power state is above the new ceiling, expecting device ")
                    _T("to go to the ceiling D%u"), szDevName, dxExpected);
            }

            if(!pdx.SetDeviceCeiling(dxCeiling))
            {
                SetLastError(lastErr);
                ERRFAIL("SetDeviceCeiling()");
                goto Error;
            }

            // verify the device is notified accordingly
            fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);
            if(!ValidatePowerChangeCeiling(fChange, _T("SetDeviceCeiling()"), szDevName,
                dxActual, dxExpected, dxCurrent))
            {
                goto Error;
            }
        }
    }

Error:
    pdx.StopTestDriver();
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// LowerFloorPassive
//
// Parameters:
//  DeviceDx  Bitmask indicating the device states to be supported by the 
//            test driver
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI LowerFloorPassive(UCHAR DeviceDx)
{
    BOOL fChange;
    TCHAR szDevName[MAX_DEVICE_NAME] = _T("");
    HANDLE hPwr = NULL;
    DWORD lastErr = ERROR_SUCCESS;
    CEDEVICE_POWER_STATE dxActual, dxFloor;

    // device test object
    CPDX pdx;

    if(!pdx.StartTestDriver(DeviceDx))
    {
        FAIL("StartTestDriver()");
        goto Error;
    }

    LOG(_T("Created device named \"%s\" with DeviceDx=0x%02X"),
        pdx.GetDeviceName(szDevName, MAX_DEVICE_NAME), DeviceDx);

    // iterate floor through states D0-D4
    for(dxFloor = D0; dxFloor <= D4;
        dxFloor = (CEDEVICE_POWER_STATE)((UINT)dxFloor + 1))
    {
        // release previous power requirements
        if(hPwr)
        {
            lastErr = ReleasePowerRequirement(hPwr);
            if(ERROR_SUCCESS != lastErr)
            {
                SetLastError(lastErr);
                ERRFAIL("ReleasePowerRequirement()");
                goto Error;
            }

            fChange = pdx.WaitForPowerChange(g_tNotifyWait, NULL);
        }

        if(D0 != pdx.GetCurrentPowerState())
        {
            // fatal-- this should never be the case when we just
            // started the driver with no restrictions
            FAIL("Power state is not D0!");
            goto Error;
        }

        // set the floor value
        LOG(_T("Calling SetPowerRequirement(%s, D%u, POWER_NAME) to adjust the floor"),
            szDevName, dxFloor);
        hPwr = SetPowerRequirement(szDevName, dxFloor, POWER_NAME, NULL, 0);
        if(NULL == hPwr)
        {
            ERRFAIL("SetPowerRequirement()");
            goto Error;
        }

        fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);

        if(!ValidatePowerChange(fChange, _T("SetPowerRequirement()"), szDevName,
            dxActual, D0, D0))
        {
            goto Error;
        }
    }

Error:
    if(hPwr)
    {
        lastErr = ReleasePowerRequirement(hPwr);
        if(ERROR_SUCCESS != lastErr)
        {
            SetLastError(lastErr);
            ERRFAIL("ReleasePowerRequirement()");
        }
        hPwr = NULL;
    }

    pdx.StopTestDriver();
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// LowerFloorActive
//
// Parameters:
//  DeviceDx  Bitmask indicating the device states to be supported by the 
//            test driver
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI LowerFloorActive(UCHAR DeviceDx)
{
    BOOL fChange;
    TCHAR szDevName[MAX_DEVICE_NAME] = _T("");
    HANDLE hPwr = NULL;
    DWORD lastErr = ERROR_SUCCESS;
    CEDEVICE_POWER_STATE dxRequest, dxActual;
    CEDEVICE_POWER_STATE dxCurrent, dxFloorActual;
    CEDEVICE_POWER_STATE dxFloor, dxExpected;

    // device test object
    CPDX pdx;

    if(!pdx.StartTestDriver(DeviceDx))
    {
        FAIL("StartTestDriver()");
        goto Error;
    }

    LOG(_T("Created device named \"%s\" with DeviceDx=0x%02X"),
        pdx.GetDeviceName(szDevName, MAX_DEVICE_NAME), DeviceDx);

    dxFloor = D4;
    for(dxRequest = D0; dxRequest <= D4;
        dxRequest = (CEDEVICE_POWER_STATE)((UINT)dxRequest + 1))
    {
        // if the driver does not support this state, it should not request to
        // transition to it
        if(!(DeviceDx & DX_MASK(dxRequest)))
        {
            LOG(_T("\"%s\" does not support power state D%u... skipping request"),
                szDevName, dxRequest);
            continue;
        }

        dxCurrent = pdx.GetCurrentPowerState();

        // request the supported state
        LOG(_T("Calling DevicePowerNotify(%s, D%u, POWER_NAME)"), szDevName, dxRequest);
        if(!pdx.SetDesiredPowerState(dxRequest))
        {
            FAIL("SetDesiredPowerState()");
            goto Error;
        }

        // determine the result we expect to transition to
        if(dxRequest >= dxFloor)
        {
            // expect it to transition to the floor
            dxExpected = RemapDx(dxFloor, DeviceDx);
            LOG(_T("\"%s\" requesting D%u (below the current floor) expecting D%u"),
                szDevName, dxRequest, dxExpected);
        }
        else
        {
            // expect it to transition to the requested value
            dxExpected = dxRequest;
            LOG(_T("\"%s\" requesting D%u (above the current floor) expecting D%u"),
                szDevName, dxRequest, dxExpected);
        }

        fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);
        if(!ValidatePowerChange(fChange, _T("DevicePowerNotify()"), szDevName,
            dxActual, dxExpected, dxCurrent))
        {
            goto Error;
        }

        // iterate floor through states D0-D4
        for(dxFloor = D0; dxFloor <= D4;
            dxFloor = (CEDEVICE_POWER_STATE)((UINT)dxFloor + 1))
        {
            // release previous power requirements
            if(hPwr)
            {
                lastErr = ReleasePowerRequirement(hPwr);
                if(ERROR_SUCCESS != lastErr)
                {
                    SetLastError(lastErr);
                    ERRFAIL("ReleasePowerRequirement()");
                    goto Error;
                }
                fChange = pdx.WaitForPowerChange(g_tNotifyWait, NULL);
            }
            dxCurrent = pdx.GetCurrentPowerState();
            dxFloorActual = RemapDx(dxFloor, DeviceDx);

            // set the floor value
            LOG(_T("Calling SetPowerRequirement(%s, D%u, POWER_NAME) to adjust the floor"),
                szDevName, dxFloor);

            LOG(_T("\"%s\" expecting actual floor to be D%u"), szDevName, dxFloorActual);
            if(dxRequest > dxFloorActual)
            {
                // our request is below the current floor, we expect the floor
                dxExpected = dxFloorActual;
                LOG(_T("\"%s\" requested power state is below the floor, expecting device ")
                    _T("to go to the floor D%u"), szDevName, dxExpected);
            }
            else if(dxRequest == dxFloorActual)
            {
                dxExpected = dxFloorActual;
                LOG(_T("\"%s\" requested power state equal to the new floor, expecting device ")
                    _T("to go to D%u"), szDevName, dxExpected);
            }
            else
            {
                dxExpected = dxRequest;
                LOG(_T("\"%s\" requested power state is above the new floor, expecting device ")
                    _T("to remain at D%u"), szDevName, dxExpected);
            }

            hPwr = SetPowerRequirement(szDevName, dxFloor, POWER_NAME, NULL, 0);
            if(NULL == hPwr)
            {
                ERRFAIL("SetPowerRequirement()");
                goto Error;
            }

            fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);

            if(!ValidatePowerChange(fChange, _T("SetPowerRequirement()"), szDevName,
                dxActual, dxExpected, dxCurrent))
            {
                goto Error;
            }
        }
    }

Error:
    if(hPwr)
    {
        lastErr = ReleasePowerRequirement(hPwr);
        if(ERROR_SUCCESS != lastErr)
        {
            SetLastError(lastErr);
            ERRFAIL("ReleasePowerRequirement()");
        }
        hPwr = NULL;
    }

    pdx.StopTestDriver();
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////
// RequestUnsupportedStates
//
// Parameters:
//  DeviceDx  Bitmask indicating the device states to be supported by the 
//            test driver
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails, or possibly other
//  special conditions.
////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI RequestUnsupportedStates(UCHAR DeviceDx)
{
    BOOL fChange;
    TCHAR szDevName[MAX_DEVICE_NAME] = _T("");
    CEDEVICE_POWER_STATE dxRequest, dxExpected;
    CEDEVICE_POWER_STATE dxCurrent, dxActual;

    // device test object
    CPDX pdx;

    if(!pdx.StartTestDriver(DeviceDx))
    {
        FAIL("StartTestDriver()");
        goto Error;
    }

    LOG(_T("Created device named \"%s\" with DeviceDx=0x%02X"),
        pdx.GetDeviceName(szDevName, MAX_DEVICE_NAME), DeviceDx);

    // iterate through states D4-D0
    for(dxRequest = D4; dxRequest >= D0;
        dxRequest = (CEDEVICE_POWER_STATE)((UINT)dxRequest - 1))
    {
        if(DeviceDx & DX_MASK(dxRequest))
        {
            LOG(_T("\"%s\" supports power state D%u"), szDevName, dxRequest);
            continue;
        }

        dxCurrent = pdx.GetCurrentPowerState();

        LOG(_T("\"%s\" requesting unsupported power state D%u..."), szDevName, dxRequest);

        // determine what state we expect when we request this one
        dxExpected = RemapDx(dxRequest, DeviceDx);
        LOG(_T("\"%s\" expecting request to go to power state D%u"), szDevName, dxExpected);

        if(!pdx.SetDesiredPowerState(dxRequest))
        {
            FAIL("SetDesiredPowerState()");
            goto Error;
        }

        fChange = pdx.WaitForPowerChange(g_tNotifyWait, &dxActual);
        if(!ValidatePowerChange(fChange, _T("DevicePowerNotify()"), szDevName,
            dxActual, dxExpected, dxCurrent))
        {
            goto Error;
        }
    }

Error:
    pdx.StopTestDriver();
    return GetTestResult();
}

////////////////////////////////////////////////////////////////////////////////

BOOL ValidatePowerChange(BOOL fChanged, LPCTSTR pszChange, LPTSTR pszDevName,
    CEDEVICE_POWER_STATE dxActual, CEDEVICE_POWER_STATE dxExpected, CEDEVICE_POWER_STATE dxPrevious)
{
    BOOL fRet = FALSE;
    if(fChanged)
    {
        if(dxPrevious == dxExpected)
        {
            // should NOT have received notification if the previous state was
            // the state we expected to transition to
            LOG(_T("FAIL: After %s, \"%s\" was previously in D%u and should ")
                _T("have received no notification"),
                pszChange, pszDevName, dxPrevious);
            FAIL("Driver unexpectely received power change notification");
            fRet = FALSE;
        }
        else if(dxActual != dxExpected)
        {
            // received notification, but the driver was told to change to the incorrect state
            LOG(_T("FAIL: After %s, \"%s\" was incorrectly set to state D%u"),
                pszChange, pszDevName, dxActual);
            FAIL("Driver was set to an unexpected power state");
            fRet = FALSE;
        }
        else
        {
            // received notification and the driver transitioned to the correct state
            LOG(_T("PASS: After %s, \"%s\" was correctly set to state D%u"),
                pszChange, pszDevName, dxActual);
            fRet = TRUE;
        }
    }
    else
    {
        // failed to receive notification, check to see if the previous state was
        // the same as what we wanted, and succeed if so
        if(dxPrevious == dxExpected)
        {
            // should not have received notification because the driver was already in
            // the correct power state
            LOG(_T("PASS: After %s, \"%s\" received no notification and ")
                _T("correctly remained in state D%u"),
                pszChange, pszDevName, dxExpected);
            fRet = TRUE;
        }
        else
        {
            // should have received notification, but did not
            LOG(_T("FAIL: After %s, \"%s\" received no notification when ")
                _T("it should have gone to state D%u"),
                pszChange, pszDevName, dxExpected);
            FAIL("Driver failed to receive power change notification");
            fRet = FALSE;
        }
    }
    return fRet;
}


// Validate Power Change after Ceiling has been updated.
// SetDeviceCeiling will switch between power states and depending on the ceiling in
// both the states, the device power state may or may not be updated.
// Therefore, we will not check for notification of update after ceiling change.
// Only check if device was updated to the correct power state eventually.
BOOL ValidatePowerChangeCeiling(BOOL fChanged, LPCTSTR pszChange, LPTSTR pszDevName,
    CEDEVICE_POWER_STATE dxActual, CEDEVICE_POWER_STATE dxExpected, CEDEVICE_POWER_STATE dxPrevious)
{
    BOOL fRet = FALSE;

    if(fChanged)
    {
        LOG(_T("The device received notification to update its power state."));
    }

    if(dxActual != dxExpected)
    {
        // the driver was told to change to the incorrect state
        LOG(_T("FAIL: After %s, \"%s\" was incorrectly set to state D%u"),
            pszChange, pszDevName, dxActual);
        FAIL("Driver was set to an unexpected power state");
        fRet = FALSE;
    }
    else
    {
        // the driver transitioned to the correct state
        LOG(_T("PASS: After %s, \"%s\" was correctly set to state D%u"),
            pszChange, pszDevName, dxActual);
        fRet = TRUE;
    }

    return fRet;
}


DWORD RegQueryTypedValue(HKEY hk, LPCWSTR pszValue, PVOID pvData,
                         LPDWORD pdwSize, DWORD dwType)
{
    DWORD dwActualType;

    DWORD dwStatus = RegQueryValueEx(hk, pszValue, NULL, &dwActualType,
        (PBYTE) pvData, pdwSize);
    if(dwStatus == ERROR_SUCCESS && dwActualType != dwType)
    {
        dwStatus = ERROR_INVALID_DATA;
    }

    return dwStatus;
}

BOOL RestoreDefaultTimeouts()
{
    TCHAR szPath[MAX_PATH];
    HKEY hKey;
    StringCchPrintf(szPath, MAX_PATH, _T("%s\\%s"), PWRMGR_REG_KEY, _T("Timeouts"));
    DWORD dwStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szPath, 0, 0, &hKey);

    //restore original settings
    if(dwStatus == ERROR_SUCCESS)
    {
        DWORD dwTimeoutVal = g_sysTimeouts.dwACSuspendTimeout;
        DWORD dwRet = RegSetValueEx(hKey, _T("ACSuspend"), 0, REG_DWORD, (LPBYTE) &dwTimeoutVal, sizeof(DWORD));
        if(dwRet != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_FAIL, _T("Can not restore AC Suspend value!"));
        }

        dwTimeoutVal = g_sysTimeouts.dwACUserIdleTimeout;
        dwRet = RegSetValueEx(hKey, _T("ACUserIdle"), 0, REG_DWORD, (LPBYTE) &dwTimeoutVal, sizeof(DWORD));
        if(dwRet != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_FAIL, _T("Can not restore AC UserIdle value!"));
        }

        dwTimeoutVal = g_sysTimeouts.dwACSysIdleTimeout;
        dwStatus = RegSetValueEx(hKey, _T("ACSystemIdle"), 0, REG_DWORD, (LPBYTE) &dwTimeoutVal, sizeof(DWORD));
        if(dwStatus != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_FAIL, _T("Can not restore ACSystemIdle value!"));
        }
    }

    RegCloseKey(hKey);

    SimulateReloadTimeoutsEvent();
    return TRUE;
}

BOOL SetDefaultTimeouts()
{
    memset(&g_sysTimeouts, 0, sizeof(SYS_TIMEOUTS));
    TCHAR szPath[MAX_PATH];
    HKEY hKey;
    StringCchPrintf(szPath, MAX_PATH, _T("%s\\%s"), PWRMGR_REG_KEY, _T("Timeouts"));
    DWORD dwStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szPath, 0, 0, &hKey);

    //first, retrieve and save original settings
    if(dwStatus == ERROR_SUCCESS)
    {
        DWORD   dwValue = 0;
        DWORD   cbSize = sizeof(DWORD);
    
        //get ACSuspendTimeout
        DWORD dwRet = RegQueryTypedValue(hKey, _T("ACSuspend"), &dwValue, &cbSize, REG_DWORD);
        if(dwRet != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_FAIL, _T("Can not get AC Suspend value!"));
            return FALSE;
        }
        else
        {
            g_sysTimeouts.dwACSuspendTimeout = dwValue;
        }

        //get ACUserIdleTimeout
        dwRet = RegQueryTypedValue(hKey, _T("ACUserIdle"), &dwValue, &cbSize, REG_DWORD);
        if(dwRet != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_FAIL, _T("Can not get ACUserIdle value!"));
            return FALSE;
        }
        else
        {
            g_sysTimeouts.dwACUserIdleTimeout= dwValue;
        }

        //get SystemTimeout
        dwRet = RegQueryTypedValue(hKey, _T("ACSystemIdle"), &dwValue, &cbSize, REG_DWORD);
        if(dwRet != ERROR_SUCCESS)
        {
            g_pKato->Log(LOG_FAIL, _T("Can not get ACSystemIdle value!"));
            return FALSE;
        }
        else
        {
            g_sysTimeouts.dwACSysIdleTimeout = dwValue;
        }
    }
    else
    {
        g_pKato->Log(LOG_FAIL, _T("Can not open key %s!"), szPath);
        return FALSE;
    }

    //change these time outs to test values
    BOOL fRet = FALSE;
    DWORD dwTimeoutVal = TST_SYS_ACSUSPENDTIMOUT;
    dwStatus = RegSetValueEx(hKey, _T("ACSuspend"), 0, REG_DWORD, (LPBYTE) &dwTimeoutVal, sizeof(DWORD));
    if(dwStatus != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_FAIL, _T("Can not set AC Suspend value!"));
        goto EXIT;
    }

    dwTimeoutVal = TST_SYS_ACUSERIDLETIMEOUT;
    dwStatus = RegSetValueEx(hKey, _T("ACUserIdle"), 0, REG_DWORD, (LPBYTE) &dwTimeoutVal, sizeof(DWORD));
    if(dwStatus != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_FAIL, _T("Can not set AC UserIdle value!"));
        goto EXIT;
    }

    dwTimeoutVal = TST_SYS_ACSYSIDLETIMEOUT;
    dwStatus = RegSetValueEx(hKey, _T("ACSystemIdle"), 0, REG_DWORD, (LPBYTE) &dwTimeoutVal, sizeof(DWORD));
    if(dwStatus != ERROR_SUCCESS)
    {
        g_pKato->Log(LOG_FAIL, _T("Can not set ACSystemIdle value!"));
        goto EXIT;
    }

    fRet = TRUE;

EXIT:

    RegCloseKey(hKey);
    if(fRet == FALSE)
    {
        RestoreDefaultTimeouts();
        return fRet;
    }

    //now notify system to reload time outs values
    SimulateReloadTimeoutsEvent();
    Sleep(300);

    //issue a user event, make system to "on" state
    SimulateUserEvent();
    Sleep(300);
    return TRUE;
}

BOOL RestorePdaTimeouts()
{
    BOOL bRet = FALSE;
    HKEY hKey = NULL;
    DWORD dwDisposition;
    DWORD dwLen = sizeof(DWORD);

    // Set Display timeout
    hKey = NULL;
    DWORD dwTimeoutVal = g_dwUserIdleTimeout;

    if(ERROR_SUCCESS != RegCreateKeyEx(HKEY_CURRENT_USER, _T("ControlPanel\\Power"), 0, NULL,
                                        REG_OPTION_NON_VOLATILE, 0, NULL, &hKey, &dwDisposition))
    {
        goto EXIT;
    }

    if(ERROR_SUCCESS != RegSetValueEx(hKey, _T("Display"), 0, REG_DWORD,(LPBYTE)&dwTimeoutVal, dwLen))
    {
        goto EXIT;
    }

    RegCloseKey(hKey);

    // Set Backlight timeout
    hKey = NULL;
    dwTimeoutVal = g_dwBacklightTimeout;

    if(ERROR_SUCCESS != RegCreateKeyEx(HKEY_CURRENT_USER, _T("ControlPanel\\Backlight"), 0, NULL,
                                        REG_OPTION_NON_VOLATILE, 0, NULL, &hKey, &dwDisposition))
    {
        goto EXIT;
    }

    if(ERROR_SUCCESS != RegSetValueEx(hKey, _T("ACTimeout"), 0, REG_DWORD,(LPBYTE)&dwTimeoutVal, dwLen))
    {
        goto EXIT;
    }

    RegCloseKey(hKey);


    SimulateReloadTimeoutsEvent();

    // notify system of new timeouts
    PREFAST_SUPPRESS(25084, "suppress EVENT_ALL_ACCESS warning");
    HANDLE hBLEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("BackLightChangeEvent"));
    if (hBLEvent)
    {
        SetEvent(hBLEvent);
        CloseHandle(hBLEvent);
    }

    PREFAST_SUPPRESS(25084, "suppress EVENT_ALL_ACCESS warning");
    HANDLE hPMEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("PowerManager/ReloadActivityTimeouts"));
    if (hPMEvent)
    {
        SetEvent(hPMEvent);
        CloseHandle(hPMEvent);
    }

    NotifyWinUserSystem(NWUS_MAX_IDLE_TIME_CHANGED);

    bRet = TRUE;

EXIT:

    if (hKey)
    {
        RegCloseKey(hKey);
    }

    NKDbgPrintfW(_T("BacklightTimeout=%d, UserTimeout=%d"), g_dwBacklightTimeout, g_dwUserIdleTimeout);

    return bRet;
}

BOOL SetPdaTimeouts()
{
    BOOL bRet = FALSE;
    HKEY hKey = NULL;
    DWORD dwDisposition;
    DWORD dwLen = sizeof(DWORD);

    // first, retrieve and save original settings

    // Display\UserIdle timeout
    DWORD dwStatus = RegOpenKeyEx(HKEY_CURRENT_USER, _T("ControlPanel\\Power"), 0, 0, &hKey);
    if(dwStatus != ERROR_SUCCESS)
    {
        FAIL("SetPdaTimeouts() Failed.");
        goto EXIT;
    }

    DWORD   dwValue = 0;
    DWORD   cbSize = sizeof(DWORD);
    DWORD dwRet = RegQueryTypedValue(hKey, _T("Display"), &dwValue, &cbSize, REG_DWORD);
    if(dwRet != ERROR_SUCCESS)
    {
        FAIL("SetPdaTimeouts() Failed ");
        goto EXIT;
    }
    g_dwUserIdleTimeout = dwValue;
    RegCloseKey(hKey);

    // Backlight timeout
    dwStatus = RegOpenKeyEx(HKEY_CURRENT_USER, _T("ControlPanel\\Backlight"), 0, 0, &hKey);
    if(dwStatus != ERROR_SUCCESS)
    {
        FAIL("SetPdaTimeouts() Failed ");
        goto EXIT;
    }
    dwValue = 0;
    cbSize = sizeof(DWORD);
    dwRet = RegQueryTypedValue(hKey, _T("ACTimeout"), &dwValue, &cbSize, REG_DWORD);
    if(dwRet != ERROR_SUCCESS)
    {
        FAIL("SetPdaTimeouts() Failed ");
        goto EXIT;
    }
    g_dwBacklightTimeout = dwValue;
    RegCloseKey(hKey);


    //change these time outs to test values
    // Set Display timeout
    hKey = NULL;
    DWORD dwTimeoutVal = TST_PDA_BACKLIGHTTIMEOUT;

    if(ERROR_SUCCESS != RegCreateKeyEx(HKEY_CURRENT_USER, _T("ControlPanel\\Power"), 0, NULL,
        REG_OPTION_NON_VOLATILE, 0, NULL, &hKey, &dwDisposition))
        goto EXIT;

    if(ERROR_SUCCESS != RegSetValueEx(hKey, _T("Display"), 0, REG_DWORD,(LPBYTE)&dwTimeoutVal, dwLen))
        goto EXIT;

    RegCloseKey(hKey);

    // Set Backlight timeout
    hKey = NULL;
    dwTimeoutVal = TST_PDA_BACKLIGHTTIMEOUT;

    if(ERROR_SUCCESS != RegCreateKeyEx(HKEY_CURRENT_USER, _T("ControlPanel\\Backlight"), 0, NULL,
        REG_OPTION_NON_VOLATILE, 0, NULL, &hKey, &dwDisposition))
        goto EXIT;

    if(ERROR_SUCCESS != RegSetValueEx(hKey, _T("ACTimeout"), 0, REG_DWORD,(LPBYTE)&dwTimeoutVal, dwLen))
        goto EXIT;

    RegCloseKey(hKey);


    SimulateReloadTimeoutsEvent();

    // notify system of new timeouts
    PREFAST_SUPPRESS(25084, "suppress EVENT_ALL_ACCESS warning");
    HANDLE hBLEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("BackLightChangeEvent"));
    if (hBLEvent)
    {
        SetEvent(hBLEvent);
        CloseHandle(hBLEvent);
    }

    PREFAST_SUPPRESS(25084, "suppress EVENT_ALL_ACCESS warning");
    HANDLE hPMEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("PowerManager/ReloadActivityTimeouts"));
    if (hPMEvent)
    {
        SetEvent(hPMEvent);
        CloseHandle(hPMEvent);
    }

    NotifyWinUserSystem(NWUS_MAX_IDLE_TIME_CHANGED);

    bRet = TRUE;

EXIT:

    if (hKey)
    {
        RegCloseKey(hKey);
    }

    if(bRet == FALSE)
    {
        RestorePdaTimeouts();
    }

    return bRet;
}

BOOL SetTimeOuts()
{
    if(g_bPdaVersion == TRUE)
    {
        // use PDA version timeouts
        if(!SetPdaTimeouts())
        {
            return FALSE;
        }
    }
    else
    {
        // use DEFAULT power model timeouts
        if(!SetDefaultTimeouts())
        {
            return FALSE;
        }
    }

    return TRUE;
}

BOOL RestoreTimeOuts()
{
    if(g_bPdaVersion == TRUE)
    {
        // use PDA version timeouts
        if(!RestorePdaTimeouts())
        {
            return FALSE;
        }
    }
    else
    {
        // use DEFAULT power model timeouts
        if(!RestoreDefaultTimeouts())
        {
            return FALSE;
        }
    }

    return TRUE;
}

