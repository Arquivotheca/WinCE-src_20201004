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
//******************************************************************************

/*++
Module Name:

PipeTest.cpp

Abstract:
    USB Driver Transfer Test.

Functions:

Notes:

--*/

#define __THIS_FILE__   TEXT("PerfTst.cpp")

#include <windows.h>
#include "usbperf.h"
#include "sysmon.h"
#include "Perftst.h"
#include <mdpgperf.h>
#include <guidgenerator.h>

#define TIMING_ROUNDS 50
#define MAX_RESULT_SIZE 200

//Set max transfer limit
#define MAX_TRANSFER_AMOUNT 0x60000

#define PERF_WAIT_TIME      20*1000

// next three macros shall be identical to those defined in 
// "<WinCEroot>\public\COMMON\oak\drivers\usb\hcd\USB20\ehci\td.h"
#define CHAIN_DEPTH 4
#define MAX_BLOCK_PAYLOAD 4096
#define MAX_QTD_PAYLOAD (5*MAX_BLOCK_PAYLOAD)
#define MAX_TRANSFER_BUFFIZE ((CHAIN_DEPTH*MAX_QTD_PAYLOAD)-MAX_BLOCK_PAYLOAD)

BOOL            g_bIsoch = FALSE;
static  UINT    g_uiRounds, g_uiCallbackCnt, g_uiGenericCallbackCnt, g_uiGenericTotalCnt;
static  HANDLE  g_hCompEvent = NULL, g_hGenericEvent = NULL, g_hIssueNextTransfer = NULL;
static  UINT    g_uiFullQueLimit = 0x7FFFFFFF;

extern  UINT    g_uiProfilingInterval;
extern  BOOL    g_fCeLog;
extern  HANDLE  g_hFlushEvent;

extern BOOL IsHighSpeed(UsbClientDrv* pClientDrv);


// To enable writing profile data to CELOG, set to PROFILE_CELOG
#define USE_PROFILE_CELOG   0 // PROFILE_CELOG


// Used by CePerf
LPVOID pCePerfInternal;

// PerfScenario entry points
extern PFN_PERFSCEN_OPENSESSION  g_lpfnPerfScenarioOpenSession;
extern PFN_PERFSCEN_CLOSESESSION g_lpfnPerfScenarioCloseSession;
extern PFN_PERFSCEN_ADDAUXDATA   g_lpfnPerfScenarioAddAuxData;
extern PFN_PERFSCEN_FLUSHMETRICS g_lpfnPerfScenarioFlushMetrics;
extern PERFSCENARIO_STATUS       g_PerfScenarioStatus;

// The output filename
WCHAR g_szPerfOutputFilename[MAX_PATH] = {0};


// Define the statistics we wish to capture
enum {PERF_ITEM_AVGTHROUGHPUT=0,
      PERF_ITEM_CPU_USAGE,
      PERF_ITEM_MEM_USAGE};

CEPERF_BASIC_ITEM_DESCRIPTOR g_rgPerfItems1[] = {
    {
        INVALID_HANDLE_VALUE,
        CEPERF_TYPE_STATISTIC,
        L"Average Throughput (bps)",
        CEPERF_STATISTIC_RECORD_MIN
    },
    {
        INVALID_HANDLE_VALUE,
        CEPERF_TYPE_STATISTIC,
        L"CPU Usage (%*1000)",
        CEPERF_STATISTIC_RECORD_MIN
    },
    {
        INVALID_HANDLE_VALUE,
        CEPERF_TYPE_STATISTIC,
        L"MEM Usage (%*1000)",
        CEPERF_STATISTIC_RECORD_MIN
    },
};
#define NUM_PERF_ITEMS1 (sizeof(g_rgPerfItems1) / sizeof(CEPERF_BASIC_ITEM_DESCRIPTOR))


// The following functions are used to determine where to put the resulting
// performance output file.
// GetVirtualRelease is part of qa.dll, which cannot be brought into kernel-mode
// because of its dependency on shlwapi.dll (user-mode).
#define RELEASE_PATH TEXT("\\release\\")

//---------------------------------------------------------------------------
// Purpose:    Check to see if file exists
// Arguments:
//   __in  LPCTSTR pszFile - The path to the file you wish to check.
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
// Purpose:    Return the path to a given file, with filename removed.
// Arguments:
//   __inout  LPTSTR pFile - The full path to a file, on return, contains
//                             the path without the filename.
// Returns:
//   TRUE
//---------------------------------------------------------------------------
BOOL WINAPI PathRemoveFileSpec(__inout_z LPTSTR pFile)
{
    LPTSTR pSZ = pFile;

    while (*pSZ != 0)
    {
        pSZ ++;
    }

    while (pSZ != pFile && *pSZ != TEXT('\\'))
    {
        pSZ --;
    }
    *pSZ = 0;

    return TRUE;
}

//---------------------------------------------------------------------------
// Purpose:   Set the path to the output file.
// Arguments:
//   __out  TCHAR *pszRelease - Buffer to contain full output file path.
// Returns:
//   Number of characters copied into the output buffer.
//---------------------------------------------------------------------------
DWORD SetOutputFile(__out_ecount(cchRelease) TCHAR *pszRelease, DWORD cchRelease)
{
    DWORD copied = 0;
    *pszRelease = NULL;

    if (FileExists(RELEASE_PATH))
    {
        g_pKato->Log(LOG_DETAIL, TEXT("[SetOutputFile]:  Found Release dir\n"));
        StringCchCopy(pszRelease, cchRelease, RELEASE_PATH);
    }

    StringCchCat(pszRelease, cchRelease, PERF_OUTFILE);

    // update the char count
    copied = _tcslen(pszRelease);

    g_pKato->Log(LOG_DETAIL, TEXT("[SetOutputFile]:  output file path = {%s}\n"), pszRelease);

    // Delete any existing output file, we want to start clean.
    // Ignore failure.
    DeleteFile(pszRelease);

    // This should be non-zero in all cases.
    ASSERT(copied);

    return copied;
}


//---------------------------------------------------------------------------
// Purpose: Calculate appropriate GUID for the test case.
//          We make use of GUIDGenerator functionality to hash our test case
//          name into a GUID.
//
// Parameters:    
//   __out GUID *out_pGuidUSBPerf - Pointer to the calculated GUID.
//   __in  DWORD in_szTestCase    - String to be hashed into a GUID.
//
// Returns:
//   VOID.  Actual return value is in out_guidUSBPerf.
//---------------------------------------------------------------------------
VOID GetTestGUID(GUID *out_pGuidUSBPerf,
                 LPTSTR in_szTestString)
{
    if(!GenerateGUID(in_szTestString, (BYTE*)out_pGuidUSBPerf, 16))
    {
        g_pKato->Log(LOG_DETAIL, L"GenerateGUID returned FALSE");
    }
}

//---------------------------------------------------------------------------
// Purpose: Concatenate a test param to the scenario name.
// Arguments:
//   __inout WCHAR inout_szScenarioName[] - String to concatenate to.
//   __in  DWORD in_cchScenarioName       - Size of string buffer
//   __in  LPCTSTR in_szName              - Name of parameter to concatenate.
//   __in  LPCTSTR in_szValue             - Value of parameter to concatenate.
//
// Returns:
//   VOID.  Actual return value is in inout_szScenarioName.
//---------------------------------------------------------------------------
VOID ConcatTestParamString(__inout_ecount_z(in_cchScenarioName) WCHAR inout_szScenarioName[],
                           DWORD   in_cchScenarioName,
                           LPCTSTR in_szName,
                           LPCTSTR in_szValue)
{
    HRESULT hr;
    LPCTSTR szSeparator        = TEXT("; ");
    LPCTSTR szOpenParenthesis  = TEXT("(");
    LPCTSTR szCloseParenthesis = TEXT(")");

    hr = StringCchCat(inout_szScenarioName, in_cchScenarioName, szSeparator);
    VERIFY(SUCCEEDED(hr));
    hr = StringCchCat(inout_szScenarioName, in_cchScenarioName, in_szName);
    VERIFY(SUCCEEDED(hr));
    hr = StringCchCat(inout_szScenarioName, in_cchScenarioName, szOpenParenthesis);
    VERIFY(SUCCEEDED(hr));
    hr = StringCchCat(inout_szScenarioName, in_cchScenarioName, in_szValue);
    VERIFY(SUCCEEDED(hr));
    hr = StringCchCat(inout_szScenarioName, in_cchScenarioName, szCloseParenthesis);
    VERIFY(SUCCEEDED(hr));
}


//---------------------------------------------------------------------------
// Purpose:   Generate a unique scenario name.
// Arguments:
//   __in  LPCTSTR out_szScenarioName       - Return buffer.
//   __in  DWORD   in_cchScenarioName       - Size of buffer in characters.
//   __in  LPCTSTR in_szTestDescription     - Original test name.
//   __in  DWORD   in_dwPacketSize          - Packetsize used in test
//   __in  DWORD   in_dwBlockSize           - Blocksize used in test
//   __in  BOOL    in_fExtendedPerfTestData - Extended perf test?
//   __in  DWORD   in_dwLargeTransferSize   - Size of large "blocked" transfer
//   __in  INT     in_iTiming               - If non-standard test, which
//                                            side is used for timing calc
//   __in  INT     in_iBlocking             - If non-standard test, which
//                                            side is blocking
//   __in  INT     in_iPhysMem              - If non-standard test, which
//                                            side(s) is using phys mem
// Returns:
//   VOID
//---------------------------------------------------------------------------
VOID GenerateScenarioName(__out_ecount(in_cchScenarioName) WCHAR out_szScenarioName[],
                          DWORD   in_cchScenarioName,
                          LPCTSTR in_szTestDescription,
                          DWORD   in_dwPacketSize,
                          DWORD   in_dwBlockSize,
                          BOOL    in_fExtendedPerfTestData,
                          DWORD   in_dwLargeTransferSize,
                          INT     in_iTiming,
                          INT     in_iBlocking,
                          INT     in_iPhysMem)
{
    HRESULT hr;
    const UINT STR_SIZE_16=16;
    WCHAR szTmpStr[STR_SIZE_16] = {0};


    // Initial name
    hr = StringCchCopy(out_szScenarioName, in_cchScenarioName, in_szTestDescription);
    VERIFY(SUCCEEDED(hr));

    // Packet size
    hr = StringCchPrintf(szTmpStr, _countof(szTmpStr), L"%d", in_dwPacketSize);
    VERIFY(SUCCEEDED(hr));
    ConcatTestParamString(out_szScenarioName, in_cchScenarioName, g_szPacketSize, szTmpStr);

    // Block size
    hr = StringCchPrintf(szTmpStr, _countof(szTmpStr), L"%d", in_dwBlockSize);
    VERIFY(SUCCEEDED(hr));
    ConcatTestParamString(out_szScenarioName, in_cchScenarioName, g_szBlockSize, szTmpStr);

    if (in_fExtendedPerfTestData)
    {
        // Large Transfer Size
        hr = StringCchPrintf(szTmpStr, _countof(szTmpStr), L"%d", in_dwLargeTransferSize);
        VERIFY(SUCCEEDED(hr));
        ConcatTestParamString(out_szScenarioName, in_cchScenarioName, g_szLargeTransferSize, szTmpStr);

        // Timing Calculation
        ConcatTestParamString(out_szScenarioName, in_cchScenarioName, g_szTiming, g_szTimingStrings[in_iTiming]);

        // Blocking Side
        ConcatTestParamString(out_szScenarioName, in_cchScenarioName, g_szBlocking, g_szBlockingStrings[in_iBlocking]);

        // Physical Memory Usage
        ConcatTestParamString(out_szScenarioName, in_cchScenarioName, g_szPhysMem, g_szPhysMemStrings[in_iPhysMem]);
    }
}

//---------------------------------------------------------------------------
// Purpose:   Outputs data via CePerf/Perfcenario
// Arguments:
//   __in  LPCTSTR in_szTestDescription      - Becomes scenario name.
//   __in  LPCTSTR in_szTransferType         - bulk/interrupt/isochronous
//   __in  DWORD   in_dwThoughput            - Throughput to report
//   __in  DWORD   in_dwCPUUsage             - CPU usage to report
//   __in  DWORD   in_dwMemUsage             - MEM usage to report
//   __in  DWORD   in_dwPacketSize           - Packetsize used in test
//   __in  DWORD   in_dwBlockSize            - Blocksize used in test
//   __in  BOOL    in_fOut                   - Is direction of test out/in
//   __in  INT     in_iTiming                - If non-standard test, which
//                                             side is used for timing calc
//   __in  INT     in_iBlocking              - If non-standard test, which
//                                             side is blocking
//   __in  INT     in_iPhysMem               - If non-standard test, which
//                                             side(s) is using phys mem
//   __in  INT     in_uiQparam               - only for Isoc -
//                                             microframe frequency
//   __in  INT     in_uiCallPrio             - for standard tests -
//                                             what is calling thread priority
//   __in  BOOL    in_fFullQue               - for standard tests -
//                                             "keep-queue-full" scenario
//   __in  BOOL    in_fHighSpeed             - Are we running HighSpeed or
//                                             FullSpeed?
// Returns:
//   VOID
//---------------------------------------------------------------------------
VOID OutputDataToPerfScenario(LPCTSTR in_szTestDescription,
                              LPCTSTR in_szTransferType,
                              DWORD   in_dwThoughput,
                              DWORD   in_dwCPUUsage,
                              DWORD   in_dwMemUsage,
                              DWORD   in_dwPacketSize,
                              DWORD   in_dwBlockSize,
                              BOOL    in_fOut,
                              BOOL    in_fExtendedPerfTestData,
                              DWORD   in_dwLargeTransferSize,
                              INT     in_iTiming,
                              INT     in_iBlocking,
                              INT     in_iPhysMem,
                              UINT    in_uiQparam, 
                              UINT    in_uiCallPrio,
                              BOOL    in_fFullQue,
                              BOOL    in_fHighSpeed)
{
    HRESULT hr;
    HANDLE hPerfSession1 = INVALID_HANDLE_VALUE;
    GUID guidUSBPerf     = {0};

    const UINT STR_SCENARIONAME_SIZE = 384;
    WCHAR szScenarioName[STR_SCENARIONAME_SIZE] = {0};

    const UINT STR_SIZE = 128;
    WCHAR szScenarioNamespace[STR_SIZE]         = {0};
    WCHAR szSessionNamespace[STR_SIZE]          = {0};


    // Check on the condition of PerfScenario.dll
    if (PERFSCEN_INITIALIZED != g_PerfScenarioStatus)
    {
        return;
    }

    // Need SessionNamespace to open CePerf and PerfScenario sessions.
    hr = StringCchPrintf(szSessionNamespace, _countof(szSessionNamespace), L"%s\\%s", MDPGPERF_USB, in_szTransferType);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"Could not create PerfScenario session namespace - hr: %#x", hr);
        return;
    }


    // Open CePerf session
    hr = MDPGPerfOpenSession(&hPerfSession1, szSessionNamespace);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"MDPGPerfOpenSession FAILED - hr: %#x", hr);
        goto PerfCleanup;
    }

    hr = CePerfRegisterBulk(hPerfSession1, g_rgPerfItems1, NUM_PERF_ITEMS1, 0);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"CePerfRegisterBulk FAILED - hr: %#x", hr);
        goto PerfCleanup;
    }

    // Open PerfScenario session
    hr = g_lpfnPerfScenarioOpenSession(szSessionNamespace, TRUE);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"PerfScenarioOpenSession FAILED - hr: %#x", hr);
        g_PerfScenarioStatus = PERFSCEN_ABORT;
        goto PerfCleanup;
    }


    // Set measured statistics
    hr = CePerfAddStatistic(g_rgPerfItems1[PERF_ITEM_AVGTHROUGHPUT].hTrackedItem,
                            in_dwThoughput, NULL);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"CePerfAddStatistic [PERF_ITEM_AVGTHROUGHPUT] FAILED - hr: %#x", hr);
    }

    hr = CePerfAddStatistic(g_rgPerfItems1[PERF_ITEM_CPU_USAGE].hTrackedItem,
                            in_dwCPUUsage, NULL);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"CePerfAddStatistic [PERF_ITEM_CPU_USAGE] FAILED - hr: %#x", hr);
    }

    hr = CePerfAddStatistic(g_rgPerfItems1[PERF_ITEM_MEM_USAGE].hTrackedItem,
                            in_dwMemUsage, NULL);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"CePerfAddStatistic [PERF_ITEM_MEM_USAGE] FAILED - hr: %#x", hr);
    }


    // Set auxiliary packet size data
    const UINT AUX_DATA_SIZE = 32;
    WCHAR szAuxData[AUX_DATA_SIZE] = {0};

    hr = StringCchPrintf(szAuxData,
                         AUX_DATA_SIZE,
                         L"%d",
                         in_dwPacketSize);
    VERIFY(SUCCEEDED(hr));
    hr = g_lpfnPerfScenarioAddAuxData(g_szPacketSize, szAuxData);
    if (FAILED(hr))
    {
        g_PerfScenarioStatus = PERFSCEN_ABORT;
        g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData [%s] FAILED - hr: %#x", g_szPacketSize, hr);
    }

    // Set auxiliary block size data
    hr = StringCchPrintf(szAuxData,
                         AUX_DATA_SIZE,
                         L"%d",
                         in_dwBlockSize);
    VERIFY(SUCCEEDED(hr));
    hr = g_lpfnPerfScenarioAddAuxData(g_szBlockSize, szAuxData);
    if (FAILED(hr))
    {
        g_PerfScenarioStatus = PERFSCEN_ABORT;
        g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData [%s] FAILED - hr: %#x", g_szBlockSize, hr);
    }

    // Set auxiliary transfer type data
    hr = g_lpfnPerfScenarioAddAuxData(g_szTransferType, in_szTransferType);
    if (FAILED(hr))
    {
        g_PerfScenarioStatus = PERFSCEN_ABORT;
        g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData [%s] FAILED - hr: %#x", g_szTransferType, hr);
    }

    // Set auxiliary direction data
    if (in_fOut)
    {
        hr = StringCchPrintf(szAuxData, AUX_DATA_SIZE, L"Out");
    }
    else
    {
        hr = StringCchPrintf(szAuxData, AUX_DATA_SIZE, L"In");
    }
    VERIFY(SUCCEEDED(hr));
    hr = g_lpfnPerfScenarioAddAuxData(g_szDirection, szAuxData);
    if (FAILED(hr))
    {
        g_PerfScenarioStatus = PERFSCEN_ABORT;
        g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData [%s] FAILED - hr: %#x", g_szDirection, hr);
    }

    // Set auxiliary speed data
    if (in_fHighSpeed)
    {
        hr = StringCchPrintf(szAuxData, AUX_DATA_SIZE, L"HighSpeed");
    }
    else
    {
        hr = StringCchPrintf(szAuxData, AUX_DATA_SIZE, L"FullSpeed");
    }
    VERIFY(SUCCEEDED(hr));
    hr = g_lpfnPerfScenarioAddAuxData(g_szSpeed, szAuxData);
    if (FAILED(hr))
    {
        g_PerfScenarioStatus = PERFSCEN_ABORT;
        g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData [%s] FAILED - hr: %#x", g_szSpeed, hr);
    }

    // If the tests are the non-standard tests, include even more auxilliary data.
    if (in_fExtendedPerfTestData)
    {
        // Large Transfer Size
        hr = StringCchPrintf(szAuxData,
                             AUX_DATA_SIZE,
                             L"%d",
                             in_dwLargeTransferSize);
        VERIFY(SUCCEEDED(hr));
        hr = g_lpfnPerfScenarioAddAuxData(g_szLargeTransferSize, szAuxData);
        if (FAILED(hr))
        {
            g_PerfScenarioStatus = PERFSCEN_ABORT;
            g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData [%s] FAILED - hr: %#x", g_szLargeTransferSize, hr);
        }

        // Transfer Time Calculation
        hr = g_lpfnPerfScenarioAddAuxData(g_szTiming, g_szTimingStrings[in_iTiming]);
        if (FAILED(hr))
        {
            g_PerfScenarioStatus = PERFSCEN_ABORT;
            g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData [%s] FAILED - hr: %#x", g_szTiming, hr);
        }

        // Blocking Side
        hr = g_lpfnPerfScenarioAddAuxData(g_szBlocking, g_szBlockingStrings[in_iBlocking]);
        if (FAILED(hr))
        {
            g_PerfScenarioStatus = PERFSCEN_ABORT;
            g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData [%s] FAILED - hr: %#x", g_szBlocking, hr);
        }

        // Physical Memory Usage
        hr = g_lpfnPerfScenarioAddAuxData(g_szPhysMem, g_szPhysMemStrings[in_iPhysMem]);
        if (FAILED(hr))
        {
            g_PerfScenarioStatus = PERFSCEN_ABORT;
            g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData [%s] FAILED - hr: %#x", g_szPhysMem, hr);
        }
    }
    else
    { 
        // QParam
        if (in_uiQparam > 0 && in_iPhysMem == 0)
        {
            // Mask off the appropriate bits of in_uiQparam indicating the microframe divisor.
            hr = g_lpfnPerfScenarioAddAuxData(g_szMfrFreq, g_szMfrFreqStrings[in_uiQparam&3]);
            if (FAILED(hr))
            {
                g_PerfScenarioStatus = PERFSCEN_ABORT;
                g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData [%s] FAILED - hr: %#x", g_szMfrFreq, hr);
            }
        }

        // Priority
        if (in_uiCallPrio > 0)
        {
            hr = g_lpfnPerfScenarioAddAuxData(g_szPriority, g_szPriorityStrings[in_uiCallPrio]);
            if (FAILED(hr))
            {
                g_PerfScenarioStatus = PERFSCEN_ABORT;
                g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData [%s] FAILED - hr: %#x", g_szPriority, hr);
            }
        }

        // FullQ
        hr = g_lpfnPerfScenarioAddAuxData(L"FullQueue", (in_fFullQue) ? L"TRUE" : L"FALSE");
        if (FAILED(hr))
        {
            g_PerfScenarioStatus = PERFSCEN_ABORT;
            g_pKato->Log(LOG_FAIL, L"PerfScenarioAddAuxData [FullQueue] FAILED - hr: %#x", hr);
        }
    }

    // Remember that the Scenario "Name" must be unique in the PerfScenario output file.
    // The Scenario "Namespace" can be the same for each test.
    // Unique test case names are the base for Scenario name.
    GenerateScenarioName(szScenarioName,
                         _countof(szScenarioName),
                         in_szTestDescription,
                         in_dwPacketSize,
                         in_dwBlockSize,
                         in_fExtendedPerfTestData,
                         in_dwLargeTransferSize,
                         in_iTiming,
                         in_iBlocking,
                         in_iPhysMem);

                         
    // Calculate appropriate GUID
    GetTestGUID(&guidUSBPerf, szScenarioName);
    g_pKato->Log(LOG_DETAIL, L"Test subcase \"%s\", GUID: %#x", szScenarioName, guidUSBPerf);
                
    hr = StringCchPrintf(szScenarioNamespace,
                         _countof(szScenarioNamespace),
                         L"%s\\%s",
                         PERF_SCENARIO_NAMESPACE,
                         in_szTestDescription);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_DETAIL, L"Could not create PerfScenario scenario namespace - hr: %#x", hr);
        return;
    }

    // Set the output filename once.
    if (g_szPerfOutputFilename[0] == 0)
    {
        SetOutputFile(g_szPerfOutputFilename, _countof(g_szPerfOutputFilename));
    }

    // Flush CePerf/PerfScenario data
    hr = g_lpfnPerfScenarioFlushMetrics(FALSE,
                                        &guidUSBPerf,
                                        szScenarioNamespace,
                                        szScenarioName,
                                        g_szPerfOutputFilename,
                                        NULL,
                                        NULL);
    if (S_OK != hr)
    {
        g_PerfScenarioStatus = PERFSCEN_ABORT;
        g_pKato->Log(LOG_DETAIL, L"Error calling PerfScenarioFlushMetrics. Results will not be written to XML");
    }


PerfCleanup:
    // Tear down CePerf and PerfScenario sessions.
    hr = CePerfDeregisterBulk(g_rgPerfItems1, NUM_PERF_ITEMS1);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"CePerfDeregisterBulk FAILED - hr: %#x", hr);
    }

    hr = CePerfCloseSession(hPerfSession1);
    if (FAILED(hr))
    {
        g_pKato->Log(LOG_FAIL, L"CePerfCloseSession FAILED - hr: %#x", hr);
    }

    hr = g_lpfnPerfScenarioCloseSession(szSessionNamespace);
    if (FAILED(hr))
    {
        g_PerfScenarioStatus = PERFSCEN_ABORT;
        g_pKato->Log(LOG_FAIL, L"PerfScenarioCloseSession FAILED - hr: %#x", hr);
    }
}

//---------------------------------------------------------------------------
// Purpose: Allocate physical memeory
// Returns:   PVOID; NULL for failure.
//---------------------------------------------------------------------------
PVOID AllocPMemory(PDWORD pdwPhAddr,PDWORD pdwPageLength)
{
    PVOID InDataBlock;
    DWORD dwPages = *pdwPageLength/PAGE_SIZE;

    if (*pdwPageLength == 0)
    {
        g_pKato->Log(LOG_FAIL, TEXT("AllocPMemory tried to allocate NO MEMORY\n"),  *pdwPageLength);
        return NULL;
    }
    if ((dwPages*PAGE_SIZE) < (*pdwPageLength))
        dwPages += 1;

    InDataBlock = AllocPhysMem(dwPages*PAGE_SIZE,
                               PAGE_READWRITE|PAGE_NOCACHE,
                               0,    // alignment
                               0,    // Reserved
                               pdwPhAddr);
    if (InDataBlock==NULL)
    {
        DEBUGMSG(DBG_ERR,(TEXT("AllocPMemory failed to allocate a memory page\n")));
        *pdwPageLength = 0;
    }
    else
    {
        *pdwPageLength = dwPages*PAGE_SIZE;
        DEBUGMSG(DBG_DETAIL,(TEXT(" AllocPMemory allocated %u bytes of Memory\n"), *pdwPageLength));
        g_pKato->Log(LOG_DETAIL, TEXT("AllocPMemory allocated %u bytes of Memory\n"), *pdwPageLength);
    }

    return InDataBlock;
}

//---------------------------------------------------------------------------
// Purpose:   Dispatch to Bulk/Int or Isoc.
// Arguments: 
//
// Returns:   TPR_XXX.
//---------------------------------------------------------------------------
DWORD
NormalPerfTests(UsbClientDrv *pDriverPtr,
                UCHAR uEPType,
                int iID,
                int iPhy,
                BOOL fOUT,
                int iTiming,
                int iBlocking,
                BOOL in_fHighSpeed,
                LPCTSTR in_szTestDescription)
{

//These priority levels (of Priority256 type) are used by device drivers.
//      160 is in the middle of the range for "real-time below" drivers
//      110 is the priority for USBAUD_THREAD_PRIORITY - Audio Function driver
//      96 is the lowest priority in the time-critical range
    
static UCHAR ucPrioLevels[4] = {NORMAL_PRIO,110,160,96}; // NORMAL_PRIO is 0, we never manipulate this value!
static TCHAR* pEndptTypes[4] = { TEXT("Control"), TEXT("Isoch"), TEXT("Bulk"), TEXT("Interrupt") }; 

    if (uEPType != USB_ENDPOINT_TYPE_BULK && uEPType != USB_ENDPOINT_TYPE_INTERRUPT && uEPType != USB_ENDPOINT_TYPE_ISOCHRONOUS)
    {
        g_pKato->Log(LOG_FAIL, TEXT("Invalid endpoint type: none of BULK, INTERRUPT, ISOCHRONOUS"));
        return TPR_SKIP;
    }

    //  0000 0000 0000 XqqQ 0000 0000 0000 bbbb
    UINT uiQparam   = iBlocking >> 17;
    UINT uiFullQue  = iBlocking & 0x010000;

    //  0000 0000 0000 XppX 0000 0000 0000 tttt
    UINT uiCallPrio = iTiming >> 17;
    UINT uiOrigPrio = THREAD_PRIORITY_ERROR_RETURN;

    // clear overloaded bits for these two params
    iTiming   &= 0xF;
    iBlocking &= 0xF;

    if (pDriverPtr == NULL || 
       (iTiming < HOST_TIMING || iTiming > RAW_TIMING) || 
       (iBlocking < HOST_BLOCK || iBlocking > BLOCK_NONE) || 
       (iPhy < 0 || iPhy > PHYS_MEM_ALL))
    {
        g_pKato->Log(LOG_FAIL, TEXT("Invalid parameter type: TIMING, BLOCKING or PHYSMEM invalid."));
        return TPR_SKIP;
    }

    if (iBlocking == BLOCK_NONE)
    {
        iBlocking = 0; // for non-special tests
    }

    if (uiCallPrio > CRIT_PRIO)
        uiCallPrio = NORMAL_PRIO;

    if (uiCallPrio != NORMAL_PRIO){
        uiOrigPrio = CeGetThreadPriority(GetCurrentThread());
        if (uiOrigPrio == THREAD_PRIORITY_ERROR_RETURN)
    {
            g_pKato->Log(LOG_FAIL, TEXT("Cannot get priority level of current thread"));
            return TPR_FAIL;
        }
    }

    TP_FIXEDPKSIZE  PerfResults = {0};
    PerfResults.uNumofSizes = NUM_BLOCK_SIZES;

    UCHAR uEPIndex = 0;
    //first find out the loopback pair's IN/OUT endpoint addresses
    while(g_pTstDevLpbkInfo[iID]->pLpbkPairs[uEPIndex].ucType != uEPType){
        uEPIndex ++;
        if (uEPIndex >= g_pTstDevLpbkInfo[iID]->uNumOfPipePairs)
        {
            g_pKato->Log(LOG_FAIL, TEXT("Can not find data pipe pair of the type %s "),  g_szEPTypeStrings[uEPType]);
            return TPR_SKIP;
        }
    }

    //then retrieve the corresponding endpoint structures
    LPCUSB_ENDPOINT pOUTEP = NULL;
    LPCUSB_ENDPOINT pINEP = NULL;

    for (UINT i = 0;i < pDriverPtr->GetNumOfEndPoints(); i++)
    {
        LPCUSB_ENDPOINT curPoint=pDriverPtr->GetDescriptorPoint(i);
        ASSERT(curPoint);
        if (curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uEPIndex].ucOutEp){
            pOUTEP = curPoint;
        }
        if (curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uEPIndex].ucInEp){
            pINEP = curPoint;
        }

    }
    if (pOUTEP == NULL || pINEP == NULL)  //one or both endpoints are missing
    {
        g_pKato->Log(LOG_FAIL, TEXT("Cannot find one or both endpoints of a Pipe Pair"));
        return TPR_SKIP;
    }

    // set thread priority, if necessary
    if (uiCallPrio != NORMAL_PRIO)
    {
        NKDbgPrintfW(TEXT("Boosting Thread Priority to %d\n"), ucPrioLevels[uiCallPrio]);

        // convert from 0,1,2,3 to 0, high, real-time below, real-time critical
        if (!CeSetThreadPriority(GetCurrentThread(),ucPrioLevels[uiCallPrio]))
        {
            g_pKato->Log(LOG_FAIL, TEXT("Cannot set desired thread priority level at %u"),ucPrioLevels[uiCallPrio]);
            return TPR_FAIL;
        }
    }

     
    DWORD dwResult = TPR_SKIP;
    DWORD dwSleepTime = 1000;
    PerfResults.usPacketSize = (pOUTEP->Descriptor.wMaxPacketSize & PACKETS_PART_MASK);

    NKDbgPrintfW(TEXT("Using %s endpoint at port #%i;  EA address = 0x%x, packetsize = %d, Blocks per uFrame %d\n"),
                pEndptTypes[uEPType],iID,pOUTEP->Descriptor.bEndpointAddress,(pOUTEP->Descriptor.wMaxPacketSize & PACKETS_PART_MASK),
                (pOUTEP->Descriptor.wMaxPacketSize & HIGHSPEED_MULTI_PACKETS_MASK) >> 11);

    //Set up the performance monitor code and Calibrate CPU throughput for 1000ms
    USBPerf_StartSysMonitor(2,1000);

    for(UINT uIndex = 0; uIndex<NUM_BLOCK_SIZES; uIndex++)
    {
        if (BLOCK_SIZES[uIndex] < (DWORD)(pOUTEP->Descriptor.wMaxPacketSize & (USHORT)PACKETS_PART_MASK) &&
           (uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS  || 0 != uiFullQue || iBlocking != 0))
        {
                continue;
        }

        if (BLOCK_SIZES[uIndex] < 256 && uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS && 0 != uiFullQue)
        {
                continue;
        }

        // The UHCI isochronous driver expects a number of frames less than
        // FRAME_LIST_LENGTH - ISOCH_TD_WAKEUP_INTERVAL (=1024-50).
        // In addition, the full-speed reflector endpoints for ISOCH are often 64 bytes (ex. see lpbkcfg1.dll).
        // This puts an upper limit on the block size we can use for the performance tests.
        // Thus, we'll cap full-speed ISOCH perf testing to a max block size of 16384 bytes.
        if ((BLOCK_SIZES[uIndex] > 16384)  &&
            (uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS) &&
            !IsHighSpeed(pDriverPtr) && 
            (64 <= (pOUTEP->Descriptor.wMaxPacketSize & (USHORT)PACKETS_PART_MASK)))
        {
                continue;
        }

        PerfResults.UnitTPs[uIndex].dwBlockSize = BLOCK_SIZES[uIndex];

        UINT    uiMaxPacketRatio = BLOCK_SIZES[uIndex] / (pOUTEP->Descriptor.wMaxPacketSize & PACKETS_PART_MASK);
        UINT    uiRounds = 64;

        if ((pOUTEP->Descriptor.wMaxPacketSize & (USHORT)PACKETS_PART_MASK) >= 1024)
        {
            if (uiMaxPacketRatio >= 1)
                uiRounds = 32;
            if (uiMaxPacketRatio >= 4)
                uiRounds = 16;
            if (uiMaxPacketRatio >= 8)
                uiRounds = 8;
            if (uiMaxPacketRatio >= 32)
                uiRounds = 2;
        }
        else if ((pOUTEP->Descriptor.wMaxPacketSize & (USHORT)PACKETS_PART_MASK) >= 256)
        {
            if (uiMaxPacketRatio >= 2)
                uiRounds = 32;
            if (uiMaxPacketRatio >= 4)
                uiRounds = 16;
            if (uiMaxPacketRatio >= 8)
                uiRounds = 8;
            if (uiMaxPacketRatio >= 16)
                uiRounds = 4;
            if (uiMaxPacketRatio >= 32)
                uiRounds = 2;
        }
        else if ((pOUTEP->Descriptor.wMaxPacketSize & (USHORT)PACKETS_PART_MASK) >= 64)
        {
            if (uiMaxPacketRatio >= 4)
                uiRounds = 32;
            if (uiMaxPacketRatio >= 8)
                uiRounds = 16;
            if (uiMaxPacketRatio >= 16)
                uiRounds = 8;
            if (uiMaxPacketRatio >= 32)
                uiRounds = 2;
        }
        else
        {
            if (uiMaxPacketRatio >= 4)
                uiRounds = 32;
            if (uiMaxPacketRatio >= 8)
                uiRounds = 16;
            if (uiMaxPacketRatio >= 16)
                uiRounds = 8;
            if (uiMaxPacketRatio >= 32)
                uiRounds = 2;
        }

        UINT uiFullQueLimit = 0;

        if (0 != uiFullQue)
        {
            // If synchronous completion is set, clear this limit
           
            if (RAW_TIMING != iTiming)
            {
                // set the full queue limit at the calculated value
                uiFullQueLimit = uiRounds;
            }

            uiRounds = MAX_TRANSFER_AMOUNT / BLOCK_SIZES[uIndex];
        }

        if (g_fCeLog)
        {
            // flush buffer from any records gathered during the pause
            if (NULL != g_hFlushEvent){
                SetEvent(g_hFlushEvent);
                Sleep(500);
            }
            CeLogMsg(TEXT(">>>USBPerf: scenario begin, %u*%u."),BLOCK_SIZES[uIndex],uiRounds);
        }

        if (uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS){
            g_bIsoch = TRUE;
            dwResult = Isoch_Perf(pDriverPtr, pOUTEP, pINEP, 
                                  BLOCK_SIZES[uIndex], uiRounds, 
                                  (iPhy == PHYS_MEM_HOST), // BOOL: either HOST or none
                                  &(PerfResults.UnitTPs[uIndex]), fOUT, 
                                  iTiming,
                                  uiQparam, uiCallPrio, uiFullQueLimit,
                                  in_fHighSpeed, in_szTestDescription);
            g_bIsoch = FALSE;
        }
        else 
        {
            g_bIsoch = FALSE;
            dwResult = BulkIntr_Perf(pDriverPtr, pOUTEP, pINEP, 
                                     uEPType, // bulk or interrupt
                                     BLOCK_SIZES[uIndex], uiRounds, 
                                     iPhy, 
                                     &(PerfResults.UnitTPs[uIndex]), fOUT, 
                                     iTiming, iBlocking, 
                                     uiCallPrio, uiFullQueLimit,
                                     in_fHighSpeed, in_szTestDescription);
        }

        if (g_fCeLog){
            CeLogMsg(TEXT("<<<USBPerf: scenario end, %u*%u."),BLOCK_SIZES[uIndex],uiRounds);
            if (NULL != g_hFlushEvent)
            {
                SetEvent(g_hFlushEvent);
               
            }
        }
        Sleep(dwSleepTime);
    } 

    USBPerf_StopSysMonitor();

    // restore thread priority, if necessary
    if (uiCallPrio != NORMAL_PRIO)
    {
        if (!CeSetThreadPriority(GetCurrentThread(),uiOrigPrio)){
            g_pKato->Log(LOG_FAIL, TEXT("Cannot restore original thread priority level at %u"),uiOrigPrio);
            return TPR_FAIL;
        }
    }

    PrintOutPerfResults(PerfResults,in_szTestDescription);

    return dwResult;
}


//---------------------------------------------------------------------------
// Purpose:   All Bulk/Interrupt tests.
// Arguments: many
//
// Returns:   TPR_XXX.
//---------------------------------------------------------------------------

DWORD  BulkIntr_Perf(UsbClientDrv *pDriverPtr, 
                     LPCUSB_ENDPOINT lpOUTPipePoint, 
                     LPCUSB_ENDPOINT lpINPipePoint,
                     UCHAR uType, 
                     DWORD dwBlockSize, 
                     UINT  uiRounds, 
                     int iPhy, 
                     PONE_THROUGHPUT pTP,
                     BOOL fOUT, 
                     int iTiming, 
                     int iBlocking,
                     //UINT uiQparam, 
                     UINT uiCallPrio,
                     UINT uiFullQueLimit,
                     BOOL in_fHighSpeed,
                     LPCTSTR in_szTestDescription)
{
    if (pDriverPtr == NULL || lpOUTPipePoint == NULL || lpINPipePoint == NULL || /*uiQparam > 1 ||*/
       uiRounds == 0 || uiRounds > 65535 || dwBlockSize < 0 || dwBlockSize > 1024*1024) //prefast cautions
    {
        g_pKato->Log(LOG_FAIL, TEXT("Invalid parameter(s) - skipping test."));
        return TPR_SKIP;
    }

    TCHAR szTestType[14] = {0};
    if (uType == USB_ENDPOINT_TYPE_BULK)
    {
        wcscpy_s(szTestType, _countof(szTestType), TEXT("Bulk"));
    }
    else
    {
        wcscpy_s(szTestType, _countof(szTestType), TEXT("Interrupt"));
    }

    TIME_REC  TimeRec = {0};
    if (QueryPerformanceFrequency(&TimeRec.perfFreq) == FALSE || TimeRec.perfFreq.QuadPart == 0)
    {
        TimeRec.perfFreq.QuadPart = 1000; //does not support high resolution
    }
    NKDbgPrintfW(TEXT("Performance Frequency is: %d"), TimeRec.perfFreq.QuadPart);

    LARGE_INTEGER   liTimingAPI;
    LARGE_INTEGER   liTimeIssueTransferAPI;
    UINT            uiNumIssueTransferAPI = 0;
    liTimeIssueTransferAPI.QuadPart = 0;

    LPCUSB_FUNCS        lpDrvUsbFxns = pDriverPtr->lpUsbFuncs;
    USB_HANDLE          usbHandle    = pDriverPtr->GetUsbHandle();
    LPCUSB_ENDPOINT     lpPipePoint  = fOUT?lpOUTPipePoint:lpINPipePoint;
    USB_PIPE            hResultPipe  = NULL;
    USB_PIPE            hPipe;

    hPipe = (*(lpDrvUsbFxns->lpOpenPipe))(usbHandle, &(lpPipePoint->Descriptor));
    if (hPipe==NULL){
        g_pKato->Log(LOG_FAIL, TEXT("Error at Open Pipe - skipping test."));
        return TPR_FAIL;
    }
    if (iTiming == DEV_TIMING || iTiming == SYNC_TIMING)
    {
        hResultPipe = fOUT?(*(lpDrvUsbFxns->lpOpenPipe))(usbHandle, &(lpINPipePoint->Descriptor)):hPipe;
        if (hResultPipe == NULL){
            g_pKato->Log(LOG_FAIL, TEXT("Error at Open Result Pipe - skipping test."));
            //try to close already opened pipe
            (*(lpDrvUsbFxns->lpClosePipe))(hPipe);
            return TPR_FAIL;
        }
    }

    USB_TDEVICE_PERFLPBK utperfdl = {0};
    utperfdl.uOutEP = lpOUTPipePoint->Descriptor.bEndpointAddress;

    USB_TDEVICE_REQUEST utdr = {0};
    utdr.bmRequestType = USB_REQUEST_CLASS;
    utdr.wLength = sizeof(USB_TDEVICE_PERFLPBK);

    if (fOUT)
    {
        utperfdl.uDirection = 0;
        utdr.bRequest = TEST_REQUEST_PERFOUT;
    }
    else 
    {
        utperfdl.uDirection = 1;
        utdr.bRequest = TEST_REQUEST_PERFIN;
    }

    if (iTiming != RAW_TIMING)
    {
        if (uType == USB_ENDPOINT_TYPE_BULK)
        {
            if (iTiming == HOST_TIMING)
            {
                if (iBlocking == HOST_BLOCK)
                {
                    if (iPhy == PHYS_MEM_FUNC || iPhy == PHYS_MEM_ALL)
                    {
                        NKDbgPrintfW(TEXT("host blocking, host timing, physical memory on the device.\n"));
                        utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_HBLOCKING_HTIMING_PDEV:TEST_REQUEST_PERFIN_HBLOCKING_HTIMING_PDEV;
                    }
                    else 
                    {
                        NKDbgPrintfW(TEXT("host blocking, host timing.\n"));
                        utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_HBLOCKING_HTIMING:TEST_REQUEST_PERFIN_HBLOCKING_HTIMING;
                    }
                }
                else if (iBlocking == FUNCTION_BLOCK)
                {
                    if (iPhy == PHYS_MEM_FUNC || iPhy == PHYS_MEM_ALL)
                    {
                        NKDbgPrintfW(TEXT("function blocking, host timing, physical memory on the device.\n"));
                        utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_DBLOCKING_HTIMING_PDEV:TEST_REQUEST_PERFIN_DBLOCKING_HTIMING_PDEV;
                    }
                    else 
                    {
                        NKDbgPrintfW(TEXT("function blocking, host timing.\n"));
                        utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_DBLOCKING_HTIMING:TEST_REQUEST_PERFIN_DBLOCKING_HTIMING;
                    }
                }
                else 
                {
                    NKDbgPrintfW(TEXT("host timing.\n"));
                }
            }
            else if (iTiming == DEV_TIMING)
            {
                if (iBlocking == HOST_BLOCK)
                {
                    if (iPhy == PHYS_MEM_FUNC || iPhy == PHYS_MEM_ALL)
                    {
                        NKDbgPrintfW(TEXT("host blocking, function timing, physical memory.\n"));
                        utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_HBLOCKING_DTIMING_PDEV:TEST_REQUEST_PERFIN_HBLOCKING_DTIMING_PDEV;
                    }
                    else 
                    {
                        NKDbgPrintfW(TEXT("host blocking, function timing\n"));
                        utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_HBLOCKING_DTIMING:TEST_REQUEST_PERFIN_HBLOCKING_DTIMING;
                    }
                }
                else if (iBlocking == FUNCTION_BLOCK)
                {
                    if (iPhy == PHYS_MEM_FUNC || iPhy == PHYS_MEM_ALL)
                    {
                        NKDbgPrintfW(TEXT("function blocking, function timing, physical memory.\n"));
                        utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_DBLOCKING_DTIMING_PDEV:TEST_REQUEST_PERFIN_DBLOCKING_DTIMING_PDEV;
                    }
                    else 
                    {
                        NKDbgPrintfW(TEXT("function blocking, function timing.\n"));
                        utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_DBLOCKING_DTIMING:TEST_REQUEST_PERFIN_DBLOCKING_DTIMING;
                    }
                }
                else 
                {
                    g_pKato->Log(LOG_FAIL, TEXT("Invalid Function timing - skipping test."));
                    (*(lpDrvUsbFxns->lpClosePipe))(hResultPipe);
                    (*(lpDrvUsbFxns->lpClosePipe))(hPipe);
                    return TPR_SKIP;
                }
            }
            else 
            {
                if (iBlocking == HOST_BLOCK)
                {
                    if (iPhy == PHYS_MEM_FUNC || iPhy == PHYS_MEM_ALL)
                    {
                        NKDbgPrintfW(TEXT("host blocking, sync timing, physical memory.\n"));
                        utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_HBLOCKING_STIMING_PDEV:TEST_REQUEST_PERFIN_HBLOCKING_STIMING_PDEV;
                    }
                    else 
                    {
                        NKDbgPrintfW(TEXT("host blocking, sync timing.\n"));
                        utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_HBLOCKING_STIMING:TEST_REQUEST_PERFIN_HBLOCKING_STIMING;
                    }
                }
                else if (iBlocking == FUNCTION_BLOCK)
                {
                    if (iPhy == PHYS_MEM_FUNC || iPhy == PHYS_MEM_ALL)
                    {
                        NKDbgPrintfW(TEXT("device blocking, sync timing, physical memory.\n"));
                        utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_DBLOCKING_STIMING_PDEV:TEST_REQUEST_PERFIN_DBLOCKING_STIMING_PDEV;
                    }
                    else 
                    {
                        NKDbgPrintfW(TEXT("device blocking, sync timing.\n"));
                        utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_DBLOCKING_STIMING:TEST_REQUEST_PERFIN_DBLOCKING_STIMING;
                    }
                }
                else 
                {
                    g_pKato->Log(LOG_FAIL, TEXT("Invalid blocking/timing - skipping test."));
                    (*(lpDrvUsbFxns->lpClosePipe))(hPipe);
                    (*(lpDrvUsbFxns->lpClosePipe))(hResultPipe);
                    return TPR_SKIP;
                }
            }
        }
    } // RAW_TIMING

    if (iBlocking == HOST_BLOCK && dwBlockSize*uiRounds > MAX_TRANSFER_BUFFIZE)
    {
        dwBlockSize = MAX_TRANSFER_BUFFIZE/uiRounds;
        g_pKato->Log(LOG_DETAIL, TEXT("Block size clipped at %u for %u rounds, to accommodate maximum 76 KBytes per transfer."),dwBlockSize,uiRounds);
    }

    if (iBlocking == FUNCTION_BLOCK)
    {
        utperfdl.dwBlockSize = dwBlockSize * uiRounds;
        utperfdl.usRepeat = 1;
    }
    else 
    {
        utperfdl.dwBlockSize = dwBlockSize;
        utperfdl.usRepeat = (USHORT)uiRounds;
    }

    if (iBlocking == HOST_BLOCK)
    {
        dwBlockSize *= uiRounds;
        uiRounds = 1;
    }

    PPerfTransStatus            pTrSt;
    PBYTE                       pBuffer = NULL;
    ULONG                       pPhyBufferAddr = NULL;

    pTrSt = (PPerfTransStatus) new PerfTransStatus[uiRounds];
    if (pTrSt == NULL)
    {
        (*(lpDrvUsbFxns->lpClosePipe))(hPipe);
        (*(lpDrvUsbFxns->lpClosePipe))(hResultPipe);
        g_pKato->Log(LOG_FAIL, TEXT("Cannot allocate transaction status - failing test."));
        return TPR_FAIL;
    }

    DWORD dwRealLen = dwBlockSize;
    if (iPhy == PHYS_MEM_HOST || iPhy == PHYS_MEM_ALL)
    {
        pBuffer = (PBYTE)AllocPMemory(&pPhyBufferAddr, &dwRealLen);
    }
    else 
    {
        PREFAST_SUPPRESS(419, "Potential buffer overrun: The size parameter 'dwRealLen' is being used without validation.");
        pBuffer = (PBYTE) new BYTE [dwRealLen];
    }

    if (pBuffer == NULL)
    {
        delete[] pTrSt;
        (*(lpDrvUsbFxns->lpClosePipe))(hPipe);
        (*(lpDrvUsbFxns->lpClosePipe))(hResultPipe);
        g_pKato->Log(LOG_FAIL, TEXT("Cannot allocate buffer - failing test."));
        return TPR_FAIL;
    }
    memset(pBuffer,0x5A,dwRealLen);
    dwRealLen -= 15;
    for (UINT i=0; i<dwRealLen; i+=16)
    {
        ((DWORD*)pBuffer)[i>>2] = dwBlockSize-i;
    }

    g_uiRounds = uiRounds;
    for(UINT i = 0; i < uiRounds; i++)
    {
        pTrSt[i].hTransfer=0;
        pTrSt[i].fDone=FALSE;
        pTrSt[i].pTimeRec = &TimeRec;
        pTrSt[i].flCPUPercentage = 0.0;
        pTrSt[i].flMemPercentage = 0.0;
    }
    NKDbgPrintfW(TEXT("packetsize is %d, block size = %d, rounds = %d"), 
                lpPipePoint->Descriptor.wMaxPacketSize, utperfdl.dwBlockSize, utperfdl.usRepeat);


    USB_TRANSFER hVTransfer = NULL;
    DWORD  dwRet = TPR_PASS;

#if 0
    //-----------------------------------------------------------------------------------
    //  COUNT API CALLING TIME
    //-----------------------------------------------------------------------------------

    //issue command to netchip2280
    USB_TRANSFER hVTransfer = pDriverPtr->IssueVendorTransfer(NULL, NULL, USB_OUT_TRANSFER, (PUSB_DEVICE_REQUEST)&utdr, (PVOID)&utdl, 0);
    if (hVTransfer != NULL){
        (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);
    }

    Sleep(300);

    NKDbgPrintfW(TEXT("PHYADDR +0x%p"), pPhyBufferAddr);

    if (uType == USB_ENDPOINT_TYPE_BULK)
    {
        TimeCounting(&TimeRec, TRUE);
        for(int i = 0; i < uiRounds; i++)
        {
            pTrSt[i].hTransfer=(*lpDrvUsbFxns->lpIssueBulkTransfer)(
                                                                    hPipe,
                                                                    DummyNotify,
                                                                    &pTrSt[i],
                                                                    USB_OUT_TRANSFER|USB_SEND_TO_ENDPOINT|USB_NO_WAIT,
                                                                    usBlockSize,
                                                                    pBuffer,
                                                                    (ULONG)pPhyBufferAddr);
        }
        TimeCounting(&TimeRec, FALSE);
    }
    else
    {
        TimeCounting(&TimeRec, TRUE);
        for(int i = 0; i < uiRounds; i++)
        {
            pTrSt[i].hTransfer=(*lpDrvUsbFxns->lpIssueInterruptTransfer)(
                                                                    hPipe,
                                                                    DummyNotify,
                                                                    &pTrSt[i],
                                                                    USB_OUT_TRANSFER|USB_SEND_TO_ENDPOINT|USB_NO_WAIT,
                                                                    usBlockSize,
                                                                    pBuffer,
                                                                    (ULONG)pPhyBufferAddr);
        }
        TimeCounting(&TimeRec, FALSE);
    }

    Sleep(10000);

    bRet = TRUE;
    for(int i = 0; i < uiRounds; i++)
    {
        if (pTrSt[i].hTransfer == NULL || pTrSt[i].IsFinished == FALSE){
            g_pKato->Log(LOG_FAIL,TEXT("Transfer #%d did not complete!"), i);
            bRet = FALSE;
            continue;
        }
        if (!(*lpDrvUsbFxns->lpCloseTransfer)(pTrSt[i].hTransfer))
        {
            g_pKato->Log(LOG_FAIL,TEXT("Transfer #%d failed to close!"), i);
            bRet = FALSE;
        }
    }

    if (bRet == FALSE)
    {
        g_pKato->Log(LOG_FAIL,TEXT("Performance test on %s transfer API calling time failed!"), szTestType);
        goto EXIT_BulkInt;
    }
    else
    {
        double AvgTime = CalcAvgTime(TimeRec, uiRounds);
        if (uType == USB_ENDPOINT_TYPE_BULK){
            g_pKato->Log(LOG_DETAIL, TEXT("Average calling time for IssueBulkTransfer()  with packetsize %d, block size %d is %.3f mil sec"),
                                                     lpPipePoint->Descriptor.wMaxPacketSize, usBlockSize, AvgTime);
        }
        else {
            g_pKato->Log(LOG_DETAIL, TEXT("Average calling time for IssueInterruptTransfer()  with packetsize %d, block size %d is %.3f mil sec"),
                                                     lpPipePoint->Descriptor.wMaxPacketSize, usBlockSize, AvgTime);
        }
    }

    Sleep(5000);
#endif


    //-----------------------------------------------------------------------------------
    //  COUNT throughput
    //-----------------------------------------------------------------------------------

    USHORT  usResultBuf[MAX_RESULT_SIZE];
    DWORD   dwFlags = USB_SEND_TO_ENDPOINT | USB_NO_WAIT | ((fOUT)?USB_OUT_TRANSFER:USB_IN_TRANSFER); //  USB_SHORT_TRANSFER_OK ???
    LPTRANSFER_NOTIFY_ROUTINE   lpNotifyHandler; // ThroughputNotify

    //issue command to netchip2280
#ifdef EXTRA_TRACES
    NKDbgPrintfW(TEXT("Going to issue vendor transfer; data = %02X %02X %04X %08X.\n"),
        utperfdl.uOutEP,utperfdl.uDirection,utperfdl.usRepeat,utperfdl.dwBlockSize);
#endif
    hVTransfer = pDriverPtr->IssueVendorTransfer(NULL, NULL, USB_OUT_TRANSFER, (PUSB_DEVICE_REQUEST)&utdr, (PVOID)&utperfdl, 0);
    if (hVTransfer != NULL)
    {
#ifdef EXTRA_TRACES
        NKDbgPrintfW(TEXT("Going to close vendor transfer.\n"));
#endif
        (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);
    }
    else 
    {
        g_pKato->Log(LOG_FAIL, TEXT("Issue Vendor transfer for %s failed!"),szTestType);
        dwRet = TPR_FAIL;
        goto EXIT_BulkInt;
    }

    //generic event only for DEV_TIMING or SYNC_TIMING types
    g_hGenericEvent = (iTiming==DEV_TIMING||iTiming==SYNC_TIMING)?CreateEvent(0,FALSE,FALSE,NULL):NULL;

    //any events only for non-RAW transfers
    if (iTiming != RAW_TIMING)
    {
        g_hCompEvent = CreateEvent(0,FALSE,FALSE,NULL);
        lpNotifyHandler = ThroughputNotify; 
    }
    else 
    {
        g_hCompEvent = NULL;
        lpNotifyHandler = NULL;
        dwFlags &= ~USB_NO_WAIT;
#ifdef EXTRA_TRACES
        NKDbgPrintfW(TEXT("Synchronized return from USB calls (raw timing).\n"));
#endif
    }


    if (iTiming == SYNC_TIMING)
    {
        USB_TRANSFER timingTransfer;
        memset(usResultBuf,0,sizeof(unsigned short) * MAX_RESULT_SIZE);
        g_uiGenericCallbackCnt = 0;
        g_uiGenericTotalCnt = TIMING_ROUNDS;
        NKDbgPrintfW(TEXT("Going to issue %u sync timing transfers.\n"),g_uiGenericTotalCnt);
        (*(lpDrvUsbFxns->lpResetPipe))(hPipe);
        Sleep(200);
        for(int i = 0; i < TIMING_ROUNDS; i++)
        {
            ResetEvent(g_hGenericEvent);
            timingTransfer = (*lpDrvUsbFxns->lpIssueBulkTransfer)(hPipe,
                                                                  GenericNotify,
                                                                  (LPVOID)&TimeRec,
                                                                  dwFlags, //((fOUT)?USB_OUT_TRANSFER:USB_IN_TRANSFER)|USB_SEND_TO_ENDPOINT|USB_NO_WAIT,
                                                                  MAX_RESULT_SIZE,
                                                                  usResultBuf,
                                                                  NULL); //(ULONG)pPhyBufferAddr);

            if (timingTransfer == NULL)
            {
                g_pKato->Log(LOG_FAIL, TEXT("Failed to accept #%u out of %u timing transfers, test will quit!\r\n"),
                                            i,g_uiGenericTotalCnt);
                dwRet = TPR_FAIL;
            }
            else 
            {
                if (WaitForSingleObject(g_hGenericEvent,PERF_WAIT_TIME) != WAIT_OBJECT_0){
                    (*lpDrvUsbFxns->lpAbortTransfer)(timingTransfer, USB_NO_WAIT);
                    g_pKato->Log(LOG_FAIL, TEXT("Timeout for transfer #%u - only %u out of %u timing transfers complete within %u msec, test will quit!\r\n"),
                                                i,g_uiGenericCallbackCnt,g_uiGenericTotalCnt,PERF_WAIT_TIME);
                    dwRet = TPR_FAIL;
                }
                (*lpDrvUsbFxns->lpCloseTransfer)(timingTransfer);
            }
            if (TPR_FAIL == dwRet)
            {
                g_uiGenericTotalCnt = 0;
                goto EXIT_BulkInt;
            }
        }
    }
    else 
    {
        if (iTiming != DEV_TIMING){
            Sleep(300);
        }
        TimeCounting(&TimeRec, TRUE);
    }


    
    NKDbgPrintfW(TEXT("Going to issue %i %s transfers %u bytes each, QLimit=%u.\n"),uiRounds,szTestType,dwBlockSize,uiFullQueLimit);

    g_uiCallbackCnt = 0;
    g_uiRounds = uiRounds;
    if (0 != uiFullQueLimit)
    {
        g_uiFullQueLimit = uiFullQueLimit - 1;
        g_hIssueNextTransfer = CreateEvent(0,FALSE,FALSE,NULL);
        g_pKato->Log(LOG_DETAIL,TEXT("Fixed data amount of %u bytes in %u blocks, %u bytes each."),
            uiRounds*dwBlockSize,uiRounds,dwBlockSize);
    }
    else
    {
        g_uiFullQueLimit = 0x7FFFFFFF;
        g_hIssueNextTransfer = NULL;
    }

    if (uType == USB_ENDPOINT_TYPE_BULK)
    {

        if (0 != g_uiProfilingInterval)
        {
            ProfileStart(g_uiProfilingInterval,(USE_PROFILE_CELOG|PROFILE_BUFFER|PROFILE_START));
        }

        USBPerf_ResetAvgs();
        
        for(UINT i = 0; i < uiRounds; i++){
#ifdef EXTRA_TRACES
            NKDbgPrintfW(TEXT("Submitting transfer #%i.\n"),i);
#endif
            QueryPerformanceCounter(&liTimingAPI);
            HANDLE hTmp =
            pTrSt[i].hTransfer=(*lpDrvUsbFxns->lpIssueBulkTransfer)(
                                                                    hPipe,
                                                                    lpNotifyHandler, //ThroughputNotify,
                                                                    &pTrSt[i],
                                                                    dwFlags,
                                                                    dwBlockSize,
                                                                    pBuffer,
                                                                    (ULONG)pPhyBufferAddr);
            if (hTmp)
            {
                LARGE_INTEGER liPerfCost;
                QueryPerformanceCounter(&liPerfCost);
                liTimeIssueTransferAPI.QuadPart += (liPerfCost.QuadPart - liTimingAPI.QuadPart);
                uiNumIssueTransferAPI++;
            }

           
            if (0 != uiFullQueLimit)
            {
                if (i < g_uiFullQueLimit)
                {
                    if (NULL == hTmp)
                    {
                        
                        g_uiFullQueLimit = i;
                        i--; // the last one was not accepted, decrement the counter and trigger wait
                    }
                    /*
                    if (i+1 == g_uiFullQueLimit)
                    {
                        NKDbgPrintfW(TEXT("Changed transfer method. Issued %u, completed %u, limit is %u, last returned %x."),
                                    i+1,g_uiCallbackCnt,g_uiFullQueLimit,hTmp);
                    }
                    */
                    continue;
                }

                // wait for callback to signal issuing of next transfer
                if (NULL != hTmp)
                {
                    // do not wait if notification is received already and count is updated
                    if (i<g_uiCallbackCnt)
                    {
                        continue;
                    }
                    // One sec is more than enough wait for many small transfers up to 40K
                    if (WAIT_OBJECT_0 == WaitForSingleObject(g_hIssueNextTransfer,1000))
                    {
                        ResetEvent(g_hIssueNextTransfer);
                        continue;
                    }
                }

               
                g_uiFullQueLimit = 0x7FFFFFFF;
                g_pKato->Log(LOG_FAIL,TEXT("Bulk Transfer #%d %s, only %u complete, test will quit!"),
                    i,hTmp?TEXT("timed out"):TEXT("not accepted"),g_uiCallbackCnt);
                dwRet = TPR_FAIL;
                break;
            }
            else 
            {
                if (NULL == hTmp)
                {
                    g_pKato->Log(LOG_FAIL,TEXT("Transfer #%d not accepted!"), i);
                    g_uiRounds--;
                    dwRet = TPR_FAIL;
                    //ASSERT(pTrSt[i].hTransfer);
                }
                else if (iTiming == RAW_TIMING)
                {
                    pTrSt[i].fDone = TRUE; // mark sync transfers as complete

                    // If profiling is enabled don't measure CPU load or memory because they are skewed by profiling
                    if (g_uiProfilingInterval != 0) 
                    {
                        pTrSt[i].flCPUPercentage = 0;
                        pTrSt[i].flMemPercentage = 0;
                    }
                    else 
                    {
                        pTrSt[i].flCPUPercentage = USBPerf_MarkCPU();
                        pTrSt[i].flMemPercentage = USBPerf_MarkMem();
                        USBPerf_ResetAvgs();
                    }
                }
            }
        }
        
        if (0 != g_uiProfilingInterval)
        {
            ProfileStop();
        }
    }
    else{

        USBPerf_ResetAvgs();

        for(UINT i = 0; i < uiRounds; i++){
#ifdef EXTRA_TRACES
            NKDbgPrintfW(TEXT("Submitting transfer #%i.\n"),i);
#endif
            QueryPerformanceCounter(&liTimingAPI);
            HANDLE hTmp =
            pTrSt[i].hTransfer=(*lpDrvUsbFxns->lpIssueInterruptTransfer)(
                                                                    hPipe,
                                                                    lpNotifyHandler, //ThroughputNotify,
                                                                    &pTrSt[i],
                                                                    dwFlags,
                                                                    dwBlockSize,
                                                                    pBuffer,
                                                                    (ULONG)pPhyBufferAddr);
            if (hTmp)
            {
                LARGE_INTEGER liPerfCost;
                QueryPerformanceCounter(&liPerfCost);
                liTimeIssueTransferAPI.QuadPart += (liPerfCost.QuadPart - liTimingAPI.QuadPart);
                uiNumIssueTransferAPI++;
            }

            // if full-queue-test is executed, we push transfers here until:
            if (0 != uiFullQueLimit)
            {

                if (i < g_uiFullQueLimit)
                {
                    if (NULL == hTmp)
                    {
                        // if the saturation point is reached - back off one transfer
                        g_uiFullQueLimit = i;
                        i--; // the last one was not accepted, decrement the counter and trigger wait
                    }
                    /*
                    if (i+1 == g_uiFullQueLimit)
                        NKDbgPrintfW(TEXT("Changed transfer method. Issued %u, completed %u, limit is %u, last returned %x."),
                                    i+1,g_uiCallbackCnt,g_uiFullQueLimit,hTmp);
                    */
                    continue;
                }

                // wait for callback to signal issuing of next transfer
                if (NULL != hTmp)
                {
                    // do not wait if notification is received already and count is updated
                    if (i<g_uiCallbackCnt)
                    {
                        continue;
                    }
                    // One sec is more than enough wait for many small transfers up to 40K
                    if (WAIT_OBJECT_0 == WaitForSingleObject(g_hIssueNextTransfer,1000))
                    {
                        ResetEvent(g_hIssueNextTransfer);
                        continue;
                    }
                }

               
                g_uiFullQueLimit = 0x7FFFFFFF;
                g_pKato->Log(LOG_FAIL,TEXT("Interrupt Transfer #%d %s, only %u complete, test will quit!"),
                    i,hTmp?TEXT("timed out"):TEXT("not accepted"),g_uiCallbackCnt);
                dwRet = TPR_FAIL;
                break;
            }
            else 
            {
                if (NULL == hTmp)
                {
                    g_pKato->Log(LOG_FAIL,TEXT("Transfer #%d not accepted!"), i);
                    g_uiRounds--;
                    dwRet = TPR_FAIL;
                    //ASSERT(pTrSt[i].hTransfer);
                }
                else if (iTiming == RAW_TIMING)
                {
                    pTrSt[i].fDone = TRUE; // mark sync transfers as complete
                    // If profiling is enabled don't measure CPU load or memory because they are skewed by profiling
                    if (g_uiProfilingInterval != 0) 
                    {
                        pTrSt[i].flCPUPercentage = 0;
                        pTrSt[i].flMemPercentage = 0;
                    } 
                    else 
                    {
                        pTrSt[i].flCPUPercentage = USBPerf_MarkCPU();
                        pTrSt[i].flMemPercentage = USBPerf_MarkMem();
                        USBPerf_ResetAvgs();
                    }
                }
            }
        }
    }

    // for raw sync host counting, measure time now
    if (iTiming == RAW_TIMING)
    {
        TimeCounting(&TimeRec, FALSE);
        if (0 == g_uiRounds)
        {
            g_pKato->Log(LOG_FAIL, TEXT("No transfer completed, test will quit!\r\n"));
            dwRet = TPR_FAIL;
            goto EXIT_BulkInt;
        }
    }
    else 
    {
        // Wait for all transfers to complete
        DWORD dwWait = WAIT_TIMEOUT;
        if (g_uiRounds)
        {
            dwWait = WaitForSingleObject(g_hCompEvent, PERF_WAIT_TIME);
        }
        NKDbgPrintfW(TEXT("Got notification %X for %i of %u rounds complete.\n"),g_hCompEvent,g_uiCallbackCnt,uiRounds);

        if (dwWait != WAIT_OBJECT_0){
            g_pKato->Log(LOG_FAIL, TEXT("Only %u transfer(s) completed within %i msec, test will quit!\r\n"),g_uiCallbackCnt,PERF_WAIT_TIME);
            dwRet = TPR_FAIL;
            // do not exit now - incomplete transfers must be aborted!
        }
    }

    int iSysPerfs = 0;
    float flTotalCPUper = 0.0;
    float flTotalMemper = 0.0;

    for(UINT i = 0; i < uiRounds; i++)
    {
        if (pTrSt[i].fDone == FALSE)
        {
            if (pTrSt[i].hTransfer != NULL)
            {
                g_pKato->Log(LOG_FAIL,TEXT("Transfer #%d did not complete!"), i);
                (*lpDrvUsbFxns->lpAbortTransfer)(pTrSt[i].hTransfer, USB_NO_WAIT);
            }
            dwRet = TPR_FAIL;
            continue;
        }
        if (!(*lpDrvUsbFxns->lpCloseTransfer)(pTrSt[i].hTransfer))
        {
            g_pKato->Log(LOG_FAIL,TEXT("Transfer #%d failed to close!"), i);
            dwRet = TPR_FAIL;
        }

        if (pTrSt[i].flCPUPercentage > 0.0)
        {
            flTotalCPUper += pTrSt[i].flCPUPercentage;
            flTotalMemper += pTrSt[i].flMemPercentage;
            iSysPerfs ++;
            pTrSt[i].flCPUPercentage = 0.0;
            pTrSt[i].flMemPercentage = 0.0;
        }
    }

    if (iTiming == DEV_TIMING || iTiming == SYNC_TIMING)
    {
        memset(usResultBuf,0,sizeof(unsigned short) * MAX_RESULT_SIZE);
        g_uiGenericCallbackCnt = 0;
        g_uiGenericTotalCnt = 1;
        ResetEvent(g_hGenericEvent);
        USB_TRANSFER hInfTransfer=(*lpDrvUsbFxns->lpIssueBulkTransfer)(hResultPipe,
                                                                       GenericNotify,
                                                                       (PVOID)0xcccccccc,
                                                                       USB_IN_TRANSFER |USB_SEND_TO_ENDPOINT|USB_NO_WAIT,
                                                                       MAX_RESULT_SIZE*sizeof(unsigned short),
                                                                       usResultBuf,
                                                                       NULL);
        if (hInfTransfer == NULL)
        {
            NKDbgPrintfW(TEXT("Failed to issue result transfer.\n"));
            dwRet = TPR_FAIL;
        }
        else 
        {
            if (WaitForSingleObject(g_hGenericEvent,PERF_WAIT_TIME) != WAIT_OBJECT_0){
                (*lpDrvUsbFxns->lpAbortTransfer)(hInfTransfer, USB_NO_WAIT);
                g_pKato->Log(LOG_FAIL, TEXT("Result transfer not completed with %u secs, test will quit!\r\n"),PERF_WAIT_TIME);
                dwRet = TPR_FAIL;
            }
            (*lpDrvUsbFxns->lpCloseTransfer)(hInfTransfer);
            NKDbgPrintfW(TEXT("\t Device side result = '%s'\n"),usResultBuf);
        }
    }


    if (dwRet != TPR_PASS)
    {
        g_pKato->Log(LOG_FAIL,TEXT("Transfer throughput performance test '%s' failed!"), szTestType);
        goto EXIT_BulkInt;
    }
    else
    {
        double AvgThroughput = CalcAvgThroughput(TimeRec, uiRounds, dwBlockSize);
        float CpuUsage = 0;
        float MemUsage = 0;

        g_pKato->Log(LOG_DETAIL, TEXT("Average throughput for %s Transfer with packetsize %d, block size %d is %.3f Mbps"),
                                                 szTestType,lpPipePoint->Descriptor.wMaxPacketSize, dwBlockSize, AvgThroughput);
        

        if (pTP != NULL)
        {
            pTP->dbThroughput = AvgThroughput;
            if (iSysPerfs == 0)
            {
                g_pKato->Log(LOG_DETAIL, TEXT("WARNING: No CPU/Memory usage number available"));
            }
            else
            {
                CpuUsage = flTotalCPUper / (float)iSysPerfs;
                MemUsage = flTotalMemper / (float)iSysPerfs;
                g_pKato->Log(LOG_DETAIL, TEXT("Average CPU usage is %2.3f %%; Mem usage is %2.3f %%"),CpuUsage, MemUsage);
                
                pTP->flCPUUsage = CpuUsage;
                pTP->flMemUsage = MemUsage;
            }
        }


        // Output result to CePerf/PerfScenario
        if (utperfdl.dwBlockSize == dwBlockSize)   // Distinguishes between basic standard tests and additional tests.
        {
            // Standard tests
            OutputDataToPerfScenario(in_szTestDescription,
                                     szTestType,
                                     DWORD (AvgThroughput*1000000),
                                     DWORD (CpuUsage*1000),
                                     DWORD (MemUsage*1000),
                                     lpPipePoint->Descriptor.wMaxPacketSize,
                                     dwBlockSize,
                                     fOUT,
                                     FALSE,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     uiCallPrio,
                                     0!=uiFullQueLimit,
                                     in_fHighSpeed);
        }
        else 
        {
            // Additional tests
            DWORD dwBlockSizeToUse    = 0;
            DWORD dwLargeTransferSize = 0;
            if (HOST_BLOCK == iBlocking)
            {
                dwBlockSizeToUse    = utperfdl.dwBlockSize;
                dwLargeTransferSize = dwBlockSize;
            }
            else
            {
                dwBlockSizeToUse    = dwBlockSize;
                dwLargeTransferSize = utperfdl.dwBlockSize;
            }

            dwBlockSizeToUse = (HOST_BLOCK == iBlocking) ? utperfdl.dwBlockSize : dwBlockSize;
            OutputDataToPerfScenario(in_szTestDescription,
                                     szTestType,
                                     DWORD (AvgThroughput*1000000),
                                     DWORD (CpuUsage*1000),
                                     DWORD (MemUsage*1000),
                                     lpPipePoint->Descriptor.wMaxPacketSize,
                                     dwBlockSizeToUse,     // Note: differs depending on who is blocking.
                                     fOUT,
                                     TRUE,
                                     dwLargeTransferSize,  // Note: differs depending on who is blocking.
                                     iTiming,
                                     iBlocking,
                                     iPhy,
                                     0,
                                     0,
                                     0,
                                     in_fHighSpeed);
        }
    }


EXIT_BulkInt:
    g_uiRounds = 0;
    g_uiCallbackCnt = 0;

    if (uiNumIssueTransferAPI!=0)
    {
        liTimingAPI.QuadPart = TimeRec.perfFreq.QuadPart * uiNumIssueTransferAPI;
        liTimeIssueTransferAPI.QuadPart *= 1000000000L;
        liTimeIssueTransferAPI.QuadPart /= liTimingAPI.QuadPart;
        NKDbgPrintfW(TEXT("PERF: Issue%sTransfer() called %u times, average cost is %u.%03u usec."),szTestType,
            uiNumIssueTransferAPI,liTimeIssueTransferAPI.LowPart/1000,liTimeIssueTransferAPI.LowPart%1000);
    }

    if ((*(lpDrvUsbFxns->lpClosePipe))(hPipe)==FALSE)
    {
        g_pKato->Log(LOG_FAIL, TEXT("Error at Close Pipe"));
        DEBUGMSG(DBG_ERR,(TEXT("Error at Close Pipe")));
        dwRet = TPR_FAIL;
    }
    if (hResultPipe!=NULL)
    {
        if (fOUT && (*(lpDrvUsbFxns->lpClosePipe))(hResultPipe)==FALSE)
        {
            g_pKato->Log(LOG_FAIL, TEXT("Error at Close ResultPipe"));
            DEBUGMSG(DBG_ERR,(TEXT("Error at Close ResultPipe")));
            dwRet = TPR_FAIL;
        }
    }

    if (pPhyBufferAddr)
    {
        FreePhysMem(pBuffer);
    }
    else if (pBuffer)
    {
        delete[] pBuffer;
    }

    if (pTrSt)
    {
        delete[] pTrSt;
    }

    if (g_hCompEvent != NULL)
    {
        CloseHandle(g_hCompEvent);
        g_hCompEvent = NULL;
    }
    if (g_hGenericEvent != NULL)
    {
        CloseHandle(g_hGenericEvent);
        g_hGenericEvent = NULL;
    }
    if (g_hIssueNextTransfer != NULL)
    {
        CloseHandle(g_hIssueNextTransfer);
        g_hIssueNextTransfer = NULL;
    }

    return dwRet;
}// BulkInt_Perf


//---------------------------------------------------------------------------
// Purpose:   All isochronous tests.
// Arguments: 
//
// Returns:   TPR_XXX.
//---------------------------------------------------------------------------

DWORD  Isoch_Perf(UsbClientDrv *pDriverPtr, 
                  LPCUSB_ENDPOINT lpOUTPipePoint,  
                  LPCUSB_ENDPOINT lpINPipePoint,
                  DWORD dwBlockSize, 
                  UINT  uiRounds, 
                  BOOL bPhy, 
                  PONE_THROUGHPUT pTP, 
                  BOOL fOUT,
                  int iTiming, 
                  UINT uiQparam, 
                  UINT uiCallPrio,
                  UINT uiFullQueLimit,
                  BOOL in_fHighSpeed,
                  LPCTSTR in_szTestDescription)
{
    if (pDriverPtr == NULL || lpOUTPipePoint == NULL || lpINPipePoint == NULL || uiQparam > 3 ||
        uiRounds == 0 || uiRounds > 65535 || dwBlockSize < 0 || dwBlockSize > 1024*1024) //prefast cautions
    {
        g_pKato->Log(LOG_FAIL, TEXT("Invalid parameter(s) - skipping test."));
        return TPR_SKIP;
    }
    
    TIME_REC  TimeRec = {0};
    if (QueryPerformanceFrequency(&TimeRec.perfFreq) == FALSE || TimeRec.perfFreq.QuadPart == 0)
    {
        TimeRec.perfFreq.QuadPart = 1000; //does not support high resolution
    }
    NKDbgPrintfW(TEXT("Performance Frequency is: %d"), TimeRec.perfFreq.QuadPart);

    LARGE_INTEGER   liTimingAPI;
    LARGE_INTEGER   liTimeIssueTransferAPI;
    UINT            uiNumIssueTransferAPI = 0;
    liTimeIssueTransferAPI.QuadPart = 0;

    LPCUSB_FUNCS        lpDrvUsbFxns = pDriverPtr->lpUsbFuncs;
    USB_HANDLE          usbHandle   = pDriverPtr->GetUsbHandle();
    LPCUSB_ENDPOINT     lpPipePoint = fOUT?lpOUTPipePoint:lpINPipePoint;
    USB_PIPE            hPipe;
    
    hPipe = (*(lpDrvUsbFxns->lpOpenPipe))(usbHandle,&(lpPipePoint->Descriptor));
    if (hPipe==NULL)
    {
        g_pKato->Log(LOG_FAIL, TEXT("Error at Open Pipe"));
        return TPR_FAIL;
    }
    (*(lpDrvUsbFxns->lpResetPipe))(hPipe);

    // requests for loopback DLL are straightforward when run Isoc
    USB_TDEVICE_PERFLPBK    utperfdl = {0};
    utperfdl.uOutEP         = lpOUTPipePoint->Descriptor.bEndpointAddress;
    utperfdl.dwBlockSize    = dwBlockSize;
    utperfdl.usRepeat       = (USHORT)uiRounds;
    utperfdl.uDirection     = fOUT?0:1;

    USB_TDEVICE_REQUEST     utdr = {0};
    utdr.bmRequestType      = USB_REQUEST_CLASS;
    utdr.bRequest           = fOUT?TEST_REQUEST_PERFOUT:TEST_REQUEST_PERFIN;
    utdr.wLength            = sizeof(USB_TDEVICE_PERFLPBK);



    DWORD   dwPacketSize   = (lpPipePoint->Descriptor.wMaxPacketSize & PACKETS_PART_MASK) * (((lpPipePoint->Descriptor.wMaxPacketSize & HIGHSPEED_MULTI_PACKETS_MASK) >> 11)+1);
    DWORD   dwNumofPackets = (dwBlockSize + dwPacketSize - 1)/dwPacketSize;
    DWORD*  pBlockLengths  = NULL;


    if (0 != uiQparam)
    {
        BOOL fSkipTest = FALSE;
        DWORD dwMaxPacket = dwPacketSize;
        dwPacketSize >>= uiQparam;
        if (dwPacketSize < 8)
        {
            g_pKato->Log(LOG_DETAIL,TEXT("Microframe size is %u, skipping test for sizes less than 8!"), dwPacketSize);
            fSkipTest = TRUE;
        }
        if (dwMaxPacket != (dwPacketSize<<uiQparam))
        {
            g_pKato->Log(LOG_DETAIL,TEXT("Max. packet size %u is not a whole multiple of microframe size %u, skipping test!"),
                                        dwMaxPacket,dwPacketSize);
            fSkipTest = TRUE;
        }
        if (fSkipTest)
        {
            (*(lpDrvUsbFxns->lpClosePipe))(hPipe);
            g_pKato->Log(LOG_FAIL, TEXT("Microframe parameter(s) incompatible - skipping test."));
            return TPR_SKIP;
        }
        dwNumofPackets <<= uiQparam;
        g_pKato->Log(LOG_DETAIL,TEXT("Using %u microframes of %u bytes within packet of %u bytes."), 
                                    (1<<uiQparam),dwPacketSize,dwMaxPacket);
    }

    pBlockLengths = new DWORD[dwNumofPackets*3];
    if (pBlockLengths == NULL)
    {
        (*(lpDrvUsbFxns->lpClosePipe))(hPipe);
        g_pKato->Log(LOG_FAIL, TEXT("Cannot allocate packet blocks - failing test."));
        return TPR_FAIL;
    }

    // If a buffer is a multiple of packets,break transfers down into a number of packets
    // if the buffer is smaller than a packet,restrict the transfer size to the buffer size.
    // the IssueIsocTransfer API requires buffer size be <= maximum packet size
    for(DWORD dwi = 0; dwi < dwNumofPackets; dwi ++){
        pBlockLengths[dwi] = (dwBlockSize < dwPacketSize) ? dwBlockSize : dwPacketSize;
    }
    PDWORD  pCurLengths = pBlockLengths + dwNumofPackets;
    PDWORD  pCurErrors  = pBlockLengths + dwNumofPackets * 2;

    PPerfTransStatus            pTrSt;
    PBYTE                       pBuffer = NULL;
    ULONG                       pPhyBufferAddr = NULL;

    pTrSt = (PPerfTransStatus) new PerfTransStatus[uiRounds];
    if (pTrSt == NULL)
    {
        delete[] pBlockLengths;
        (*(lpDrvUsbFxns->lpClosePipe))(hPipe);
        g_pKato->Log(LOG_FAIL, TEXT("Cannot allocate transaction status - failing test."));
        return TPR_FAIL;
    }

    DWORD dwRealLen = dwNumofPackets * dwPacketSize;
    if (bPhy == TRUE)
    {
        pBuffer = (PBYTE)AllocPMemory(&pPhyBufferAddr, &dwRealLen);
    }
    else
    {
        PREFAST_SUPPRESS(419, "Potential buffer overrun: The size parameter 'dwRealLen' is being used without validation.");
        pBuffer = (PBYTE) new BYTE [dwRealLen];
    }

    if (pBuffer == NULL)
    {
        delete[] pTrSt;
        delete[] pBlockLengths;
        (*(lpDrvUsbFxns->lpClosePipe))(hPipe);
        g_pKato->Log(LOG_FAIL, TEXT("Cannot allocate buffer - failing test."));
        return TPR_FAIL;
    }
    memset(pBuffer,0x5A,dwRealLen);
    dwRealLen -= 15;
    for (UINT i=0; i<dwRealLen; i+=16)
    {
        ((DWORD*)pBuffer)[i>>2] = dwBlockSize-i;
    }

    g_uiRounds = uiRounds;
    for(UINT i = 0; i < uiRounds; i++)
    {
        pTrSt[i].hTransfer=0;
        pTrSt[i].fDone=FALSE;
        pTrSt[i].pTimeRec = &TimeRec;
        pTrSt[i].flCPUPercentage = 0.0;
        pTrSt[i].flMemPercentage = 0.0;
    }
    NKDbgPrintfW(TEXT("packetsize is %d, block size = %d, rounds = %d"), 
                (lpPipePoint->Descriptor.wMaxPacketSize & PACKETS_PART_MASK), utperfdl.dwBlockSize, utperfdl.usRepeat);


    USB_TRANSFER hVTransfer = NULL;
    DWORD  dwRet = TPR_PASS;

#if 0
    //-----------------------------------------------------------------------------------
    //  COUNT API CALLING TIME
    //-----------------------------------------------------------------------------------

    //issue command to netchip2280
    USB_TRANSFER hVTransfer = pDriverPtr->IssueVendorTransfer(NULL, NULL, USB_OUT_TRANSFER, (PUSB_DEVICE_REQUEST)&utdr, (PVOID)&utdl, 0);
    if (hVTransfer != NULL)
    {
        (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);
    }
    Sleep(300);

    if (pDriverPtr->GetFrameNumber(&dwCurFrame) == FALSE)
    {
        g_pKato->Log(LOG_FAIL,TEXT("GetCurrentFrameNumber failed!"));
        goto EXIT_Isoc;
    }
    else
    {
        NKDbgPrintfW(TEXT("Current frame number is: %d"), dwCurFrame);
    }

    NKDbgPrintfW(TEXT("PHYADDR +0x%p"), pPhyBufferAddr);

    TimeCounting(&TimeRec, TRUE);
    pTrSt[0].hTransfer=(*lpDrvUsbFxns->lpIssueIsochTransfer)(
                                                            hPipe,
                                                            DummyNotify,
                                                            &pTrSt[0],
                                                            USB_SEND_TO_ENDPOINT|USB_NO_WAIT|USB_OUT_TRANSFER,
                                                            dwCurFrame+500,
                                                            dwNumofPackets,
                                                            pBlockLengths,
                                                            pBuffer,
                                                            (ULONG)pPhyBufferAddr);
    for(int i = 1; i < uiRounds; i++)
    {
        pTrSt[i].hTransfer=(*lpDrvUsbFxns->lpIssueIsochTransfer)(
                                                                hPipe,
                                                                DummyNotify,
                                                                &pTrSt[i],
                                                                USB_START_ISOCH_ASAP|USB_SEND_TO_ENDPOINT|USB_NO_WAIT|USB_OUT_TRANSFER,
                                                                0,
                                                                dwNumofPackets,
                                                                pBlockLengths,
                                                                pBuffer,
                                                                (ULONG)pPhyBufferAddr);
    }
    TimeCounting(&TimeRec, FALSE);

    Sleep(10000);

    BOOL bRet = TRUE;
    // Get Transfer Status.

    for(int i = 0; i < uiRounds; i++)
    {
        if (pTrSt[i].hTransfer == NULL || pTrSt[i].IsFinished == FALSE){
            g_pKato->Log(LOG_FAIL,TEXT("Transfer #%d failed!"), i);
            bRet = FALSE;
            continue;
        }

        if (!(*lpDrvUsbFxns->lpIsTransferComplete)(pTrSt[i].hTransfer))
        {
            g_pKato->Log(LOG_FAIL, TEXT("Transfer #%d:  IsTransferComplet not return true after call back"), i);
            bRet = FALSE;
            continue;
        }

        if (!(*lpDrvUsbFxns->lpGetIsochResults)(pTrSt[i].hTransfer, dwNumofPackets, pCurLengths,pCurErrors))  
        { // fail
            g_pKato->Log(LOG_FAIL, TEXT(" Transfer #%d: Failure:GetIsochStatus return FALSE, before transfer complete, NumOfBlock(%lx)"), i, dwNumofPackets);
            bRet = FALSE;
        };

        if (!(*lpDrvUsbFxns->lpCloseTransfer)(pTrSt[i].hTransfer))
        {
            g_pKato->Log(LOG_FAIL,TEXT("Transfer #%d failed to close!"), i);
            bRet = FALSE;
        }
    }

    if (bRet == FALSE)
    {
        g_pKato->Log(LOG_FAIL,TEXT("Performance test on ISOCH transfer API calling time failed!"));
        goto EXIT_Isoc;
    }
    else
    {
        double AvgTime = CalcAvgTime(TimeRec, uiRounds-1);
        g_pKato->Log(LOG_DETAIL, TEXT("Average calling time for IssueIsochTransfer()  with packetsize %d, block size %d is %.3f mil sec"),
                                                 lpPipePoint->Descriptor.wMaxPacketSize, usBlockSize, AvgTime);
    }

    Sleep(5000);
#endif


    //-----------------------------------------------------------------------------------
    //  COUNT throughput
    //-----------------------------------------------------------------------------------

    DWORD   dwCurFrame;
    DWORD   dwFlags = USB_SEND_TO_ENDPOINT | USB_NO_WAIT | ((fOUT)?USB_OUT_TRANSFER:USB_IN_TRANSFER); //  USB_SHORT_TRANSFER_OK ???
    LPTRANSFER_NOTIFY_ROUTINE   lpNotifyHandler; // ThroughputNotify

    //issue command to netchip2280
#ifdef EXTRA_TRACES
    NKDbgPrintfW(TEXT("Going to issue vendor transfer.\n"));
#endif
    hVTransfer = pDriverPtr->IssueVendorTransfer(NULL, NULL, USB_OUT_TRANSFER, (PUSB_DEVICE_REQUEST)&utdr, (PVOID)&utperfdl, 0);
    if (hVTransfer != NULL)
    {
#ifdef EXTRA_TRACES
        NKDbgPrintfW(TEXT("Going to close vendor transfer.\n"));
#endif
        (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);
    }
    else 
    {
        g_pKato->Log(LOG_FAIL, TEXT("Issue Vendor transfer for Isochronous failed!"));
        dwRet = TPR_FAIL;
        goto EXIT_Isoc;
    }


    // prepare initial frame
    dwCurFrame = 0;
    if (pDriverPtr->GetFrameNumber(&dwCurFrame) == FALSE){
        g_pKato->Log(LOG_FAIL,TEXT("GetCurrentFrameNumber failed!"));
        dwRet = TPR_FAIL;
        goto EXIT_Isoc;
    }
    NKDbgPrintfW(TEXT("Current frame number is: %d"), dwCurFrame);
    dwCurFrame += 500;

    // we need this event only for non-RAW transfers
    if (iTiming != RAW_TIMING)
    {
        g_hCompEvent = CreateEvent(0,FALSE,FALSE,NULL);
        lpNotifyHandler = ThroughputNotify; 
    }
    else 
    {
        g_hCompEvent = NULL;
        lpNotifyHandler = NULL;
        dwFlags &= ~USB_NO_WAIT;
        TimeCounting(&TimeRec, TRUE);
#ifdef EXTRA_TRACES
        NKDbgPrintfW(TEXT("Synchronized return from USB calls (raw timing).\n"));
#endif
    }


    //
    // Fire it up!
    //
    NKDbgPrintfW(TEXT("Going to issue %i Isochronous transfers, QLimit=%u.\n"),uiRounds,uiFullQueLimit);

    g_uiCallbackCnt = 0;
    g_uiRounds = uiRounds;
    if (0 != uiFullQueLimit)
    {
        g_uiFullQueLimit = uiFullQueLimit - 1;
        g_hIssueNextTransfer = CreateEvent(0,FALSE,FALSE,NULL);
        g_pKato->Log(LOG_DETAIL,TEXT("Fixed data amount of %u bytes in %u blocks, %u bytes each."),
            uiRounds*dwBlockSize,uiRounds,dwBlockSize);
    }
    else
    {
        g_uiFullQueLimit = 0x7FFFFFFF;
        g_hIssueNextTransfer = NULL;
    }


    if (0 != g_uiProfilingInterval)
    {
        ProfileStart(g_uiProfilingInterval,(USE_PROFILE_CELOG|PROFILE_BUFFER|PROFILE_START));
    }

    USBPerf_ResetAvgs();
    
    for(UINT i = 0; i < uiRounds; i++){
#ifdef EXTRA_TRACES
        NKDbgPrintfW(TEXT("Submitting transfer #%i...\n"),i);
#endif
        QueryPerformanceCounter(&liTimingAPI);
        HANDLE hTmp =
        pTrSt[i].hTransfer=(*lpDrvUsbFxns->lpIssueIsochTransfer)(
                                                                hPipe,
                                                                lpNotifyHandler, //ThroughputNotify,
                                                                &pTrSt[i],
                                                                dwFlags, //USB_SEND_TO_ENDPOINT|USB_NO_WAIT|dwFlags,
                                                                dwCurFrame,
                                                                dwNumofPackets,
                                                                pBlockLengths,
                                                                pBuffer,
                                                                (ULONG)pPhyBufferAddr);
        if (hTmp)
        {
            LARGE_INTEGER liPerfCost;
            QueryPerformanceCounter(&liPerfCost);
            liTimeIssueTransferAPI.QuadPart += (liPerfCost.QuadPart - liTimingAPI.QuadPart);
            uiNumIssueTransferAPI++;
        }

        // all transfers after #0 are w/ modified params
        dwCurFrame = 0;
        dwFlags |= USB_START_ISOCH_ASAP;

        // if full-queue-test is executed, we push transfers here until:
        if (0 != uiFullQueLimit)
        {

            if (i < g_uiFullQueLimit)
            {
                if (NULL == hTmp)
                {
                    // if the saturation point is reached - back off one transfer
                    g_uiFullQueLimit = i;
                    i--; // the last one was not accepted, decrement the counter and trigger wait
                }/*
                if (i+1 == g_uiFullQueLimit)
                    NKDbgPrintfW(TEXT("Changed transfer method. Issued %u, completed %u, limit is %u, last returned %x."),
                                i+1,g_uiCallbackCnt,uiFullQueLimit,hTmp);
                */
                continue;
            }

            // wait for callback to signal issuing of next transfer
            if (NULL != hTmp)
            {
                // do not wait if notification is received already and count is updated
                if (i<g_uiCallbackCnt)
                {
                    continue;
                }
                // 60 sec is adequate for many small transfers up to 8000 times
                if (WAIT_OBJECT_0 == WaitForSingleObject(g_hIssueNextTransfer,60000))
                {
                    ResetEvent(g_hIssueNextTransfer);
                    continue;
                }
            }

           
            g_uiFullQueLimit = 0x7FFFFFFF;
            g_pKato->Log(LOG_FAIL,TEXT("Isoch Transfer #%d %s, only %u complete, test will quit!"),
                i,hTmp?TEXT("timed out"):TEXT("not accepted"),g_uiCallbackCnt);
            dwRet = TPR_FAIL;
            break;
        }
        else 
        {
            if (NULL == hTmp)
            {
                 g_pKato->Log(LOG_FAIL,TEXT("Transfer #%d Parameters: %08X, %08X, %08X, %08X, %08X, %08X, %08X, %08x, %08x"), i,
                                                                        hPipe,
                                                                        lpNotifyHandler, //ThroughputNotify,
                                                                        &pTrSt[i],
                                                                        dwFlags, //USB_SEND_TO_ENDPOINT|USB_NO_WAIT|dwFlags,
                                                                        dwCurFrame,
                                                                        dwNumofPackets,
                                                                        pBlockLengths,
                                                                        pBuffer,
                                                                        (ULONG)pPhyBufferAddr);
                g_pKato->Log(LOG_FAIL,TEXT("Transfer #%d not accepted! %08x"), i, GetLastError());
                g_uiRounds--;
                dwRet = TPR_FAIL;
                //ASSERT(pTrSt[i].hTransfer);
            }
            else if (iTiming == RAW_TIMING)
            {
                pTrSt[i].fDone = TRUE; // mark sync transfers as complete
                // If profiling is enabled don't measure CPU load or memory because they are skewed by profiling
                if (g_uiProfilingInterval != 0) 
                {
                    pTrSt[i].flCPUPercentage = 0;
                    pTrSt[i].flMemPercentage = 0;
                } 
                else 
                {
                    pTrSt[i].flCPUPercentage = USBPerf_MarkCPU();
                    pTrSt[i].flMemPercentage = USBPerf_MarkMem();
                    USBPerf_ResetAvgs();
                }
            }
        }
    }

    if (0 != g_uiProfilingInterval){
        ProfileStop();
    }


    // for raw sync host counting, measure time now
    if (iTiming == RAW_TIMING)
    {
        TimeCounting(&TimeRec, FALSE);
        if (0 == g_uiRounds)
        {
            g_pKato->Log(LOG_FAIL, TEXT("No transfer completed, test will quit!\r\n"));
            dwRet = TPR_FAIL;
            goto EXIT_Isoc;
        }
    }
    else 
    {
        // Wait for all transfers to complete 
        DWORD dwWait = WAIT_TIMEOUT;
        if (g_uiRounds)
        {
            dwWait = WaitForSingleObject(g_hCompEvent, PERF_WAIT_TIME*3);
        }
        NKDbgPrintfW(TEXT("Got notification %X for %i of %u rounds complete.\n"),g_hCompEvent,g_uiCallbackCnt,uiRounds);

        if (dwWait != WAIT_OBJECT_0)
        {
            g_pKato->Log(LOG_FAIL, TEXT("Only %u transfer(s) completed within %i msec, test will quit!\r\n"),
                                        g_uiCallbackCnt,PERF_WAIT_TIME*3);
            dwRet = TPR_FAIL;
            // do not exit now - incomplete transfers must be aborted!
        }
    }

    int iSysPerfs = 0;
    float flTotalCPUper = 0.0;
    float flTotalMemper = 0.0;

    for(UINT i = 0; i < uiRounds; i++)
    {
        if (pTrSt[i].fDone == FALSE)
        {
            if (pTrSt[i].hTransfer != NULL)
            {
                g_pKato->Log(LOG_FAIL,TEXT("Transfer #%d did not complete!"), i);
                (*lpDrvUsbFxns->lpAbortTransfer)(pTrSt[i].hTransfer, USB_NO_WAIT);
            }
            dwRet = TPR_FAIL;
            continue;
        }
        if (!(*lpDrvUsbFxns->lpIsTransferComplete)(pTrSt[i].hTransfer))
        {
            g_pKato->Log(LOG_FAIL, TEXT("Transfer #%d:  IsTransferComplete() return FALSE."), i);
            dwRet = TPR_FAIL;
        }
        if (!(*lpDrvUsbFxns->lpGetIsochResults)(pTrSt[i].hTransfer, dwNumofPackets, pCurLengths,pCurErrors))
        { // fail
            g_pKato->Log(LOG_FAIL, TEXT(" Transfer #%d:  GetIsochStatus() return FALSE for %u packets."), i, dwNumofPackets);
            dwRet = TPR_FAIL;
        };
        if (!(*lpDrvUsbFxns->lpCloseTransfer)(pTrSt[i].hTransfer))
        {
            g_pKato->Log(LOG_FAIL,TEXT("Transfer #%d failed to close!"), i);
            dwRet = TPR_FAIL;
        }

        if (pTrSt[i].flCPUPercentage > 0.0)
        {
            flTotalCPUper += pTrSt[i].flCPUPercentage;
            flTotalMemper += pTrSt[i].flMemPercentage;
            iSysPerfs ++;
            pTrSt[i].flCPUPercentage = 0.0;
            pTrSt[i].flMemPercentage = 0.0;
        }
    }

    if (dwRet != TPR_PASS)
    {
        g_pKato->Log(LOG_FAIL,TEXT("Performance test on Isochronous transfer throughput failed!"));
        goto EXIT_Isoc;
    }
    else
    {
        double AvgThroughput = CalcAvgThroughput(TimeRec, uiRounds-1, dwBlockSize);
        float CpuUsage = 0;
        float MemUsage = 0;

        g_pKato->Log(LOG_DETAIL, TEXT("Average throughput for Isochronous Transfer with packetsize %d, block size %d is %.3f Mbps"),
                                                 (lpPipePoint->Descriptor.wMaxPacketSize & PACKETS_PART_MASK), dwBlockSize, AvgThroughput);
        
        if (pTP != NULL)
        {
            pTP->dbThroughput = AvgThroughput;
            if (iSysPerfs == 0)
            {
                g_pKato->Log(LOG_DETAIL, TEXT("WARNING: No CPU/Memory usage number available"));
            }
            else 
            {
                CpuUsage = flTotalCPUper / (float)iSysPerfs;
                MemUsage = flTotalMemper / (float)iSysPerfs;
                g_pKato->Log(LOG_DETAIL, TEXT("Average CPU usage is %2.3f %%; Mem usage is %2.3f %%"),CpuUsage, MemUsage);

                pTP->flCPUUsage = CpuUsage;
                pTP->flMemUsage = MemUsage;
            }

            // Output result to CePerf/PerfScenario
            OutputDataToPerfScenario(in_szTestDescription,
                                     L"Isochronous",
                                     DWORD (AvgThroughput*1000000),
                                     DWORD (CpuUsage*1000),
                                     DWORD (MemUsage*1000),
                                     (lpPipePoint->Descriptor.wMaxPacketSize & PACKETS_PART_MASK),
                                     dwBlockSize,
                                     fOUT,
                                     FALSE,
                                     0,
                                     0,
                                     0,
                                     0,
                                     uiQparam,
                                     uiCallPrio,
                                     0!=uiFullQueLimit,
                                     in_fHighSpeed);
        }
   }


EXIT_Isoc:
    g_uiRounds = 0;
    g_uiCallbackCnt = 0;

    if (uiNumIssueTransferAPI!=0)
    {
        liTimingAPI.QuadPart = TimeRec.perfFreq.QuadPart * uiNumIssueTransferAPI;
        liTimeIssueTransferAPI.QuadPart *= 1000000000L;
        liTimeIssueTransferAPI.QuadPart /= liTimingAPI.QuadPart;
        NKDbgPrintfW(TEXT("PERF: IssueIsochTransfer() called %u times, average cost is %u.%03u usec."),
            uiNumIssueTransferAPI,liTimeIssueTransferAPI.LowPart/1000,liTimeIssueTransferAPI.LowPart%1000);
    }

    if ((*(lpDrvUsbFxns->lpClosePipe))(hPipe)==FALSE)
    {
        g_pKato->Log(LOG_FAIL, TEXT("Error at Close Pipe"));
        DEBUGMSG(DBG_ERR,(TEXT("Error at Close Pipe")));
        dwRet = TPR_FAIL;
    }

    if (pPhyBufferAddr)
    {
        FreePhysMem(pBuffer);
    }
    else if (pBuffer)
    {
        delete[] pBuffer;
    }
    if (pTrSt)
    {
        delete[] pTrSt;
    }

    if (pBlockLengths)
    {
        delete[] pBlockLengths;
    }
    if (g_hCompEvent != NULL)
    {
        CloseHandle(g_hCompEvent);
        g_hCompEvent = NULL;
    }
    if (g_hIssueNextTransfer != NULL)
    {
        CloseHandle(g_hIssueNextTransfer);
        g_hIssueNextTransfer = NULL;
    }

    return dwRet;
}// Isoch_Perf


//---------------------------------------------------------------------------
//
// Purpose:   Measure time interval.
//
//---------------------------------------------------------------------------
VOID TimeCounting(PTIME_REC pTimeRec, BOOL bStart)
{
    if (pTimeRec == NULL)
    {
        return;
    }

    LARGE_INTEGER   curTick;

    if (pTimeRec->perfFreq.QuadPart <= 1000)
    {
        curTick.LowPart = GetTickCount();
    }
    else
    {
        QueryPerformanceCounter(&curTick);
    }

    if (bStart == TRUE)
    {
        if (pTimeRec->startTime.QuadPart != 0)
        {
#ifdef EXTRA_TRACES
            NKDbgPrintfW(TEXT("This timer %x appears to be started already!\n"), pTimeRec);
#endif
            g_pKato->Log(LOG_DETAIL, TEXT("Timer appears to be started already!"));
            ASSERT(0);
        }
        pTimeRec->startTime.QuadPart = curTick.QuadPart;
    }
    else
    {
        if (pTimeRec->endTime.QuadPart != 0)
        {
#ifdef EXTRA_TRACES
            NKDbgPrintfW(TEXT("This timer %x appears to be stopped already!\n"), pTimeRec);
#endif
            g_pKato->Log(LOG_DETAIL, TEXT("Timer appears to be stopped already!"));
            ASSERT(0);
        }
        pTimeRec->endTime.QuadPart = curTick.QuadPart;
    }
}


//---------------------------------------------------------------------------
//
// Purpose:  Calculate average time
//
//---------------------------------------------------------------------------
double CalcAvgTime(const TIME_REC& TimeRec, UINT uiRounds)
{
    if (TimeRec.perfFreq.QuadPart > 1000)
    {
        LARGE_INTEGER Duration;
        Duration.QuadPart = TimeRec.endTime.QuadPart - TimeRec.startTime.QuadPart;
        double result = ((double)(Duration.QuadPart)*1000.0)/((double)(TimeRec.perfFreq.QuadPart));

        return result / (double)uiRounds;
    }
    else
    {
        return (double)(TimeRec.endTime.LowPart - TimeRec.startTime.LowPart) / (double)uiRounds;
    }
}


//---------------------------------------------------------------------------
//
// Purpose:  Calculate average throughput
//
//---------------------------------------------------------------------------
double CalcAvgThroughput(const TIME_REC& TimeRec, UINT uiRounds, DWORD dwBlockSize)
{
    double result = CalcAvgTime(TimeRec, uiRounds);
    result = ((double)(dwBlockSize*8)/result) /1000.0;
    return result;
}

//---------------------------------------------------------------------------
//
// Purpose:  Handle non-consequential notifications.
//
//---------------------------------------------------------------------------
DWORD WINAPI DummyNotify(LPVOID lpvNotifyParameter)
{
#ifdef EXTRA_TRACES
    NKDbgPrintfW(TEXT("DummyNotify(%x)\n"),lpvNotifyParameter);
#endif

    PPerfTransStatus pTransStatus=(PPerfTransStatus)lpvNotifyParameter;
    if (!pTransStatus)
        return (DWORD)-1;

    pTransStatus->fDone = TRUE;
    return 1;
}


//---------------------------------------------------------------------------
//
// Purpose:  Handle notifications for transfer complations
//           to bulk/int endpoints, measure timing
//
//---------------------------------------------------------------------------
DWORD WINAPI GenericNotify(LPVOID lpvNotifyParameter)
{
#ifdef EXTRA_TRACES
    NKDbgPrintfW(TEXT("GenericNotify(%x) currently #%u out of %u\n"),lpvNotifyParameter,g_uiGenericCallbackCnt,g_uiGenericTotalCnt);
#endif

    PTIME_REC pTimeRec = (PTIME_REC)lpvNotifyParameter;
    if (!pTimeRec)
    {
        return (DWORD)-1;
    }
    
    g_uiGenericCallbackCnt++;
    if (g_uiGenericCallbackCnt >= g_uiGenericTotalCnt)
    {
        if (g_uiGenericTotalCnt > 1 && (int)pTimeRec != 0xcccccccc)
        {
            NKDbgPrintfW(TEXT("...Start sync timing at round #%u...\n"),g_uiGenericCallbackCnt);
            TimeCounting(pTimeRec, TRUE);
        }
#if IS_PULSE_EVENT_CAUSING_TROUBLE
        SetEvent(g_hGenericEvent);
    }
    else 
    {
        PulseEvent(g_hGenericEvent);
#endif
    }
    SetEvent(g_hGenericEvent);

    return 1;
}

//---------------------------------------------------------------------------
//
// Purpose:  Handle notifications for transfer completions
//           and measure time elapsed for the trasnsfer.
//
//---------------------------------------------------------------------------
DWORD WINAPI ThroughputNotify(LPVOID lpvNotifyParameter)
{
#ifdef EXTRA_TRACES
    NKDbgPrintfW(TEXT("ThroughputNotify(%x) currently #%u out of %u\n"),lpvNotifyParameter,g_uiCallbackCnt,g_uiRounds);
#endif

    PPerfTransStatus pTransStatus=(PPerfTransStatus)lpvNotifyParameter;
    if (!pTransStatus)
        return (DWORD)-1;

    pTransStatus->fDone = TRUE;

    g_uiCallbackCnt++;
    if (g_uiCallbackCnt == 1 && g_bIsoch == TRUE) 
    {
        TimeCounting(pTransStatus->pTimeRec, TRUE);
    }

    if (g_uiCallbackCnt == g_uiRounds)
    {
        TimeCounting(pTransStatus->pTimeRec, FALSE);
        SetEvent(g_hCompEvent);
    }

    // If profiling is enabled don't measure CPU load or memory because they are skewed by profiling
    if (g_uiProfilingInterval != 0) 
    {
        pTransStatus->flCPUPercentage = 0;
        pTransStatus->flMemPercentage = 0;
    } 
    else 
    {
        pTransStatus->flCPUPercentage = USBPerf_MarkCPU();
        pTransStatus->flMemPercentage = USBPerf_MarkMem();
        USBPerf_ResetAvgs();
    }

    if (g_uiCallbackCnt >= g_uiFullQueLimit){
        SetEvent(g_hIssueNextTransfer);
    }

    return 1;
}


//---------------------------------------------------------------------------
//
// Purpose:  Print out performance results
//
//---------------------------------------------------------------------------
VOID PrintOutPerfResults(const TP_FIXEDPKSIZE& PerfResults,LPCTSTR szTestDescription)
{
    if (PerfResults.uNumofSizes == 0)
        return;

    g_pKato->Log(LOG_DETAIL, TEXT("\r\n"));
    g_pKato->Log(LOG_DETAIL, TEXT(" *** '%s' results:\r\n"),szTestDescription);
    g_pKato->Log(LOG_DETAIL, TEXT("\r\n"));

    TCHAR   szOneLine[1024] = {0};
    LPCTSTR pszFormat = TEXT("%s\r\n");

    //print the first line
    wcscpy_s(szOneLine, _countof(szOneLine), TEXT("HEAD         "));
    for(int i = 0; i < PerfResults.uNumofSizes; i++)
    {
        TCHAR   szBlockSizes[11] = {0};
        _stprintf_s(szBlockSizes, _countof(szBlockSizes), TEXT("%-10d"), PerfResults.UnitTPs[i].dwBlockSize);
        wcscat_s(szOneLine, _countof(szOneLine), szBlockSizes);
        if (wcslen(szOneLine) >= 1016)
        {
            break; 
        }
    }
    g_pKato->Log(LOG_DETAIL, pszFormat, szOneLine);
    if (g_fCeLog)
        CeLogMsg(pszFormat, szOneLine);

    //print the throughput
    memset(szOneLine, 0, sizeof(szOneLine));
    TCHAR szPacketSize[14] = {0};
    _stprintf_s(szPacketSize, _countof(szPacketSize), TEXT("%-13d"), PerfResults.usPacketSize);
    wcscpy_s(szOneLine, _countof(szOneLine), szPacketSize);
    for(int i = 0; i < PerfResults.uNumofSizes; i++)
    {
        TCHAR szTP[11] = {0};
        _stprintf_s(szTP, _countof(szTP), TEXT("%-10.3f"), PerfResults.UnitTPs[i].dbThroughput);
        wcscat_s(szOneLine, _countof(szOneLine), szTP);
        if (wcslen(szOneLine) >= 1015)
        {
            break; 
        }
    }
    g_pKato->Log(LOG_DETAIL, pszFormat, szOneLine);
    if (g_fCeLog)
    {
        CeLogMsg(pszFormat, szOneLine);
    }

    //print the CPU Usage
    memset(szOneLine, 0, sizeof(szOneLine));
    wcscpy_s(szOneLine, _countof(szOneLine), TEXT("CPU Usage    "));
    for(int i = 0; i < PerfResults.uNumofSizes; i++)
    {
        TCHAR   szCPU[11] = {0};
        _stprintf_s(szCPU, _countof(szCPU), TEXT("%-10.3f"), PerfResults.UnitTPs[i].flCPUUsage);
        wcscat_s(szOneLine, _countof(szOneLine), szCPU);
        if (wcslen(szOneLine) >= 1015)
        {
            break; 
        }
    }
    g_pKato->Log(LOG_DETAIL, pszFormat, szOneLine);
    if (g_fCeLog)
    {
        CeLogMsg(pszFormat, szOneLine);
    }

    //print the MEM Usage
    memset(szOneLine, 0, sizeof(szOneLine));
    wcscpy_s(szOneLine, _countof(szOneLine), TEXT("MEM Usage    "));
    for(int i = 0; i < PerfResults.uNumofSizes; i++)
    {
        TCHAR   szMEM[11] = {0};
        _stprintf_s(szMEM, _countof(szMEM), TEXT("%-10.3f"), PerfResults.UnitTPs[i].flMemUsage);
        wcscat_s(szOneLine, _countof(szOneLine), szMEM);
        if (wcslen(szOneLine) >= 1015)
        {
            break; 
        }
    }
    g_pKato->Log(LOG_DETAIL, pszFormat, szOneLine);
    if (g_fCeLog)
    {
        CeLogMsg(pszFormat, szOneLine);
        if (NULL != g_hFlushEvent)
        {
            PulseEvent(g_hFlushEvent);
            Sleep(500);
        }
    }

    g_pKato->Log(LOG_DETAIL, TEXT("\r\n"));
    g_pKato->Log(LOG_DETAIL, TEXT("\r\n"));
}

