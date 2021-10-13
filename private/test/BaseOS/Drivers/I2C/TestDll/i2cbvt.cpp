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
// Tst_BvtReadI2C
// Performs the first read transaction specified in config file
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails 

TESTPROCAPI Tst_BvtReadI2C(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
  
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    DWORD dwRet=TPR_SKIP;
    long index=0;
    if(TestInit() && !gdwInitFailed)
    {
        do{
            if(SUCCESS!= Hlp_GetNextTransIndex(gI2CTrans, I2C_OP_READ, &index))
            {
                g_pKato->Log(LOG_DETAIL,L"Could not find any Read Transaction. Test Skipped");
                dwRet=TPR_SKIP;
                break;
            }
            
            g_pKato->Log(LOG_DETAIL,L"Found 1st READ Transaction at index, i=%d",index);
            g_pKato->Log(LOG_DETAIL,L"Preparing for Transaction for following I2c Bus device and transaction details:");
            PrintI2CData(gI2CTrans, index);
            
            if(SUCCESS != Hlp_SetupBusData(gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
                break;
            }
            
            g_pKato->Log(LOG_DETAIL, L"Set up Bus Data for index, i=%d", index);
            if(SUCCESS!=Hlp_SetupDeviceData(gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
                break;
            }
            
            g_pKato->Log(LOG_DETAIL,L"Set up Device Data for index, i=%d",index);    
            if(SUCCESS!=Hlp_I2CRead( gI2CTrans, index) )
            {
                dwRet=TPR_FAIL;
                g_pKato->Log(LOG_FAIL,L"FAIL: READ transaction failed");
                break;
            }
            
            g_pKato->Log(LOG_FAIL,L"Success Reading from Device");  

            dwRet=TPR_PASS;
            
            
        }while(0);

        if(SUCCESS!=Hlp_CloseI2CHandle(gI2CTrans, index))
        {
            dwRet=TPR_FAIL;
        }
        
    }


    
    return dwRet;
}

////////////////////////////////////////////////////////////////////////////////
// Tst_BvtWriteI2C
// Performs the first write transaction specified in config file
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails 
///////////////////////////////////////////////////////////////////////////////////
TESTPROCAPI Tst_BvtWriteI2C(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
  
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    
    DWORD dwRet=TPR_SKIP;
    long index=0;
    
    if(TestInit() && !gdwInitFailed)
    {
        do
        {
            if(SUCCESS!= Hlp_GetNextTransIndex(gI2CTrans, I2C_OP_WRITE, &index))
            {
                g_pKato->Log(LOG_DETAIL,L"Could not find any Write Transaction. Test Skipped");
                dwRet=TPR_SKIP;
                break;
            }
            
            g_pKato->Log(LOG_DETAIL,L"Found 1st WRITE Transaction at index, i=%d",index);
            g_pKato->Log(LOG_DETAIL,L"Preparing for Transaction for following I2c Bus device and transaction details:");

            PrintI2CData(gI2CTrans, index);

            if(SUCCESS!=Hlp_SetupBusData(gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
                break;
            }
            
            g_pKato->Log(LOG_DETAIL,L"Set up Bus Data for index, i=%d",index);
            if(SUCCESS!=Hlp_SetupDeviceData(gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
                break;
            }
            
            g_pKato->Log(LOG_DETAIL,L"Set up Device Data for index, i=%d,",index);    
            if(SUCCESS!=Hlp_I2CWrite( gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
                g_pKato->Log(LOG_FAIL,L"FAIL: WRITE transaction failed");
                break;
            }
            
            g_pKato->Log(LOG_FAIL,L"Success Writing to Device");  
            
            dwRet=TPR_PASS;
   
        }while(0);

        if(SUCCESS != Hlp_CloseI2CHandle(gI2CTrans, index))
        {
            dwRet=TPR_FAIL;
        }
        
    }
    
    return dwRet;
}


////////////////////////////////////////////////////////////////////////////////
