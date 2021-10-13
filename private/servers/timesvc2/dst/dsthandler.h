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
#pragma once

class DSTHandler
{
    enum DSTTimerType
    {
        Timer_None,
        Timer_DST_Start,
        Timer_DST_End,
        Timer_Year_Boundary,
    };
    
    //Runtime intialized constants when service startup
    DWORD           m_dwYearBoundaryDelay;  //The adjustment of year boundary time, in 100 nano-seconds
    DWORD           m_dwDynDSTUpdateDelay;  //The delay between a timezone registry change and the upcoming dynamic DST update, in 100 nano-seconds.
    
    //Cached system time zone information for current year
    TIME_ZONE_INFORMATION m_tzi;            //Normalized timezone information
    LONG            m_lBiasAdjustment;      //Indicating the adjustment of StandardBias and DaylightBias for normalization
    BOOL            m_bInverted;            //Indicating we exchange the Standard and Daylight settings for normalization
    
    SYSTEMTIME      m_stStandardDate;       //The actual DST end date for current year in local time
    LONGLONG        m_llStandardDate;       //The actual DST end date for current year in system time for comparision
    
    SYSTEMTIME      m_stDaylightDate;       //The actual DST start date for current year in local time
    LONGLONG        m_llDaylightDate;       //The actual DST start date for current year in system time for comparision
    
    BOOL            m_bDSTEnabled;          //Indicating if current timezone support DST for current year

    //Timer for the DST StandardDate, DaylightDate and year boundary
    HANDLE          m_hDSTTimerEvent;        //timer event
    
    //Notification event for DST handling
    HANDLE          m_hPreTimeChangeEvent;  //Event for pre-time change
    HANDLE          m_hTimeChangeEvent;     //Event for time change
    HANDLE          m_hTzChangeEvent;       //Event for timezoen change

    
    //Current status
    WORD            m_wCurYear;             //Current year from the local time
    BOOL            m_bDstON;              //Current DST status, maybe reverted if m_bReverted is TRUE
    LONGLONG        m_llSystemTimeNow;      // FILETIME for pseudo GetSystemTime and GetLocalTime
   
    //dynamic DST variables
    HANDLE          m_hTzRegChangeEvent;    //Event for timezone registry change
    HKEY            m_hTzRegRoot;           //Timezone registry root key
    
    HANDLE          m_hDynDSTTimerEvent;    //Event for the defered dynamic DST update timer
    BOOL            m_bDynDSTUpdatePending; //Indicating if there is a defered dynamic DST update scheduled

protected:
    DSTHandler();
    
    //Initialization for the DSTHandler.
    BOOL Init();
    
    //Should we automatically adjust the local time for DST status change
    BOOL AutoDST() const;
    
    //Update internal cached data due to timezone or time change
    void CacheUpdate();

    //Setup the DST timer according to current time and the DST settings
    BOOL SetupDSTTimer();
    
    //Re-determine DST status based on current time and timezone settings
    BOOL RedetermineDST();

    //Update the dynamic DST information for current year
    BOOL UpdateDynamicDST(BOOL bForce);
    
    //Set the daylight status
    BOOL SetDaylightTime(BOOL bOn);
    
    //Handle the DST start timer
    BOOL OnTimer_DSTStart();
    
    //Handle the DST end timer
    BOOL OnTimer_DSTEnd();
    
    //Handle the year boundary timer
    BOOL OnTimer_YearBoundary();

public:
    ~DSTHandler();
    
    static DSTHandler* Create()
    {
        DSTHandler* pHandler = new DSTHandler();
        if (!pHandler->Init())
        {
            delete pHandler;
            pHandler = NULL;
        }
        
        return pHandler;
    }

    //Return internal events
    HANDLE GetDynDSTTimerEvent() const { return m_hDynDSTTimerEvent;}
    HANDLE GetDSTTimerEvent() const {return m_hDSTTimerEvent;}
    HANDLE GetPreTimeChangeEvent() const {return m_hPreTimeChangeEvent;}
    HANDLE GetTimeChangeEvent() const {return m_hTimeChangeEvent;}
    HANDLE GetTzChangeEvent() const {return m_hTzChangeEvent;}
    HANDLE GetTzRegChangeEvent() const {return m_hTzRegChangeEvent;}
    
    //Timezone registry change notification handler
    BOOL OnTzRegChange();
    
    //Pre-Time change event handler
    BOOL OnPreTimeChange();

    //Handle all other events
    BOOL OnDstGenericEvent();
    
    //Overridden Time APIs 
    BOOL SetSystemTime(const SYSTEMTIME* lpSystemTime);
    BOOL SetLocalTime(const SYSTEMTIME* lpSystemTime);
    void GetSystemTime(LPSYSTEMTIME lpSystemTime);
    void GetLocalTime(LPSYSTEMTIME lpSystemTime);
    
};


