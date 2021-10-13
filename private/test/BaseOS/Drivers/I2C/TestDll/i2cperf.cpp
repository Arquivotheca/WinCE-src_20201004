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
//  TESTI2C TUX DLL
//
//          Contains Performance Test cases and other related helper functions
//
//
////////////////////////////////////////////////////////////////////////////////
  
#include "i2ctest.h"            
#include "globals.h"


// Required by CEPerf
LPVOID pCePerfInternal;

HINSTANCE g_hLibPerfScenario = 0;
PFN_PERFSCEN_OPENSESSION  g_lpfnPerfScenarioOpenSession  = NULL;
PFN_PERFSCEN_CLOSESESSION g_lpfnPerfScenarioCloseSession = NULL;
PFN_PERFSCEN_ADDAUXDATA   g_lpfnPerfScenarioAddAuxData   = NULL;
PFN_PERFSCEN_FLUSHMETRICS g_lpfnPerfScenarioFlushMetrics = NULL;

// Keep track of how we're doing with PerfScenario.
PERFSCENARIO_STATUS g_PerfScenarioStatus = PERFSCEN_UNINITIALIZED;

// PERF_ITEMS1: Transition times
DWORD DEFAULT_PERF_FLAGS = CEPERF_RECORD_ABSOLUTE_TICKCOUNT |
                           CEPERF_RECORD_ABSOLUTE_PERFCOUNT |
                           CEPERF_DURATION_RECORD_MIN;
static CEPERF_BASIC_ITEM_DESCRIPTOR g_rgPerfItems1[] = {
    {
        INVALID_HANDLE_VALUE,
        CEPERF_TYPE_DURATION,
        L"Read Perf at Standard Speed",
        DEFAULT_PERF_FLAGS
    },
    {
        INVALID_HANDLE_VALUE,
        CEPERF_TYPE_DURATION,
        L"Read Perf at Fast Speed",
        DEFAULT_PERF_FLAGS
    },
    {
        INVALID_HANDLE_VALUE,
        CEPERF_TYPE_DURATION,
        L"Read Perf at High Speed",
        DEFAULT_PERF_FLAGS
    },
    {
        INVALID_HANDLE_VALUE,
        CEPERF_TYPE_DURATION,
        L"Write Perf at Standard Speed",
        DEFAULT_PERF_FLAGS
    },
    {
        INVALID_HANDLE_VALUE,
        CEPERF_TYPE_DURATION,
        L"Write Perf at Fast Speed",
        DEFAULT_PERF_FLAGS
    },
    {
        INVALID_HANDLE_VALUE,
        CEPERF_TYPE_DURATION,
        L"Write Perf at High Speed",
        DEFAULT_PERF_FLAGS
    }
};


WCHAR g_szPerfOutputFilename[MAX_PATH] = {0};


//---------------------------------------------------------------------------
// Purpose:    Check to see if file/directory exists
// Arguments:
//   __in  LPCTSTR pszFile - The full path you wish to check.
// Returns:
//   TRUE  if found
//   FALSE otherwise
//---------------------------------------------------------------------------
BOOL FileExists(LPCTSTR pszFile)
{
    DWORD attribs = GetFileAttributes(pszFile);
    BOOL result = (INVALID_FILE_ATTRIBUTES != attribs);
    return result;
}

//---------------------------------------------------------------------------
// Purpose:   Set the path to the output file.
// Arguments:
//   __out  TCHAR *pszRelease - Buffer to contain full output file path.
//  LPCSTR logFile - Name of Log File

// Returns:
//   Number of characters copied into the output filename buffer.
//---------------------------------------------------------------------------
DWORD SetOutputFile(__out_ecount(cchRelease) TCHAR *pszRelease, DWORD cchRelease, TCHAR* logFile)
{
    size_t copied = 0;
    *pszRelease = NULL;

    if(FileExists( RELEASE_PATH ))
    {
        g_pKato->Log(LOG_DETAIL, TEXT( "[SetOutputFile]:  Found Release dir\n" ));
        StringCchCopy(pszRelease, cchRelease, RELEASE_PATH);
    }

    HRESULT hr = StringCchCat(pszRelease, cchRelease, logFile);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL,
                     L"[SetOutputFile]: StringCchCat could not create perf output file name.");
        ASSERT(FALSE);
    }

    

    // update the char count
    hr = StringCchLength(pszRelease, MAX_PATH, &copied);
    if (FAILED(hr) || (0 == copied))
    {
        g_pKato->Log(LOG_FAIL,
                     L"[SetOutputFile]: Length of perf output file name is invalid.");
        ASSERT(FALSE);
    }

    g_pKato->Log(LOG_DETAIL, TEXT( "[SetOutputFile]:  output file path = {%s}\n" ), pszRelease );

    // Delete any existing output file, we want to start clean.
    // Ignore failure.
    DeleteFile(pszRelease);

    return copied;
}

//---------------------------------------------------------------------------
// Purpose:
//  Initialize PerfScenario function pointers.
//
// Arguments:
//  None.
//
// Returns:
//  TRUE on success; FALSE otherwise.
//---------------------------------------------------------------------------
BOOL InitPerfScenario(VOID)
{
    // If we're already initialized, just return.
    if (PERFSCEN_INITIALIZED == g_PerfScenarioStatus)
    {
        return TRUE;
    }

    // Load PerfScenario
    g_hLibPerfScenario = LoadLibrary(PERFSCENARIO_DLL);
    if (g_hLibPerfScenario)
    {
        g_lpfnPerfScenarioOpenSession  = reinterpret_cast<PFN_PERFSCEN_OPENSESSION>  (GetProcAddress(g_hLibPerfScenario, L"PerfScenarioOpenSession"));
        g_lpfnPerfScenarioCloseSession = reinterpret_cast<PFN_PERFSCEN_CLOSESESSION> (GetProcAddress(g_hLibPerfScenario, L"PerfScenarioCloseSession"));
        g_lpfnPerfScenarioAddAuxData   = reinterpret_cast<PFN_PERFSCEN_ADDAUXDATA>   (GetProcAddress(g_hLibPerfScenario, L"PerfScenarioAddAuxData"));
        g_lpfnPerfScenarioFlushMetrics = reinterpret_cast<PFN_PERFSCEN_FLUSHMETRICS> (GetProcAddress(g_hLibPerfScenario, L"PerfScenarioFlushMetrics"));

        if ((NULL == g_lpfnPerfScenarioOpenSession)  ||
            (NULL == g_lpfnPerfScenarioCloseSession) ||
            (NULL == g_lpfnPerfScenarioAddAuxData)   ||
            (NULL == g_lpfnPerfScenarioFlushMetrics))
        {
            g_PerfScenarioStatus = PERFSCEN_ABORT;
            g_pKato->Log(LOG_FAIL, L"GetProcAddress(PERFSCENARIO_DLL) FAILED.");

            BOOL bRet = FreeLibrary(g_hLibPerfScenario);
            if (!bRet)
            {
                g_pKato->Log(LOG_FAIL,
                             L"FreeLibrary(PERFSCENARIO_DLL) FAILED.  GLE=%#x",
                             GetLastError());
            }
            
        }
        else
        {
            g_PerfScenarioStatus = PERFSCEN_INITIALIZED;
        }
    }
    else
    {
        g_PerfScenarioStatus = PERFSCEN_ABORT;
        g_pKato->Log(LOG_FAIL, L"LoadLibrary(PERFSCENARIO_DLL) FAILED.  GLE=%#x", GetLastError());
    }

    return (PERFSCEN_INITIALIZED == g_PerfScenarioStatus) ? TRUE : FALSE;
}

//---------------------------------------------------------------------------
// Purpose:
//  Test to measure time taken by i2C to read data from a device register
//
// Arguments:
//  Standard Tux TESTPROC arguments.
//
// Returns:
//  TPR_PASS on pass, TPR_FAIL otherwise.
//---------------------------------------------------------------------------
TESTPROCAPI I2CReadPerfTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(tpParam);
    UNREFERENCED_PARAMETER(tpParam);
    
    DWORD dwRet = TPR_PASS; 
    
    DWORD dwSpeed = 0;
    DWORD dwSetSpeed = I2C_STANDARD_SPEED; // Default value of I2C Speed
    BYTE  byData = 0;

    HANDLE hPerfSession1 = INVALID_HANDLE_VALUE;

    GUID guidI2CPerf = {0};
    PERF_ITEM enPerfItem;
    CEPERF_ITEM_DATA ceperfItemData;
    LONG lReadDuration = 0;
    LONG lReadTotalDuration = 0;
    LONG lReadAveDuration = 0;
    DWORD dwReadThroughput = 0; // KBps
    LONG index = 0;

    // The shell doesn't necessarily want us to execute the test.
    // Make sure before executing.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    //-------------------------------------------------------------------------
    // 1. Do the necessary CEPerf/PerfScenario setup
    //-------------------------------------------------------------------------
    if (!InitPerfScenario())
    {
        return TPR_FAIL;
    }

    do
    {

        // Open CePerf session
        HRESULT hr = MDPGPerfOpenSession(&hPerfSession1, MDPGPERF_I2C);
        if (FAILED(hr))
        {
            g_pKato->Log(LOG_FAIL, L"MDPGPerfOpenSession FAILED - hr: %#x", hr);
            dwRet = TPR_FAIL;
            break;
        }

        // Register the performance items with CEPerf
        hr = CePerfRegisterBulk(hPerfSession1, g_rgPerfItems1, NUM_PERF_ITEMS1, 0);
        if (FAILED(hr))
        {
            g_pKato->Log(LOG_FAIL, L"CePerfRegisterBulk FAILED - hr: %#x", hr);
            dwRet = TPR_FAIL;
            break;
        }

        // Open PerfScenario session
        hr = g_lpfnPerfScenarioOpenSession(MDPGPERF_I2C, TRUE);
        if (FAILED(hr))
        {
            g_pKato->Log(LOG_FAIL, L"PerfScenarioOpenSession FAILED - hr: %#x", hr);
            g_PerfScenarioStatus = PERFSCEN_ABORT;
            dwRet = TPR_FAIL;
            break;
        }

        if(TestInit() && !gdwInitFailed)
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

            g_pKato->Log(LOG_DETAIL, L"Setting Bus Data for index, i=%d ...", index);
            if(SUCCESS != Hlp_SetupBusData(gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
                break;
            }
            
            g_pKato->Log(LOG_DETAIL, L"Setting Device Data for index, i=%d ...", index);
            if(SUCCESS!=Hlp_SetupDeviceData(gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
                break;
            }

            // Initialize the CEPERF Item Data Structure
            ceperfItemData.wVersion = 1;
            ceperfItemData.wSize = sizeof(ceperfItemData);

            // Perform Read operation at different i2C Speeds

            for(int i=0; i< SPEED_VALID_COUNT; i++)
            {
                dwSpeed = g_adwValidTransSpeed[i];

                g_pKato->Log(LOG_DETAIL, L"Setting the i2C speed: %d ...", dwSpeed);
                
                if(!Hlp_SetI2CSpeed(I2CHandleTable[gI2CTrans[index].HandleId].hI2C, dwSpeed))
                {
                    g_pKato->Log(LOG_FAIL, L"FAIL: Failed to set I2C transaction speed");
                    dwRet = TPR_FAIL;
                }

                lReadTotalDuration = 0;
                lReadAveDuration = 0;

                if(dwSetSpeed == I2C_HIGH_SPEED)
                {
                    enPerfItem = PERF_ITEM_DUR_READ_HIGH_SPEED;
                }
                else if(dwSetSpeed == I2C_FAST_SPEED)
                {
                    enPerfItem = PERF_ITEM_DUR_READ_FAST_SPEED;
                }
                else
                {
                    enPerfItem = PERF_ITEM_DUR_READ_STAN_SPEED;
                }
                    
                for(INT iter = 0; iter < NUM_PERF_MEASUREMENTS; iter++)
                {
                    g_pKato->Log(LOG_DETAIL, 
                                 L"ITERATION %d: Reading Data of length: %d from Device register %x  ...", 
                                 iter, 
                                 gI2CTrans[index].dwNumOfBytes, 
                                 gI2CTrans[index].dwRegisterAddress);

                    lReadDuration = 0;
                    
                    CePerfBeginDuration(g_rgPerfItems1[enPerfItem].hTrackedItem);
                    if(I2cRead(I2CHandleTable[gI2CTrans[index].HandleId].hI2C, gI2CTrans[index].dwRegisterAddress, gI2CTrans[index].BufferPtr, gI2CTrans[index].dwNumOfBytes))
                    {
                        CePerfEndDuration(g_rgPerfItems1[enPerfItem].hTrackedItem, &ceperfItemData );
                        lReadDuration = lReadDuration + (LONG)ceperfItemData.Duration.dwTickCount;
                        g_pKato->Log(LOG_DETAIL, L"Data read from device => ");
                        PrintBufferData(gI2CTrans[index].BufferPtr, gI2CTrans[index].dwNumOfBytes);
                    }
                    else
                    {
                        CePerfEndDuration(g_rgPerfItems1[enPerfItem].hTrackedItem, NULL );
                        g_pKato->Log(LOG_FAIL, L"FAIL: Failed to read data from device");
                        dwRet = TPR_FAIL;
                        break;
                    }

                    g_pKato->Log(LOG_DETAIL, TEXT("------------------------------------------------------------"));
                    g_pKato->Log(LOG_DETAIL, TEXT("ITERATION %d: Time taken for I2C Read transaction: %d ms"), iter, lReadDuration);
                    g_pKato->Log(LOG_DETAIL, TEXT("------------------------------------------------------------"));

                    lReadTotalDuration = lReadTotalDuration + lReadDuration;
                }

                if(lReadTotalDuration)
                {
                    lReadAveDuration = lReadTotalDuration / NUM_PERF_MEASUREMENTS;
                    dwReadThroughput = (gI2CTrans[index].dwNumOfBytes * NUM_PERF_MEASUREMENTS) / lReadTotalDuration ;

                    g_pKato->Log(LOG_DETAIL, TEXT("********************************************************************"));
                    g_pKato->Log(LOG_DETAIL, TEXT("Average Time taken for I2C Read transaction @ speed %d : %d ms"), dwSetSpeed, lReadAveDuration);
                    g_pKato->Log(LOG_DETAIL, TEXT("I2C Read throughput @ speed %d : %d KBps"), dwSetSpeed, dwReadThroughput);
                    g_pKato->Log(LOG_DETAIL, TEXT("********************************************************************"));
                }
                
            }

            
            
            //------------------------------------------------------------------------
            // Flush performance results.
            //------------------------------------------------------------------------
            g_pKato->Log(LOG_DETAIL, L"");
            g_pKato->Log(LOG_DETAIL, L"Flush results to log file.");
            guidI2CPerf = I2C_PERF_GUID;

            // Set the output filename once.
            if (0 == g_szPerfOutputFilename[0])
            {
                SetOutputFile(g_szPerfOutputFilename, _countof(g_szPerfOutputFilename), PERF_I2C_OUTFILE);
            }

            // Flush CePerf/PerfScenario data
            hr = g_lpfnPerfScenarioFlushMetrics(
                                                FALSE,
                                                &guidI2CPerf,
                                                PERF_I2C_NAMESPACE,
                                                lpFTE->lpDescription,
                                                g_szPerfOutputFilename,
                                                NULL,
                                                NULL
                                                );
            if(S_OK != hr)
            {
                g_PerfScenarioStatus = PERFSCEN_ABORT;
                g_pKato->Log(LOG_DETAIL,
                             L"Error calling PerfScenarioFlushMetrics. Results will not be written to XML");
            }

        }
        else
        {
            dwRet = TPR_SKIP;
        }
    }while(0);

    //------------------------------------------------------------------------
    // Tear down CePerf and PerfScenario sessions.
    //------------------------------------------------------------------------
    // Close the PerfScenario session
    HRESULT hr = g_lpfnPerfScenarioCloseSession(MDPGPERF_I2C);
    if (FAILED(hr))
    {
        g_PerfScenarioStatus = PERFSCEN_ABORT;
        g_pKato->Log(LOG_FAIL, L"PerfScenarioCloseSession FAILED - hr: %#x", hr);
        dwRet = TPR_FAIL;
    }

    // Close a handle to the library.
    if (g_hLibPerfScenario)
    {
        BOOL bRet = FreeLibrary(g_hLibPerfScenario);
        if (!bRet)
        {
            g_pKato->Log(LOG_FAIL,
                         L"FreeLibrary(PERFSCENARIO_DLL) FAILED.  GLE=%#x",
                         GetLastError());
            dwRet = TPR_FAIL;
        }
        g_PerfScenarioStatus = PERFSCEN_UNINITIALIZED;
    }

    // Deregister tracked items.
    hr = CePerfDeregisterBulk(g_rgPerfItems1, NUM_PERF_ITEMS1);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"CePerfDeregisterBulk FAILED - hr: %#x", hr);
        dwRet = TPR_FAIL;
    }

    // Close the CEPerf session.
    hr = CePerfCloseSession(hPerfSession1);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"CePerfCloseSession FAILED - hr: %#x", hr);
        dwRet = TPR_FAIL;
    }    
    
    // Close the i2c handle

    if(SUCCESS != Hlp_CloseI2CHandle(gI2CTrans, index))
    {
        dwRet=TPR_FAIL;
    }

    return dwRet;
}

//---------------------------------------------------------------------------
// Purpose:
//  Test to measure time taken by i2C to write data to a device register
//
// Arguments:
//  Standard Tux TESTPROC arguments.
//
// Returns:
//  TPR_PASS on pass, TPR_FAIL otherwise.
//---------------------------------------------------------------------------
TESTPROCAPI I2CWritePerfTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(tpParam);
    UNREFERENCED_PARAMETER(tpParam);
    
    DWORD dwRet = TPR_PASS; 
    
    DWORD dwSpeed = 0;
    DWORD dwSetSpeed = I2C_STANDARD_SPEED; // Default value of I2C Speed
    BYTE  byData = 0;

    HANDLE hPerfSession1 = INVALID_HANDLE_VALUE;

    GUID guidI2CPerf = {0};
    PERF_ITEM enPerfItem;
    CEPERF_ITEM_DATA ceperfItemData;
    LONG lWriteDuration = 0;
    LONG lWriteTotalDuration = 0;
    LONG lWriteAveDuration = 0;
    DWORD dwWriteThroughput = 0; // KBps
    LONG index = 0;

    // The shell doesn't necessarily want us to execute the test.
    // Make sure before executing.
    if(uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    //-------------------------------------------------------------------------
    // 1. Do the necessary CEPerf/PerfScenario setup
    //-------------------------------------------------------------------------
    if (!InitPerfScenario())
    {
        return TPR_FAIL;
    }

    do
    {

        // Open CePerf session
        HRESULT hr = MDPGPerfOpenSession(&hPerfSession1, MDPGPERF_I2C);
        if (FAILED(hr))
        {
            g_pKato->Log(LOG_FAIL, L"MDPGPerfOpenSession FAILED - hr: %#x", hr);
            dwRet = TPR_FAIL;
            break;
        }

        // Register the performance items with CEPerf
        hr = CePerfRegisterBulk(hPerfSession1, g_rgPerfItems1, NUM_PERF_ITEMS1, 0);
        if (FAILED(hr))
        {
            g_pKato->Log(LOG_FAIL, L"CePerfRegisterBulk FAILED - hr: %#x", hr);
            dwRet = TPR_FAIL;
            break;
        }

        // Open PerfScenario session
        hr = g_lpfnPerfScenarioOpenSession(MDPGPERF_I2C, TRUE);
        if (FAILED(hr))
        {
            g_pKato->Log(LOG_FAIL, L"PerfScenarioOpenSession FAILED - hr: %#x", hr);
            g_PerfScenarioStatus = PERFSCEN_ABORT;
            dwRet = TPR_FAIL;
            break;
        }
        
        if(TestInit() && !gdwInitFailed)
        {
            // Get the first READ transaction from the list of transactions
            if(SUCCESS!= Hlp_GetNextTransIndex(gI2CTrans, I2C_OP_WRITE, &index))
            {
                g_pKato->Log(LOG_DETAIL,L"Could not find any Read Transaction. Test Skipped");
                dwRet=TPR_SKIP;
                break;
            }
            
            g_pKato->Log(LOG_DETAIL,L"Found 1st READ Transaction at index, i=%d",index);
            PrintI2CData(gI2CTrans, index);

            
            g_pKato->Log(LOG_DETAIL, L"Setting Bus Data for index, i=%d ...", index);
            if(SUCCESS != Hlp_SetupBusData(gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
                break;
            }
            
            g_pKato->Log(LOG_DETAIL, L"Setting Device Data for index, i=%d ...", index);
            if(SUCCESS!=Hlp_SetupDeviceData(gI2CTrans, index))
            {
                dwRet=TPR_FAIL;
                break;
            }

            // Initialize the CEPERF Item Data Structure
            ceperfItemData.wVersion = 1;
            ceperfItemData.wSize = sizeof(ceperfItemData);

            // Perform Write operation at different i2C Speeds

            for(int i=0; i< SPEED_VALID_COUNT; i++)
            {
                dwSpeed = g_adwValidTransSpeed[i];

                g_pKato->Log(LOG_DETAIL, L"Setting the i2C speed: %d ...", dwSpeed);
                
                if(!Hlp_SetI2CSpeed(I2CHandleTable[gI2CTrans[index].HandleId].hI2C, dwSpeed))
                {
                    g_pKato->Log(LOG_FAIL, L"FAIL: Failed to set I2C transaction speed");
                    dwRet = TPR_FAIL;
                }

                lWriteTotalDuration = 0;
                lWriteAveDuration = 0;

                if(dwSetSpeed == I2C_HIGH_SPEED)
                {
                    enPerfItem = PERF_ITEM_DUR_WRITE_HIGH_SPEED;
                }
                else if(dwSetSpeed == I2C_FAST_SPEED)
                {
                    enPerfItem = PERF_ITEM_DUR_WRITE_FAST_SPEED;
                }
                else
                {
                    enPerfItem = PERF_ITEM_DUR_WRITE_STAN_SPEED;
                }
                    
                for(INT iter = 0; iter < NUM_PERF_MEASUREMENTS; iter++)
                {
                    g_pKato->Log(LOG_DETAIL, 
                                 L"ITERATION %d: Writing Data of length: %d to Device register %x  ...", 
                                 iter, 
                                 gI2CTrans[index].dwNumOfBytes, 
                                 gI2CTrans[index].dwRegisterAddress);

                    lWriteDuration = 0;

                    // Before writing new value to register, save the current value

                    g_pKato->Log(LOG_DETAIL,L"Reading the current value =>");
                    if(TRUE== I2cRead(I2CHandleTable[gI2CTrans[index].HandleId].hI2C, 
                                    gI2CTrans[index].dwRegisterAddress,
                                    &byData, 
                                    gI2CTrans[index].dwNumOfBytes))
                    {   
                        PrintBufferData(&byData, gI2CTrans[index].dwNumOfBytes);
                    }
                    else
                    {
                        g_pKato->Log(LOG_DETAIL,L"WARNING: Failed to read the current value of register");
                    }
                    
                    CePerfBeginDuration(g_rgPerfItems1[enPerfItem].hTrackedItem);
                    if(I2cWrite(I2CHandleTable[gI2CTrans[index].HandleId].hI2C, gI2CTrans[index].dwRegisterAddress, gI2CTrans[index].BufferPtr, gI2CTrans[index].dwNumOfBytes))
                    {
                        CePerfEndDuration(g_rgPerfItems1[enPerfItem].hTrackedItem, &ceperfItemData );
                        lWriteDuration = lWriteDuration + (LONG)ceperfItemData.Duration.dwTickCount;
                        g_pKato->Log(LOG_DETAIL, L"Data written to device => ");
                        PrintBufferData(gI2CTrans[index].BufferPtr, gI2CTrans[index].dwNumOfBytes);
                    }
                    else
                    {
                        CePerfEndDuration(g_rgPerfItems1[enPerfItem].hTrackedItem, NULL );
                        g_pKato->Log(LOG_FAIL, L"FAIL: Failed to write data to device");
                        dwRet = TPR_FAIL;
                        break;
                    }

                    // Restore the original value of the register
            
                    g_pKato->Log(LOG_DETAIL,L"Writing the original value =>");
                    PrintBufferData(&byData, gI2CTrans[index].dwNumOfBytes);
                    
                    if(TRUE != I2cWrite(I2CHandleTable[gI2CTrans[index].HandleId].hI2C, 
                            gI2CTrans[index].dwRegisterAddress,
                            &byData, 
                            gI2CTrans[index].dwNumOfBytes))
                    {
                        g_pKato->Log(LOG_DETAIL,L"WARNING: Failed to write the original value to register");
                    }

                    g_pKato->Log(LOG_DETAIL, TEXT("------------------------------------------------------------"));
                    g_pKato->Log(LOG_DETAIL, TEXT("ITERATION %d: Time taken for I2C Write transaction: %d ms"), iter, lWriteDuration);
                    g_pKato->Log(LOG_DETAIL, TEXT("------------------------------------------------------------"));

                    lWriteTotalDuration = lWriteTotalDuration + lWriteDuration;
                }

                if(lWriteTotalDuration)
                {
                    lWriteAveDuration = lWriteTotalDuration / NUM_PERF_MEASUREMENTS;
                    dwWriteThroughput = (gI2CTrans[index].dwNumOfBytes * NUM_PERF_MEASUREMENTS) / lWriteTotalDuration ;
                
                    g_pKato->Log(LOG_DETAIL, TEXT("********************************************************************"));
                    g_pKato->Log(LOG_DETAIL, TEXT("Average Time taken for I2C Write transaction @ speed %d : %d ms"), dwSetSpeed, lWriteAveDuration);
                    g_pKato->Log(LOG_DETAIL, TEXT("I2C Write throughput @ speed %d : %d KBps"), dwSetSpeed, dwWriteThroughput);
                    g_pKato->Log(LOG_DETAIL, TEXT("********************************************************************"));
                }
                
            }
            
            //------------------------------------------------------------------------
            // Flush performance results.
            //------------------------------------------------------------------------
            g_pKato->Log(LOG_DETAIL, L"");
            g_pKato->Log(LOG_DETAIL, L"Flush results to log file.");
            guidI2CPerf = I2C_PERF_GUID;

            // Set the output filename once.
            if (0 == g_szPerfOutputFilename[0])
            {
                SetOutputFile(g_szPerfOutputFilename, _countof(g_szPerfOutputFilename), PERF_I2C_OUTFILE);
            }

            // Flush CePerf/PerfScenario data
            hr = g_lpfnPerfScenarioFlushMetrics(
                                                FALSE,
                                                &guidI2CPerf,
                                                PERF_I2C_NAMESPACE,
                                                lpFTE->lpDescription,
                                                g_szPerfOutputFilename,
                                                NULL,
                                                NULL
                                                );
            if(S_OK != hr)
            {
                g_PerfScenarioStatus = PERFSCEN_ABORT;
                g_pKato->Log(LOG_DETAIL,
                             L"Error calling PerfScenarioFlushMetrics. Results will not be written to XML");
            }

        }
        else
        {
            dwRet = TPR_SKIP;
        }
    }while(0);


    //------------------------------------------------------------------------
    // Tear down CePerf and PerfScenario sessions.
    //------------------------------------------------------------------------
    // Close the PerfScenario session
    HRESULT hr = g_lpfnPerfScenarioCloseSession(MDPGPERF_I2C);
    if (FAILED(hr))
    {
        g_PerfScenarioStatus = PERFSCEN_ABORT;
        g_pKato->Log(LOG_FAIL, L"PerfScenarioCloseSession FAILED - hr: %#x", hr);
        dwRet = TPR_FAIL;
    }

    // Close a handle to the library.
    if (g_hLibPerfScenario)
    {
        BOOL bRet = FreeLibrary(g_hLibPerfScenario);
        if (!bRet)
        {
            g_pKato->Log(LOG_FAIL,
                         L"FreeLibrary(PERFSCENARIO_DLL) FAILED.  GLE=%#x",
                         GetLastError());
            dwRet = TPR_FAIL;
        }
        g_PerfScenarioStatus = PERFSCEN_UNINITIALIZED;
    }

    // Deregister tracked items.
    hr = CePerfDeregisterBulk(g_rgPerfItems1, NUM_PERF_ITEMS1);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"CePerfDeregisterBulk FAILED - hr: %#x", hr);
        dwRet = TPR_FAIL;
    }

    // Close the CEPerf session.
    hr = CePerfCloseSession(hPerfSession1);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"CePerfCloseSession FAILED - hr: %#x", hr);
        dwRet = TPR_FAIL;
    }    
    
    // Close the i2c handle

    if(SUCCESS != Hlp_CloseI2CHandle(gI2CTrans, index))
    {
        dwRet=TPR_FAIL;
    }

    return dwRet;
}

