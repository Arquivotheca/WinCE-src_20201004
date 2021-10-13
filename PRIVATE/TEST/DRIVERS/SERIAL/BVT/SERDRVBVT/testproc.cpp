// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
// Copyright (c) 2000 Microsoft Corporation.  All rights reserved.
//                                                                     
// --------------------------------------------------------------------
#include "TESTPROC.H"
#include "UTIL.H"

#define WRITE_SIZE              10
#define READ_SIZE               128

#define DEF_CONST_TIMEOUT       100
#define DEF_MULTIPLIER_TIMOUT   10
#define MAX_DELTA_TIMEOUT       5

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
    UINT iBaudRate = 0;
    UINT iDataBits = 0;
    UINT iParity = 0;
    UINT iStopBits = 0;
    DWORD dwCommPort = 0;
    DWORD dwBytesWritten = 0;
    DCB dcbCommPort = {0};

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

    dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

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
                        if(!WriteFile(hCommPort, junkBuffer, WRITE_SIZE, &dwBytesWritten, NULL))
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

    dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

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

    dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

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
                ERRFAIL("EscapeCommFunction()");
                goto Error;
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

    Sleep(5000);  
    __try
    {        
        if(!WriteFile(hCommPort, buffer, WRITE_SIZE, &dwBytesWritten, NULL))
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

    dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

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

    dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

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

    dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

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

    dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

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

    dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

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
    hThread = CreateThread(NULL, 0, &CloseHandleThread, (LPVOID)hCommPort, 0, NULL);
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

    dwCommPort = ((TPS_EXECUTE *)tpParam)->dwThreadNumber;

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
