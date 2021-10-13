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

/*++

Abstract:
 
    This file contains the implementation of the 
    performance tests used to test the serial port.
 
--*/

#define __THIS_FILE__ TEXT("perf.cpp")

#include "PSerial.h"
#include "GSerial.h"
#include <stdio.h>
#include "Comm.h"

#ifdef UNDER_CE
// For ceperf, etc.
#include <MDPGPerf.h> 
#endif //UNDER_CE

#ifdef UNDER_NT
#include <crtdbg.h>
#define ASSERT _ASSERT
#endif

#ifdef UNDER_CE
#define HIGHER_PRIORITY    (-1)
#else
#define HIGHER_PRIORITY    1
#endif //UNDER_CE


#ifdef UNDER_CE
// Required by CEPerf
LPVOID pCePerfInternal;

// We'll load PerfScenario explicitly
LPCTSTR                   PERFSCENARIO_DLL               = L"PerfScenario.dll";
HINSTANCE                 g_hLibPerfScenario             = 0;
PFN_PERFSCEN_OPENSESSION  g_lpfnPerfScenarioOpenSession  = NULL;
PFN_PERFSCEN_CLOSESESSION g_lpfnPerfScenarioCloseSession = NULL;
PFN_PERFSCEN_ADDAUXDATA   g_lpfnPerfScenarioAddAuxData   = NULL;
PFN_PERFSCEN_FLUSHMETRICS g_lpfnPerfScenarioFlushMetrics = NULL;

// Keep track of how we're doing with PerfScenario.dll
PERFSCENARIO_STATUS g_PerfScenarioStatus = PERFSCEN_UNINITIALIZED;

// Define the statistics we wish to capture
// PERF_ITEMS1: Transition times
enum {PERF_ITEM_DUR_SERIAL,
      PERF_ITEM_STA_SERIAL,
      NUM_PERF_ITEMS1};

// PERF_ITEMS1: Transition times
DWORD DEFAULT_PERF_FLAGS = CEPERF_RECORD_ABSOLUTE_TICKCOUNT;

static CEPERF_BASIC_ITEM_DESCRIPTOR g_rgPerfItems1[] = {
    {
        INVALID_HANDLE_VALUE,
        CEPERF_TYPE_DURATION,
        L"Time To Send (in microseconds)",
        DEFAULT_PERF_FLAGS | CEPERF_DURATION_RECORD_MIN
    },
    {
        INVALID_HANDLE_VALUE,
        CEPERF_TYPE_STATISTIC,
        L"Transfer Rate (in bits/seconds)",
        DEFAULT_PERF_FLAGS | CEPERF_STATISTIC_RECORD_SHORT
    }
};

// The following variables are used to determine where to put 
// the resulting performance output file.
WCHAR   g_szPerfOutputFilename[MAX_PATH] = {0};
#define PERF_OUTFILE            L"SerialPerformanceTests.xml"
#define PERF_SERIAL_NAMESPACE   L"PerfSerial"
#define RELEASE_PATH            L"\\release\\"

// Function Prototypes
BOOL InitPerfScenario(VOID);
DWORD SetOutputFile(__out_ecount(cchRelease) TCHAR *pszRelease, DWORD cchRelease);
#endif //UNDER_CE


/*++
 
SerialPerfTest:
 
Arguments:
 
    TUX standard arguments.
 
Return Value:
 
    TPR_HANDLED: for TPM_QUERY_THREAD_COUNT
    TPR_EXECUTE: for TPM_EXECUTE
    TPR_NOT_HANDLED: for all other messages.
 
Notes:
 
    
 
--*/


#define MAX_BUFFER 1024  // 1 KByte
#define TEST_TIME  1000*30  //30 sec   


TESTPROCAPI SerialPerfTest( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    BOOL    bRtn                        = FALSE;
    BOOL    bAck                        = FALSE;
    DWORD   dwResult                    = TPR_PASS;
    DWORD   dwIndex                     = 0;
    DWORD   dwStart                     = 0;
    DWORD   dwStop                      = 0;
    double  measuredBaudRate            = 0;
    CommPort* hCommPort                 = NULL;
    DWORD   dwNumOfBytesTransferred     = 0;
    DWORD   dwBytes                     = 0;
    DWORD   dwNumOfMilliSeconds         = 0;
    DWORD   dwNumOfBytes                = 0;
    BYTE    dataIntegrityResult         = DATA_INTEGRITY_FAIL;
    LPVOID  lpBuffer                    = NULL;
    LPBYTE  lpByte                      = NULL;
    DWORD   dwBaudRate                  = 0 ;
    DWORD   dwErrorCount                = 0;
    BOOL    bCollectPerfData            = TRUE;
#ifdef UNDER_CE
    DWORD   dwTestCount                 = 1;
    GUID    guidSerialPerf              = {0x29aba6fe, 0xebd6, 0x4ad8, { 0x92, 0x38, 0x96, 0xdc, 0x5c, 0x4e, 0x79, 0x44 }};
    HRESULT hr;
    HANDLE  hPerfSession1               = INVALID_HANDLE_VALUE;
#endif //UNDER_CE
        
    if( uMsg == TPM_QUERY_THREAD_COUNT )
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return SPR_HANDLED;
    }
    else if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    } // end if else if

    DWORD dwPerfDataFactor = g_PerfData;

    /*--------------------------------------------------------------------
        Sync the begining of the test.
      --------------------------------------------------------------------*/

    hCommPort = CreateCommObject(g_bCommDriver);
    FUNCTION_ERROR( hCommPort == NULL, return FALSE );
    hCommPort->CreateFile( g_lpszCommPort, 
                           GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           OPEN_EXISTING, 0, NULL );

    bRtn = BeginTestSync( hCommPort, lpFTE->dwUniqueID );
    DEFAULT_ERROR( FALSE == bRtn, return TPR_ABORT )

    bRtn = SetupDefaultPort( hCommPort );
    COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup );

    DCB mDCB;
    mDCB.DCBlength=sizeof(DCB);
    
    bRtn = GetCommState(hCommPort,&mDCB);
    COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

    switch (lpFTE->dwUserData) 
    {
        case 1:
        default:
            mDCB.BaudRate=CBR_9600;
            dwNumOfBytes = dwPerfDataFactor*MAX_BUFFER;
            dwBaudRate = 9600;
            break;
        case 2:
            mDCB.BaudRate=CBR_19200;
            dwNumOfBytes = dwPerfDataFactor*2*MAX_BUFFER;
            dwBaudRate = 19200;
            break;
        case 3:
            mDCB.BaudRate=CBR_38400;
            dwNumOfBytes = dwPerfDataFactor*4*MAX_BUFFER;
            dwBaudRate = 38400;
            break;
        case 4:
            mDCB.BaudRate=CBR_57600;
            dwNumOfBytes = dwPerfDataFactor*8*MAX_BUFFER;
            dwBaudRate = 57600;
            break;
        case 5:
            mDCB.BaudRate=CBR_115200;
            dwNumOfBytes = dwPerfDataFactor*16*MAX_BUFFER;
            dwBaudRate = 115200;
            break;
    }

    g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Data to be transferred = %d Bytes\n"), __THIS_FILE__, __LINE__,dwNumOfBytes );
    g_pKato->Log( LOG_DETAIL, TEXT("In %s @ line %d: Baud Rate for Supported Buses set to = %d\n"), __THIS_FILE__, __LINE__,dwBaudRate );

    if(!g_fDisableFlow)
    {
        //Enable CTS-RTS Handshaking.
        mDCB.fOutxCtsFlow = TRUE;
        mDCB.fRtsControl = RTS_CONTROL_HANDSHAKE;
    }

    bRtn=SetCommState(hCommPort,&mDCB);
    COMM_ERROR( hCommPort, FALSE == bRtn, dwResult = TPR_ABORT; goto DXTCleanup);

    //Initial Timeout
    COMMTIMEOUTS    cto;
    cto.ReadIntervalTimeout         =   TEST_TIME;
    cto.ReadTotalTimeoutMultiplier  =   10;  
    cto.ReadTotalTimeoutConstant    =   dwPerfDataFactor*TEST_TIME;
    cto.WriteTotalTimeoutMultiplier =   10; 
    cto.WriteTotalTimeoutConstant   =   dwPerfDataFactor*TEST_TIME; 

    bRtn = SetCommTimeouts( hCommPort, &cto );
    COMM_ERROR( hCommPort, FALSE == bRtn,dwResult=TPR_ABORT; goto DXTCleanup);

    //Allocate Buffer
    g_pKato->Log( LOG_DETAIL,
                 TEXT("In %s @ line %d:Expected to transfer %u bytes \n"),
                 __THIS_FILE__, __LINE__,dwNumOfBytes );
   
    lpBuffer = malloc(dwNumOfBytes);

    if (lpBuffer == NULL)
    {
        g_pKato->Log( LOG_FAIL,
                 TEXT("In %s @ line %d:Failed to Allocate memory for buffer\n"), __THIS_FILE__, __LINE__);
        
        bRtn = TRUE;
        dwResult = TPR_SKIP;
        goto DXTCleanup;
    }
    
    //clear the memory just in case
    memset(lpBuffer, 0,dwNumOfBytes); 

    if( g_fMaster )
    {
        //Intiailize the Buffer
        if(lpBuffer)
        {
            lpByte = (LPBYTE)lpBuffer;
        }
        
        for (dwIndex = 0; dwIndex <dwNumOfBytes; dwIndex++)
        {
            if(lpByte)
            {
                *lpByte = (BYTE)dwIndex%256;
                lpByte++;
             }
             else
             {
                g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Invalid buffer access \n"), __THIS_FILE__, __LINE__);
                dwResult = TPR_SKIP;
                goto DXTCleanup;
             }
        }
        g_pKato->Log( LOG_DETAIL, TEXT("MASTER:Start Write "));

#ifdef UNDER_CE
        //-------------------------------------------------------------------------
        // 1. Do the necessary CEPerf/PerfScenario setup
        //-------------------------------------------------------------------------
        if (!InitPerfScenario())
        {
            bCollectPerfData = FALSE;
        }

        if (bCollectPerfData)
        {
            // Open CePerf session
            hr = MDPGPerfOpenSession(&hPerfSession1, MDPGPERF_SERIAL);
            if (FAILED(hr))
            {
                g_pKato->Log(LOG_FAIL, L"MDPGPerfOpenSession FAILED - hr: %#x", hr);
                bCollectPerfData = FALSE;
            }
        }

        if (bCollectPerfData)
        {
            // Register the performance items with CEPerf
            hr = CePerfRegisterBulk(hPerfSession1, g_rgPerfItems1, NUM_PERF_ITEMS1, 0);
            if (FAILED(hr))
            {
                g_pKato->Log(LOG_FAIL, L"CePerfRegisterBulk FAILED - hr: %#x", hr);
                bCollectPerfData = FALSE;
            }
        }

        if (bCollectPerfData)
        {
            // Open PerfScenario session
            hr = g_lpfnPerfScenarioOpenSession(MDPGPERF_SERIAL, TRUE);
            if (FAILED(hr))
            {
                g_pKato->Log(LOG_FAIL, L"PerfScenarioOpenSession FAILED - hr: %#x", hr);
                g_PerfScenarioStatus = PERFSCEN_ABORT;
                bCollectPerfData = FALSE;
            }
        }

        if (bCollectPerfData)
        {
            // Add the number of bytes to be sent as Aux data to the XML file
            TCHAR szByte[10] = {0};
            _itot_s(dwNumOfBytes, szByte, 10, 10);
            hr = g_lpfnPerfScenarioAddAuxData(L"Bytes To Send", szByte);
            if (FAILED(hr))
            {
                g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData FAILED - hr: %#x", hr);
                bCollectPerfData = FALSE;
            }
        }
#endif //UNDER_CE

        dwStart = GetTickCount();

#ifdef UNDER_CE
        if (bCollectPerfData)
        {
            CePerfBeginDuration(g_rgPerfItems1[PERF_ITEM_DUR_SERIAL].hTrackedItem);
        }
#endif //UNDER_CE

        // Send data over the serial port
        bRtn = WriteFile(hCommPort,lpBuffer, dwNumOfBytes,&dwNumOfBytesTransferred, NULL);

#ifdef UNDER_CE
        if (bCollectPerfData)
        {
            CePerfEndDuration(g_rgPerfItems1[PERF_ITEM_DUR_SERIAL].hTrackedItem, NULL );
        }
#endif //UNDER_CE

        dwStop = GetTickCount();

#ifdef UNDER_CE
        if (bCollectPerfData)
        {
            CePerfAddStatistic(g_rgPerfItems1[PERF_ITEM_STA_SERIAL].hTrackedItem, ((dwNumOfBytesTransferred*8*1000)/(dwStop-dwStart)), NULL);
        }
#endif //UNDER_CE

        if (bRtn  == 0 || dwNumOfBytesTransferred != dwNumOfBytes) //We did not write any data
        {
            g_pKato->Log(LOG_FAIL,TEXT("Error !! In %s @ line %d:Expected to transfer %u bytes Actually sent %u bytes \n \n")
                                       ,__THIS_FILE__, __LINE__,dwNumOfBytes,dwNumOfBytesTransferred);
            dwResult = TPR_FAIL;
        }

        dwBytes = 0;
        dwErrorCount = 0 ;

        g_pKato->Log( LOG_DETAIL, TEXT("MASTER:Waiting for Data Integrity Result "));             

        while(dwErrorCount <10 )
        {
             //Wait For Integrity Result
            bRtn = ReadFile(hCommPort,&dataIntegrityResult, sizeof(dataIntegrityResult),&dwBytes, NULL);

            if (bRtn!=TRUE) 
            {
                g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:DataIntegrityResult Failed to receive Retry !!"),__THIS_FILE__, __LINE__);
                dwErrorCount++;
                continue;
            }

            if(dwBytes!=sizeof(dataIntegrityResult))
            {
                g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:DataIntegrityResult Failed:Expected %d Bytes  Received %d Bytes !!")
                              ,__THIS_FILE__, __LINE__,sizeof(dataIntegrityResult),dwBytes);
            }
            else
            {
                bAck = TRUE;
            }

            break;
        }
        
        if(bAck)
        {
            if(dataIntegrityResult & DATA_INTEGRITY_PASS )
            {
                g_pKato->Log( LOG_DETAIL,TEXT("DataIntegrityResult received  PASSED !!"));
            }
            else            
            {
                g_pKato->Log( LOG_FAIL,TEXT("DataIntegrityResult received  FAILED !!"));
                dwResult = TPR_FAIL;        
            }
        }
        else
        {
            dwResult = TPR_FAIL;
            g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:DataIntegrityResult Failed to receive Multiple retries "),__THIS_FILE__, __LINE__);
        }
    }

    // Slave
    else 
    {
        //Purging Comm Port before Reading 
        if(hCommPort)
        {
            PurgeComm( hCommPort, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR );
        }
                 
        dwStart = GetTickCount();
        bRtn = ReadFile(hCommPort,lpBuffer, dwNumOfBytes,&dwNumOfBytesTransferred, NULL);
        dwStop = GetTickCount();

        if (bRtn  == 0 || dwNumOfBytesTransferred == 0) //We did not write any data
        {
            g_pKato->Log(LOG_FAIL,TEXT("Error !! In %s @ line %d:Expected to Receive  %u bytes Actually sent %u bytes \n \n")
                         ,__THIS_FILE__, __LINE__,dwNumOfBytes,dwNumOfBytesTransferred);
            dwResult = TPR_FAIL;
        }
        
         //Check the data
         if(dwResult == TPR_PASS)
         {
            g_pKato->Log( LOG_DETAIL, TEXT("SLAVE: Received  %d Bytes" ),dwNumOfBytesTransferred);
            g_pKato->Log( LOG_DETAIL, TEXT("SLAVE:Checking Data Received"));
            lpByte = (LPBYTE)lpBuffer;
            ASSERT(dwNumOfBytesTransferred <=dwNumOfBytes);
        
            for (dwIndex = 0; dwIndex < dwNumOfBytesTransferred; dwIndex++)
            {
                if(lpByte)
                {
                    if (*lpByte != dwIndex%256)
                    {
                         g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:Data Mismatch! \n"),__THIS_FILE__, __LINE__);
                         g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:ERROR: Expecting %u, Got %u\n")
                                      ,__THIS_FILE__, __LINE__,dwIndex%256,*lpByte);
                         dwResult = TPR_FAIL;
                         break;
                    }
                    lpByte++;
                }
                else
                {
                     g_pKato->Log( LOG_FAIL,TEXT("In %s @ line %d:Invalid buffer access \n"), __THIS_FILE__, __LINE__);
                     dwResult = TPR_SKIP;
                     goto DXTCleanup;
                }    
            }

            if (dwIndex == dwNumOfBytesTransferred)
            {
                dataIntegrityResult = DATA_INTEGRITY_PASS;
                g_pKato->Log(LOG_DETAIL,TEXT("PASSED !! data integrity check"));
            }
            else
            {
                dataIntegrityResult = DATA_INTEGRITY_FAIL;
                g_pKato->Log(LOG_DETAIL,TEXT("FAILED !! data integrity check"));
            }
                
            //Report result to Master
            dwBytes = 0;
            bRtn = WriteFile(hCommPort,&dataIntegrityResult, sizeof(dataIntegrityResult),&dwBytes, NULL);
            if (bRtn == FALSE )
            {
                g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:Can not send reply to Master! \n"),__THIS_FILE__, __LINE__);
                g_pKato->Log(LOG_FAIL,TEXT("In %s @ line %d:ERROR: WriteFile Failed. Last Error was: %d\n")
                                               ,__THIS_FILE__, __LINE__,GetLastError());
                dwResult = TPR_ABORT;
            }
        }
    }

    /*    Report result if the test is not aborted or failed */
    if (dwResult == TPR_PASS)  
    {
        dwNumOfMilliSeconds = dwStop - dwStart; 

        if(dwNumOfMilliSeconds == 0 )
        {
           g_pKato->Log(LOG_DETAIL,TEXT("Transfer Took Less than 1 msec(Resolution of the GetTickCount()) Try Increasing the Data Payload"));
        }
        else
        {
#ifndef UNDER_NT
            measuredBaudRate = dwNumOfBytesTransferred *8*1000/(dwNumOfMilliSeconds) ; //in bits per sec
#else
            measuredBaudRate =(int)((double) (dwNumOfBytesTransferred*8*1000) /((double)(dwNumOfMilliSeconds))); //in bits per sec
#endif 
            g_pKato->Log(LOG_DETAIL,TEXT("Time elasped: %u msec"),dwNumOfMilliSeconds);
                           
//Tests run in NT env Generate floating point error             
#ifndef UNDER_NT            
            g_pKato->Log(LOG_DETAIL,TEXT("Measured transfer rate: %10.2f bits/sec"),(double)measuredBaudRate);
#else        
            g_pKato->Log(LOG_DETAIL,TEXT("Measured transfer rate: %10.2d bits/sec"),(int)measuredBaudRate);
#endif

#ifdef UNDER_CE
            if( g_fMaster && bCollectPerfData)
            {
                //------------------------------------------------------------------------
                // 4. Flush performance results.
                //------------------------------------------------------------------------
                g_pKato->Log(LOG_DETAIL, L"");
                g_pKato->Log(LOG_DETAIL, 
                             L"TEST #%d: Flush results to log file.", 
                             dwTestCount++);

                // Set the output filename once.
                if (0 == g_szPerfOutputFilename[0])
                {
                    SetOutputFile(g_szPerfOutputFilename, _countof(g_szPerfOutputFilename));
                }

                // Flush CePerf/PerfScenario data
                hr = g_lpfnPerfScenarioFlushMetrics(
                                                    FALSE,
                                                    &guidSerialPerf,
                                                    PERF_SERIAL_NAMESPACE,
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
#endif //UNDER_CE
        }
    }

    DXTCleanup:
    if( FALSE == bRtn)
        dwResult = TPR_ABORT;

    if(hCommPort)
    {
        delete hCommPort ;
        hCommPort = NULL;
    }

    if (lpBuffer)
    {
        free(lpBuffer);
        lpBuffer = NULL;
    }

#ifdef UNDER_CE
    if( g_fMaster )
    {
        //------------------------------------------------------------------------
        // 5. Tear down CePerf and PerfScenario sessions.
        //------------------------------------------------------------------------
        // Close the PerfScenario session
        hr = g_lpfnPerfScenarioCloseSession(MDPGPERF_SERIAL);
        if (FAILED(hr))
        {
            g_PerfScenarioStatus = PERFSCEN_ABORT;
            g_pKato->Log(LOG_FAIL, L"PerfScenarioCloseSession FAILED - hr: %#x", hr);
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
            }
            g_PerfScenarioStatus = PERFSCEN_UNINITIALIZED;
        }

        // Deregister tracked items.
        hr = CePerfDeregisterBulk(g_rgPerfItems1, NUM_PERF_ITEMS1);
        if (FAILED(hr))
        {
            g_pKato->Log(LOG_FAIL, L"CePerfDeregisterBulk FAILED - hr: %#x", hr);
        }

        // Close the CEPerf session.
        hr = CePerfCloseSession(hPerfSession1);
        if (FAILED(hr))
        {
            g_pKato->Log(LOG_FAIL, L"CePerfCloseSession FAILED - hr: %#x", hr);
        }
    }
#endif //UNDER_CE

   /* --------------------------------------------------------------------
        Sync the end of a test of the test.
    -------------------------------------------------------------------- */
    return EndTestSync( NULL, lpFTE->dwUniqueID, dwResult );
    
}// end SerialPerfTest( ... ) 

#ifdef UNDER_CE
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
// Returns:
//   Number of characters copied into the output filename buffer.
//---------------------------------------------------------------------------
DWORD SetOutputFile(__out_ecount(cchRelease) TCHAR *pszRelease, DWORD cchRelease)
{
    size_t copied = 0;
    *pszRelease = NULL;

    if(FileExists( RELEASE_PATH ))
    {
        g_pKato->Log(LOG_DETAIL, TEXT( "[SetOutputFile]:  Found Release dir\n" ));
        StringCchCopy(pszRelease, cchRelease, RELEASE_PATH);
    }

    HRESULT hr = StringCchCat(pszRelease, cchRelease, PERF_OUTFILE);
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
#endif //UNDER_CE