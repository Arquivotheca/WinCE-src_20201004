//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include "dst.h"
#include "DSTUtils.h"
#include "DSTHandler.h"
#include "DynamicDST.h"

static TCHAR c_strDSTTimerEvent[] = _T("DSTTimerEvent");
static TCHAR c_strDSTTimerNotification[] = _T("\\\\.\\Notifications\\NamedEvents\\DSTTimerEvent");

static TCHAR c_strDynDSTTimerEvent[] = _T("DynDSTTimerEvent");
static TCHAR c_strDynDSTTimerNotification[] = _T("\\\\.\\Notifications\\NamedEvents\\DynDSTTimerEvent");

static TCHAR c_strTimeChangeEvent[] = _T("TimeChangeEvent");
static TCHAR c_strTimeChangeNotification[] = _T("\\\\.\\Notifications\\NamedEvents\\TimeChangeEvent");

static TCHAR c_strTzRegChangeEvent[] = _T("TzRegChangeEvent");
static TCHAR c_strTzRegChangeNotification[] = _T("\\\\.\\Notifications\\NamedEvents\\TzRegChangeEvent");

const TCHAR c_strRegKeyClock[] = _T("Software\\Microsoft\\Clock");
const TCHAR c_strAutoDST[]  = _T("AutoDST");

DSTHandler::DSTHandler():
    m_lBiasAdjustment(0),
    m_bInverted(FALSE),
    m_llStandardDate(0LL),
    m_llDaylightDate(0LL),
    m_bDSTEnabled(FALSE),
    m_hDSTTimerEvent(NULL),
    m_hPreTimeChangeEvent(NULL),
    m_hTimeChangeEvent(NULL),
    m_hTzChangeEvent(NULL),
    m_hDynDSTTimerEvent(NULL),
    m_wCurYear(0),
    m_bDstON(FALSE),
    m_hTzRegRoot(NULL),
    m_bDynDSTUpdatePending(FALSE)
{
    ZeroMemory(&m_tzi, sizeof(TIME_ZONE_INFORMATION));
    ZeroMemory(&m_stStandardDate, sizeof(SYSTEMTIME));
    ZeroMemory(&m_stDaylightDate, sizeof(SYSTEMTIME));

    CeRunAppAtTime(c_strDynDSTTimerNotification, NULL);
    CeRunAppAtTime(c_strDSTTimerNotification, NULL);
    CeRunAppAtEvent(c_strTimeChangeNotification, NOTIFICATION_EVENT_NONE);
}

DSTHandler::~DSTHandler()
{
    CeRunAppAtTime(c_strDSTTimerNotification, NULL);
    if (m_hDSTTimerEvent!= NULL)
    {
        CloseHandle(m_hDSTTimerEvent);
        m_hDSTTimerEvent = NULL;
    }
    
    CeRunAppAtTime(c_strDynDSTTimerNotification, NULL);
    if (m_hDynDSTTimerEvent != NULL)
    {
        CloseHandle(m_hDynDSTTimerEvent);
        m_hDynDSTTimerEvent = NULL;
    }

    if (m_hPreTimeChangeEvent != NULL)
    {
        CloseHandle(m_hTimeChangeEvent);
        m_hTimeChangeEvent = NULL;
    }

    CeRunAppAtEvent(c_strTimeChangeNotification, NOTIFICATION_EVENT_NONE);
    if (m_hTimeChangeEvent != NULL)
    {
        CloseHandle(m_hTimeChangeEvent);
        m_hTimeChangeEvent = NULL;
    }
    
    if (m_hTzChangeEvent != NULL)
    {
        CloseHandle(m_hTzChangeEvent);
        m_hTzChangeEvent = NULL;
    }
    
    if (m_hTzRegChangeEvent != NULL)
    {
        CeFindCloseRegChange(m_hTzRegChangeEvent);
        m_hTzRegChangeEvent = NULL;
    }
    
    if (m_hTzRegRoot != NULL)
    {
        RegCloseKey(m_hTzRegRoot);
        m_hTzRegRoot = NULL;
    }
}

BOOL DSTHandler::Init()
{
    //Get the system timer resolution for CeRunAppAtTime
    DWORD dwTimerRes = 0;
    if (!KernelLibIoControl((HANDLE)KMOD_CORE, 
                            IOCTL_KLIB_GETALARMRESOLUTION, 
                            NULL, 
                            0, 
                            &dwTimerRes, 
                            sizeof(DWORD), 
                            NULL))
    {
        //If this call failed, we will take the default value
        dwTimerRes = MAX_TIMER_RESOLUTION;
    }

    //Calculate the actuall delay value for year boundary timer and the defered dynamic DST update timer
    m_dwYearBoundaryDelay = MILLISECOND_TO_FILETIME(dwTimerRes + YEAR_BOUNDARY_DELAY);
    m_dwDynDSTUpdateDelay = MILLISECOND_TO_FILETIME(dwTimerRes + DYN_DST_UPDATE_DELAY);

    //Initialize DST timer event
    m_hDSTTimerEvent = CreateEvent(NULL, FALSE, FALSE, c_strDSTTimerEvent);
    if (m_hDSTTimerEvent == NULL)
        return FALSE;
    
    //Initialize dynamic DST update timer
    m_hDynDSTTimerEvent = CreateEvent(NULL, FALSE, FALSE, c_strDynDSTTimerEvent);
    if (m_hDynDSTTimerEvent == NULL)
        return FALSE;

    //Initialize pre-time change notification event
    m_hPreTimeChangeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (m_hPreTimeChangeEvent == NULL)
        return FALSE;
    
    //Initialize time change notification event
    m_hTimeChangeEvent = CreateEvent(NULL, FALSE, FALSE, c_strTimeChangeEvent);
    if (m_hTimeChangeEvent == NULL)
        return FALSE;
    
    //Initialize timezone change notification event
    m_hTzChangeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (m_hTzChangeEvent == NULL)
        return FALSE;
    
    //Open timezone registry key for registry notification
    if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_LOCAL_MACHINE, cszTimeZones, 0, 0, &m_hTzRegRoot))
        return FALSE;

    //Register timezone registry notification
    m_hTzRegChangeEvent = CeFindFirstRegChange(m_hTzRegRoot, TRUE, REG_NOTIFY_CHANGE_LAST_SET);
    if (m_hTzRegChangeEvent == INVALID_HANDLE_VALUE)
    {
        m_hTzRegChangeEvent = NULL;
        return FALSE;
    }

    //Register time change notification
    if (!CeRunAppAtEvent(c_strTimeChangeNotification, NOTIFICATION_EVENT_TIME_CHANGE))
        return FALSE;
    
    //Set the time event manually for initialization
    SetEvent(m_hDSTTimerEvent);
        
    return TRUE;
}

BOOL DSTHandler::AutoDST() const
{
    DWORD dwRet = 0;
    HKEY hKey = NULL;
    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, c_strRegKeyClock, 0, 0, &hKey))
    {
        DWORD dwSize = sizeof(DWORD);
        RegQueryValueEx(hKey, c_strAutoDST, 0, NULL, (LPBYTE)&dwRet, &dwSize);
        RegCloseKey(hKey);
    }
    return (BOOL)dwRet;
}

BOOL DSTHandler::SetDaylightTime(BOOL bOn)
{
    if ((!m_bDSTEnabled || !AutoDST()) && m_bInverted)
    {
        m_bDstON = TRUE;
        ::SetDaylightTime(0);
    }
    else if (m_bDstON != bOn)
    {
        m_bDstON = bOn;
        
        DEBUGMSG(TRUE, (TEXT("[DSTHandler]Set DST status to %s"), m_bDstON ? TEXT("ON") : TEXT("OFF")));

        BOOL bRealStatus = m_bDstON;
        if (m_bInverted)
        {
            bRealStatus = !bRealStatus;
        }
        ::SetDaylightTime(bRealStatus ? 1 : 0);
    }

    return TRUE;
}


void DSTHandler::CacheUpdate()
{
    SYSTEMTIME stLocal;
    GetLocalTime(&stLocal);
    m_wCurYear = stLocal.wYear;

    DWORD dwTZ = GetTimeZoneInformation(&m_tzi);
    m_bDstON = (dwTZ == TIME_ZONE_ID_DAYLIGHT);

    m_bDSTEnabled = DoesTzSupportDST(&m_tzi);
    
    if (m_bDSTEnabled)
    {
        GetStandardDate(m_wCurYear, &m_tzi, &m_stStandardDate, (FILETIME*)&m_llStandardDate);
        GetDaylightDate(m_wCurYear, &m_tzi, &m_stDaylightDate, (FILETIME*)&m_llDaylightDate);
    }
    else
    {
        ZeroMemory(&m_stStandardDate, sizeof(SYSTEMTIME));
        m_llStandardDate = 0LL;
        ZeroMemory(&m_stDaylightDate, sizeof(SYSTEMTIME));
        m_llDaylightDate = 0LL;
    }
    
    //Normalize the timezone information
    if (m_llStandardDate < m_llDaylightDate)
    {
        LONGLONG llTemp = m_llStandardDate;
        m_llStandardDate = m_llDaylightDate;
        m_llDaylightDate = llTemp;
        
        SYSTEMTIME stTemp = m_stStandardDate;
        m_stStandardDate = m_stDaylightDate;
        m_stDaylightDate = stTemp;
        
        stTemp = m_tzi.StandardDate;
        m_tzi.StandardDate = m_tzi.DaylightDate;
        m_tzi.DaylightDate = stTemp;
        
        LONG lTemp = m_tzi.StandardBias;
        m_tzi.StandardBias = m_tzi.DaylightBias;
        m_tzi.DaylightBias = lTemp;
        
        m_bDstON = !m_bDstON;
        
        m_bInverted = TRUE;
        
        DEBUGMSG(TRUE, (TEXT("[DSTHandler]Daylight and standard settings exchanged due to timezone normalization")));
    }
    else
    {
        m_bInverted = FALSE;
    }
    
    
    if (m_tzi.StandardBias != 0)
    {
        m_lBiasAdjustment = m_tzi.StandardBias;
        m_tzi.StandardBias -= m_lBiasAdjustment;
        m_tzi.DaylightBias -= m_lBiasAdjustment;
        m_tzi.Bias += m_lBiasAdjustment;
        
        DEBUGMSG(TRUE, (TEXT("[DSTHandler]Daylight and Standard bias shifted by %d due to timezone normalization"), m_lBiasAdjustment));
    }
    else
    {
        m_lBiasAdjustment = 0;
    }
    
    DEBUGMSG(TRUE, (TEXT("[DSTHandler]DST is %s for year %d, current DST status is %s"), m_bDSTEnabled ? TEXT("ENABLED") : TEXT("DISABLED"), m_wCurYear, m_bDstON ? TEXT("ON") : TEXT("OFF")));
    if (m_bDSTEnabled)
    {
        DEBUGMSG(TRUE, (TEXT("[DSTHandler]DST start time %02d-%02d-%04d %02d:%02d:%02d"),
            m_stDaylightDate.wMonth, m_stDaylightDate.wDay, m_stDaylightDate.wYear,
            m_stDaylightDate.wHour, m_stDaylightDate.wMinute, m_stDaylightDate.wSecond));

        DEBUGMSG(TRUE, (TEXT("[DSTHandler]DST end time %02d-%02d-%04d %02d:%02d:%02d"),
            m_stStandardDate.wMonth, m_stStandardDate.wDay, m_stStandardDate.wYear,
            m_stStandardDate.wHour, m_stStandardDate.wMinute, m_stStandardDate.wSecond));
    }
    // Update internal time cache based on new bias/DST status
    SetLocalTime(&stLocal);
}

BOOL DSTHandler::SetupDSTTimer()
{
    //By default set the timer to the begining of next year
    SYSTEMTIME stTimer = {0};

    if (m_bDSTEnabled)  //Current timezone support DST, we should monitor the next DST time
    {
        ASSERT(m_llDaylightDate < m_llStandardDate);
        if (m_bDstON)  //Current DST is on, we need to monitor the StandardDate time
        {
            stTimer = m_stStandardDate;
        }
        else    //Current DST is OFF, we need to monitor either the DaylightDate or YearBoundary time
        {
            SYSTEMTIME stSystem;
            LONGLONG llSystem;
            GetSystemTime(&stSystem);
            VERIFY(SystemTimeToFileTime(&stSystem, (FILETIME*)&llSystem));
            
            //Current system time is less than the daylight date, we need to monitor the DaylightDate time
            if (llSystem < m_llDaylightDate)
            {
                stTimer = m_stDaylightDate;
            }
        }
    }

    //If there is no timer set or the timer is not in this year, we will set it to the year boundary
    if (stTimer.wYear != m_wCurYear)
    {
        ZeroMemory(&stTimer, sizeof(SYSTEMTIME));

        //Because of the timer resolution, we calculate the year boundary time with the timer resolution to 
        //make sure this timer is always triggered in the new year
        stTimer.wYear = m_wCurYear + 1;
        stTimer.wMonth = 1;
        stTimer.wDay = 1;

        LONGLONG llTimer = 0;
        VERIFY(SystemTimeToFileTime(&stTimer, (FILETIME*)&llTimer));
        llTimer += m_dwYearBoundaryDelay;
        VERIFY(FileTimeToSystemTime((const FILETIME*)&llTimer, &stTimer));

    }

    ResetEvent(m_hDSTTimerEvent);

    DEBUGMSG(TRUE, (TEXT("[DSTHandler]Set next timer at time %02d-%02d-%04d %02d:%02d:%02d\r\n"), stTimer.wMonth, stTimer.wDay, stTimer.wYear, stTimer.wHour, stTimer.wMinute, stTimer.wSecond));
    return CeRunAppAtTime(c_strDSTTimerNotification, &stTimer);
}

BOOL DSTHandler::RedetermineDST()
{
    if (!m_bDSTEnabled || !AutoDST())
    {
        return SetDaylightTime(FALSE);
    }

    SYSTEMTIME stSystem;
    GetSystemTime(&stSystem);

    LONGLONG llNow;
    VERIFY(SystemTimeToFileTime(&stSystem, (FILETIME*)&llNow));

    BOOL bInDST = FALSE;

    ASSERT(m_llDaylightDate <= m_llStandardDate);
    ASSERT(m_tzi.StandardBias == 0);

    bInDST = (m_llDaylightDate <= llNow) && (llNow < m_llStandardDate);

    return SetDaylightTime(bInDST);
}

BOOL DSTHandler::UpdateDynamicDST(BOOL bForce)
{
    //If there is a pending dynamic DST update, we will force to update it
    if (m_bDynDSTUpdatePending)
        bForce = TRUE;

    //Disable the registry change notification
    CeFindCloseRegChange(m_hTzRegChangeEvent);
    m_hTzRegChangeEvent = NULL;
    
    //Clear the defered dynamic DST update timer
    CeRunAppAtTime(c_strDynDSTTimerNotification, NULL);
    
    //Clear the pending flag
    m_bDynDSTUpdatePending = FALSE;

    //Perform the dynamic DST update
    int iRet = DynamicDSTUpdate(m_hTzRegRoot, m_wCurYear, bForce);
    if (iRet == ERROR_SUCCESS)
    {
        // Dynamic DST updated time zone information
        // Then update cached time zone as well
        CacheUpdate();
    }
    else if (iRet != ERROR_ALREADY_EXISTS)
    {
        // Dynamic DST encountered an error
        return FALSE;
    }

    m_hTzRegChangeEvent = CeFindFirstRegChange(m_hTzRegRoot, TRUE, REG_NOTIFY_CHANGE_LAST_SET);
    if (m_hTzRegChangeEvent == INVALID_HANDLE_VALUE)
    {
        m_hTzRegChangeEvent = NULL;
        return FALSE;
    }
    
    return TRUE;
}

BOOL DSTHandler::OnTzRegChange()
{
    SYSTEMTIME stLocal;
    GetLocalTime(&stLocal);

    LONGLONG ftLocal;
    VERIFY(SystemTimeToFileTime(&stLocal, (FILETIME*)&ftLocal));
    ftLocal += m_dwDynDSTUpdateDelay;
    VERIFY(FileTimeToSystemTime((FILETIME*)&ftLocal, &stLocal));

    DEBUGMSG(TRUE, (_T("[TIMESVC DST]Schedule a defered dynamic DST update at time %02d:%02d:%02d\n"), 
                stLocal.wHour, 
                stLocal.wMinute, 
                stLocal.wSecond));

    m_bDynDSTUpdatePending = TRUE;
    if (!CeRunAppAtTime(c_strDynDSTTimerNotification, &stLocal))
    {
        DEBUGMSG(TRUE, (_T("[TIMESVC DST]Failed to schedule a defered dynamic DST update, update now"))); 
        UpdateDynamicDST(TRUE);
        m_bDynDSTUpdatePending = FALSE;
    }

    return CeFindNextRegChange(m_hTzRegChangeEvent);
}

BOOL DSTHandler::OnPreTimeChange()
{
    BOOL bRet = FALSE;
    // Overwrite the current time with the SetLocalTime/SetSystemTime parameter
    if (g_lpTimeChangeInformation->dwType == TM_SYSTEMTIME)
    {
        DEBUGMSG(TRUE, (TEXT("[DSTHandler]SetSystemTime(%02d-%02d-%04d %02d:%02d:%02d)\r\n"),
            g_lpTimeChangeInformation->stNew.wMonth,
            g_lpTimeChangeInformation->stNew.wDay,
            g_lpTimeChangeInformation->stNew.wYear,
            g_lpTimeChangeInformation->stNew.wHour,
            g_lpTimeChangeInformation->stNew.wMinute,
            g_lpTimeChangeInformation->stNew.wSecond
            ));
        SetSystemTime(&(g_lpTimeChangeInformation->stNew));
    }
    else
    {
        DEBUGMSG(TRUE, (TEXT("[DSTHandler]SetLocalTime(%02d-%02d-%04d %02d:%02d:%02d)\r\n"),
            g_lpTimeChangeInformation->stNew.wMonth,
            g_lpTimeChangeInformation->stNew.wDay,
            g_lpTimeChangeInformation->stNew.wYear,
            g_lpTimeChangeInformation->stNew.wHour,
            g_lpTimeChangeInformation->stNew.wMinute,
            g_lpTimeChangeInformation->stNew.wSecond
            ));
        SetLocalTime(&(g_lpTimeChangeInformation->stNew));
    }

    CacheUpdate();

    if (!UpdateDynamicDST(FALSE))
    {
        goto Error;
    }

    if (!RedetermineDST())
    {
        goto Error;
    }
    
    // DST Timer should not be set up here, because actual time in Kernel has not updated yet.
    // It should be set upon the time change notification

    bRet = TRUE;

Error:
    return bRet;
}

BOOL DSTHandler::OnDstGenericEvent()
{
    BOOL bRet = FALSE;

    CacheUpdate();

    if (!UpdateDynamicDST(FALSE))
    {
        goto Error;
    }

    if (!RedetermineDST())
    {
        goto Error;
    }
    
    if (!SetupDSTTimer())
    {
        goto Error;
    }

    bRet = TRUE;

Error:
    return bRet;
}


BOOL DSTHandler::SetSystemTime(const SYSTEMTIME* lpSystemTime)
{
    return SystemTimeToFileTime(lpSystemTime, (FILETIME*)&m_llSystemTimeNow);
}

BOOL DSTHandler::SetLocalTime(const SYSTEMTIME* lpSystemTime)
{
    BOOL    bRet = FALSE;
    LONGLONG llLocalTime;
    if (SystemTimeToFileTime(lpSystemTime, (FILETIME*)&llLocalTime))
    {
        if (m_bDstON)
        {
            m_llSystemTimeNow = llLocalTime + MINUTES_TO_FILETIME(m_tzi.Bias + m_tzi.DaylightBias);
        }
        else
        {
            m_llSystemTimeNow = llLocalTime + MINUTES_TO_FILETIME(m_tzi.Bias + m_tzi.StandardBias);
        }
        bRet = TRUE;
    }
    return bRet;
}

void DSTHandler::GetSystemTime(LPSYSTEMTIME lpSystemTime)
{
    FileTimeToSystemTime((FILETIME*)&m_llSystemTimeNow, lpSystemTime);
}

void DSTHandler::GetLocalTime(LPSYSTEMTIME lpSystemTime)
{
    LONGLONG llLocalTime;
    if (m_bDstON)
    {
        llLocalTime = m_llSystemTimeNow - MINUTES_TO_FILETIME(m_tzi.Bias + m_tzi.DaylightBias);
    }
    else
    {
        llLocalTime = m_llSystemTimeNow - MINUTES_TO_FILETIME(m_tzi.Bias + m_tzi.StandardBias);
    }
    FileTimeToSystemTime((FILETIME*)&llLocalTime, lpSystemTime);
}
