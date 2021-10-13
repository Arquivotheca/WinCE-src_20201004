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
// Tst_WriteI2C
// Performs all the write transactions specified in config file
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails 
///////////////////////////////////////////////////////////////////////////////////

TESTPROCAPI Tst_WriteI2C(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    DWORD dwRet=TPR_PASS;
    BOOL bFound=FALSE;
    
    if(TestInit() && !gdwInitFailed)
    {
        for(INT index=0;index<gI2CTransSize;index++)
        {                    
            if(I2C_OP_WRITE==gI2CTrans[index].Operation)
            {
                bFound=TRUE;
                g_pKato->Log(LOG_DETAIL, L"*****************************************");
                g_pKato->Log(LOG_DETAIL, L"Performing WRITE transaction: TRANS ID %d", gI2CTrans[index].dwTransactionID);
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

                g_pKato->Log(LOG_DETAIL, L"Performing WRITE operation ...");                
                if(SUCCESS!=Hlp_I2CWrite( gI2CTrans, index) )
                {
                    dwRet=TPR_FAIL;
                    g_pKato->Log(LOG_FAIL,L"FAIL: WRITE transaction failed");
                    continue;
                }
                
            }
              

        }//for index
        
        for(index=0;index<gI2CTransSize;index++)
        {
            if(SUCCESS!=Hlp_CloseI2CHandle(gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
            }
        }
        
        if(!bFound)
        {
            g_pKato->Log(LOG_DETAIL,L"Could not find any Write Transaction. Test Skipped");
            dwRet=TPR_SKIP;
        }
        
    } //if TestInit
    else
    {
        dwRet=TPR_SKIP;
    }
    
    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
// Tst_LoopbackI2C
// Performs all the loopback transactions specified in config file
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails 
///////////////////////////////////////////////////////////////////////////////////

TESTPROCAPI Tst_LoopbackI2C(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    DWORD dwRet=TPR_PASS; //will change to skip if no loopback transaction found.
    BOOL bFound=FALSE;
    
    if(TestInit() && !gdwInitFailed)
    {
        for(INT index=0;index<gI2CTransSize;index++)
        {            
            if(I2C_OP_LOOPBACK==gI2CTrans[index].Operation)
            {
                bFound=TRUE;  
                g_pKato->Log(LOG_DETAIL, L"*****************************************");
                g_pKato->Log(LOG_DETAIL, L"Performing LOOPBACK transaction: TRANS ID %d", gI2CTrans[index].dwTransactionID);
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
                
                if(SUCCESS!=Hlp_I2CLoopback( gI2CTrans, index) )
                {
                    dwRet=TPR_FAIL;
                    g_pKato->Log(LOG_FAIL,L"FAIL: LOOPBACK transaction failed");
                    continue;
                }
                
            }
              

        }//for index
        
        for(index=0;index<gI2CTransSize;index++)
        {
         if(SUCCESS!=Hlp_CloseI2CHandle(gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
            }
        }
        
        if(!bFound)
        {
            g_pKato->Log(LOG_DETAIL,L"Could not find any Loopback Transaction. Test Skipped");
            dwRet=TPR_SKIP;
        }
    } //if TestInit
    else
    {
        dwRet=TPR_SKIP;
    }

    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
// Tst_ReadI2C
// Performs all the read transactions specified in config file
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails 
///////////////////////////////////////////////////////////////////////////////////

TESTPROCAPI Tst_ReadI2C(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    DWORD dwRet=TPR_PASS;
    BOOL bFound=FALSE;

    if(TestInit() && !gdwInitFailed)
    {
        for(INT index=0;index<gI2CTransSize;index++)
        {          
            if(I2C_OP_READ==gI2CTrans[index].Operation)
                {
                    bFound=TRUE;
                    g_pKato->Log(LOG_DETAIL, L"*****************************************");
                    g_pKato->Log(LOG_DETAIL, L"Performing READ transaction: TRANS ID %d", gI2CTrans[index].dwTransactionID);
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
                    
                    g_pKato->Log(LOG_DETAIL, L"Performing READ Operation ...");
                    if(SUCCESS!=Hlp_I2CRead( gI2CTrans, index) )
                    {
                        dwRet=TPR_FAIL;
                        g_pKato->Log(LOG_FAIL,L"FAIL: READ transaction failed");
                        continue;
                    }

                }
            
        }//for index
        
        for(index=0;index<gI2CTransSize;index++)
        {
            if(SUCCESS!=Hlp_CloseI2CHandle(gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
            }
        }
        
        if(!bFound)
        {
            g_pKato->Log(LOG_DETAIL,L"Could not find any Read Transaction. Test Skipped");
            dwRet=TPR_SKIP;
        }
                
    } //if TestInit
    else
    {
        dwRet=TPR_SKIP;
    }

    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
// Tst_APII2C
// Tests the i2C bus with multiple masters trying to access
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails 
///////////////////////////////////////////////////////////////////////////////////

TESTPROCAPI Tst_MultipleMasterI2C(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    DWORD dwThreadId = 0;
    DWORD retVal = TPR_PASS;
    PI2CTRANSPARAMS pI2CParams = NULL;
    LONG lThreadCount = 0;

    if(TestInit() && !gdwInitFailed)
    {
    
        HANDLE hThread[MAX_THREADS];
        memset(hThread, 0, sizeof(hThread));
        PI2CTRANSPARAMS pI2CParams = new I2CTRANSPARAMS[gI2CTransSize];
        
        if(pI2CParams == NULL)
        {
            g_pKato->Log(LOG_FAIL, L"FAIL: Out of memory");
            retVal = TPR_FAIL;
            goto Error;
        }
        
        memset(pI2CParams, 0, sizeof(PI2CTRANSPARAMS)*gI2CTransSize);

        //create threads that performs the I2c transactions
        for(LONG lThread = 0; lThread < gI2CTransSize; lThread++)
        {
            pI2CParams[lThread].dwThreadNo = lThread;
            hThread[lThread] = CreateThread(NULL, 0, I2CTransactionThread, (LPVOID)(&pI2CParams[lThread]), 0, &dwThreadId);
            if(hThread[lThread] == NULL)
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: Can not create thread No. %d", lThread);
                retVal = TPR_FAIL;
                break;
            }
            else
            {
                g_pKato->Log(LOG_DETAIL, L"THREAD %d CREATED WITH THREAD ID %d", lThread, dwThreadId);
            }

            lThreadCount ++;

            if(lThreadCount == MAX_THREADS)
            {
                // Max number of threads reached
                break;
            }
        }

        //wait all threads to finish
        BOOL bAllOK = TRUE;
        DWORD dwExitCode = SUCCESS;
        for(INT i = 0; i < lThreadCount; i++)
        {
            if(hThread[i] == NULL)
            {   //thread had failed on creation
                bAllOK = FALSE;
                continue;
            }
            
            if(WaitForSingleObject(hThread[i], WAIT_THREAD_DONE) != WAIT_OBJECT_0){
                TerminateThread(hThread[i], FAILURE);    
                bAllOK = FALSE;
                g_pKato->Log(LOG_FAIL, L"FAIL: Thread %d could not exit properly!", i);
            }
            else
            {
                GetExitCodeThread(hThread[i], &dwExitCode);
                if(dwExitCode != SUCCESS)
                {
                    bAllOK = FALSE;
                    g_pKato->Log(LOG_FAIL, L"FAIL: Thread %d got some errors!", i);
                }
            }

            CloseHandle(hThread[i]);
            hThread[i] = NULL;
        }

        if(bAllOK == FALSE)
        {
            retVal = TPR_FAIL;
            goto Error;
        }


    }
    else
    {
        retVal = TPR_SKIP;
    }

    Error:
        
    if(pI2CParams != NULL)
    {
        delete[]  pI2CParams;
    }
            
    return retVal;

}

////////////////////////////////////////////////////////////////////////////////
//I2CTransactionThread
//Thread to perform a I2C Transaction
//
// Parameters:
//  LPVOID lpParameter  Thread Parameter
//
// Return value:
//  Exit Code -1 if any problems; 0 otherwise
////////////////////////////////////////////////////////////////////////////////
DWORD WINAPI I2CTransactionThread (LPVOID lpParameter) 
{
    DWORD dwExitCode = SUCCESS;
    
    PI2CTRANSPARAMS pParams = (PI2CTRANSPARAMS)lpParameter;
    if(pParams == NULL){
        g_pKato->Log(LOG_FAIL, L"FAIL: Invalid Transaction Thread Parameters");
        ExitThread(FAILURE);
        return FAILURE;
    }

    DWORD dwSetSpeed = I2C_STANDARD_SPEED; // Default value of I2C Speed
    
    HANDLE hI2C = INVALID_HANDLE_VALUE;
    I2CTRANS lI2CTrans = gI2CTrans[pParams->dwThreadNo];

    g_pKato->Log(LOG_DETAIL, L"******************");
    g_pKato->Log(LOG_DETAIL, L"THREAD %d STARTED", pParams->dwThreadNo);
    g_pKato->Log(LOG_DETAIL, L"******************");

    do
    {
        
        // Open I2C Handle

        g_pKato->Log(LOG_DETAIL, L"THREAD %d: Opening the handle to %s ...", pParams->dwThreadNo, lI2CTrans.pwszDeviceName);

        hI2C = I2cOpen(lI2CTrans.pwszDeviceName);

        if(INVALID_HANDLE_VALUE == hI2C)
        {
            g_pKato->Log(LOG_FAIL, L"FAIL: THREAD %d: Failed to open handle", pParams->dwThreadNo);
            dwExitCode = FAILURE;
            break;
        }
        else
        {
            g_pKato->Log(LOG_DETAIL, L"THREAD %d: Handle %x opened successfully", pParams->dwThreadNo, hI2C);
        }

        // Set the Subordinate address 

        g_pKato->Log(LOG_DETAIL, L"THREAD %d: Setting the Subordinate address: %x ...", pParams->dwThreadNo, lI2CTrans.dwSubordinateAddress);
        
        if(!I2cSetSlaveAddress(hI2C, lI2CTrans.dwSubordinateAddress))
        {
            g_pKato->Log(LOG_FAIL, L"FAIL: THREAD %d: Failed to set Subordinate address", pParams->dwThreadNo);
            dwExitCode = FAILURE;
            break;
        }

        // Set the Subordinate address size

        g_pKato->Log(LOG_DETAIL, L"THREAD %d: Setting the Subordinate address size: %d ...", pParams->dwThreadNo, lI2CTrans.dwSubordinateAddressSize);
        
        if(!I2cSetSlaveAddrSize(hI2C, lI2CTrans.dwSubordinateAddressSize))
        {
            g_pKato->Log(LOG_FAIL, L"FAIL: THREAD %d: Failed to set the Subordinate address size", pParams->dwThreadNo);
            dwExitCode = FAILURE;
            break;
        }

        // Set the register address size

        g_pKato->Log(LOG_DETAIL, L"THREAD %d: Setting the Register address size: %d ...", pParams->dwThreadNo, lI2CTrans.dwRegisterAddressSize);

        if(!I2cSetRegAddrSize(hI2C, lI2CTrans.dwRegisterAddressSize))
        {
            g_pKato->Log(LOG_FAIL, L"FAIL: THREAD %d: Failed to set register addr size", pParams->dwThreadNo);
            dwExitCode = FAILURE;
            break;
        }

        // Set the transaction speed

        g_pKato->Log(LOG_DETAIL, L"THREAD %d: Setting the i2C speed: %d ...", pParams->dwThreadNo, lI2CTrans.dwSpeedOfTransaction);
                
        if(!Hlp_SetI2CSpeed(hI2C, lI2CTrans.dwSpeedOfTransaction))
        {
            g_pKato->Log(LOG_FAIL, L"FAIL: THREAD %d: Failed to set I2C transaction speed", pParams->dwThreadNo);
            dwExitCode = FAILURE;
        }

        if(I2C_OP_WRITE==lI2CTrans.Operation)
        {
            g_pKato->Log(LOG_DETAIL, L"*****************************************");
            g_pKato->Log(LOG_DETAIL, L"THREAD %d: Performing WRITE transaction: TRANS ID %d", pParams->dwThreadNo, lI2CTrans.dwTransactionID);
            g_pKato->Log(LOG_DETAIL, L"*****************************************");
            
            
            g_pKato->Log(LOG_DETAIL, L"THREAD %d: Performing WRITE operation ...", pParams->dwThreadNo);                
            if(SUCCESS!=Hlp_I2CWrite( hI2C, lI2CTrans) )
            {
                g_pKato->Log(LOG_FAIL,L"FAIL: THREAD %d: WRITE transaction failed", pParams->dwThreadNo);
                dwExitCode = FAILURE;
                break;
            }
            
        }
        else if(I2C_OP_READ==lI2CTrans.Operation)
        {
            g_pKato->Log(LOG_DETAIL, L"*****************************************");
            g_pKato->Log(LOG_DETAIL, L"THREAD %d: Performing READ transaction: TRANS ID %d", pParams->dwThreadNo, lI2CTrans.dwTransactionID);
            g_pKato->Log(LOG_DETAIL, L"*****************************************");
                   
            g_pKato->Log(LOG_DETAIL, L"THREAD %d: Performing READ Operation ...", pParams->dwThreadNo);
            if(SUCCESS!=Hlp_I2CRead(  hI2C, lI2CTrans) )
            {
                g_pKato->Log(LOG_FAIL,L"FAIL: THREAD %d: READ transaction failed", pParams->dwThreadNo);
                dwExitCode = FAILURE;
                break;
            }

        }
        else if(I2C_OP_LOOPBACK==lI2CTrans.Operation)
        {  
            g_pKato->Log(LOG_DETAIL, L"*****************************************");
            g_pKato->Log(LOG_DETAIL, L"THREAD %d: Performing LOOPBACK transaction: TRANS ID %d", pParams->dwThreadNo, lI2CTrans.dwTransactionID);
            g_pKato->Log(LOG_DETAIL, L"*****************************************");
            
            if(SUCCESS!=Hlp_I2CLoopback(  hI2C, lI2CTrans) )
            {
                g_pKato->Log(LOG_FAIL,L"FAIL: THREAD %d: LOOPBACK transaction failed", pParams->dwThreadNo);
                dwExitCode = FAILURE;
                break;
            }     
        }

    }while(0);

    // Close the i2c handle

    g_pKato->Log(LOG_DETAIL, L"THREAD %d: Closing the handle to %s ...", pParams->dwThreadNo, lI2CTrans.pwszDeviceName);

    if(INVALID_HANDLE_VALUE != hI2C)
    {
        I2cClose(hI2C);
        hI2C = INVALID_HANDLE_VALUE;
    }

    g_pKato->Log(LOG_DETAIL, L"******************");
    g_pKato->Log(LOG_DETAIL, L"THREAD %d COMPLETED", pParams->dwThreadNo);
    g_pKato->Log(LOG_DETAIL, L"******************");

    return dwExitCode;
}

 

////////////////////////////////////////////////////////////////////////////////
