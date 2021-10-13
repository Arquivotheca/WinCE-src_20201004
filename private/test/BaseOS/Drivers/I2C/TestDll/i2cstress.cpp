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
// Tst_StressI2c
// Stress the I2C by performing read/write operations in several iterations for a given duration
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails 
///////////////////////////////////////////////////////////////////////////////////

TESTPROCAPI Tst_StressI2c(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    DWORD dwRet=TPR_PASS;
    DWORD dwTimeElapsed = 0;
    DWORD dwStartTime = 0;
    DWORD dwIteration = 1;
    BYTE byDataBuffer[I2C_MAX_LENGTH] = {};
    
    if(TestInit() && !gdwInitFailed)
    {
        g_pKato->Log(LOG_DETAIL, TEXT("Test Will Run for: %d mins"),g_dwStressTime/MINS_ADJUST);
        dwStartTime = GetTickCount();
        
        do 
        {
            g_pKato->Log(LOG_DETAIL, TEXT("*******************"));
            g_pKato->Log(LOG_DETAIL, TEXT("BEGIN ITERATION: %d"),dwIteration);
            g_pKato->Log(LOG_DETAIL, TEXT("*******************"));

            for(INT index=0; index < gI2CTransSize; index++)
            {
                g_pKato->Log(LOG_DETAIL, L"*****************************************");
                g_pKato->Log(LOG_DETAIL, L"Performing transaction: TRANS ID %d", gI2CTrans[index].dwTransactionID);
                g_pKato->Log(LOG_DETAIL, L"*****************************************");

                if(SUCCESS!=Hlp_SetupBusData(gI2CTrans, index))
                {
                        dwRet=TPR_FAIL;
                        continue;
                }
                
                g_pKato->Log(LOG_DETAIL, L"Setting the Device Data ...");
                PrintI2CData(gI2CTrans, index);
                if(SUCCESS!=Hlp_SetupDeviceData(gI2CTrans, index))
                {
                        dwRet=TPR_FAIL;
                        continue;
                }
                
                if(I2C_OP_WRITE==gI2CTrans[index].Operation)
                {
                    g_pKato->Log(LOG_DETAIL, L"Performing WRITE Operation ...");
                    if(SUCCESS!=Hlp_I2CWrite( gI2CTrans, index) )
                    {
                        dwRet=TPR_FAIL;
                        g_pKato->Log(LOG_FAIL,L"FAIL: WRITE transaction failed");
                        continue;
                    }
                    g_pKato->Log(LOG_DETAIL, L"Write at RegisterAddress=%d", gI2CTrans[index].dwRegisterAddress );
                }                    

                else if(I2C_OP_READ==gI2CTrans[index].Operation)
                {
                    g_pKato->Log(LOG_DETAIL, L"Performing READ Operation ...");
                    if(SUCCESS!=Hlp_I2CRead( gI2CTrans, index) )
                    {
                        dwRet=TPR_FAIL;
                        g_pKato->Log(LOG_FAIL,L"FAIL: READ transaction failed");
                        continue;
                    }
                    g_pKato->Log(LOG_DETAIL, L"Read at RegisterAddress=%d", gI2CTrans[index].dwRegisterAddress );

                }
                else if(I2C_OP_LOOPBACK==gI2CTrans[index].Operation)
                {
                    if(SUCCESS!=Hlp_I2CLoopback( gI2CTrans, index) )
                    {
                        dwRet=TPR_FAIL;
                        g_pKato->Log(LOG_FAIL,L"FAIL: LOOPBACK transaction failed");
                        continue;
                    }
                    g_pKato->Log(LOG_DETAIL, L"Read at RegisterAddress=%d", gI2CTrans[index].dwRegisterAddress );
                }
                    
            }//for index
                
            g_pKato->Log(LOG_DETAIL, TEXT("*******************"));
            g_pKato->Log(LOG_DETAIL, TEXT("END ITERATION: %d"), dwIteration);
            g_pKato->Log(LOG_DETAIL, TEXT("*******************"));

            dwTimeElapsed = GetTickCount() - dwStartTime;
            g_pKato->Log(LOG_DETAIL, TEXT("Time Elapsed: %d mins, %d secs"),dwTimeElapsed / MINS_ADJUST, (dwTimeElapsed/1000) % 60);

            // Increment the iteration count
            dwIteration ++;

        } while((GetTickCount() - dwStartTime) <(UINT32)g_dwStressTime);
        
        for(INT index=0; index<gI2CTransSize; index++)
        {
            if(SUCCESS!=Hlp_CloseI2CHandle(gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
            }
        }
    } //if TestInit
    else
    {
        dwRet=TPR_SKIP;
    }

    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
