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
// Tst_APII2C
// Tests the I2C API with valid and invalid params
//
// Parameters:
//  uMsg            Message code.
//  tpParam         Additional message-dependent data.
//  lpFTE           Function table entry that generated this call.
//
// Return value:
//  TPR_PASS if the test passed, TPR_FAIL if the test fails 
///////////////////////////////////////////////////////////////////////////////////

TESTPROCAPI Tst_APII2C(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }
    
    DWORD dwRet=TPR_PASS;
    HANDLE hI2C = NULL;
    DWORD dwSubordinateAddrSize = 0;
    DWORD dwRegAddrSize = 0;
    DWORD dwSpeed = 0;
    DWORD dwSetSpeed = 0;
    DWORD dwExpSpeed = 0;
    DWORD dwTimeOut = 0;
    BYTE  byData = 0;
    DWORD dwLength = 0;
    BYTE bBuffer[I2C_MAX_LENGTH]={0};
    LONG index = 0;

    I2CTRANS lI2CReadTrans = {0};
    I2CTRANS lI2CWriteTrans = {0};

    if(TestInit() && !gdwInitFailed)
    {
        do
        {
            // Get the first READ transaction from the list of transactions
            if(SUCCESS!= Hlp_GetNextTransIndex(gI2CTrans, I2C_OP_READ, &index))
            {
                g_pKato->Log(LOG_DETAIL,L"Could not find any Read Transaction. Test Skipped");
                dwRet=TPR_SKIP;
                break;
            }
            
            g_pKato->Log(LOG_DETAIL,L"Found 1st READ Transaction at index, i=%d",index);
            PrintI2CData(gI2CTrans, index);

            lI2CReadTrans = gI2CTrans[index];

            index = 0;

            // Get the first WRITE transaction from the list of transactions
            if(SUCCESS!= Hlp_GetNextTransIndex(gI2CTrans, I2C_OP_WRITE, &index))
            {
                g_pKato->Log(LOG_DETAIL,L"Could not find any Write Transaction. Test Skipped");
                dwRet=TPR_SKIP;
                break;
            }
            
            g_pKato->Log(LOG_DETAIL,L"Found 1st WRITE Transaction at index, i=%d",index);
            PrintI2CData(gI2CTrans, index);

            lI2CWriteTrans = gI2CTrans[index];
            
            // Test the I2C APIs for valid range of parameters

            g_pKato->Log(LOG_DETAIL, L"******************************************");
            g_pKato->Log(LOG_DETAIL, L"Testing I2C APIs with valid parameters ...");
            g_pKato->Log(LOG_DETAIL, L"******************************************");

            // Test I2cOpen API

            g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2COpen with device name: %s ...", lI2CReadTrans.pwszDeviceName);

            hI2C = I2cOpen(lI2CReadTrans.pwszDeviceName);

            if(INVALID_HANDLE_VALUE == hI2C)
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: I2COpen API failed");
                dwRet = TPR_FAIL;
                break;
            }
            else
            {
                g_pKato->Log(LOG_DETAIL, L"I2COpen API succeeded as expected");
            }

            // Test I2cSetSlaveAddrSize API

            for(int i=0; i< SUBORDINATE_ADDR_SIZE_VALID_COUNT; i++)
            {
                dwSubordinateAddrSize = g_adwValidSubordinateAddrSize[i];

                g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cSetSlaveAddrSize with Subordinate address size: %d ...", dwSubordinateAddrSize);
                
                if(I2cSetSlaveAddrSize(hI2C, dwSubordinateAddrSize))
                {
                    g_pKato->Log(LOG_DETAIL, L"I2cSetSlaveAddrSize API succeeded as expected");
                }
                else
                {
                    g_pKato->Log(LOG_FAIL, L"FAIL: I2cSetSlaveAddrSize API failed");
                    dwRet = TPR_FAIL;
                }
            }
            

            // Test I2cSetRegAddrSize API

            for(dwRegAddrSize=0; dwRegAddrSize< 3; dwRegAddrSize++)
            {   
                g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cSetRegAddrSize with Reg address size: %d ...", dwRegAddrSize);

                if(I2cSetRegAddrSize(hI2C, dwRegAddrSize))
                {
                    g_pKato->Log(LOG_DETAIL, L"I2cSetRegAddrSize API succeeded as expected");
                }
                else
                {
                    g_pKato->Log(LOG_FAIL, L"FAIL: I2cSetRegAddrSize API failed");
                    dwRet = TPR_FAIL;
                }
            }

            // Test I2cSetSpeed API

            for(int i=0; i< SPEED_VALID_COUNT; i++)
            {
                dwSpeed = g_adwValidTransSpeed[i];

                if ( dwSpeed <= I2C_STANDARD_SPEED )
                {
                    dwExpSpeed = I2C_STANDARD_SPEED;
                }
                else if ( dwSpeed <= I2C_FAST_SPEED ) 
                {
                    dwExpSpeed = I2C_FAST_SPEED;
                }
                else if ( dwSpeed <= I2C_HIGH_SPEED )
                {
                    dwExpSpeed = I2C_HIGH_SPEED;
                }

                g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cSetSpeed with speed: %d ...", dwSpeed);
                
                dwSetSpeed = I2cSetSpeed(hI2C, dwSpeed);

                if(dwSetSpeed)
                {
                    if(dwSetSpeed == dwExpSpeed)
                    {
                        g_pKato->Log(LOG_DETAIL, L"I2cSetSpeed API succeeded as expected");
                        g_pKato->Log(LOG_DETAIL, L"Actual speed set to %d", dwSetSpeed);
                    }
                    else if((dwSetSpeed == I2C_STANDARD_SPEED) || 
                            (dwSetSpeed == I2C_FAST_SPEED) || 
                            (dwSetSpeed == I2C_HIGH_SPEED))
                    {
                        g_pKato->Log(LOG_DETAIL, L"WARNING: Actual speed %d is not same as expected speed %d", dwSetSpeed, dwExpSpeed);
                    }
                    else
                    {
                        g_pKato->Log(LOG_FAIL, L"FAIL: Actual speed %d set is not a valid I2C transaction speed", dwSetSpeed);
                        dwRet = TPR_FAIL;
                    }
                }
                else
                {
                    g_pKato->Log(LOG_FAIL, L"FAIL: I2cSetSpeed API failed");
                    dwRet = TPR_FAIL;
                }
            }

            // Test I2cSetTimeOut API

            for(int i=0; i< TIMEOUT_VALID_COUNT; i++)
            {
                dwTimeOut = g_adwValidTimeOut[i];

                g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cSetTimeOut with time out: %d ...", dwTimeOut);
                
                if(I2cSetTimeOut(hI2C, dwTimeOut))
                {
                    g_pKato->Log(LOG_DETAIL, L"I2cSetTimeOut API succeeded as expected");
                }
                else
                {
                    g_pKato->Log(LOG_FAIL, L"FAIL: I2cSetTimeOut API failed");
                    dwRet = TPR_FAIL;
                }
            }

            // Test I2cSetSlaveAddress API

            g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cSetSlaveAddress with Subordinate address: %d ...", lI2CWriteTrans.dwSubordinateAddress);
            
            if(I2cSetSlaveAddress(hI2C, lI2CWriteTrans.dwSubordinateAddress))
            {
                g_pKato->Log(LOG_DETAIL, L"I2cSetSlaveAddress API succeeded as expected");
            }
            else
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: I2cSetSlaveAddress API failed");
                dwRet = TPR_FAIL;
            }

            g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cWrite with Reg Addr: %d, Data: %x, Length: %d ...", 
                    lI2CWriteTrans.dwRegisterAddress, 
                    lI2CWriteTrans.BufferPtr, 
                    lI2CWriteTrans.dwNumOfBytes);

            // Test I2cWrite API

            // Set the Subordinate address size and register address size as per the transaction

            g_pKato->Log(LOG_DETAIL, L"Set the Subordinate address size: %d ...", lI2CWriteTrans.dwSubordinateAddressSize);
            g_pKato->Log(LOG_DETAIL, L"Set the register address size: %d ...", lI2CWriteTrans.dwRegisterAddressSize);

            if(!I2cSetSlaveAddrSize(hI2C, lI2CWriteTrans.dwSubordinateAddressSize))
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: Failed to set the Subordinate address size");
                dwRet = TPR_FAIL;
            }

            if(!I2cSetRegAddrSize(hI2C, lI2CWriteTrans.dwRegisterAddressSize))
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: Failed to set the register address size");
                dwRet = TPR_FAIL;
            }

            // Before writing new value to register, save the original value

            g_pKato->Log(LOG_DETAIL,L"Reading the original value =>");
            if(TRUE == I2cRead(hI2C, lI2CWriteTrans.dwRegisterAddress, bBuffer, lI2CWriteTrans.dwNumOfBytes))
            {
                PrintBufferData(bBuffer, lI2CWriteTrans.dwNumOfBytes);
            }
            else
            {
                g_pKato->Log(LOG_FAIL,L"Failed to read the original value of register");
            }

            if(I2cWrite(hI2C, lI2CWriteTrans.dwRegisterAddress, lI2CWriteTrans.BufferPtr, lI2CWriteTrans.dwNumOfBytes))
            {
                g_pKato->Log(LOG_DETAIL, L"I2cWrite API succeeded as expected");
            }
            else
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: I2cWrite API failed");
                dwRet = TPR_FAIL;
            }

            // Restore the original value of the register
                            
            g_pKato->Log(LOG_DETAIL,L"Writing the original value =>");
            PrintBufferData(bBuffer, dwLength);
            
            if(TRUE != I2cWrite(hI2C, lI2CWriteTrans.dwRegisterAddress, bBuffer, lI2CWriteTrans.dwNumOfBytes))
            {
                g_pKato->Log(LOG_FAIL,L"Failed to write the original value to register");
            }

            // Test I2cSetSlaveAddress API

            g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cSetSlaveAddress with Subordinate address: %d ...", lI2CReadTrans.dwSubordinateAddress);
            
            if(I2cSetSlaveAddress(hI2C, lI2CReadTrans.dwSubordinateAddress))
            {
                g_pKato->Log(LOG_DETAIL, L"I2cSetSlaveAddress API succeeded as expected");
            }
            else
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: I2cSetSlaveAddress API failed");
                dwRet = TPR_FAIL;
            }

            // Test I2cRead API
            
            g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cRead with Reg Addr: %d, Data: %x, Length: %d ...", 
                            lI2CReadTrans.dwRegisterAddress, 
                            lI2CReadTrans.BufferPtr, 
                            lI2CReadTrans.dwNumOfBytes);

            if(I2cRead(hI2C, lI2CReadTrans.dwRegisterAddress, lI2CReadTrans.BufferPtr, lI2CReadTrans.dwNumOfBytes))
            {
                g_pKato->Log(LOG_DETAIL, L"I2cRead API succeeded as expected");
            }
            else
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: I2cRead API failed");
                dwRet = TPR_FAIL;
            }

            // Test I2cClose API

            g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cClose ...");

            if(INVALID_HANDLE_VALUE != hI2C)
            {
                I2cClose(hI2C);
                hI2C = INVALID_HANDLE_VALUE;
            }

            // Test the I2C APIs for invalid range of parameters

            
            g_pKato->Log(LOG_DETAIL, L"******************************************");
            g_pKato->Log(LOG_DETAIL, L"Testing I2C APIs with invalid parameters ...");
            g_pKato->Log(LOG_DETAIL, L"******************************************");

            // Open the handle to I2C

            g_pKato->Log(LOG_DETAIL, L"Opening the handle to I2C device name: %s ...", lI2CReadTrans.pwszDeviceName);

            hI2C = I2cOpen(lI2CReadTrans.pwszDeviceName);

            if(INVALID_HANDLE_VALUE == hI2C)
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: Failed to open tha handle");
                dwRet = TPR_FAIL;
                break;
            }

            // Test I2cSetSlaveAddrSize API

            for(int i=0; i< SUBORDINATE_ADDR_SIZE_INVALID_COUNT; i++)
            {
                dwSubordinateAddrSize = g_adwInvalidSubordinateAddrSize[i];

                g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cSetSlaveAddrSize with Subordinate address size: %d ...", dwSubordinateAddrSize);
                
                if(!I2cSetSlaveAddrSize(hI2C, dwSubordinateAddrSize))
                {
                    g_pKato->Log(LOG_DETAIL, L"I2cSetSlaveAddrSize API failed as expected");
                }
                else
                {
                    g_pKato->Log(LOG_FAIL, L"FAIL: I2cSetSlaveAddrSize API succeeded for invalid Subordinate address size");
                    dwRet = TPR_FAIL;
                }
            }

            // Test I2cSetRegAddrSize API

            dwRegAddrSize = 3;
            
            g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cSetRegAddrSize with Reg address size: %d ...", dwRegAddrSize);

            if(!I2cSetRegAddrSize(hI2C, dwRegAddrSize))
            {
                g_pKato->Log(LOG_DETAIL, L"I2cSetRegAddrSize API failed as expected");
            }
            else
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: I2cSetRegAddrSize API succeeded for invalid register address size");
                dwRet = TPR_FAIL;
            }

            // Test I2cSetTimeOut API

            
            for(int i=0; i< TIMEOUT_INVALID_COUNT; i++)
            {
                dwTimeOut = g_adwInvalidTimeOut[i];

                g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cSetTimeOut with time out: %d ...", dwTimeOut);
                
                if(!I2cSetTimeOut(hI2C, dwTimeOut))
                {
                    g_pKato->Log(LOG_DETAIL, L"I2cSetTimeOut API failed as expected");
                }
                else
                {
                    g_pKato->Log(LOG_FAIL, L"FAIL: I2cSetTimeOut API succeeded for invalid time out");
                    dwRet = TPR_FAIL;
                }
            }

            // Test I2cWrite API

            // Set the Subordinate address as per transaction
            
            g_pKato->Log(LOG_DETAIL, L"Set the Subordinate address: %d ...", lI2CWriteTrans.dwSubordinateAddress);
            
            if(!I2cSetSlaveAddress(hI2C, lI2CWriteTrans.dwSubordinateAddress))
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: Failed to set the Subordinate address");
                dwRet = TPR_FAIL;
            }


            // Set the Subordinate address and register address size as per transaction

            g_pKato->Log(LOG_DETAIL, L"Set the Subordinate address size: %d ...", lI2CWriteTrans.dwSubordinateAddressSize);
            g_pKato->Log(LOG_DETAIL, L"Set the register address size: %d ...", lI2CWriteTrans.dwRegisterAddressSize);
            
            if(!I2cSetSlaveAddrSize(hI2C, lI2CWriteTrans.dwSubordinateAddressSize))
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: Failed to set Subordinate address size %d", dwSubordinateAddrSize);
                dwRet = TPR_FAIL;
            }

            if(!I2cSetRegAddrSize(hI2C, lI2CWriteTrans.dwRegisterAddressSize))
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: Failed to set register address size %d", dwRegAddrSize);
                dwRet = TPR_FAIL;
            }

            // Test for NULL data buffer

            // Before writing new value to register, save the original value

            g_pKato->Log(LOG_DETAIL,L"Reading the original value =>");
            if(TRUE== I2cRead(hI2C, lI2CWriteTrans.dwRegisterAddress, bBuffer, lI2CWriteTrans.dwNumOfBytes))
            {
                PrintBufferData(bBuffer, lI2CWriteTrans.dwNumOfBytes);
            }
            else
            {
                g_pKato->Log(LOG_FAIL,L"Failed to read the original value of register");
            }

            g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cWrite with Reg Addr: %d, Data Buffer: NULL, Length: %d ...", 
                    lI2CWriteTrans.dwRegisterAddress, 
                    lI2CWriteTrans.dwNumOfBytes);

            if(!I2cWrite(hI2C, lI2CWriteTrans.dwRegisterAddress, NULL, lI2CWriteTrans.dwNumOfBytes))
            {
                g_pKato->Log(LOG_DETAIL, L"I2cWrite API failed as expected");
            }
            else
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: I2cWrite API succeeded for NULL data buffer");
                dwRet = TPR_FAIL;
            }

            // Test for invalid data length

            dwLength = I2C_MAX_LENGTH + 1;

            g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cWrite with Reg Addr: %d, Data Buffer: %x, Length: %d ...", 
                    lI2CWriteTrans.dwRegisterAddress, 
                    lI2CWriteTrans.BufferPtr,
                    dwLength);

            if(!I2cWrite(hI2C, lI2CWriteTrans.dwRegisterAddress, lI2CWriteTrans.BufferPtr, dwLength))
            {
                g_pKato->Log(LOG_DETAIL, L"I2cWrite API failed as expected");
            }
            else
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: I2cWrite API succeeded for invalid data length");
                dwRet = TPR_FAIL;
            }

            // Restore the original value of the register
                            
            g_pKato->Log(LOG_DETAIL,L"Writing the original value =>");
            PrintBufferData(bBuffer, dwLength);
            
            if(TRUE != I2cWrite(hI2C, lI2CWriteTrans.dwRegisterAddress, bBuffer, lI2CWriteTrans.dwNumOfBytes))
            {
                g_pKato->Log(LOG_FAIL,L"Failed to write the original value to register");
            }

            // Test I2cRead API

            // Set the Subordinate address

            g_pKato->Log(LOG_DETAIL, L"Set the Subordinate address: %d ...", lI2CReadTrans.dwSubordinateAddress);
            
            if(!I2cSetSlaveAddress(hI2C, lI2CWriteTrans.dwSubordinateAddress))
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: Failed to set the Subordinate address");
                dwRet = TPR_FAIL;
            }
            
            // Test for NULL data buffer

            g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cRead with Reg Addr: %d, Data Buffer: NULL, Length: %d ...", 
                    lI2CReadTrans.dwRegisterAddress, 
                    lI2CReadTrans.dwNumOfBytes);

            if(!I2cRead(hI2C, lI2CReadTrans.dwRegisterAddress, NULL, lI2CReadTrans.dwNumOfBytes))
            {
                g_pKato->Log(LOG_DETAIL, L"I2cRead API failed as expected");
            }
            else
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: I2cRead API succeeded for NULL data buffer");
                dwRet = TPR_FAIL;
            }

            // Test for invalid data length

            dwLength = I2C_MAX_LENGTH + 1;

            g_pKato->Log(LOG_DETAIL, L"Testing I2C API - I2cRead with Reg Addr: %d, Data Buffer: %x, Length: %d ...", 
                    lI2CReadTrans.dwRegisterAddress, 
                    lI2CReadTrans.BufferPtr,
                    dwLength);

            if(!I2cRead(hI2C, lI2CReadTrans.dwRegisterAddress, lI2CReadTrans.BufferPtr, dwLength))
            {
                g_pKato->Log(LOG_DETAIL, L"I2cRead API failed as expected");
            }
            else
            {
                g_pKato->Log(LOG_FAIL, L"FAIL: I2cRead API succeeded for invalid data length");
                dwRet = TPR_FAIL;
            }

        }while(0);
    } 
    else
    {   
        dwRet = TPR_SKIP;
    }

    if(hI2C != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hI2C);
    }
    
    return dwRet;
}
 

////////////////////////////////////////////////////////////////////////////////
