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
// Implementation of the WiFiRoam_t class.
//
// ----------------------------------------------------------------------------

#include "WiFiRoam_t.hpp"
#include "NetlogAnalyser_t.hpp"

#include <Utils.hpp>

#include <assert.h>
#include <backchannel.h>

using namespace ce::qa;

// The singleton instance:
static WiFiRoam_t *s_pSingletonInstance = NULL;
static ce::critical_section s_SingletonLocker;

// ----------------------------------------------------------------------------
//
// Creates and/or retreives the singleton instance.
//
WiFiRoam_t *
WiFiRoam_t::
GetInstance(
    IN NetWinMain_t *pWinMain)
{
    if (NULL == s_pSingletonInstance)
    {
        ce::gate<ce::critical_section> locker(s_SingletonLocker);
        if (NULL == s_pSingletonInstance)
        {
            NetlogAnalyser_t::StartupInitialize();
            
            s_pSingletonInstance = new WiFiRoam_t(pWinMain);
            if (NULL == s_pSingletonInstance)
            {
                assert(NULL != s_pSingletonInstance);
                LogError(TEXT("Can't allocate WiFiRoam_t object!"));
            }
        }
    }
    return s_pSingletonInstance;
}

// ----------------------------------------------------------------------------
//
// Deletes the singleton instance.
//
void
WiFiRoam_t::
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

                NetlogAnalyser_t::ShutdownCleanup();
            }
        }
    }
}
    
// ----------------------------------------------------------------------------
//
// Constructor:
//
WiFiRoam_t::
WiFiRoam_t(
    IN NetWinMain_t *pWinMain)
    : m_pWinMain(pWinMain),
      m_ThreadId(0),
      m_ThreadResult(0),
      m_InterruptHandle(NULL),
      m_pNetlogAnalyser(NULL),
      m_MinEAPOLRoamTime(_I64_MAX),
      m_MaxEAPOLRoamTime(0),
      m_TotalEAPOLRoamTime(0),
      m_TotalEAPOLRoamSamples(0),
      m_StatusMsgTime(0)
{
    assert(NULL != pWinMain);
    m_StatusMsg[0] = TEXT('\0');
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
WiFiRoam_t::
~WiFiRoam_t(void)
{
    if (NULL != m_pNetlogAnalyser)
    {
        delete m_pNetlogAnalyser;
        m_pNetlogAnalyser = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Displays the program's command-line arguments.
//
void
WiFiRoam_t::
PrintUsage(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Initializes the configuration from the specified command-line args.
//
DWORD
WiFiRoam_t::
Init(
    IN int    argc,
    IN TCHAR *argv[])
{
    DWORD result = ERROR_SUCCESS;
    
    // Allocate the netlog packet-capture service.
    m_pNetlogAnalyser = new NetlogAnalyser_t(gszMainProgName);
    if (NULL == m_pNetlogAnalyser)
    {
        LogError(TEXT("Can't allocate NetlogAnalyser object\n"));
        return ERROR_OUTOFMEMORY;
    }

    // Initialize backchannel timeouts to 2sec.
    g_ResultTimeoutSecs  = 2;
    g_CommandTimeoutSecs = 2;
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Starts the tests running in a sub-thread.
//
DWORD
WiFiRoam_t::
Start(
    IN HANDLE InterruptHandle)
{
    Wait(100);

    ce::gate<ce::critical_section> locker(m_Locker);
    
    m_InterruptHandle = InterruptHandle;

    if (!IsAlive())
    {
        m_ThreadHandle = CreateThread(NULL, 0, WiFiRoam_t::ThreadProc,
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
WiFiRoam_t::
ThreadProc(
    IN LPVOID pParameter)
{
    WiFiRoam_t *pThread = (WiFiRoam_t *) pParameter;
    assert(pThread != NULL);

    // Until told to stop...
    HRESULT hr = S_OK;
    while (!pThread->m_pWinMain->IsInterrupted())
    {
        long runTime = WiFUtils::SubtractTickCounts(GetTickCount(),
                               pThread->m_pWinMain->GetStartTime()) / 1000;
        pThread->ConnectCMConnection(runTime);

        Sleep(1000); // 1 second
    }
    
    return HRESULT_CODE(hr);
}

// ----------------------------------------------------------------------------
//
// Waits the specified time then interrupts the test thread.
// Returns immediately if the thread is already finished.
// If grace-time is non-zero, terminates the thread if it ignores
// the interruption too long.
//
DWORD
WiFiRoam_t::
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
WiFiRoam_t::
Stop(
    IN long MaxGraceTimeMS)
{
    return Wait(1, MaxGraceTimeMS);
}

// ----------------------------------------------------------------------------
//
// Accumulates the specified EAPOL roam-time calculations.
//
void
WiFiRoam_t::
AddEAPOLRoamTimes(
    IN _int64 MinRoamTime,
    IN _int64 MaxRoamTime,
    IN _int64 TotalRoamTime,
    IN  long  TotalRoamSamples)
{
    if (TotalRoamSamples)
    {
        ce::gate<ce::critical_section> locker(m_Locker);
        if (m_MinEAPOLRoamTime > MinRoamTime)
            m_MinEAPOLRoamTime = MinRoamTime;
        if (m_MaxEAPOLRoamTime < MaxRoamTime)
            m_MaxEAPOLRoamTime = MaxRoamTime;
        m_TotalEAPOLRoamTime    += TotalRoamTime;
        m_TotalEAPOLRoamSamples += TotalRoamSamples;
    }
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
// Draws the current test status in the specified window using the
// specified drawing context.
//
void
WiFiRoam_t::
Draw(
    IN HWND hWnd,
    IN HDC  hdc)
{
    ConnStatus_t status = GetStatus();
    TCHAR        runTime[30];
    
    ScreenContext screen(hWnd,hdc);

    int col1 = 12;
    int col2 = screen.GetColumnIndent(col1, 2, 1);

    Utils::FormatRunTime(m_pWinMain->GetStartTime(),
                         runTime, COUNTOF(runTime));

    if (0 == status.firstCycle)
    {
        screen.AddMessage(2, TEXT("Starting"));
    }
    else
    {
        screen.AddMessage(2, TEXT("%u / %u / %u"), status.firstCycle,
                                                   status.cycleTransitions,
                                                   status.currentCycle);
    }
    
    int errorCount = 0;
    if (status.failedNoConn)
    {
        if (++errorCount > 1) screen.AddNewline();
        screen.AddMessage(col2, TEXT("no conn: %u"), 
                                status.failedNoConn);
    }
    if (status.failedLongConn)
    {
        if (++errorCount > 1) screen.AddNewline();
        screen.AddMessage(col2, TEXT("long conn: %u"), 
                                status.failedLongConn);
    }
    if (status.failedMultiConn)
    {
        if (++errorCount > 1) screen.AddNewline();
        screen.AddMessage(col2, TEXT("multi conn: %u"), 
                                status.failedMultiConn);
    }
    if (status.failedUnknownAP)
    {
        if (++errorCount > 1) screen.AddNewline();
        screen.AddMessage(col2, TEXT("unknown ap: %u"),
                                status.failedUnknownAP);
    }
    if (status.failedCellGlitch)
    {
        if (++errorCount > 1) screen.AddNewline();
        screen.AddMessage(col2, TEXT("cell glitch: %u"),
                                status.failedCellGlitch);
    }
    if (errorCount > 1) screen.AddNewline();
    else                screen.AddWideline();

    screen.AddMessage(col1, TEXT("test time: %s"), runTime);
    screen.AddNewline();

    screen.AddMessage(col1, TEXT("conn status: %s"), status.connStatusString);
    screen.AddWideline();

    screen.AddMessage(col1, TEXT("hom wifi: %u"),   status.countHomeWiFiConn);
    screen.AddMessage(col2, TEXT("hspot wifi: %u"), status.countHotSpotWiFiConn);
    screen.AddNewline();

    screen.AddMessage(col1, TEXT("off wifi: %u"),  status.countOfficeWiFiConn);
    screen.AddMessage(col2, TEXT("off roam: %u"), status.countOfficeWiFiRoam);
    screen.AddNewline();

    screen.AddMessage(col1, TEXT("cell: %u"),     status.countCellConn);
    screen.AddMessage(col2, TEXT("dtpt usb: %u"), status.countDTPTConn);
    screen.AddWideline();

    if (0 < m_TotalEAPOLRoamSamples)
    {
        screen.AddMessage(col1, TEXT("roam times (min/avg/max):"));
        screen.AddNewline();

        screen.AddMessage(col1*2, TEXT("eapol time: %I64i / %.0lf / %I64ims"),
                                  m_MinEAPOLRoamTime,
                                 (double)m_TotalEAPOLRoamTime
                               / (double)m_TotalEAPOLRoamSamples,
                                  m_MaxEAPOLRoamTime);
        screen.AddWideline();
    }

    DWORD flags = status.connStatus;
    if (flags & CONN_WIFI_LAN)
    {
        screen.AddMessage(col1, TEXT("wifi adapter: %s"),
                                status.wifiConn.GetInterfaceName());
        screen.AddNewline();

        screen.AddMessage(col1*2, TEXT("ssid: %s"),
                                  status.wifiConn.GetAPSsid());
        screen.AddNewline();

        screen.AddMessage(col1*2, TEXT("bssid: %s"),
                                  status.wifiConn.GetAPMAC());
        screen.AddWideline();
    }

    if (flags & CONN_CELL_RADIO_CONN)
    {
        screen.AddMessage(col1, TEXT("cell net: %s"),
                                status.cellNetType);
        screen.AddNewline();

        screen.AddMessage(col1, TEXT("cell conn: %s"),
                                status.cellNetDetails);
        screen.AddWideline();
    }
    
    // Display the status message.
    if (m_StatusMsg[0])
    {
        screen.AddMessage(2, TEXT("%s"), m_StatusMsg);
        screen.AddNewline();
    }

    screen.Refresh();
    
    // Age off old status messages.
    if (m_StatusMsg[0])
    {
        long messageAge = WiFUtils::SubtractTickCounts(GetTickCount(), 
                                                       m_StatusMsgTime);
        if (m_StatusMsgLifetime * 1000 < messageAge)
        {
            ClearStatusMsg();
        }
    }
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
        if (FAILED(hr) && hr != STRSAFE_E_INSUFFICIENT_BUFFER)
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
WiFiRoam_t::
SetStatusMsg(
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
        SafeCopy(m_StatusMsg, buffer, COUNTOF(m_StatusMsg));
        m_StatusMsgTime = GetTickCount();
    }
}

// ----------------------------------------------------------------------------
//
// Clears the status area of the UI.
//
void
WiFiRoam_t::    
ClearStatusMsg(void)
{
    ce::gate<ce::critical_section> locker(m_Locker);
    m_StatusMsg[0] = TEXT('\0');
}

// ----------------------------------------------------------------------------
