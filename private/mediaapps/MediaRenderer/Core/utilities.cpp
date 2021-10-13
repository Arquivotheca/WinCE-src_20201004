//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

// Utilities: Implementation of utilities

#include "stdafx.h"

DWORD OnStateChanged(av::IEventSink* pSubscriber, LPCWSTR pszVariable, LPCWSTR pszValue)
{
    if(!pSubscriber || !pszVariable || !pszValue)
        return av::ERROR_AV_POINTER;

    return pSubscriber->OnStateChanged(pszVariable, pszValue);
}

DWORD OnStateChanged(av::IEventSink* pSubscriber, LPCWSTR pszVariable, long lValue)
{
    if(!pSubscriber || !pszVariable)
        return av::ERROR_AV_POINTER;

    return pSubscriber->OnStateChanged(pszVariable, lValue);
}


DWORD MultiOnStateChanged(av::IEventSink* pSubscriber, const LPCWSTR vpszchangedStateVars[], LPCWSTR vpsznewValues[])
{
    if(!pSubscriber || !vpszchangedStateVars || !vpsznewValues)
        return av::ERROR_AV_POINTER;

    DWORD retOSC;
    unsigned int i = 0;

    while(0 != vpszchangedStateVars[i])
    {
        retOSC = pSubscriber->OnStateChanged(vpszchangedStateVars[i], vpsznewValues[i]);
        if(av::SUCCESS_AV != retOSC)
            return retOSC;
        ++i;
    }

    return av::SUCCESS_AV;
}


DWORD MultiOnStateChanged(av::IEventSink* pSubscriber, const LPCWSTR vpszchangedStateVars[], long vnnewValues[])
{
    if(!pSubscriber || !vpszchangedStateVars || !vnnewValues)
        return av::ERROR_AV_POINTER;

    DWORD retOSC;
    unsigned int i = 0;

    while(0 != vpszchangedStateVars[i])
    {
        retOSC = pSubscriber->OnStateChanged(vpszchangedStateVars[i], vnnewValues[i]);
        if(av::SUCCESS_AV != retOSC)
            return retOSC;
        ++i;
    }

    return av::SUCCESS_AV;
}


void FormatMediaTime(LONGLONG lTime, ce::wstring* pstr)
{
    // Handle bad pointer
    if (pstr == NULL)
    {
        return;
    }

    // Handle undersized numbers - less than 0
    if (lTime < 0)
    {
        pstr->assign(L"0:00:00");
        return;
    }

    // Handle outsized numbers - over 1000 hours
    if (lTime >= (LONGLONG)1000 * 60 * 60 * 1000)
    {
        pstr->assign(L"999:59:59");
        return;
    }

    // convert input fom ms to 10ths of second.
    DWORD time = lTime / 100;

    // Convert to hr/min/sec/tenths
    int tenths = time % 10;
    time /= 10;
    int sec = time % 60;
    time /= 60;
    int min = time % 60;
    time /= 60;
    int hr =  time;
    
    // Format the time - but keep the string short
    if (hr >= 10)
    {
        swprintf_s(pstr->get_buffer(), pstr->capacity(), L"%d:%02d:%02d", hr, min, sec);
    }
    else
    {
        swprintf_s(pstr->get_buffer(), pstr->capacity(), L"%d:%02d:%02d.%d", hr, min, sec, tenths);
    }
}


HRESULT ParseMediaTime(LPCWSTR pszTime, LONGLONG* plTime)
{
    int hr, min, sec, ms;
    
    // calculate ms media time in millisecond units
    if(4 != swscanf(pszTime, L"%d:%d:%d.%d", &hr, &min, &sec, &ms))
    {
        // calculate sec media time in 100-nanosecond units
        if(3 != swscanf(pszTime, L"%d:%d:%d", &hr, &min, &sec))
        {
            return E_FAIL;
        }
        ms = 0;
    }

    // calculate media time in millisecond units
    *plTime = (3600*1000ll * hr + 60*1000ll * min + 1000ll * sec + ms);
    
    return S_OK;
}

DWORD DBToAmpFactor(LONG lDB)
{
    double dAF;

    // REMIND hack to make mixer code work- it only handles 16-bit factors and
    //  cannot amplify
    if (0 <= lDB) return 0x0000FFFF;

    // input lDB is 100ths of decibels

    dAF = pow(10.0, (0.5+((double)lDB))/2000.0);

    // This gives me a number in the range 0-1
    // normalise to 0-65535

    return (DWORD)(dAF*65535);
}


LONG AmpFactorToDB(DWORD dwFactor)
{
    if(1>=dwFactor)
    {
        return -10000;
    }
    else if (0xFFFF <= dwFactor) 
    {
        return 0;    // This puts an upper bound - no amplification
    }
    else
    {
        return (LONG)(2000.0 * log10((-0.5+(double)dwFactor)/65536.0));
    }
}


LONG VolumeLinToLog(short LinKnobValue)
{
    LONG lLinMin = DBToAmpFactor(AX_MIN_VOLUME);
    LONG lLinMax = DBToAmpFactor(AX_MAX_VOLUME);

    LONG lLinTemp = (LONG) (LinKnobValue - MIN_VOLUME_RANGE) * (lLinMax - lLinMin)
        / (MAX_VOLUME_RANGE - MIN_VOLUME_RANGE) + lLinMin;

    LONG LogValue = AmpFactorToDB( lLinTemp );

    return( LogValue );
}


short VolumeLogToLin(LONG LogValue)
{
    LONG lLinMin = DBToAmpFactor(AX_MIN_VOLUME);
    LONG lLinMax = DBToAmpFactor(AX_MAX_VOLUME);

    short LinKnobValue = (short) ( ((LONG) DBToAmpFactor(LogValue) - lLinMin) *
        (MAX_VOLUME_RANGE - MIN_VOLUME_RANGE) / (lLinMax - lLinMin) + MIN_VOLUME_RANGE);

    return( LinKnobValue );
}
