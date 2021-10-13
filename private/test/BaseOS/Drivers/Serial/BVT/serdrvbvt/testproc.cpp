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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#include "TESTPROC.H"
#include "UTIL.H"

#define WRITE_SIZE              10
#define READ_SIZE               128

#define DEF_CONST_TIMEOUT       100
#define DEF_MULTIPLIER_TIMOUT   10
#define MAX_DELTA_TIMEOUT       5

#define QUEUE_ENTRIES   3
#define MAX_NAMELEN     200
#define QUEUE_SIZE      (QUEUE_ENTRIES * (sizeof(POWER_BROADCAST) + MAX_NAMELEN))
    
// --------------------------------------------------------------------
TESTPROCAPI 
Tst_Stub(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;    

    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // not a multi-threaded test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    retVal = TPR_PASS;   
    
Error:    

    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_WritePort(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
//
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;
    HANDLE hCommPort = INVALID_HANDLE_VALUE;
    COMMPROP commProps = {0};
    COMMTIMEOUTS    Cto;
    UINT iBaudRate = 0;
    UINT iDataBits = 0;
    UINT iParity = 0;
    UINT iStopBits = 0;
    DWORD dwCommPort = 0;
    DWORD dwBytesWritten = 0;
    DCB dcbCommPort = {0};
    BOOL bRtn = TRUE ;

    BYTE junkBuffer[WRITE_SIZE];
    
    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // start a thread for each comm port
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = Util_QueryCommPortCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }
  
    if(-1==COMx)
    {
          dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

          if(!COMMask[dwCommPort])
                return TPR_PASS;

    }
    else
        dwCommPort = COMx;

    // open comm port
    //
    LOG(TEXT("%s on COM%u"), lpFTE->lpDescription, dwCommPort);
    hCommPort = Util_OpenCommPort(dwCommPort);
    if(INVALID_HANDLE(hCommPort))
    {
        ERRFAIL("Util_OpenCommPort()");
        goto Error;
    }

    // query comm port properies
    //
    LOG(TEXT("Calling GetCommProperties() on COM%u"), dwCommPort);
    __try
    {
        if(!GetCommProperties(hCommPort, &commProps))
        {
            LOG(TEXT("GetCommProperties() failed on COM%u"), dwCommPort);
            ERRFAIL("GetCommProperties()");
            goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("GetCommProperties() threw an exception on COM%u"), dwCommPort);
        ERRFAIL("GetCommProperties()");
        goto Error;
    }

    // send bytes for each supported baud rate, data-bits, parity, and stop-bits
    //
    for(iBaudRate = 0; iBaudRate < NUM_BAUD_RATES; iBaudRate++)
    {
        // skip this baud rate if it is not supported
        if(!(BAUD_RATES[iBaudRate] & commProps.dwSettableBaud))
        {
            LOG(TEXT("COM%u does not support %s baud"), dwCommPort, SZ_BAUD_RATES[iBaudRate]);
            continue;
        }

        for(iDataBits = 0; iDataBits < NUM_DATA_BITS; iDataBits++)
        {
            if(!(DATA_BITS[iDataBits] & commProps.wSettableData))
            {
                LOG(TEXT("COM%u does not support %s data bits"), dwCommPort, SZ_DATA_BITS[iDataBits]);
                continue;
            }
            
            for(iParity = 0; iParity < NUM_PARITY; iParity++)
            {
                if(!(PARITY[iParity] & commProps.wSettableStopParity))
                {
                    LOG(TEXT("COM%u does not support %s parity"), dwCommPort, SZ_PARITY[iParity]);
                    continue;
                }
                    
                for(iStopBits = 0; iStopBits < NUM_STOP_BITS; iStopBits++)
                {
                    if(!(STOP_BITS[iStopBits] & commProps.wSettableStopParity))
                    {
                        LOG(TEXT("COM%u does not support %s stop bits"), dwCommPort, SZ_STOP_BITS[iStopBits]);
                        continue;
                    }

                    LOG(TEXT("Calling GetCommState() on COM%u"), dwCommPort);
                    __try
                    {
                        if(!GetCommState(hCommPort, &dcbCommPort))
                        {
                            LOG(TEXT("GetCommState() failed on COM%u"), dwCommPort);
                            ERRFAIL("GetCommState()");
                            goto Error;
                        }                    
                    }
                    __except(EXCEPTION_EXECUTE_HANDLER)
                    {
                        LOG(TEXT("GetCommState() threw an exception on COM%u"), dwCommPort);
                        ERRFAIL("GetCommState()");
                        goto Error;
                    }

                    //Check timouts....if WriteTotalTimeoutConstant or WriteTotalTimeoutMultiplier, we will hang test
                    LOG(TEXT("Calling GetCommTimeouts() on COM%u"), dwCommPort);
                    __try
                    {
                    if(!GetCommTimeouts(hCommPort, &Cto))
                        {
                            LOG(TEXT("GetCommState() failed on COM%u"), dwCommPort);
                            ERRFAIL("GetCommState()");
                            goto Error;
                        }                    
                    }
                    __except(EXCEPTION_EXECUTE_HANDLER)
                    {
                        LOG(TEXT("GetCommState() threw an exception on COM%u"), dwCommPort);
                        ERRFAIL("GetCommState()");
                        goto Error;
                    }
                    if(Cto.WriteTotalTimeoutConstant == 0 || Cto.WriteTotalTimeoutMultiplier == 0)
                        {
                        WARN("GetCommTimeouts()");
                        LOG(TEXT("WARNING!! :: WriteTotalTimeoutConstant in serial driver on COM%u == 0.....may create undesired hangs in system"), dwCommPort);
                        LOG(TEXT("Setting WriteTotalTimeoutConstant to 100 so test will not hang"));
                        Cto.WriteTotalTimeoutConstant = DEF_CONST_TIMEOUT;
                        Cto.WriteTotalTimeoutMultiplier = DEF_MULTIPLIER_TIMOUT;
                        LOG(TEXT("Calling SetCommTimeouts() on COM%u"), dwCommPort);
                    __try
                        {
                        if(!SetCommTimeouts(hCommPort, &Cto))
                            {
                            LOG(TEXT("SetCommTimeouts() failed on COM%u"), dwCommPort);
                            ERR("SetCommTimeouts()");
                            continue;
                            }
                        }
                    __except(EXCEPTION_EXECUTE_HANDLER)
                        {
                        LOG(TEXT("SetCommTimeouts() threw an exception on COM%u"), dwCommPort);
                        ERRFAIL("SetCommTimeouts()");
                        goto Error;
                        }
                    }
                     

                    // MSDN: The number of data bits must be 5 to 8 bits. 
                    // The use of 5 data bits with 2 stop bits is an invalid combination, 
                    // as is 6, 7, or 8 data bits with 1.5 stop bits. 
                    
                    dcbCommPort.BaudRate = BAUD_RATES_VAL[iBaudRate];
                    dcbCommPort.ByteSize = DATA_BITS_VAL[iDataBits];
                    dcbCommPort.Parity = PARITY_VAL[iParity];
                    dcbCommPort.StopBits = STOP_BITS_VAL[iStopBits];

                    LOG(TEXT("Calling SetCommState() on COM%u"), dwCommPort);
                    __try
                    {
                        if(!SetCommState(hCommPort, &dcbCommPort))
                        {
                            LOG(TEXT("SetCommState() failed on COM%u, %s baud, %s data-bits, %s parity, %s stop-bits"),
                                dwCommPort, SZ_BAUD_RATES[iBaudRate], SZ_DATA_BITS[iDataBits], SZ_PARITY[iParity],
                                SZ_STOP_BITS[iStopBits]);
                            ERR("SetCommState()");
                            continue;
                        }
                    }
                    __except(EXCEPTION_EXECUTE_HANDLER)
                    {
                        LOG(TEXT("SetCommState() threw an exception on COM%u"), dwCommPort);
                        ERRFAIL("SetCommState()");
                        goto Error;
                    } 
                    
                    LOG(TEXT("Sending on COM%u at %s baud, %s data-bits, %s parity, %s stop-bits."),
                        dwCommPort, SZ_BAUD_RATES[iBaudRate], SZ_DATA_BITS[iDataBits], 
                        SZ_PARITY[iParity], SZ_STOP_BITS[iStopBits]);

                    __try
                    {
                        bRtn = WriteFile(hCommPort, junkBuffer, WRITE_SIZE, &dwBytesWritten, NULL);

                        if(!bRtn || dwBytesWritten!=WRITE_SIZE )
                        {
                                LOG(TEXT("Failed to send on COM%u at %s baud, %s data-bits, %s parity, %s stop-bits."),
                                dwCommPort, SZ_BAUD_RATES[iBaudRate], SZ_DATA_BITS[iDataBits], 
                                SZ_PARITY[iParity], SZ_STOP_BITS[iStopBits]);
                                ERRFAIL("WriteFile()");
                        }

                    }
                    __except(EXCEPTION_EXECUTE_HANDLER)
                    {
                        LOG(TEXT("WriteFile() threw an exception on COM%u"), dwCommPort);
                        ERRFAIL("WriteFile()");
                        goto Error;
                    }   
                }
            }
        }
    }

    // close comm port
    //
    if(!CloseHandle(hCommPort))
    {
        ERRFAIL("CloseHandle()");
        goto Error;
    }
    hCommPort = INVALID_HANDLE_VALUE;

    LOG(TEXT("%s passed on COM%u"), lpFTE->lpDescription, dwCommPort);
    retVal = TPR_PASS;   
    
Error:    

    if(VALID_HANDLE(hCommPort))
    {
        CloseHandle(hCommPort);
    }

    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_SetCommEvents(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;    
    DWORD dwCommMask = 0;
    DWORD dwCommPort = 0;
    UINT iCommMask = 0;
    HANDLE hCommPort = INVALID_HANDLE_VALUE;

    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // not a multi-threaded test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = Util_QueryCommPortCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    
    if(-1==COMx)
    {
        dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

        if(!COMMask[dwCommPort])
           return TPR_PASS;
    }
    else
        dwCommPort = COMx;

    // open comm port
    //
    LOG(TEXT("%s on COM%u"), lpFTE->lpDescription, dwCommPort);
    hCommPort = Util_OpenCommPort(dwCommPort);
    if(INVALID_HANDLE(hCommPort))
    {
        ERRFAIL("Util_OpenCommPort()");
        goto Error;
    }

    for(iCommMask = 0; iCommMask < NUM_COMM_EVENTS; iCommMask++)
    {
        LOG(TEXT("Testing SetCommMask(%s) on COM%u..."), SZ_COMM_EVENTS[iCommMask], dwCommPort);
        __try
        {
            if(!SetCommMask(hCommPort, COMM_EVENTS[iCommMask]))
            {
                LOG(TEXT("Failed SetCommMask(%s) on COM%u"), SZ_COMM_EVENTS[iCommMask], dwCommPort);
                ERRFAIL("SetCommMask()");
                goto Error;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            LOG(TEXT("SetCommMask() threw an exception on COM%u"), dwCommPort);
            FAIL("SetCommMask()");
            goto Error;
        }

        __try
        {
            if(!GetCommMask(hCommPort, &dwCommMask))
            {
                LOG(TEXT("!!! Failed GetCommMask() on COM%u..."), dwCommPort);
                ERRFAIL("GetCommMask()");
                goto Error;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            LOG(TEXT("GetCommMask() threw an exception on COM%u"), dwCommPort);
            FAIL("GetCommMask()");
            goto Error;
        }
        
        LOG(TEXT("Verifying event masks matched on COM%u..."), dwCommPort);
        if(COMM_EVENTS[iCommMask] != dwCommMask)
        {
            LOG(TEXT("!!! SetCommMask() and GetCommMask() mismatch on COM%u"), dwCommPort);
            FAIL("Mismatched comm masks");
            goto Error;
        }
    }
    
    LOG(TEXT("%s passed on COM%u"), lpFTE->lpDescription, dwCommPort);
    retVal = TPR_PASS;   
    
Error:    
    
    if(VALID_HANDLE(hCommPort))
    {
        CloseHandle(hCommPort);
    }
    
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_EscapeCommFunction(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;    
    DWORD dwCommPort = 0;
    UINT iCommFunction = 0;
    HANDLE hCommPort = INVALID_HANDLE_VALUE;

    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // not a multi-threaded test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = Util_QueryCommPortCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    if(-1==COMx)
    {
        dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
        
        if(!COMMask[dwCommPort])
                return TPR_PASS;
    }
    else
        dwCommPort = COMx;

    // open comm port
    //
    LOG(TEXT("%s on COM%u"), lpFTE->lpDescription, dwCommPort);
    hCommPort = Util_OpenCommPort(dwCommPort);
    if(INVALID_HANDLE(hCommPort))
    {
        ERRFAIL("Util_OpenCommPort()");
        goto Error;
    }

    // test every comm function
    //
    for(iCommFunction = 0; iCommFunction < NUM_COMM_FUNCTIONS; iCommFunction++)
    {
        LOG(TEXT("Testing EscapeCommFunction(%s) on COM%u..."), SZ_COMM_FUNCTIONS[iCommFunction], dwCommPort);
        __try
        {
            if(!EscapeCommFunction(hCommPort, COMM_FUNCTIONS[iCommFunction]))
            {
                LOG(TEXT("Failed EscapeCommFunction(%s) on COM%u"), SZ_COMM_FUNCTIONS[iCommFunction], dwCommPort);
                ERR("EscapeCommFunction()");
                //goto Error;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            LOG(TEXT("EscapeCommFunction() threw and exception on COM%u"), dwCommPort);
            FAIL("EscapeCommFunction()");
            goto Error;
        }       
    }

    LOG(TEXT("%s passed on COM%u"), lpFTE->lpDescription, dwCommPort);
    retVal = TPR_PASS;   
    
Error:    

    // clean up comm port
    //
    if(VALID_HANDLE(hCommPort))
    {
        CloseHandle(hCommPort);
        hCommPort = INVALID_HANDLE_VALUE;
    }

    return retVal;
}

// --------------------------------------------------------------------
DWORD WINAPI SendThread(LPVOID pData)
// --------------------------------------------------------------------
{
    BYTE buffer[WRITE_SIZE];
    DWORD dwBytesWritten = 0;
    HANDLE hCommPort = (HANDLE)pData;
    BOOL bRtn = TRUE;

    Sleep(5000);  
    __try
    {  
        bRtn =WriteFile(hCommPort, buffer, WRITE_SIZE, &dwBytesWritten, NULL);
        if(dwBytesWritten!=WRITE_SIZE || !bRtn)
        {
            ERRFAIL("WriteFile()");
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("WriteFile() threw an exception in SendThread()"));
    }
    ExitThread(0);
    return 0;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_EventTxEmpty(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;
    DWORD dwCommPort = 0;
    DWORD dwEventMask = 0;
    HANDLE hCommPort = INVALID_HANDLE_VALUE;
    HANDLE hThread = NULL;
    COMMTIMEOUTS    Cto;

    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // not a multi-threaded test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = Util_QueryCommPortCount();;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    
    if(-1==COMx)
    {
        dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

        if(!COMMask[dwCommPort])
           return TPR_PASS;
    }
    else
        dwCommPort = COMx;

    // open comm port
    //
    LOG(TEXT("%s on COM%u"), lpFTE->lpDescription, dwCommPort);
    hCommPort = Util_OpenCommPort(dwCommPort);
    if(INVALID_HANDLE(hCommPort))
    {
        ERRFAIL("Util_OpenCommPort()");
        goto Error;
    }

    // set-up TxEmpty event to wait on
    //
    LOG(TEXT("Calling SetCommMask(EV_TXEMPTY) on COM%u..."), dwCommPort);
    __try
    {        
        if(!SetCommMask(hCommPort, EV_TXEMPTY))
        {
            LOG(TEXT("SetCommMask() failed on COM%u"), dwCommPort);
            ERRFAIL("SetCommMask()");
            goto Error;
        }    
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("SetCommMask() threw an exception on COM%u"), dwCommPort);
        FAIL("SetCommMask()");
    }
    //Check timouts....if WriteTotalTimeoutConstant or WriteTotalTimeoutMultiplier, we will hang test
    LOG(TEXT("Calling GetCommTimeouts() on COM%u"), dwCommPort);
    __try
    {
    if(!GetCommTimeouts(hCommPort, &Cto))
        {
        LOG(TEXT("GetCommState() failed on COM%u"), dwCommPort);
       ERRFAIL("GetCommState()");
       goto Error;
       }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
        {
        LOG(TEXT("GetCommState() threw an exception on COM%u"), dwCommPort);
       ERRFAIL("GetCommState()");
       goto Error;
       }
    if(Cto.WriteTotalTimeoutConstant == 0 || Cto.WriteTotalTimeoutMultiplier == 0)
        {
        WARN("GetCommTimeouts()");
       LOG(TEXT("WARNING!! :: WriteTotalTimeoutConstant in serial driver on COM%u == 0.....may create undesired hangs in system"), dwCommPort);
       LOG(TEXT("Setting WriteTotalTimeoutConstant to 100 so test will not hang"));
       Cto.WriteTotalTimeoutConstant = DEF_CONST_TIMEOUT;
       Cto.WriteTotalTimeoutMultiplier = DEF_MULTIPLIER_TIMOUT;
       LOG(TEXT("Calling SetCommTimeouts() on COM%u"), dwCommPort);
       __try
           {
           if(!SetCommTimeouts(hCommPort, &Cto))
               {
               LOG(TEXT("SetCommTimeouts() failed on COM%u"), dwCommPort);
                     ERR("SetCommTimeouts()");
                     goto Error;
                     }
           }
       __except(EXCEPTION_EXECUTE_HANDLER)
           {
           LOG(TEXT("SetCommTimeouts() threw an exception on COM%u"), dwCommPort);
              ERRFAIL("SetCommTimeouts()");
              goto Error;
              }
        }
    // start data sending thread
    //
    LOG(TEXT("Creating thread to send data on COM%u..."), dwCommPort);
    hThread = CreateThread(NULL, 0, &SendThread, (LPVOID)hCommPort, 0, NULL);
    if(INVALID_HANDLE(hThread))
    {
        ERRFAIL("CreateThread()");
        goto Error;
    }

    // wait for thread to send all data
    //
    LOG(TEXT("Calling WaitCommEvent() on COM%u..."), dwCommPort);
    __try
    {
        if(!WaitCommEvent(hCommPort, &dwEventMask, NULL))
        {
            LOG(TEXT("WaitCommEvent() failed on COM%u"), dwCommPort);
            ERRFAIL("WaitCommEvent()");
            goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("WaitCommEvent() threw an exception on COM%u"), dwCommPort);
        FAIL("WaitCommEvent()");
        goto Error;
    }
    
    LOG(TEXT("Verifying proper event was returned on COM%u..."), dwCommPort);
    if(EV_TXEMPTY != dwEventMask)
    {
        LOG(TEXT("WaitCommEvent() returned incorrect event mask 0x%08x"), dwEventMask);
        FAIL("WatiCommEvent()");
        goto Error;
    }

    LOG(TEXT("%s passed on COM%u"), lpFTE->lpDescription, dwCommPort);
    retVal = TPR_PASS;   
    
Error:    

    // clean up thread
    //
    if(VALID_HANDLE(hThread))
    {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        hThread = NULL;
    }

    // clean up comm port
    //
    if(VALID_HANDLE(hCommPort))
    {
        CloseHandle(hCommPort);
        hCommPort = INVALID_HANDLE_VALUE;
    }
    
    return retVal;
}

// --------------------------------------------------------------------
DWORD WINAPI ClearEventMaskThread(LPVOID pData)
// --------------------------------------------------------------------
{
    HANDLE hCommPort = (HANDLE)pData;

    Sleep(5000);  
    __try
    {        
        if(!SetCommMask(hCommPort, 0))
        {
            ERRFAIL("SetComMask()");
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("SetComMask() threw an exception in ClearEventMaskThread()"));
    }
    ExitThread(0);
    return 0;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_WaitCommEvent(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;
    DWORD dwCommPort = 0;
    DWORD dwEventMask = 0;
    HANDLE hCommPort = INVALID_HANDLE_VALUE;
    HANDLE hThread = NULL;

    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // not a multi-threaded test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = Util_QueryCommPortCount();;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    
    if(-1==COMx)
    {
        dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
        
        if(!COMMask[dwCommPort])
                return TPR_PASS;
    }
    else
        dwCommPort = COMx;

    // open comm port
    //
    LOG(TEXT("%s on COM%u"), lpFTE->lpDescription, dwCommPort);
    hCommPort = Util_OpenCommPort(dwCommPort);
    if(INVALID_HANDLE(hCommPort))
    {
        ERRFAIL("Util_OpenCommPort()");
        goto Error;
    }

    // set-up TxEmpty event to wait on
    //
    LOG(TEXT("Calling SetCommMask(EV_RLSD|EV_RXCHAR|EV_RXFLAG|EV_ERR) on COM%u..."), dwCommPort);
    __try
    {        
        if(!SetCommMask(hCommPort, EV_RLSD|EV_RXCHAR|EV_RXFLAG|EV_ERR))
        {
            LOG(TEXT("SetCommMask() failed on COM%u"), dwCommPort);
            ERRFAIL("SetCommMask()");
            goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("SetCommMask() threw an exception on COM%u"), dwCommPort);
        FAIL("SetCommMask()");
    }
    // start data sending thread
    //
    LOG(TEXT("Creating thread to clear event mask on COM%u..."), dwCommPort);
    hThread = CreateThread(NULL, 0, &ClearEventMaskThread, (LPVOID)hCommPort, 0, NULL);
    if(INVALID_HANDLE(hThread))
    {
        ERRFAIL("CreateThread()");
        goto Error;
    }

    // wait for thread to send all data
    //
    LOG(TEXT("Calling WaitCommEvent() on COM%u..."), dwCommPort);
    __try
    {
        if(!WaitCommEvent(hCommPort, &dwEventMask, NULL))
        {
            LOG(TEXT("WaitCommEvent() failed on COM%u"), dwCommPort);
            ERRFAIL("WaitCommEvent()");
            goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("WaitCommEvent() threw an exception on COM%u"), dwCommPort);
        FAIL("WaitCommEvent()");
        goto Error;
    }
    
    LOG(TEXT("Verifying empty event mask was returned on COM%u..."), dwCommPort);
    if(0 != dwEventMask)
    {
        LOG(TEXT("WaitCommEvent() returned non-empty event mask 0x%08x"), dwEventMask);
        FAIL("WatiCommEvent()");
        goto Error;
    }

    LOG(TEXT("%s passed on COM%u"), lpFTE->lpDescription, dwCommPort);
    retVal = TPR_PASS;   
    
Error:    

    // clean up thread
    //
    if(VALID_HANDLE(hThread))
    {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        hThread = NULL;
    }

    // clean up comm port
    //
    if(VALID_HANDLE(hCommPort))
    {
        CloseHandle(hCommPort);
        hCommPort = INVALID_HANDLE_VALUE;
    }
    
    return retVal;
}

// --------------------------------------------------------------------
DWORD WINAPI CloseHandleThread(LPVOID pData)
// --------------------------------------------------------------------
{
    HANDLE hCommPort = (HANDLE)pData;

    __try
    {        
        if(!CloseHandle(hCommPort))
        {
            ERRFAIL("CloseHandle()");
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("CloseHandle() threw an exception in CloseHandleThread()"));
    }
    ExitThread(0);
    return 0;
}

DWORD WINAPI CloseHandleThreadSleep(LPVOID pData)
// --------------------------------------------------------------------
{
    HANDLE hCommPort = (HANDLE)pData;

    Sleep(5000);
    __try
    {        
        if(!CloseHandle(hCommPort))
        {
            ERRFAIL("CloseHandle()");
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("CloseHandle() threw an exception in CloseHandleThread()"));
    }
    ExitThread(0);
    return 0;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_WaitCommEventAndClose(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;
    DWORD dwCommPort = 0;
    DWORD dwEventMask = 0;
    HANDLE hCommPort = INVALID_HANDLE_VALUE;
    HANDLE hThread = NULL;

    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // not a multi-threaded test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = Util_QueryCommPortCount();;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    
    if(-1==COMx)
    {
        dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

        if(!COMMask[dwCommPort])
          return TPR_PASS;
    }
    else
        dwCommPort = COMx;

    // open comm port
    //
    LOG(TEXT("%s on COM%u"), lpFTE->lpDescription, dwCommPort);
    hCommPort = Util_OpenCommPort(dwCommPort);
    if(INVALID_HANDLE(hCommPort))
    {
        ERRFAIL("Util_OpenCommPort()");
        goto Error;
    }

    // set-up TxEmpty event to wait on
    //
    LOG(TEXT("Calling SetCommMask(EV_BREAK|EV_CTS|EV_DSR|EV_RING) on COM%u..."), dwCommPort);
    __try
    {        
        if(!SetCommMask(hCommPort, EV_BREAK|EV_CTS|EV_DSR|EV_RING))
        {
            LOG(TEXT("SetCommMask() failed on COM%u"), dwCommPort);
            ERRFAIL("SetCommMask()");
            goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("SetCommMask() threw an exception on COM%u"), dwCommPort);
        FAIL("SetCommMask()");
    }
    // start comm port closing thread
    //
    LOG(TEXT("Creating thread to close handle on COM%u..."), dwCommPort);
    hThread = CreateThread(NULL, 0, &CloseHandleThread, (LPVOID)hCommPort, 0, NULL);
    if(INVALID_HANDLE(hThread))
    {
        ERRFAIL("CreateThread()");
        goto Error;
    }

    // wait for thread to send all data
    //
    LOG(TEXT("Calling WaitCommEvent() on COM%u..."), dwCommPort);
    Sleep(5000);
    __try
    {
        if(WaitCommEvent(hCommPort, &dwEventMask, NULL))
        {
            LOG(TEXT("WaitCommEvent() succeeded on COM%u after closing handle"), dwCommPort);
            FAIL("WaitCommEvent() should have failed");
            goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("WaitCommEvent() threw an exception on COM%u"), dwCommPort);
        FAIL("WaitCommEvent()");
        goto Error;
    }       

    // thread closed the handle for us
    hCommPort = INVALID_HANDLE_VALUE;
    
    LOG(TEXT("%s passed on COM%u"), lpFTE->lpDescription, dwCommPort);
    retVal = TPR_PASS;   
    
Error:    

    // clean up thread
    //
    if(VALID_HANDLE(hThread))
    {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        hThread = NULL;
    }

    // clean up comm port
    //
    if(VALID_HANDLE(hCommPort))
    {
        CloseHandle(hCommPort);
        hCommPort = INVALID_HANDLE_VALUE;
    }
    
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_CommBreak(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    DWORD retVal = TPR_FAIL;
    DWORD dwCommPort = 0;
    HANDLE hCommPort = INVALID_HANDLE_VALUE;

    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // not a multi-threaded test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = Util_QueryCommPortCount();;
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    
    if(-1==COMx)
    {
        dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

        if(!COMMask[dwCommPort])
           return TPR_PASS;
    }
    else
        dwCommPort = COMx;

    // open comm port
    //
    LOG(TEXT("%s on COM%u"), lpFTE->lpDescription, dwCommPort);
    hCommPort = Util_OpenCommPort(dwCommPort);
    if(INVALID_HANDLE(hCommPort))
    {        
        ERRFAIL("Util_OpenCommPort()");
        goto Error;
    }

    // test SetCommBreak
    //
    LOG(TEXT("Calling SetCommBreak() on COM%u..."), dwCommPort);
    __try
    {
        if(!SetCommBreak(hCommPort))
        {
            LOG(TEXT("SetCommBreak() failed on COM%u"), dwCommPort);
            ERRFAIL("SetCommBreak()");
            goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("SetCommBreak() threw an exception on COM%u"), dwCommPort);
        FAIL("SetCommBreak()");
        goto Error;
    }

    // test ClearCommBreak
    //
    LOG(TEXT("Calling ClearCommBreak() on COM%u..."), dwCommPort);
    __try
    {
        if(!ClearCommBreak(hCommPort))
        {
            LOG(TEXT("ClearCommBreak() failed on COM%u"), dwCommPort);
            ERRFAIL("ClearCommBreak()");
            goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("ClearCommBreak() threw an exception on COM%u"), dwCommPort);
        FAIL("ClearCommBreak()");
        goto Error;
    }

    // log success
    //
    LOG(TEXT("%s passed on COM%u"), lpFTE->lpDescription, dwCommPort);
    retVal = TPR_PASS;   
    
Error:    

    // clean up comm port
    //
    if(VALID_HANDLE(hCommPort))
    {
        CloseHandle(hCommPort);
        hCommPort = INVALID_HANDLE_VALUE;
    }
    
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_RecvTimeout(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    DWORD retVal = TPR_FAIL;
    DWORD dwCommPort = 0;
    DWORD tStart = 0;
    INT tDelta = 0;
    DWORD dwBytesRead = 0;
    COMMTIMEOUTS cto = {0};
    HANDLE hCommPort = INVALID_HANDLE_VALUE;
    BYTE dummyBuffer[READ_SIZE] = {0};
    HANDLE hThread = NULL;
    
    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // not a multi-threaded test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = Util_QueryCommPortCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    
    if(-1==COMx)
    {
        dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

        if(!COMMask[dwCommPort])
           return TPR_PASS;
    }
    else
        dwCommPort = COMx;

    // open comm port
    //
    LOG(TEXT("%s on COM%u"), lpFTE->lpDescription, dwCommPort);
    hCommPort = Util_OpenCommPort(dwCommPort);
    if(INVALID_HANDLE(hCommPort))
    {        
        ERRFAIL("Util_OpenCommPort()");
        goto Error;
    }

    // set up comm timeouts
    cto.ReadIntervalTimeout = 0;
    cto.ReadTotalTimeoutMultiplier = DEF_MULTIPLIER_TIMOUT;
    cto.ReadTotalTimeoutConstant = DEF_CONST_TIMEOUT;

    __try
    {
        LOG(TEXT("Calling SetCommTimeouts() on COM%u"), dwCommPort);
        if(!SetCommTimeouts(hCommPort, &cto))
        {
            LOG(TEXT("SetCommTimeouts() failed on COM%u"), dwCommPort);
            ERRFAIL("SetCommTimeouts()");
            goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("SetCommTimeouts() threw an exception on COM%u"), dwCommPort);
        FAIL("SetCommTimeouts()");
        goto Error;
    }

    // start comm port closing thread
    //
    LOG(TEXT("Creating thread to close handle on COM%u..."), dwCommPort);
    hThread = CreateThread(NULL, 0, &CloseHandleThreadSleep, (LPVOID)hCommPort, 0, NULL);
    if(INVALID_HANDLE(hThread))
    {
        ERRFAIL("CreateThread()");
        goto Error;
    }

    __try
    {
        LOG(TEXT("Calling ReadFile() on COM%u"), dwCommPort);
        tStart = GetTickCount();

        if(ReadFile(hCommPort, dummyBuffer, READ_SIZE, &dwBytesRead, NULL) && dwBytesRead != 0)
        {
            LOG(TEXT("ReadFile() succeeded when it should have failed on COM%u"), dwCommPort);
            FAIL("Failed to test timeouts properly. Please re-run test");
            goto Error;
        }
        tStart = GetTickCount() - tStart;
        LOG(TEXT("ReadFile() timed out after %uMS. Expected it to timeout in %uMS"),
            tStart, DEF_CONST_TIMEOUT + (DEF_MULTIPLIER_TIMOUT * READ_SIZE));
        
        tDelta = (INT)tStart - (INT)(DEF_CONST_TIMEOUT + (DEF_MULTIPLIER_TIMOUT * READ_SIZE));
        tDelta = tDelta > 0 ? tDelta : -tDelta;
        if(tDelta > MAX_DELTA_TIMEOUT)
        {
            LOG(TEXT("ReadFile() timeout is not within tolerence of %uMS on COM%u"),
                MAX_DELTA_TIMEOUT, dwCommPort);
            FAIL("ReadFile()");
            goto Error;
        }
        LOG(TEXT("ReadFile() timeout is within tolerence of %uMS on COM%u"),
            MAX_DELTA_TIMEOUT, dwCommPort);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("ReadFile() threw an exception on COM%u"), dwCommPort);
        FAIL("ReadFile()");
        goto Error;
    }

    // log success
    //
    LOG(TEXT("%s passed on COM%u"), lpFTE->lpDescription, dwCommPort);
    retVal = TPR_PASS;   
    
Error:    

    // clean up comm port
    //
    if(VALID_HANDLE(hThread))
    {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        hThread = INVALID_HANDLE_VALUE;
    }

    // thread will close comm port
    // clean up comm port
    //
    if(VALID_HANDLE(hCommPort))
    {
        CloseHandle(hCommPort);
        hCommPort = INVALID_HANDLE_VALUE;
    }
    
    return retVal;
}

// --------------------------------------------------------------------
TESTPROCAPI 
Tst_SetBadCommDCB(
    UINT uMsg, 
    TPPARAM tpParam, 
    LPFUNCTION_TABLE_ENTRY lpFTE )
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);

    DWORD retVal = TPR_FAIL;  

    HANDLE hCommPort = INVALID_HANDLE_VALUE;
    DCB commDCBold = {0};
    DCB commDCBbad = {0};
    DWORD dwCommPort = 0;

    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
        // not a multi-threaded test
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = Util_QueryCommPortCount();
        retVal = SPR_HANDLED;
        goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
        retVal = TPR_NOT_HANDLED;
        goto Error;
    }

    
    if(-1==COMx)
    {
        dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

        if(!COMMask[dwCommPort])
           return TPR_PASS;
    }
    else
        dwCommPort = COMx;

    // open comm port
    //
    LOG(TEXT("%s on COM%u"), lpFTE->lpDescription, dwCommPort);
    hCommPort = Util_OpenCommPort(dwCommPort);
    if(INVALID_HANDLE(hCommPort))
    {        
        ERRFAIL("Util_OpenCommPort()");
        goto Error;
    }

    LOG(TEXT("Calling GetCommState() on COM%u"), dwCommPort);
    __try
    {
        if(!GetCommState(hCommPort, &commDCBold))
        {
            LOG(TEXT("GetCommState() failed on COM%u"), dwCommPort);
            ERRFAIL("GetCommState()");
            goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("GetCommState() threw an exception on COM%u"), dwCommPort);
        FAIL("GetCommState()");
        goto Error;
    }

    memcpy(&commDCBbad, &commDCBold, sizeof(DCB));

    // no comm port should support 0 baud rate and 0 byte size
    commDCBbad.BaudRate = 0;
    commDCBbad.ByteSize = 0;

    LOG(TEXT("Calling SetCommState() with invalid DCB on COM%u"), dwCommPort);
    __try
    {
        if(SetCommState(hCommPort, &commDCBbad))
        {
            LOG(TEXT("SetCommState() succeeded with bad DCB on COM%u"), dwCommPort);
            ERRFAIL("SetCommState()");
            goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("SetCommState() threw an exception on COM%u"), dwCommPort);
        FAIL("SetCommState()");
        goto Error;
    }


    // verify that the old DCB is still active
    LOG(TEXT("Calling GetCommState() on COM%u"), dwCommPort);
    __try
    {
        if(!GetCommState(hCommPort, &commDCBbad))
        {
            LOG(TEXT("GetCommState() failed on COM%u"), dwCommPort);
            ERRFAIL("GetCommState()");
            goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        LOG(TEXT("GetCommState() threw an exception on COM%u"), dwCommPort);
        FAIL("GetCommState()");
        goto Error;
    }

    if(0 != memcmp(&commDCBbad, &commDCBold, sizeof(DCB)))
    {
        LOG(TEXT("SetCommState() failed on bad DCB, but the old DCB was not maintainted on COM%u"),
            dwCommPort);
        FAIL("GetCommState()/SetCommState()");
        goto Error;
    }

    LOG(TEXT("%s passed on COM%u"), lpFTE->lpDescription, dwCommPort);
    retVal = TPR_PASS;
    
Error:    

    // clean up comm port
    //
    if(VALID_HANDLE(hCommPort))
    {
        CloseHandle(hCommPort);
        hCommPort = INVALID_HANDLE_VALUE;
    }

    return retVal;
}

TESTPROCAPI 
Tst_OpenCloseShare(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);
    HANDLE hCommPort1 = INVALID_HANDLE_VALUE;
    COMMPROP commProps = {0};
    DWORD retVal = TPR_SKIP;    
    TCHAR szCommPort[16] = {NULL};
    DWORD dwCommPort = 0;



    // process incoming tux messages (other than TPM_EXECUTE)
    //
    if( TPM_QUERY_THREAD_COUNT == uMsg )
    {
      // start a thread for each comm port
      ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = Util_QueryCommPortCount();
      retVal = SPR_HANDLED;
      goto Error;
    }
    else if( TPM_EXECUTE != uMsg )
    {
      retVal = TPR_NOT_HANDLED;
      goto Error;
    }    

    if(-1==COMx)
    {
        dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

        if(!COMMask[dwCommPort])
           return TPR_PASS;
    }
    else
    {
        dwCommPort = COMx;    
    }

    swprintf_s(szCommPort, _countof(szCommPort), TEXT("COM%u:"), dwCommPort);


    LOG(TEXT("Trying to open %s in share mode......"), szCommPort);
    hCommPort1 = CreateFile(szCommPort, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(INVALID_HANDLE(hCommPort1))
        {
         retVal = TPR_SKIP;
         ERR("INVALID_HANDLE on hCommPort1");
        goto Error;
       }

    LOG(TEXT("Calling GetCommProperties() on %s....."), szCommPort);
    __try
    {
    if(!GetCommProperties(hCommPort1, &commProps))
        {
        LOG(TEXT("GetCommProperties() failed "));
        ERRFAIL("GetCommProperties()");
        retVal = TPR_FAIL;
       goto Error;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
        {
        LOG(TEXT("GetCommProperties() threw an exception"));
        ERRFAIL("GetCommProperties()");
        goto Error;
        }
    LOG(TEXT("Closing COM port"));
    if(!CloseHandle(hCommPort1))
        {
        ERRFAIL("GetCommProperties()");
        retVal = TPR_FAIL;
        goto Error;
        }
    
   retVal = TPR_PASS;
 
Error:

     if(VALID_HANDLE(hCommPort1))
         {
         CloseHandle(hCommPort1);
            hCommPort1 = INVALID_HANDLE_VALUE;
            }

    return retVal;
}

inline BOOL SupportedDx(CEDEVICE_POWER_STATE dx, UCHAR deviceDx)
{
return (DX_MASK(dx) & deviceDx);
}

////////////////////////////////////////////////////////////////////////////////
// DeviceDx2String
//  Convert a bitmask Device Dx specification into a string containing D0, D1,
//  D2, D3, and D4 values as specified by the bitmask.
//
// Parameters:
//  deviceDx        Device Dx  itmask specifiying D0-D4 support.
//  szDeviceDx      Output string buffer.
//  cchDeviceDx     Length of szDeviceDx buffer, in WCHARs.
//
// Return value:
//  Pointer to the input buffer szDeviceDx. If the Device Dx bitmask specifies
//  no supported DXs, the returned string will be empty.

LPWSTR DeviceDx2String(UCHAR deviceDx, LPWSTR szDeviceDx, DWORD cchDeviceDx)
{
// start the string off empty (in case no DX are supported
VERIFY(SUCCEEDED(StringCchCopy(szDeviceDx, cchDeviceDx, L"")));
if(SupportedDx(D0, deviceDx))
    VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L"D0")));
if(SupportedDx(D1, deviceDx))
    VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L", D1")));
if(SupportedDx(D2, deviceDx))
    VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L", D2")));
if(SupportedDx(D3, deviceDx))
    VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L", D3")));
if(SupportedDx(D4, deviceDx))
    VERIFY(SUCCEEDED(StringCchCat(szDeviceDx, cchDeviceDx, L", D4")));

return szDeviceDx;
}


TESTPROCAPI 
Tst_PowerCaps(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
// --------------------------------------------------------------------
{
    UNREFERENCED_PARAMETER(lpFTE);
    DWORD dwPWRFlags = 0, retVal = TPR_PASS;
    HANDLE hCommPort = INVALID_HANDLE_VALUE;
    DWORD dwCommPort = 0, pbRet = 0, dwPWRRet = 0;
    POWER_CAPABILITIES pwrCaps = {0};
    CEDEVICE_POWER_STATE dxState, reqdxState;
    POWER_RELATIONSHIP pwrRelat;
    HANDLE hPwr = NULL;
    const UINT MAX_DEVICE_NAMELEN =    16;
    TCHAR szCommPort[MAX_DEVICE_NAMELEN] = TEXT("");
    const TCHAR SZ_COMM_PORT_FMT[] =    TEXT("COM%u:");
    WCHAR szDeviceDx[32];
    TCHAR szCurPwrState[MAX_PATH];
    GUID idGenericPMDeviceClass = { 0xA32942B7, 0x920C, 0x486b, { 0xB0, 0xE6, 0x92, 0xA7, 0x02, 0xA9, 0x9B, 0x35 } };
    BOOL b_AddInt = FALSE;

    if(TPM_QUERY_THREAD_COUNT == uMsg)
        {
        // start a thread for each comm port
           ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = Util_QueryCommPortCount();
           retVal = SPR_HANDLED;
           goto Error;
        }
    else if( TPM_EXECUTE != uMsg )
        {
           retVal = TPR_NOT_HANDLED;
           goto Error;
        }
    if(-1==COMx)
    {
        dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;
        
        if(!COMMask[dwCommPort])
           return TPR_PASS;
    }
    else
        dwCommPort = COMx;
    // open comm port
    LOG(TEXT("%s on COM%u"), lpFTE->lpDescription, dwCommPort);
    hCommPort = Util_OpenCommPort(dwCommPort);
    
    if(INVALID_HANDLE(hCommPort))
    {
        ERRFAIL("Util_OpenCommPort()");
        goto Error;
    }

    //Set the system power state to idle to acheive lower DX states
    dwPWRRet = GetSystemPowerState(szCurPwrState, MAX_PATH, &dwPWRFlags);
    LOG(TEXT("GetSystemPowerState. Current state is %s PWR Flags == %d, return == %d"), szCurPwrState, dwPWRFlags, dwPWRRet);
    if(0 != _tcscmp(szCurPwrState, _T("systemidle")))
    {
        dwPWRRet = SetSystemPowerState(NULL, POWER_STATE_IDLE, 0);
        LOG(TEXT("SetSystemPowerState POWER_STATE_IDLE %d"), dwPWRRet);
    }
    swprintf_s(szCommPort, _countof(szCommPort), SZ_COMM_PORT_FMT, dwCommPort);
    //clear buffer overrun
    szCommPort[MAX_DEVICE_NAMELEN-1]=NULL;
    //Advertise ICLASS to PM
    if(AdvertiseInterface(&idGenericPMDeviceClass,szCommPort,TRUE))
        {
        LOG(TEXT("        ---AdvertiseInterface passed!"));
        b_AddInt = TRUE;
        }

    //does the device support IOCTL_POWER_CAPABILITIES - required if PM support.
    LOG(TEXT("Calling IOCTL_POWER_CAPABILITIES %s"), szCommPort);
    if(!DeviceIoControl(hCommPort, IOCTL_POWER_CAPABILITIES, NULL, 0, &pwrCaps, sizeof(pwrCaps), &pbRet, NULL))
        {
        LOG(TEXT("---Could not get IOCTL_POWER_CAPABILITIES on COM%u"), dwCommPort);
        }
    else
        {
        //log the powercaps
        if(&pwrCaps)
            {
            LOG(TEXT("IOCTL_POWER_CAPABILTIES"));
            LOG(TEXT("    DeviceDx: %s"), DeviceDx2String(pwrCaps.DeviceDx, szDeviceDx, 32));
            LOG(TEXT("    WakeFomDx: %s"), DeviceDx2String(pwrCaps.WakeFromDx, szDeviceDx, 32));
            LOG(TEXT("    InrushDx: %s"), DeviceDx2String(pwrCaps.InrushDx, szDeviceDx, 32));
            for(dxState = D0; dxState < PwrDeviceMaximum; dxState = (CEDEVICE_POWER_STATE)((DWORD)dxState+ 1))
                {
                if(SupportedDx(dxState, pwrCaps.DeviceDx))
                    LOG(TEXT("    D%u - Power:       %u mW   - Latency:   %u mW"), dxState, pwrCaps.Power[dxState], pwrCaps.Latency[dxState]);
                }
            LOG(TEXT("    Flags: 0x%08x"), pwrCaps.Flags);
            }
            //does the device support IOCTL_POWER_SET - required if IOCTL_POWER_CAPABILTIES
            LOG(TEXT("Calling IOCTL_POWER_SET %s"), szCommPort);
            for(dxState = D0; dxState < PwrDeviceMaximum; dxState = (CEDEVICE_POWER_STATE)((DWORD)dxState + 1))
                {
                if(SupportedDx(dxState, pwrCaps.DeviceDx))
                    {
                    if(!DeviceIoControl(hCommPort, IOCTL_POWER_SET, NULL, 0, &dxState, sizeof(dxState), &pbRet, NULL))
                        {
                        LOG(TEXT("        ******Could not set IOCTL_POWER_SET D%u on COM%u GetLastError == %d"), dxState, dwCommPort, GetLastError());
                        retVal = TPR_FAIL;
                        }
                    else
                        LOG(TEXT("IOCTL_POWER_SET D%u on COM%u"), dxState, dwCommPort);
                    }
                }
            //does the device support IOCTL_POWER_GET - not required, btu serial supports
            LOG(TEXT("Calling IOCTL_POWER_GET %s"), szCommPort);
            for(dxState = D0; dxState < PwrDeviceMaximum; dxState = (CEDEVICE_POWER_STATE)((DWORD)dxState + 1))
                {
                if(SupportedDx(dxState, pwrCaps.DeviceDx))
                    {
                    if(!DeviceIoControl(hCommPort, IOCTL_POWER_GET, NULL, 0, &dxState, sizeof(dxState), &pbRet, NULL))
                        LOG(TEXT("        ---Could not get IOCTL_POWER_GET D%u on COM%u GetLastError == %d"), dxState, dwCommPort, GetLastError());
                    else
                        LOG(TEXT("IOCTL_POWER_GET D%u on COM%u"), dxState, dwCommPort);
                    }
                }
            //does the device support IOCTL_POWER_QUERY - PM depreciated, but serial supports this
            LOG(TEXT("Calling IOCTL_POWER_QUERY %s"), szCommPort);
            for(dxState = D0; dxState < PwrDeviceMaximum; dxState = (CEDEVICE_POWER_STATE)((DWORD)dxState + 1))
                {
                if(SupportedDx(dxState, pwrCaps.DeviceDx))
                    {
                    if(!DeviceIoControl(hCommPort, IOCTL_POWER_QUERY, &pwrRelat, sizeof(pwrRelat), &dxState, sizeof(dxState), &pbRet, NULL))
                        LOG(TEXT("        ---COM%u does not agree on IOCTL_POWER_QUERY for D%u GetLastError == %d"), dwCommPort, dxState, GetLastError());
                    else
                        LOG(TEXT("COM%u agrees with IOCTL_POWER_QUERY for D%u"), dwCommPort, dxState);
                    }
                }
            //does the device support IOCTL_REGISTER_POWER_RELATIONSHIP - only if POWER_CAP_PARENT set
            if((pwrCaps.Flags & POWER_CAP_PARENT) != 0)
                {
                LOG(TEXT("Calling IOCTL_REGISTER_POWER_RELATIONSHIP %s"), szCommPort);
                if(!DeviceIoControl(hCommPort, IOCTL_REGISTER_POWER_RELATIONSHIP, NULL, 0, NULL, 0, &pbRet, NULL))
                    LOG(TEXT("        ---IOCTL_REGISTER_POWER_RELATIONSHIP FAILED GetLastError == %d"), GetLastError());
                else
                    LOG(TEXT("%s supports IOCTL_REGISTER_POWER_RELATIONSHIP"), szCommPort);
                }
            //call Power Manager to set power requirements
            LOG(TEXT("Calling SetPowerRequirement %s"), szCommPort);
            for(dxState = D0; dxState < PwrDeviceMaximum; dxState = (CEDEVICE_POWER_STATE)((UINT)dxState + 1))
                {
                if(SupportedDx(dxState, pwrCaps.DeviceDx))
                    {
                    hPwr = SetPowerRequirement(szCommPort, dxState, POWER_NAME, NULL, 0);
                    if(!hPwr)
                        {
                        if(dwPWRRet == ERROR_INVALID_PARAMETER)
                            LOG(TEXT("        ---%s ERROR_INVALID_PARAMETER SetPowerRequirements for D%u"), szCommPort, dxState);
                        if(dwPWRRet == ERROR_NOT_ENOUGH_MEMORY)
                            LOG(TEXT("        ---%s ERROR_NOT_ENOUGH_MEMORY SetPowerRequirements for D%u"), szCommPort, dxState);
                        }
                    else
                        LOG(TEXT("SetPowerRequirements D%u on %s"), dxState, szCommPort);
                    }
                }
            LOG(TEXT("Calling DevicePowerNotify %s"), szCommPort);
            for(dxState = D0; dxState < PwrDeviceMaximum; dxState = (CEDEVICE_POWER_STATE)((DWORD)dxState + 1))
                {
                if(SupportedDx(dxState, pwrCaps.DeviceDx))
                    {
                    dwPWRRet = DevicePowerNotify(szCommPort, dxState, POWER_NAME);
                    if(dwPWRRet != ERROR_SUCCESS)
                        {
                        if(dwPWRRet == ERROR_FILE_NOT_FOUND)
                            LOG(TEXT("        ---%s failed on DevicePowerNotify D%u ERROR_FILE_NOT_FOUND"), szCommPort, dxState);
                        if(dwPWRRet == ERROR_WRITE_FAULT)
                            LOG(TEXT("        ---%s failed on DevicePowerNotify D%u ERROR_WRITE_FAULT"), szCommPort, dxState);
                        if(dwPWRRet == ERROR_INVALID_PARAMETER)
                            LOG(TEXT("        ---%s failed on DevicePowerNotify D%u ERROR_INVALID_PARAMETER"), szCommPort, dxState);
                        }
                    }
                }
            //can we set via SetDevicePower
            LOG(TEXT("Calling SetDevicePower %s"), szCommPort);
            for(dxState = D0; dxState < PwrDeviceMaximum; dxState = (CEDEVICE_POWER_STATE)((DWORD)dxState+ 1))
                {
                if(SupportedDx(dxState, pwrCaps.DeviceDx))
                    {
                    reqdxState = dxState;
                    dwPWRRet = SetDevicePower(szCommPort, POWER_NAME|POWER_FORCE, dxState);
                    if(dwPWRRet != ERROR_SUCCESS)
                        {
                        if(dwPWRRet == ERROR_FILE_NOT_FOUND)
                            LOG(TEXT("        ---%s failed on SetDevicePower D%u ERROR_FILE_NOT_FOUND"), szCommPort, dxState);
                        if(dwPWRRet == ERROR_WRITE_FAULT)
                            LOG(TEXT("        ---%s failed on SetDevicePower D%u ERROR_WRITE_FAULT"), szCommPort, dxState);
                        if(dwPWRRet == ERROR_INVALID_PARAMETER)
                            LOG(TEXT("        ---%s failed on SetDevicePower D%u ERROR_INVALID_PARAMETER"), szCommPort, dxState);
                        }
                    else
                        {
                        LOG(TEXT("SetDevicePower requested D%u, got D%u on %s"), reqdxState, dxState, szCommPort);
                        dwPWRRet = GetDevicePower(szCommPort, POWER_NAME, &dxState);
                        if(dwPWRRet != ERROR_SUCCESS)
                            LOG(TEXT("        ---%s failed on GetDevicePower D%u"), szCommPort, dxState);
                        else
                            {
                            if(reqdxState == dxState)
                                LOG(TEXT("GetDevicePower D%u on %s"), dxState, szCommPort);
                            else
                                LOG(TEXT("        ***GetDevicePower returned device state D%u, when last set state was D%u on %s"), dxState, reqdxState, szCommPort);
                            }
                        }
                    }
                }
            }
    Error:    

    if(VALID_HANDLE(hCommPort))
        {
           CloseHandle(hCommPort);
        }
    if(VALID_HANDLE(hPwr))
        ReleasePowerRequirement(hPwr);
    if(b_AddInt)
        AdvertiseInterface(&idGenericPMDeviceClass,szCommPort,FALSE);
    //}
    return retVal;

}

