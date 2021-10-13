//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*---------------------------------------------------------------------------*\
 *  module: dst.cpp
 *  purpose: DST support.  Need to handle the following 3 scenarios
 *          1)  DST turns on/off when system is running
 *          2)  DST turns on/off when system is suspended
 *          3)  DST turns on/off when system is powered off
 *
\*---------------------------------------------------------------------------*/

#include <windows.h>
#include <notify.h>
#include <service.h>

#include "../inc/timesvc.h"
#include "dst.h"
#include "dstrc.h"

//keep these consistant with names in public\wceshellfe\oak\ctlpnl\cplmain\regcpl.h
#define RK_CLOCK        TEXT("Software\\Microsoft\\Clock")
#define RV_INDST        TEXT("HomeDST")  //are we in DST?
#define RV_AUTODST      TEXT("AutoDST")  //do we auto-adjust DST?
#define RV_DSTUI        TEXT("ShowDSTUI")  //do we show a message at dst change?

#define DSTEVENT          _T("ShellDSTEvent")
#define DSTTZEVENT        _T("DSTTzChange")
#define DSTTIMEEVENT        _T("DSTTimeChange")

#define DSTNOTIFICATION   _T("\\\\.\\Notifications\\NamedEvents\\ShellDSTEvent")
#define DSTTIMEZONENOTIFICATION _T("\\\\.\\Notifications\\NamedEvents\\DSTTzChange")
#define DSTTIMENOTIFICATION _T("\\\\.\\Notifications\\NamedEvents\\DSTTimeChange")

#define NUMDSTEVENTS 4
#define SHUTDOWNEVENT 0
#define TZCHANGEEVENT 1
#define TIMECHANGEEVENT 2
#define DSTCHANGEEVENT 3 // This event must be last so WaitForMultipleObjects algorithm works (see DST_WaitForEvents)

#define MAX_DSTMSGLEN 256
#define MAX_DSTTITLELEN 64

// FILETIME (100-ns intervals) to minutes (10 x 1000 x 1000 x 60)
#define FILETIME_TO_MINUTES (10 * 1000 * 1000 * 60)


typedef int (*tMessageBoxW) (HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
typedef HWND (*tGetForegroundWindow) (VOID);

static DWORD g_dwTime = TIME_ZONE_ID_STANDARD;
static HINSTANCE g_hInst;
static HANDLE    g_hDSTThread;
static HANDLE    g_hDSTShutdownEvent;
static DWORD     g_dwDSTState;




// If the DSTCHANGEEVENT is fired within 1 minute on one of the DST boundaries
// (DST->STD or STD->DST) then always flip the system DST value on the system
// with a call to SetDaylightTime().  Otherwise assume that the event has fired
// because of a clock change (manual or SNTP server related) in which case we
// may or may not need to do this calculation.
#define DST_TIME_THRESHHOLD    (1 * FILETIME_TO_MINUTES)


//
//	Private DST code
//
/*++

    
Routine Description:
    Determines whether the current date and time is in daylight
    or standard time, and notifies the kernel.

    Kernel doesn't currently update this in all cases
    Manually take care of it to be safe and ensure that subsequent calls 
    to GetTimeZoneInformation return the correct value

    pNewSystemTime may be NULL to retrieve current system time, or may
    indicate time we're about to change to on clock update
    
Arguments:
    none
    
Return Value:
    current timezone value (same as return values for GetTimeZoneInformation
    
--*/
void  SetDaylightOrStandardTimeDST(SYSTEMTIME *pNewSystemTime)
{
    TIME_ZONE_INFORMATION tzi;
    DWORD dwTZ = GetTimeZoneInformation(&tzi);
   
    // If the system supports DST verify that the kernel returned the correct
    // value, otherwise make sure the kerel has us in SDT
    if (LocaleSupportsDST(&tzi))
    {
        SYSTEMTIME curTime;        
        LONGLONG  llStandard = 0, llDaylight = 0, llNow = 0;

        if (pNewSystemTime)
            memcpy(&curTime,pNewSystemTime,sizeof(SYSTEMTIME));
        else
            GetLocalTime(&curTime);

        //fix up the date structs if necessary
        if (0 == tzi.StandardDate.wYear)
            DST_DetermineChangeDate(&tzi.StandardDate, TRUE);
        if (0 == tzi.DaylightDate.wYear)
            DST_DetermineChangeDate(&tzi.DaylightDate, TRUE);

        //convert so we can do the math
        VERIFY(SystemTimeToFileTime(&tzi.StandardDate, (FILETIME *)&llStandard));
        VERIFY(SystemTimeToFileTime(&tzi.DaylightDate, (FILETIME *)&llDaylight));
        VERIFY(SystemTimeToFileTime(&curTime, (FILETIME *)&llNow));

        ASSERT(llDaylight <= llStandard);

        //the greater difference determines which zone we are in   
        if (DST_Auto() && ((llDaylight <= llStandard && llDaylight <= llNow && llNow <= llStandard) 
                        || (llDaylight > llStandard && (llDaylight <= llNow || llNow <= llStandard))))
        {
            DEBUGMSG(ZONE_DST, (_T("[TIMESVC DST]  Notifying kernel that we are in Daylight time.  GetTimeZoneInformation currently thinks we are in %s time.\r\n"), 
                     TIME_ZONE_ID_DAYLIGHT  == dwTZ ? _T("Daylight") : _T("Standard")));
            SetDaylightTime(TRUE);
            g_dwTime = TIME_ZONE_ID_DAYLIGHT;           
        }        
        else
        {
            DEBUGMSG(ZONE_DST, (_T("[TIMESVC DST]  Notifying kernel that we are in Standard time.  GetTimeZoneInformation currently thinks we are in %s time.\r\n"), 
                     TIME_ZONE_ID_DAYLIGHT  == dwTZ ? _T("Daylight") : _T("Standard")));
            SetDaylightTime(FALSE);
            g_dwTime = TIME_ZONE_ID_STANDARD;            
        }
    }
    else
    {
        ASSERT(TIME_ZONE_ID_STANDARD == dwTZ);
        if (TIME_ZONE_ID_STANDARD != dwTZ) {
            SetDaylightTime(FALSE);
        }
        g_dwTime = TIME_ZONE_ID_STANDARD;
    }
}


/*++
Routine Description:
    'main' function for DST.  Called from shell init, and re-entered from DST_WaitForEvent.
    
Arguments:
    none
    
Return Value:
    none
    
--*/
static DWORD WINAPI DST_Init(LPVOID lpUnused)
{
    DEBUGMSG(ZONE_INIT, (_T("[TIMESVC DST] System Started...\r\n")));

    // Clean up any existing events in the notification database
    CeRunAppAtEvent(DSTTIMEZONENOTIFICATION, NOTIFICATION_EVENT_NONE);
    CeRunAppAtEvent(DSTTIMENOTIFICATION, NOTIFICATION_EVENT_NONE);
    CeRunAppAtTime(DSTNOTIFICATION, 0);

    //need to know from the start if we are in standard or daylight time
    SetDaylightOrStandardTimeDST(NULL);
    for (;;)
    {
        HANDLE hDSTEvents[NUMDSTEVENTS];
        hDSTEvents[SHUTDOWNEVENT] = g_hDSTShutdownEvent;
        hDSTEvents[TZCHANGEEVENT] = DST_SetTimezoneChangeEvent();
        hDSTEvents[TIMECHANGEEVENT] = DST_SetTimeChangeEvent();
        hDSTEvents[DSTCHANGEEVENT] = DST_SetDSTEvent(); // This can be NULL if we are in a non-DST timezone

        if (NULL == hDSTEvents[TZCHANGEEVENT])
        {
            DEBUGCHK( _T("DST:  Failed to set TZ change Event"));
            break;
        }
        if (NULL == hDSTEvents[TIMECHANGEEVENT])
        {
            DEBUGCHK( _T("DST:  Failed to set Time change Event"));
            break;
        }

        if (! DST_WaitForEvents(hDSTEvents))
            break;
    } 

    DEBUGMSG(ZONE_INIT, (_T("[TIMESVC DST] Thread exited...\r\n")));
    return 0;
}    


/*++
Routine Description:
    Determines if we should auto-adjust for DST
    
Arguments:
    none
    
Return Value:
    TRUE if we want to auto adjust
    FALSE if we don't want to auto adjust
    
--*/
static BOOL  DST_Auto(void)
{
    DWORD ret = 0;
    HKEY hKey = NULL;
    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, RK_CLOCK, 0, 0, &hKey))
    {
        DWORD dwSize = sizeof(DWORD);
        RegQueryValueEx(hKey, RV_AUTODST, 0, NULL, (LPBYTE)&ret, &dwSize);
        RegCloseKey(hKey);
    }
#ifdef DEBUG
    if (ret)
    {
        DEBUGMSG(ZONE_DST, ( _T("[TIMESVC DST]  Auto change for DST is on\r\n")));
    } 
    else
    {
        DEBUGMSG(ZONE_DST, ( _T("[TIMESVC DST]  Auto change for DST is off\r\n")));
    }
#endif //DEBUG

    return (BOOL)ret;
}



/*++

    
Routine Description:
    Sets Event to be notified when timezone changes
    
Arguments:
    none
    
Return Value:
    HANDLE to timezone event
    
--*/
static HANDLE DST_SetTimezoneChangeEvent(void)
{
    HANDLE hTZEvent = CreateEvent(NULL, FALSE, FALSE, DSTTZEVENT);
    if (hTZEvent)
    {
        if (!CeRunAppAtEvent(DSTTIMEZONENOTIFICATION,
                             NOTIFICATION_EVENT_TZ_CHANGE))
        {
            CloseHandle(hTZEvent);
            hTZEvent = NULL;
        }
    }
    return hTZEvent;
}

/*++

    
Routine Description:
    Sets Event to be notified when time changes
    
Arguments:
    none
    
Return Value:
    HANDLE to timezone event
    
--*/
static HANDLE DST_SetTimeChangeEvent(void)
{
    HANDLE hTimeEvent = CreateEvent(NULL, FALSE, FALSE, DSTTIMEEVENT);
    if (hTimeEvent)
    {
        if (!CeRunAppAtEvent(DSTTIMENOTIFICATION,
                             NOTIFICATION_EVENT_TIME_CHANGE))
        {
            CloseHandle(hTimeEvent);
            hTimeEvent = NULL;
        }
    }
    return hTimeEvent;
}


/*++
Routine Description:
    Sets the DST Event 
    
Arguments:
    none
    
Return Value:
    Handle to event to be triggered when daylight time turns on (or off)
    
--*/
static HANDLE DST_SetDSTEvent (void)
{
    HANDLE hEvent = NULL;
    BOOL bEvent = FALSE;
    TIME_ZONE_INFORMATION tzi = {0};
    SYSTEMTIME curTime, notificationTime;
    LONGLONG llNow = 0, llNotification = 0;

    DWORD dwTZ = GetTimeZoneInformation(&tzi);

    // Only create the event if the locale supports DST
    if (!LocaleSupportsDST(&tzi))
        return NULL;

    switch (g_dwTime)
    {
        case TIME_ZONE_ID_DAYLIGHT :
            DEBUGMSG(ZONE_DST, ( _T("[TIMESVC DST]  Currently in Daylight Time\r\n")));
            notificationTime = tzi.StandardDate;
        break;

        case TIME_ZONE_ID_STANDARD :
            DEBUGMSG(ZONE_DST, ( _T("[TIMESVC DST]  Currently in Standard Time\r\n")));
            notificationTime = tzi.DaylightDate;
        break;

        default :
            DEBUGMSG(ZONE_DST, (_T("[TIMESVC DST]  Unknown TimeZone\r\n")));
            ASSERT(TIME_ZONE_ID_UNKNOWN == dwTZ);    //otherwise GetTZInfo is busted
            return NULL;
    }

    if (0 == notificationTime.wYear) //no specific date set - do the math
        DST_DetermineChangeDate(&notificationTime);

    //only set events that are in the future
    VERIFY(SystemTimeToFileTime(&notificationTime, (FILETIME *)&llNotification));
    GetLocalTime(&curTime);
    VERIFY(SystemTimeToFileTime(&curTime, (FILETIME *)&llNow));
    if (llNotification > llNow)
    {
        hEvent = CreateEvent(NULL, FALSE, FALSE, DSTEVENT);
        if (hEvent != INVALID_HANDLE_VALUE)
        {
            if (!CeRunAppAtTime(DSTNOTIFICATION, &notificationTime))
            {
               CloseHandle(hEvent);
               hEvent = NULL;
            }
        }

        DEBUGMSG(ZONE_DST, (_T("[TIMESVC DST]  Set TimeChange Event for %d/%d/%02d at %d:%02d\r\n"), 
                 notificationTime.wMonth, notificationTime.wDay, notificationTime.wYear,
                 notificationTime.wHour, notificationTime.wMinute));
    }

    return hEvent;
}


/*++
Routine Description:
    waits for DST event to be signaled, and optionally re-sets the
    time as appropriate.
    
Arguments:
    hEvent - handle to DST event
    
Return Value:
    none
    
--*/
static int DST_WaitForEvents(LPHANDLE DST_handles)
{
    // The last event can be NULL if we so check before we give the number of handles
    DWORD dwEvent = WaitForMultipleObjects(DST_handles[NUMDSTEVENTS-1] ? NUMDSTEVENTS : NUMDSTEVENTS-1,
                                           DST_handles, FALSE, INFINITE);

    // returning from this function causes DST_Init to reset all of our events
    // so we need to turn everything off
    CeRunAppAtEvent(DSTTIMEZONENOTIFICATION, NOTIFICATION_EVENT_NONE);
    CeRunAppAtEvent(DSTTIMENOTIFICATION, NOTIFICATION_EVENT_NONE);
    CeRunAppAtTime(DSTNOTIFICATION, 0);

    // and ditch the handles, except for shutdown event - keep that
    for (int i = 1; i < NUMDSTEVENTS; i++)
    {
        if (NULL != DST_handles[i])
        {
            CloseHandle(DST_handles[i]);
            DST_handles[i] = NULL;
        }
    }

    switch (dwEvent - WAIT_OBJECT_0)
    {
        case SHUTDOWNEVENT :
            DEBUGMSG(ZONE_INIT, (_T("[TIMESVC DST] Shutting down DST system\r\n")));
            return FALSE;

        case TZCHANGEEVENT :
        case TIMECHANGEEVENT :
        {
            // need to redetermine whether we are in daylight or standard time
            DEBUGMSG(ZONE_DST, (_T("[TIMESVC DST] Resetting DST event due to Time, Date, or TZ change\r\n")));
            SetDaylightOrStandardTimeDST(NULL);
        }
        break;

        case DSTCHANGEEVENT :
        {
            if (DST_Auto())
            {
                // stOld contains the current system time (NOT the one before the update)
                // stNew contains new system time if we have to add or subtract 1 hour
                // to process DST/STD time change.
                SYSTEMTIME stOld, stNew;
                LONGLONG llOld, llNew;                

                GetLocalTime(&stOld); 
                SystemTimeToFileTime(&stOld, (FILETIME *)&llOld);

                /*
                    Check the time zone information.  It's possible that this information has
                    changed since the GetTimeZoneInformation call in DST_SetEvent, and
                    this action needs to be based on the current timezone.
                */
                TIME_ZONE_INFORMATION tzi = {0};
                GetTimeZoneInformation(&tzi);
                LONGLONG  llStandard = 0, llDaylight = 0;

                //fix up the date structs if necessary
                if (0 == tzi.StandardDate.wYear)
                    DST_DetermineChangeDate(&tzi.StandardDate, TRUE);
                if (0 == tzi.DaylightDate.wYear)
                    DST_DetermineChangeDate(&tzi.DaylightDate, TRUE);

                //convert so we can do the math
                VERIFY(SystemTimeToFileTime(&tzi.StandardDate, (FILETIME *)&llStandard));
                VERIFY(SystemTimeToFileTime(&tzi.DaylightDate, (FILETIME *)&llDaylight));
                ASSERT(llDaylight <= llStandard);

                if (((llOld-llDaylight >= 0) && (llOld-llDaylight < DST_TIME_THRESHHOLD)) ||
                    ((llOld-llStandard >= 0) && (llOld-llStandard < DST_TIME_THRESHHOLD))) 
                {
                    // This indicates that it's less than 1 minute after the STD<->DST
                    // boundary.  Always perform the flip after this conditional block.
                }
                else
                {
                    // This indicates that we've had a DST event triggered without
                    // "naturally" crossing over the DST boundary.  This can happen
                    // for instance if the original clock is set to 1/1/04 and the 
                    // new time is set to 11/1/04.  In this case, since TimeSVC had
                    // an event scheduled to go off sometime in 4/04 to handle this
                    // event, the event is handled presently. 

                    // In this case, we *may* not need need to flip from DST<->STD.
                    // We may be in the same DST/Standard setting; i.e. on a system bootup
                    // where clock is default to 1/1/04, and new time is 11/1/04, in both cases
                    // we are in standard time and should not perform the calculations below.
                    // However, going from 1/1/04 to 6/1/04 will require a flip, since one is 
                    // STD and other is DST
                    
                    if ((llDaylight <= llStandard && llDaylight <= llOld && llOld <= llStandard) 
                     || (llDaylight > llStandard && (llDaylight <= llOld || llOld <= llStandard)))
                    {
                        // We are in day light time presently
                        if (TIME_ZONE_ID_DAYLIGHT == g_dwTime)
                            return TRUE; // no need to change
                    }
                    else {
                        // We are in standard time presently
                        if (TIME_ZONE_ID_STANDARD == g_dwTime)
                            return TRUE; // no need to change
                    }
                }

                /* 
                    If we're going from Daylight to Standard, we need to move the 
                    clock BACK by g_tzDSTBias (60 minutes usually).  If we're going 
                    from Standard to Daylight we need to move it forward even though
                    g_tzDSTBias is 0.
                */ 
                if (TIME_ZONE_ID_DAYLIGHT == g_dwTime) // clock needs to go back - so we add (g_tzDSTBias is always negative)
                {
                    llNew = llOld + (LONGLONG)((LONGLONG)tzi.DaylightBias * FILETIME_TO_MINUTES);
                    // now flip the time flag  because we're just about to change                   
                    g_dwTime = TIME_ZONE_ID_STANDARD;                        
                    SetDaylightTime(FALSE);   
                }
                else // clock needs to go forward - so we subtract (see above)
                {
                    llNew = llOld - (LONGLONG)((LONGLONG)tzi.DaylightBias * FILETIME_TO_MINUTES);
                    // now flip the time flag  because we're just about to change
                    g_dwTime = TIME_ZONE_ID_DAYLIGHT;                        
                    SetDaylightTime(TRUE);                    
                }

                FileTimeToSystemTime((FILETIME *)&llNew, &stNew);

                
                DEBUGMSG(ZONE_DST, (L"[TIMESVC DST] TZ Change: Updating clock. Old Time =%d:%02d, New Time =%d:%02d\r\n", 
                                stOld.wHour, stOld.wMinute, stNew.wHour, stNew.wMinute));

                SetLocalTime(&stNew);

                DST_ShowMessage ();
            }
        }
        break;
        default :
        {
            DEBUGMSG(ZONE_INIT, (_T("[TIMESVC DST] Shutting down DST system\r\n")));
            return FALSE;
        }
    }

    return TRUE;
}   

/*++
Routine Description:
    Determines if we should show a mesage for DST change
    
Arguments:
    none
    
Return Value:
    TRUE if we want to show a message
    FALSE if we don't want to show a message
    
--*/
static BOOL  DST_ShowMessage(void)
{
    DWORD ret = 0;
    HKEY hKey = NULL;
    if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, RK_CLOCK, 0, 0, &hKey))
    {
        DWORD dwSize = sizeof(DWORD);
        RegQueryValueEx(hKey, RV_DSTUI, 0, NULL, (LPBYTE)&ret, &dwSize);
        RegCloseKey(hKey);
    }

#ifdef DEBUG
    if (ret)
    {
        DEBUGMSG(ZONE_DST, ( _T("[TIMESVC DST]  Show Message option for DST is on\r\n")));
    } 
    else
    {
        DEBUGMSG(ZONE_DST, ( _T("[TIMESVC DST]  Show Message option for DST is off\r\n")));
    }
#endif //DEBUG

    if (ret)
    {
        ret = FALSE;

        TCHAR msg[MAX_DSTMSGLEN];
        TCHAR title[MAX_DSTTITLELEN];
        if (LoadString(g_hInst, IDS_DSTCHANGED, msg, MAX_DSTMSGLEN) &&
            LoadString(g_hInst, IDS_DSTTIMEINFO, title, MAX_DSTTITLELEN))
        {
            HMODULE hCoreDll = LoadLibrary (L"coredll.dll");
            if (hCoreDll)
            {
                tMessageBoxW pMessageBox = (tMessageBoxW)GetProcAddress(hCoreDll, L"MessageBoxW");
                tGetForegroundWindow pGetForegroundWindow =(tGetForegroundWindow)GetProcAddress (hCoreDll, L"GetForegroundWindow");
                if (pMessageBox && pGetForegroundWindow)
                {
                    HANDLE hGweApiSetReady = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"SYSTEM/GweApiSetReady");

                    if (hGweApiSetReady)
                    {
                        int fGweReady = (WAIT_OBJECT_0 == WaitForSingleObject (hGweApiSetReady, 30000));
                        CloseHandle (hGweApiSetReady);

                        if (fGweReady)
                        {
                            pMessageBox(pGetForegroundWindow(), msg, title, MB_OK);
                            ret = TRUE;
                        }
                    }
                }

                FreeLibrary (hCoreDll);
            }
        }
    }

    return (BOOL)ret;
}

// Determine first day of week [Sun-Sat] of the first day of the month.
WORD GetFirstWeekDayOfMonth(LPSYSTEMTIME pst) {
    SYSTEMTIME st;
    FILETIME   ft;
    memcpy(&st,pst,sizeof(st));
    st.wDayOfWeek = 0;
    st.wDay = 1;

    // Convert to FILETIME and then back to SYSTEMTIME to get this information.
    SystemTimeToFileTime(&st,&ft);
    FileTimeToSystemTime(&ft,&st);
    ASSERT((st.wDayOfWeek >= 0) && (st.wDayOfWeek <= 6));
    return st.wDayOfWeek;
}

// Convert a SYSTEMITIME in the day-in-month format (ultimatly from 
// TIME_ZONE_INFORMATION) into a real SYSTEMTIME that the system can process.

static BOOL DST_DetermineChangeDate(LPSYSTEMTIME pStTzChange, BOOL fThisYear)
{
    LONGLONG  llChangeDate = 0, llNow = 0;
    SYSTEMTIME stAbsolute; // Conversion from pStTzChange
    SYSTEMTIME stNow; // Current time

    ASSERT((pStTzChange->wDay >= 1) && (pStTzChange->wDay <= 5));
    memcpy(&stAbsolute,pStTzChange,sizeof(stAbsolute));

    GetLocalTime(&stNow);

    // build up the new structure
    if (stAbsolute.wYear == 0) {
        pStTzChange->wYear = stAbsolute.wYear = stNow.wYear;
    }

    // Day of week [Sun-Sat] the first day of week falls on
    WORD wFirstDayInMonth = GetFirstWeekDayOfMonth(&stAbsolute);
    // First day in month [1-7] that is on same day of week as timezone change
    WORD wFirstDayTZChange;

    // Determine the first date in this month that falls on the day of 
    // the week that the time zone change will happen on, eg 
    // first Monday in June 2006 would get wFirstDayTZChange=5.
    if (wFirstDayInMonth > pStTzChange->wDayOfWeek)
        wFirstDayTZChange = (1 + 7 -(wFirstDayInMonth - pStTzChange->wDayOfWeek));
    else
        wFirstDayTZChange = 1 + pStTzChange->wDayOfWeek - wFirstDayInMonth;

    ASSERT((wFirstDayTZChange >= 1) && (wFirstDayTZChange <= 7));

    stAbsolute.wDay = wFirstDayTZChange;

    for (DWORD i = 1; i < pStTzChange->wDay; i++) {
        FILETIME ftTemp;
        // Increment the current week by one, and see if it's a legit date.
        // SystemTimeToFileTime has all the logic to determine if the SystemTime
        // is correct - if it's not in means we've "overshot" the intended date
        // and need to pull back on.  Consider for instance if the TZ Cutover
        // was set to be 5th Thursday in July 2006.  Eventually in this loop
        // we would get to July 34th, which is bogus, so we'd need to pull
        // back to July 27th.
        stAbsolute.wDay += 7;
        if (! SystemTimeToFileTime(&stAbsolute, &ftTemp)) {
            // We overshot, pull back a week
            stAbsolute.wDay -= 7;
            break;
        }
    }

     // Need to determine if the next occurrance of this date is THIS year or NEXT year
    VERIFY(SystemTimeToFileTime(&stAbsolute, (FILETIME *)&llChangeDate));
    VERIFY(SystemTimeToFileTime(&stNow, (FILETIME *)&llNow));

    // In this case, the change-over date was calculated to be in the past.
    // Sometimes this is what we want, othertimes (like when setting up an event
    // to be fired in future) we need to re-perform this calculation so that we
    // get a date in the future.
    if (!fThisYear && (llChangeDate < llNow))
    {
        //need to redetermine the date for next year
        pStTzChange->wYear++;
        return DST_DetermineChangeDate(pStTzChange,TRUE);
    }

    memcpy(pStTzChange,&stAbsolute,sizeof(stAbsolute));
    return TRUE;
}
 

/*++
Routine Description:
    determines if the current locale supports DST based on
    information in a TIME_ZONE_INFORMATION struct
    
Arguments:
    pointer to a TIME_ZONE_INFORMATION  struct
    
Return Value:
    TRUE if the locale supports DST, FALSE otherwise
    
--*/
static BOOL LocaleSupportsDST(TIME_ZONE_INFORMATION *ptzi)
{
    ASSERT(ptzi);

    // If the month value is zero then this locale does not change for DST
    // it should never be the case that we have a DST date but not a SDT date
    ASSERT(((0 != ptzi->StandardDate.wMonth) && (0 != ptzi->DaylightDate.wMonth)) ||
           ((0 == ptzi->StandardDate.wMonth) && (0 == ptzi->DaylightDate.wMonth)));

    if ((0 != ptzi->StandardDate.wMonth) && (0 != ptzi->DaylightDate.wMonth))
        return TRUE;
    else
        return FALSE;
}

//
//	Public interface section
//
int InitializeDST (HINSTANCE hInst) {
    g_hInst = hInst;
    g_hDSTShutdownEvent = CreateEvent (NULL, TRUE, TRUE, NULL);

    return (g_hDSTShutdownEvent != NULL) ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

void DestroyDST (void) {
    CloseHandle (g_hDSTShutdownEvent);
    g_hDSTShutdownEvent = NULL;
    g_hInst = NULL;
}

int RefreshDST (void) {
    return ERROR_SUCCESS;
}

int StartDST (void) {
    if (! g_hInst)
        return ERROR_SERVICE_DOES_NOT_EXIST;

    if (WaitForSingleObject (g_hDSTShutdownEvent, 0) == WAIT_TIMEOUT)
        return ERROR_SERVICE_ALREADY_RUNNING;

    ResetEvent (g_hDSTShutdownEvent);
    g_hDSTThread = CreateThread (NULL, 0, DST_Init, NULL, 0, NULL);
    if (g_hDSTThread == NULL) {
        int iErr = GetLastError ();
        SetEvent (g_hDSTShutdownEvent);
        return iErr;
    }

    return ERROR_SUCCESS;
}

int StopDST (void) {
    if (! g_hInst)
        return ERROR_SERVICE_DOES_NOT_EXIST;

    if (WaitForSingleObject (g_hDSTShutdownEvent, 0) == WAIT_OBJECT_0)
        return ERROR_SERVICE_NOT_ACTIVE;

    SetEvent (g_hDSTShutdownEvent);
    WaitForSingleObject (g_hDSTThread, INFINITE);
    CloseHandle (g_hDSTThread);
    g_hDSTThread = NULL;

    return ERROR_SUCCESS;
}

DWORD GetStateDST (void) {
    if (! g_hInst)
        return SERVICE_STATE_UNINITIALIZED;

    if (WaitForSingleObject (g_hDSTShutdownEvent, 0) == WAIT_OBJECT_0)
        return SERVICE_STATE_OFF;

    return SERVICE_STATE_ON;
}

int   ServiceControlDST (PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut, PDWORD pdwActualOut, int *pfProcessed) {
    *pfProcessed = FALSE;
    return ERROR_INVALID_PARAMETER;
}
