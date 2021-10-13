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
Module Name:

     ifdmsg.cpp

Abstract:
Functions:
Notes:
--*/

#ifndef UNDER_CE
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>

#include <afx.h>
#include <afxtempl.h>

#else
#include "afxutil.h"
#endif

#include <winioctl.h>
#include <winsmcrd.h>
#include "ifdtest.h"

// Global used for reading String resources.
extern HINSTANCE g_hInst;


#define LOG_DEVICE_NAME L"$logfile:file=ifdtest.xml"
// Our log file
static FILE *g_pLogFile;

// These are used by the Testxxx functions
static BOOLEAN g_bTestFailed;   // This tracks if a subtest fails.
static BOOLEAN g_bReaderFailed; // This tracks if *any* TestCheck call has ever failed.
static BOOLEAN g_bTestStart;

// WTT Logger Globals
static BOOL g_bLogDeviceCreated;
static LONG g_hLogDevice;


CStringResourceNotFoundException::CStringResourceNotFoundException(UINT l_uId) {
    TCHAR l_rgchMsg[128];
    HRESULT hr = StringCchPrintf(l_rgchMsg, 
        128, 
        L"Internal Error: String Resource ID #%u not found", 
        l_uId);
    SetMessage(l_rgchMsg);
}

CWttLoggerException::CWttLoggerException(HRESULT in_hr) {
    TCHAR l_rgchMsg[128];
    HRESULT hr = StringCchPrintf(l_rgchMsg, 
        128,
        L"Internal Error: logger function call failed with HRESULT 0x%x",
        in_hr);
    SetMessage(l_rgchMsg);
}

void
LogOpen(
    __in LPCTSTR in_pchFileName
    )
{
    WCHAR l_rgchFileName[128];

    HRESULT hr = StringCchPrintf(l_rgchFileName,128,L"%s.log", in_pchFileName);
    g_pLogFile = _wfopen(l_rgchFileName, L"w");

#ifdef WTT_ENABLED
    hr= WTTLogCreateLogDevice(LOG_DEVICE_NAME, &g_hLogDevice);

    g_bLogDeviceCreated= SUCCESS(hr);

    if (!g_bLogDeviceCreated)
    {
        // handle log device creation error
        throw CWttLoggerException(hr);
    }
#endif WTT_ENABLED
}

void
LogClose(
    void
    )
{
#ifdef WTT_ENABLED
    if (g_bLogDeviceCreated) {

        HRESULT hr = WTTLogPFRollup(g_hLogDevice, 0, 0, 0, 0, 0);

        if (!SUCCESS(hr)) {
            throw CWttLoggerException(hr);
        }

        WTTLogCloseLogDevice(g_hLogDevice, LOG_DEVICE_NAME);
        WTTLogUninit();
    }
#endif WTT_ENABLED

    if (NULL!=g_pLogFile) {
        fclose(g_pLogFile);
    }
}

CString TestLoadStringResource(UINT id)
{
    CString l_strRet;
    const BYTE* pRes = 0;
    PBYTE pBuff = 0;
    
    pRes = (const BYTE *)LoadString(g_hInst, id, NULL, 0);
    if (pRes)
    {
        if (NULL == pRes-2)
        {
            throw CStringResourceNotFoundException(id);
        }

        // Get the length of the string and generate 
        // a zero terminated version.
        UINT nLen = *(const WORD *)(pRes-2) * sizeof (TCHAR);
        pBuff = (PBYTE)LocalAlloc(LPTR, nLen + 2);

        ASSERTMSG(L"OOM: Could not allocate memory in TestLoadStringResource!", pBuff);
        if (NULL != pBuff) 
        {
            memcpy(pBuff, pRes, nLen);
            *(TCHAR *)(pBuff + nLen) = TEXT('\0');
            l_strRet = (LPCTSTR)pBuff;
            LocalFree(pBuff);
        }
    }
    else
    {
        throw CStringResourceNotFoundException(id);
    }

    return l_strRet;
}

static
void
LogMessagep(
    __in DWORD in_dwVerbosity,
    __in LPCTSTR in_pchFormat,
    __in va_list in_pArg
    )
{
    const DWORD BUFF_SIZE = 128;
    WCHAR l_rgchBuffer[BUFF_SIZE];
    va_list l_pArg= in_pArg;

    HRESULT hr = StringCchVPrintf(l_rgchBuffer,
        BUFF_SIZE,
        in_pchFormat,
        l_pArg
        );

    g_pKato->Log(in_dwVerbosity, l_rgchBuffer);

    if (in_dwVerbosity == LOG_CONSOLE) {
        wprintf(L"%s\n", l_rgchBuffer);
    }

    if (g_pLogFile && in_dwVerbosity == LOG_CONSOLE) {
        fwprintf(g_pLogFile, L"%s\n", l_rgchBuffer);
    }

#ifdef WTT_ENABLED
    if (g_bLogDeviceCreated) {
        hr= WTTLogTrace(g_hLogDevice, WTT_LVL_MSG, l_rgchBuffer);

        if (!SUCCESS(hr)) {
            // handle log failure here
            throw CWttLoggerException(hr);
        }

    }
#endif WTT_ENABLED
}

static
void
TestStartp(
    __in LPCTSTR in_pchFormat,
    __in va_list in_pArg
    )
{
    WCHAR l_rgchBuffer[128];
    va_list l_pArg= in_pArg;

    if (g_bTestStart) {
        wprintf(
            L"%s",
            TestLoadStringResource(IDS_IFD_WARN_MISSING_TEST_RESULT).GetBuffer(0)
            );
    }

    g_bTestStart = TRUE;
    g_bTestFailed = FALSE;

    HRESULT hr = StringCchVPrintf(l_rgchBuffer, 128,  in_pchFormat, l_pArg);

    g_pKato->Log(LOG_DETAIL, l_rgchBuffer);

    wprintf(L"   %-50s", l_rgchBuffer);
    if (g_pLogFile) {

        fwprintf(g_pLogFile, L"   %-50s", l_rgchBuffer);
    }

#ifdef WTT_ENABLED
    if (g_bLogDeviceCreated) {

        g_pTestName= l_rgchBuffer;
        hr = WTTLogStartTestEx(g_hLogDevice, 
                             GetTestCaseUID(),
                             g_pTestName.GetBuffer(0),
                             WTTLOG_TESTCASE_TYPE_NOPARENT,
                             NULL,
                             NULL);

        if (!SUCCESS(hr)) {
            // handle test start failure here
            throw CWttLoggerException(hr);
        }
    }
#endif WTT_ENABLED
}

static
void
TestMsg(
    BOOL in_bTestEnd,
    BOOL in_bTestResult,
    __in LPCTSTR in_pchFormat,
    va_list in_pArg
    );

static
void
TestMsg(
    BOOL in_bTestEnd,
    BOOL in_bTestResult,
    __in UINT in_uFormat,
    va_list in_pArg
    )
{
    TestMsg(in_bTestEnd, in_bTestResult, TestLoadStringResource(in_uFormat).GetBuffer(0), in_pArg);
}

static
void
TestMsg(
    BOOL in_bTestEnd,
    BOOL in_bTestResult,
    __in LPCTSTR in_pchFormat,
    va_list in_pArg
    )
{
    const ULONG BUFFER_SIZE = 1000;
    HRESULT hr;
    WCHAR l_rgchBuffer[BUFFER_SIZE];

    if (in_bTestEnd && g_bTestStart == FALSE) {

        wprintf(
            L"%s",
            TestLoadStringResource(IDS_IFD_WARN_MISSING_TEST_START).GetBuffer(0)
            );
    }

    hr = StringCchVPrintf(l_rgchBuffer, BUFFER_SIZE, in_pchFormat, in_pArg);

    if (in_bTestResult == FALSE && g_bTestFailed == FALSE) {

        g_bTestFailed = TRUE;
        g_bReaderFailed = TRUE;
        wprintf(TestLoadStringResource(IDS_IFD_FAILED_STRING), l_rgchBuffer);

        g_pKato->Log(LOG_DETAIL, 
            (TestLoadStringResource(IDS_IFD_FAILED_STRING), l_rgchBuffer));

        if (g_pLogFile) {
            fwprintf(g_pLogFile, 
                TestLoadStringResource(IDS_IFD_FAILED_STRING),
                l_rgchBuffer);
        }

#ifdef WTT_ENABLED
        if (g_bLogDeviceCreated) {

            hr= WTTLogTrace(g_hLogDevice, 
                            WTT_LVL_ERR,
                            0xDEADBEEF,
                            0x1,
                            __WFILE__,
                                __LINE__,
                            l_rgchBuffer);
            
            if (!SUCCESS(hr)) {
                // handle log failure
                throw CWttLoggerException(hr);
            }
        }
#endif WTT_ENABLED
    }

    if (in_bTestEnd) {
        if (g_bTestFailed != TRUE) {
            LogMessage(LOG_CONSOLE,  IDS_IFD_PASSED);
        }
    }
}

static
void
TestCheckp(
    BOOL in_bResult,
    __in LPCTSTR in_pchFormat,
    __in va_list in_pArg
    )
{
    va_list l_pArg= in_pArg;

    TestMsg(FALSE, in_bResult, in_pchFormat, l_pArg);
}

void
TestCheck(
    ULONG in_lResult,
    __in LPCTSTR in_pchOperator,
    __in_bcount(in_uExpectedLength) const ULONG in_uExpectedResult,
    ULONG in_uResultLength,
    ULONG in_uExpectedLength,
    UCHAR in_chSw1,
    UCHAR in_chSw2,
    UCHAR in_chExpectedSw1,
    UCHAR in_chExpectedSw2,
    const UCHAR* in_pchData,
    const UCHAR* in_pchExpectedData,
    ULONG  in_uDataLength
    )
/*++

Routine Description:

    This function checks the return code, the number of bytes
    returned, the card status bytes and the data returned by
    a call to CReader::Transmit.

--*/
{
    const ULONG BUFFER_SIZE = 1000;
    HRESULT hr;
    // detect incorrect returned IOCTL status value
    if (wcscmp(in_pchOperator, L"==") == 0 &&
        in_lResult != in_uExpectedResult ||
        wcscmp(in_pchOperator, L"!=") == 0 &&
        in_lResult == in_uExpectedResult) {

        TestCheck(
            FALSE,
            IDS_IFD_IOCTL_FAILED,
            in_lResult,
            MapWinErrorToNtStatus(in_lResult),
            (wcscmp(in_pchOperator, L"!=") == 0 ? TestLoadStringResource(IDS_IFD_NOT) : TestLoadStringResource(IDS_IFD_NULL_STRING)),
            in_uExpectedResult,
            MapWinErrorToNtStatus(in_uExpectedResult)
            );

    // otherwise, look for returned byte data length incorrect from the IOCTL.  If equal, this will imply that
    // the data field returned from the card equal in length, as well.
    } else if (in_uResultLength != in_uExpectedLength) {

        TestCheck(
            FALSE,
            IDS_IFD_IOCTL_WRONG_NUMBYTES,
            in_uResultLength,
            in_uExpectedLength
            );

    // else look for status bytes unexpected value
    } else if (in_chSw1 != in_chExpectedSw1 ||
               in_chSw2 != in_chExpectedSw2){

        TestCheck(
            FALSE,
            IDS_IFD_WRONG_STATUS,
            in_chSw1,
            in_chSw2,
            in_chExpectedSw1,
            in_chExpectedSw2
            );

    // finally, check to see if the data matches expectations
    } else if (memcmp(in_pchData, in_pchExpectedData, in_uDataLength)) {

        // Data did not match.  Expand into hex string representations in two buffers

        WCHAR l_rgchData[BUFFER_SIZE], l_rgchExpectedData[BUFFER_SIZE];
        
        //make sure that the print function doesn't break out of the arrays
        for (ULONG i = 0; ((i < in_uDataLength) && (i*3 < (BUFFER_SIZE-4))); i++) {

            hr = StringCchPrintf(l_rgchData + i*3, BUFFER_SIZE - i*3, L"%02X ", in_pchData[i]);
            hr = StringCchPrintf(l_rgchExpectedData + i*3, BUFFER_SIZE - i*3,  L"%02X ", in_pchExpectedData[i]);

            if ((i + 1) % 24 == 0)  {

                l_rgchData[i * 3 + 2] = '\n';
                l_rgchExpectedData[i * 3 + 2] = '\n';
            }
        }

        TestCheck(
            FALSE,
            IDS_IFD_IOCTL_BAD_DATA,
            l_rgchData,
            l_rgchExpectedData
            );
    }
}

void
TestEnd(
    void
    )
/*++

Routine Description:

    A call to this function marks the end of a test sequence.
    A sequence is usually look like:

    TestStart(Message)
    CReaderTransmit or DeviceIoControl
    TestCheck(...)
    TestEnd()

--*/
{
#ifdef _M_ALPHA
    va_list l_pArg;
#else
    va_list l_pArg = NULL;
#endif
    TestMsg(TRUE, TRUE, IDS_IFD_NULL_STRING, l_pArg);
    g_bTestStart = FALSE;
#ifdef WTT_ENABLED
    if (g_bLogDeviceCreated) {

        // the following return value would be overriden in case an error is logged
        HRESULT hr= WTTLogEndTestEx(g_hLogDevice, 
                                    WTT_TESTCASE_RESULT_PASS,
                                    GetTestCaseUID(),
                                    g_pTestName.GetBuffer(0), 
                                    NULL,
                                    NULL);

        if (!SUCCESS(hr)) {
            // handle end test case failure
            throw CWttLoggerException(hr);
        }
    }
#endif WTT_ENABLED
}

BOOLEAN
TestFailed(
    void
    )
{
    return g_bTestFailed;
}

BOOLEAN
ReaderFailed(
    void
    )
{
     return g_bReaderFailed;
}

void
ResetReaderFailedFlag(
    void
    )
{
    g_bReaderFailed = FALSE;
}
                      

// stubs to load resources and call internal functions XXXp
void
LogMessage(
    __in DWORD in_dwVerbosity,
    __in UINT in_uFormat,
    ...
    )
{
    va_list l_pArg;
    va_start(l_pArg, in_uFormat);
    LogMessagep(in_dwVerbosity,
                TestLoadStringResource(in_uFormat).GetBuffer(0),
                l_pArg);
    va_end(l_pArg);
}

void
TestStart(
    __in UINT in_uFormat,
    ...
    )
{
    va_list l_pArg;
    va_start(l_pArg, in_uFormat);
    TestStartp(TestLoadStringResource(in_uFormat).GetBuffer(0), l_pArg);
    va_end(l_pArg);
}

void
TestCheck(
    BOOL in_bResult,
    __in UINT in_uFormat,
    ...
    )
{
    va_list l_pArg;
    va_start(l_pArg, in_uFormat);
    TestCheckp(in_bResult, TestLoadStringResource(in_uFormat).GetBuffer(0), l_pArg);
    va_end(l_pArg);
}

// below are the depricated calls till we port all strings back to resources
void
LogMessage(
    __in DWORD in_dwVerbosity,
    __in LPCTSTR in_pchFormat,
    ...
    )
{
    va_list l_pArg;
    va_start(l_pArg, in_pchFormat);
    LogMessagep(in_dwVerbosity, in_pchFormat, l_pArg);
    va_end(l_pArg);
}

void
TestStart(
    __in LPCTSTR in_pchFormat,
    ...
    )
{
    if (in_pchFormat != NULL)
    {
        va_list l_pArg;
        va_start(l_pArg, in_pchFormat);
        TestStartp(in_pchFormat, l_pArg);
        va_end(l_pArg);
    }
}

void
TestCheck(
    BOOL in_bResult,
    __in LPCTSTR in_pchFormat,
    ...
    )
{
    if (in_pchFormat != NULL)
    {
        va_list l_pArg;
        va_start(l_pArg, in_pchFormat);
        TestCheckp(in_bResult, in_pchFormat, l_pArg);
        va_end(l_pArg);
    }
}
