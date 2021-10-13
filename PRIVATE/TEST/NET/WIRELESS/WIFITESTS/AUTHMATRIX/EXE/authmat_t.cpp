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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Implementation of the AuthMat_t class.
//
// ----------------------------------------------------------------------------

#include "AuthMat_t.hpp"

#include <AuthMatrix_t.hpp>
#include <Factory_t.hpp>
#include <WiFiConfig_t.hpp>

#include <assert.h>

using namespace ce::qa;

// The singleton instance:
static AuthMat_t *s_pSingletonInstance = NULL;
static ce::critical_section s_SingletonLocker;
  
// ----------------------------------------------------------------------------
//
// Create and/or retreive the singleton instance.
//
AuthMat_t *
AuthMat_t::
GetInstance(
    IN NetWinMain_t *pWinMain)
{
    if (NULL == s_pSingletonInstance)
    {
        ce::gate<ce::critical_section> locker(s_SingletonLocker);
        if (NULL == s_pSingletonInstance)
        {
            Factory_t::GetInstance();

            s_pSingletonInstance = new AuthMat_t(pWinMain);
            if (NULL == s_pSingletonInstance)
            {
                assert(NULL != s_pSingletonInstance);
                LogError(TEXT("Can't allocate AuthMat_t object!"));
            }
        }
    }
    return s_pSingletonInstance;
}

// ----------------------------------------------------------------------------
//
// Delete the singleton instance.
//
void
AuthMat_t::
DeleteInstance(void)
{
    if (NULL != s_pSingletonInstance)
    {
        ce::gate<ce::critical_section> locker(s_SingletonLocker);
        if (NULL != s_pSingletonInstance)
        {
            if (s_pSingletonInstance->IsAlive())
            {
                assert(NULL == "should have stopped sub-thread before this");
                s_pSingletonInstance->Stop(90*1000); // Give it 90 secs to die
                delete s_pSingletonInstance;
                s_pSingletonInstance = NULL;
            }
            
            Factory_t::DeleteInstance();
        }
    }
}
    
// ----------------------------------------------------------------------------
//
// Constructor:
//
AuthMat_t::
AuthMat_t(
    IN NetWinMain_t *pWinMain)
    : m_pWinMain(pWinMain),
      m_ThreadId(0),
      m_ThreadResult(0),
      m_InterruptHandle(NULL),
      m_pFunctionTable(NULL),
      m_pCurrentFTE(NULL),
      m_TestsPassed(NULL),
      m_TestsSkipped(NULL),
      m_TestsFailed(NULL),
      m_TestsAborted(NULL),
      m_Finished(false),
      m_StatusTime(0)
{
    assert(NULL != pWinMain);
    m_Status[0] = TEXT('\0');
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
AuthMat_t::
~AuthMat_t(void)
{
}

// ----------------------------------------------------------------------------
//
// Displays the program's command-line arguments.
//
void
AuthMat_t::
PrintUsage(void)
{
    Factory_t::GetInstance()->PrintUsage();
}

// ----------------------------------------------------------------------------
//
// Initialize the configuration from the specified command-line args.
//
DWORD
AuthMat_t::
Init(
    IN int    argc,
    IN TCHAR *argv[])
{
    DWORD result = ERROR_SUCCESS;
 
    // Initialize the test-factory.
    Factory_t *pFactory = Factory_t::GetInstance();
    if (NULL == pFactory)
    {
        result = ERROR_OUTOFMEMORY;
        LogError(TEXT("Can't allocate test-factory"));
        return result;
    }
    
    // Parse the command-line options.
    result = pFactory->ParseCommandLine(argc, argv);
    if (ERROR_SUCCESS != result)
        return result;
    
    // Generate the function-table.
    result = pFactory->GetFunctionTable(&m_pFunctionTable);
    if (ERROR_SUCCESS != result)
        return result;
            
    return result;
}

// ----------------------------------------------------------------------------
//
// Starts the tests running in a sub-thread.
//
DWORD
AuthMat_t::
Start(
    IN HANDLE InterruptHandle)
{
    ce::gate<ce::critical_section> locker(m_Locker);
    
    Wait(100);
    m_InterruptHandle = InterruptHandle;

    if (!IsAlive())
    {
        m_ThreadHandle = CreateThread(NULL, 0, AuthMat_t::ThreadProc,
                                     (PVOID)this, 0, &m_ThreadId);
        if (!IsAlive())
        {
            return GetLastError();
        }
    }
    
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Thread call-back procedure.
//
DWORD WINAPI
AuthMat_t::
ThreadProc(
    IN LPVOID pParameter)
{
    AuthMat_t *testThread = (AuthMat_t *) pParameter;
    assert(testThread != NULL);
    return (DWORD) testThread->Run();
}
        
// ----------------------------------------------------------------------------
//
// Runs the sub-thread.
//
DWORD
AuthMat_t::
Run(void)
{
    DWORD result = ERROR_SUCCESS;
    long suiteTime = GetTickCount();
    
    // For each test...
    long totalTestTime = 0;
    for (m_pCurrentFTE = m_pFunctionTable ;; ++m_pCurrentFTE)
    {
        if (m_pWinMain->IsInterrupted())
        {
            m_Finished = false;
            break;
        }
        if (NULL == m_pCurrentFTE->lpDescription 
         || 0    == m_pCurrentFTE->dwUniqueID)
        {
            m_Finished = true;
            break;
        }
        if (NULL == m_pCurrentFTE->lpTestProc)
            continue;

        // Log the test start.
        LogAlways(TEXT("<TESTCASE ID=%u>"), m_pCurrentFTE->dwUniqueID);
        LogAlways(TEXT("*** vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv"));
        LogAlways(TEXT("*** TEST STARTING"));
        LogAlways(TEXT("***"));
        LogAlways(TEXT("*** Test Name:      %s"), m_pCurrentFTE->lpDescription);
        LogAlways(TEXT("*** Test ID:        %u"), m_pCurrentFTE->dwUniqueID);
        LogAlways(TEXT("*** Command Line:   %s"), m_pWinMain->GetCommandLine());
        LogAlways(TEXT("*** vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv"));

        LogStatus(TEXT("BEGIN TEST: %u \"%s\""), 
                  m_pCurrentFTE->dwUniqueID, 
                  m_pCurrentFTE->lpDescription);

        // Run the test.
        long testTime = GetTickCount();
        result = m_pCurrentFTE->lpTestProc(TPM_EXECUTE, 0, m_pCurrentFTE);
        testTime = WiFUtils::SubtractTickCounts(GetTickCount(), testTime);
        totalTestTime += testTime;

        // Log the results.
        const TCHAR *pResultName;
        switch (result)
        {
            case TPR_PASS:
                pResultName = TEXT("PASSED");
                InterlockedIncrement(&m_TestsPassed);
                break;
            case TPR_SKIP:
                pResultName = TEXT("SKIPPED");
                InterlockedIncrement(&m_TestsSkipped);
                break;
            case TPR_FAIL:
                pResultName = TEXT("FAILED");
                InterlockedIncrement(&m_TestsFailed);
                break;
            default:
                pResultName = TEXT("ABORTED");
                InterlockedIncrement(&m_TestsAborted);
                break;
        }
        
        LogStatus(TEXT("END TEST: %u \"%s\", %s, Time=%u.%03u"),
                  m_pCurrentFTE->dwUniqueID, 
                  m_pCurrentFTE->lpDescription, 
                  pResultName, 
                  testTime / 1000, 
                  testTime % 1000);

        LogAlways(TEXT("*** ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"));
        LogAlways(TEXT("*** TEST COMPLETED"));
        LogAlways(TEXT("***"));
        LogAlways(TEXT("*** Test Name:      %s"), m_pCurrentFTE->lpDescription);
        LogAlways(TEXT("*** Test ID:        %u"), m_pCurrentFTE->dwUniqueID);
        LogAlways(TEXT("*** Command Line:   %s"), m_pWinMain->GetCommandLine());
        LogAlways(TEXT("*** Result:         %s"), pResultName);
        LogAlways(TEXT("*** Execution Time: %ld:%02ld:%02ld.%03ld"),
                 (testTime / (1000*60*60)),
                 (testTime / (1000*60)) % 60,
                 (testTime /  1000) % 60,
                 (testTime % 1000));
        LogAlways(TEXT("*** ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"));
        LogAlways(TEXT("\n"));
        LogAlways(TEXT("</TESTCASE RESULT=\"%s\">"), pResultName);
    }

    // Log the final results.
    suiteTime = WiFUtils::SubtractTickCounts(GetTickCount(), suiteTime);
    
    LogAlways(TEXT("*** =================================================================="));
    LogAlways(TEXT("*** SUITE SUMMARY"));
    LogAlways(TEXT("***"));
    LogAlways(TEXT("*** Passed:        %-3ld"), m_TestsPassed);
    LogAlways(TEXT("*** Failed:        %-3ld"), m_TestsFailed);
    LogAlways(TEXT("*** Skipped:       %-3ld"), m_TestsSkipped);
    LogAlways(TEXT("*** Aborted:       %-3ld"), m_TestsAborted);
    LogAlways(TEXT("*** -------- ---------"));
    LogAlways(TEXT("*** Total:         %-3ld"), m_TestsPassed + m_TestsSkipped,
                                              + m_TestsFailed + m_TestsAborted);
    LogAlways(TEXT("***"));

    LogAlways(TEXT("*** Cumulative Test Execution Time: %ld:%02ld:%02ld.%03ld"),
                 (totalTestTime / (1000*60*60)),
                 (totalTestTime / (1000*60)) % 60,
                 (totalTestTime /  1000) % 60,
                 (totalTestTime % 1000));
    
    LogAlways(TEXT("*** Total AuthMat   Execution Time: %ld:%02ld:%02ld.%03ld"),
                 (suiteTime / (1000*60*60)),
                 (suiteTime / (1000*60)) % 60,
                 (suiteTime /  1000) % 60,
                 (suiteTime % 1000));
    
    LogAlways(TEXT("*** =================================================================="));

    // Tell the rest of the program to stop.
    m_pWinMain->KillMainWindow();
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Waits the specified time then interrupts the test thread.
// Returns immediately if the thread is already done.
// If grace-time is non-zero, terminates the thread if it ignores
// the interruption too long.
//
DWORD
AuthMat_t::
Wait(
    IN long MaxWaitTimeMS, 
    IN long MaxGraceTimeMS)
{
    if (IsAlive())
    {
        DWORD oldThreadId = m_ThreadId;

        // Do this twice, once for the normal termination and once for
        // the interrupt.
        for (int tries = 0 ; ; ++tries)
        {
            DWORD ret = WaitForSingleObject(m_ThreadHandle, MaxWaitTimeMS); 
            DWORD err = GetLastError();
            ce::gate<ce::critical_section> locker(m_Locker);
            if (oldThreadId == m_ThreadId)
            {
                if (WAIT_OBJECT_0 == ret)
                {
                    if (GetExitCodeThread(m_ThreadHandle, &ret))
                         m_ThreadResult = ret;
                    else m_ThreadResult = GetLastError();
                    m_ThreadHandle.close();
                }
                else
                if (WAIT_TIMEOUT == ret)
                {
                    if (0L >= MaxGraceTimeMS || NULL == m_InterruptHandle)
                    {
                        return ERROR_TIMEOUT;
                    }
                    else
                    {
                        if (0 == tries)
                        {
                            MaxWaitTimeMS = MaxGraceTimeMS;
                            SetEvent(m_InterruptHandle);
                            continue;
                        }
                        TerminateThread(m_ThreadHandle, ret);
                        m_ThreadHandle.close();
                        m_ThreadResult = ret;
                        SetLastError(ret);
                    }
                }
                else
                {
                    m_ThreadHandle.close();
                    m_ThreadResult = err;
                    SetLastError(err);
                }
            }
            break;
        }
    }
    
    return m_ThreadResult;
}

// ----------------------------------------------------------------------------
//
// Interrupts the test thread.
// If grace-time is non-zero, terminates the thread if it ignores
// the interruption too long.
//
DWORD
AuthMat_t::
Stop(
    IN long MaxGraceTimeMS)
{
    return Wait(1, MaxGraceTimeMS);
}

#define WIDTH(a)  (a.right  - a.left)
#define HEIGHT(a) (a.bottom - a.top)


// ----------------------------------------------------------------------------
//
// This ia a private internal class which contains enough information to
// allow DrawScreen to write info into the screen.
//
class ScreenContext
{
private:

    LONG  m_TextHeight;
    LONG  m_WideHeight;
    RECT  m_Rect, m_RectClient;
    
    HDC     m_hDC;
    HDC     m_hMemDC;
    HBITMAP m_hMemBitmap;

public:

    ScreenContext(HWND hWnd, HDC hdc);
   ~ScreenContext(void);

    int GetColumnIndent(
        int LeftIndent,
        int NumColumns,
        int ColumnIX)
    {
        int indent = LeftIndent;
        if (1 <= ColumnIX && ColumnIX < NumColumns && NumColumns >= 2)
        {
            indent += ((m_Rect.right - LeftIndent) / NumColumns) * ColumnIX;
        }
        return indent;
    }

    void AddMessage(int indent, const TCHAR *pFormat, ...);
    
    void AddNewline(void)
    {
        m_Rect.top    += m_TextHeight;
        m_Rect.bottom += m_TextHeight;
    }

    enum { WideLineExpansion = 130 }; // pct height expansion of "wide" lines 
    void AddWideline(void)
    {
        m_Rect.top    += m_WideHeight;
        m_Rect.bottom += m_WideHeight;
    }

    void Refresh(void);
};

ScreenContext::
ScreenContext(HWND hWnd, HDC hdc)
{
    m_TextHeight = 18;
    m_Rect.left = 0;
    m_Rect.top = 0;
    m_Rect.right = 300;
    m_Rect.bottom = m_TextHeight;
    
    m_hDC        = hdc;
    m_hMemDC     = NULL;
    m_hMemBitmap = NULL;

    GetClientRect(hWnd, &m_RectClient);

    m_Rect.right = m_RectClient.right;
        
    // Create a memory device contex to draw into
    m_hMemDC = CreateCompatibleDC(hdc);

    m_hMemBitmap = CreateCompatibleBitmap(hdc, WIDTH (m_RectClient),
                                               HEIGHT(m_RectClient));

    SelectObject(m_hMemDC, m_hMemBitmap);

    // Fill the background with white color.
    FillRect(m_hMemDC, &m_RectClient, (HBRUSH)GetStockObject(WHITE_BRUSH));

    // Get the height of the Text.
    TEXTMETRIC TextMetric = {0};
    HDC hFontDC = CreateDC(NULL,NULL,NULL,NULL);
    GetTextMetrics(hFontDC, &TextMetric);
    m_TextHeight = TextMetric.tmHeight + TextMetric.tmExternalLeading;
    DeleteDC(hFontDC);

    m_WideHeight = ((m_TextHeight * WideLineExpansion) + 50) / 100;

    m_Rect.bottom = m_TextHeight;
}

ScreenContext::
~ScreenContext(void)
{
    DeleteDC(m_hMemDC);

    if (m_hMemBitmap)
    {
        DeleteObject(m_hMemBitmap);
    }
}

void
ScreenContext::
Refresh(void)
{
    BitBlt(m_hDC, 0, 0, WIDTH(m_RectClient), HEIGHT(m_RectClient), 
           m_hMemDC, 0, 0, SRCCOPY);
}

void
ScreenContext::
AddMessage(int indent, const TCHAR *pFormat, ...)
{
    va_list vArgs;
    va_start(vArgs, pFormat);
    ce::tstring message;
    HRESULT hr = WiFUtils::FmtMessageV(&message, pFormat, vArgs);
    va_end(vArgs);
    if (SUCCEEDED(hr))
    {
        m_Rect.left = indent;
        DrawText(m_hMemDC, message, message.length(), &m_Rect, DT_LEFT|DT_TOP);
    }
}

// ----------------------------------------------------------------------------
//
// Formats the security modes from the specified WiFi config.
//
static void
FormatSecurityModes(
  __out_ecount(BuffChars) TCHAR *pBuffer,
                          int     BuffChars,
              const WiFiConfig_t &Config)
{
    HRESULT hr;
    switch (Config.GetAuthMode())
    {
        default:
            hr = StringCchPrintf(pBuffer, BuffChars, TEXT("%s"),
                                 Config.GetAuthName());
            break;
        case APAuthOpen:
        case APAuthShared:
        case APAuthWPA_PSK:
        case APAuthWPA2_PSK:
            hr = StringCchPrintf(pBuffer, BuffChars, TEXT("%s/%s"),
                                 Config.GetAuthName(),
                                 Config.GetCipherName());
            break;
        case APAuthWEP_802_1X:
        case APAuthWPA:
        case APAuthWPA2:
            hr = StringCchPrintf(pBuffer, BuffChars, TEXT("%s/%s/%s"),
                                 Config.GetAuthName(),
                                 Config.GetCipherName(),
                                 Config.GetEapAuthName());
            break;
    }
    if (FAILED(hr))
        pBuffer[0] = TEXT('\0');
}

// ----------------------------------------------------------------------------
//
// Draws the current test status in the specified window using the
// specified drawing context.
//
void
AuthMat_t::
Draw(
    IN HWND hWnd,
    IN HDC  hdc)
{
    TCHAR buffer[MAX_PATH];
    TCHAR runTime[30];
    TCHAR testTime[30];
    
    ScreenContext screen(hWnd,hdc);

    int col1 = 12;
    int col2 = screen.GetColumnIndent(col1, 7, 3);

    Utils::FormatRunTime(m_pWinMain->GetStartTime(),
                         runTime, COUNTOF(runTime));
    
    if (!m_pCurrentFTE
     || !m_pCurrentFTE->dwUniqueID
     || !m_pCurrentFTE->lpDescription)
    {
        if (m_pCurrentFTE)
             screen.AddMessage(2, TEXT("Finished"));
        else screen.AddMessage(2, TEXT("Waiting"));
        screen.AddMessage(col2, runTime);
        screen.AddWideline();
    }
    else
    {
        // Get the current test config.
        WiFiConfig_t apConfig, config; long startTime;
        bool started = AuthMatrix_t::GetTestConfig(&apConfig, 
                                                     &config,
                                                     &startTime);
        
        screen.AddMessage(2, TEXT("Test #%u"), m_pCurrentFTE->dwUniqueID);
        if (!started)
            screen.AddMessage(col2, runTime);
        else
        {
            Utils::FormatRunTime(startTime, testTime, COUNTOF(testTime));
            SafeCopy  (buffer, runTime,     COUNTOF(buffer));
            SafeAppend(buffer, TEXT(" - "), COUNTOF(buffer));
            SafeAppend(buffer, testTime,    COUNTOF(buffer));
            screen.AddMessage(col2, buffer);
        }
        screen.AddNewline();
        
        screen.AddMessage(col1, TEXT("%s"), m_pCurrentFTE->lpDescription);
        screen.AddWideline();

        // If possible, display the current security modes.
        if (started)
        {
            screen.AddMessage(2, TEXT("AP:"));
            screen.AddNewline();

            screen.AddMessage(col1, TEXT("ssid"));
            screen.AddMessage(col2, TEXT("%s"), apConfig.GetSSIDName());
            screen.AddNewline();

            screen.AddMessage(col1, TEXT("auth"));
            FormatSecurityModes(buffer, COUNTOF(buffer), apConfig);
            screen.AddMessage(col2, TEXT("%s"), buffer);

            if (apConfig.GetKey()[0])
            {
                screen.AddNewline();
                screen.AddMessage(col1, TEXT("key"));
                screen.AddMessage(col2, apConfig.GetKey());
            }
            
            screen.AddWideline();

            screen.AddMessage(2, TEXT("STA:"));
            screen.AddNewline();

            screen.AddMessage(col1, TEXT("auth"));
            FormatSecurityModes(buffer, COUNTOF(buffer), config);
            screen.AddMessage(col2, TEXT("%s"), buffer);
            
            if (config.GetKey()[0])
            {
                screen.AddNewline();
                screen.AddMessage(col1, TEXT("key"));
                screen.AddMessage(col2, config.GetKey());
            }

            screen.AddWideline();
        }
    }
    
    // Display the stats.
    screen.AddMessage(2, TEXT("Summary:"));
    screen.AddNewline();
    
    screen.AddMessage(col1, TEXT("passed"));
    screen.AddMessage(col2, TEXT("%ld"), m_TestsPassed);
    screen.AddNewline();
                        
    screen.AddMessage(col1, TEXT("failed"));
    screen.AddMessage(col2, TEXT("%ld"), m_TestsFailed);
    screen.AddNewline();
    
    screen.AddMessage(col1, TEXT("skipped"));
    screen.AddMessage(col2, TEXT("%ld"), m_TestsSkipped);
    screen.AddNewline();

    if (m_TestsAborted)
    {
        screen.AddMessage(col1, TEXT("aborted"));
        screen.AddMessage(col2, TEXT("%ld"), m_TestsAborted);
        screen.AddNewline();
    }
            
    screen.AddMessage(col1, TEXT("total"));
    screen.AddMessage(col2, TEXT("%ld"), m_TestsPassed + m_TestsSkipped 
                                       + m_TestsFailed + m_TestsAborted);
    screen.AddWideline();

    // Display the status message.
    if (m_Status[0])
    {
        screen.AddMessage(2, TEXT("%s"), m_Status);
        screen.AddNewline();
    }

    screen.Refresh();
}

// ----------------------------------------------------------------------------
//
// Displays the specified message in the status area of the UI.
//
static HRESULT
FormatStatusMsg(
  __out_ecount(BuffChars) OUT TCHAR       *pBuffer,
                          IN  int           BuffChars,
                          IN  const TCHAR *pFormat,
                          IN  va_list       VarArgs)
{
    HRESULT hr = E_FAIL;

    _try
    {
        hr = StringCchVPrintfEx(pBuffer, BuffChars, NULL, NULL,
                                STRSAFE_IGNORE_NULLS,
                                pFormat, 
                                VarArgs);
        if (FAILED(hr))
        {
            hr = StringCchPrintfEx(pBuffer, BuffChars, NULL, NULL,
                                   STRSAFE_IGNORE_NULLS,
                                   TEXT("Can't format \"%.128s\""), 
                                   pFormat);
        }
    }
   __except(1)
    {
        hr = StringCchPrintfEx(pBuffer, BuffChars, NULL, NULL,
                               STRSAFE_IGNORE_NULLS,
                               TEXT("Exception formatting \"%.128s\""),
                               pFormat);
    }

    return hr;
}

void
AuthMat_t::
SetStatus(
    IN const TCHAR *pFormat, ...)
{
    TCHAR buffer[MAX_PATH];
    
    va_list vArgs;
    va_start(vArgs, pFormat);
    HRESULT hr = FormatStatusMsg(buffer, COUNTOF(buffer)-1, pFormat, vArgs);
    va_end(vArgs);

    if (SUCCEEDED(hr))
    {
        ce::gate<ce::critical_section> locker(m_Locker);
        SafeCopy(m_Status, buffer, COUNTOF(m_Status));
        m_StatusTime = GetTickCount();
    }
}

// ----------------------------------------------------------------------------
//
// Clears the status area of the UI.
//
void
AuthMat_t::    
ClearStatus(void)
{
    ce::gate<ce::critical_section> locker(m_Locker);
    m_Status[0] = TEXT('\0');
}

// ----------------------------------------------------------------------------
