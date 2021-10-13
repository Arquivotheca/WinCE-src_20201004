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
#include <windows.h>

#ifdef UNDER_CE
#include <ceperf.h>
#endif

/****************************************************************************
CePerfTimer: Wrapper for CePerf and CePerfScenario
  This tool is designed as a simple wrapper for developing tests that use
  CePerf and CePerfScenario.

Example:
    // Define some markers for DDraw
    enum DDrawPerfMarkers {
        DDPERF_FLIP,
        DDPERF_BLT,
        DDPERF_LOCK,
    };
    // Declare the DDrawPerfTimer class which has three markers
    // NOTE: Names must not include '\' and cannot be longer than CEPERF_MAX_ITEM_NAME_LEN (32)
    CEPERFTIMER_BEGIN( DDrawPerfTimer )
        // Note that these don't need to be in numerical order based on marker
        CEPERFTIMER_BASIC_DURATION( DDPERF_BLT,  _T("DDraw Blt")  )
        CEPERFTIMER_BASIC_DURATION( DDPERF_FLIP, _T("DDraw Flip") )

        // Note that _SHARED can be used for tracking single markers across multiple threads
        CEPERFTIMER_BASIC_DURATION_SHARED( DDPERF_LOCK, _T("DDraw Lock") )

    CEPERFTIMER_END()

    // Inside your method, you can now use the DDrawPerfTimer class
    void PerfTest()
    {
        DDrawPerfTimer ddPerf;
        ddPerf.BeginPerfSession(
            _T("DirectDraw"),       // Namespace
            _T("Performance"),      // Session, can include \ to nest: _T("Normal\\Composited")
            _T("CePerf_DDrawResults.xml"), // Result XML (if first char isn't '\', will prepend VirtualRelease
            guidDDrawGuid           // Unique GUID for the DirectDraw performance tests

        ddPerf.BeginDuration(DDPERF_LOCK);
        // Do lock
        ddPerf.EndDuration(DDPERF_LOCK);
    }

Notes:
    * Markers should not be sparse and should begin with 0
      (The size of the internal array generated is determined by
      the largest marker value, not by how many markers are declared)
    * Marker values must be unique
    * The items can be defined in any order, regardless of marker value
    * You will get better results if BeginDuration and EndDuration are always
      called in matching pairs - the item handle is saved by BeginDuration
  **************************************************************************/

#ifdef UNDER_CE
#define CE_ONLY
#define CE_ONLY_HR

typedef struct _CEPERFTIMER_BASIC_ITEM
{
    CEPERF_BASIC_ITEM_DESCRIPTOR desc;
    DWORD marker;
} CEPERFTIMER_BASIC_ITEM;

#else // ! UNDER_CE

#define CE_ONLY {return;}
#define CE_ONLY_HR {return E_NOTIMPL;}

#define CEPERF_ITEM_DATA void

#endif

class CePerfTimer
{
public:
    CePerfTimer() CE_ONLY;
    ~CePerfTimer() CE_ONLY;

    HRESULT BeginPerfSession(
        const TCHAR * tszNamespace, 
        const TCHAR * tszCurrentSession,
        const TCHAR * tszResultFile,
        const GUID & guidNamespace) CE_ONLY_HR;
    HRESULT BeginPerfSession(
        const TCHAR * tszNamespace,
        const TCHAR * tszCurrentScenario,
        const TCHAR * tszCurrentSession,
        const TCHAR * tszResultFile,
        const GUID & guidNamespace) CE_ONLY_HR;
    HRESULT EndPerfSession() CE_ONLY_HR;

    HRESULT BeginDuration(DWORD dwMarker) CE_ONLY_HR;
    HRESULT IntermediateDuration(DWORD dwMarker, CEPERF_ITEM_DATA * lpData) CE_ONLY_HR;
    HRESULT EndDuration(DWORD dwMarker) CE_ONLY_HR;
    HRESULT EndDuration(DWORD dwMarker, HRESULT hr) CE_ONLY_HR;

    HRESULT IncrementStatistic(DWORD dwMarker) CE_ONLY_HR;
    HRESULT AddStatistic(DWORD dwMarker, DWORD dwValue) CE_ONLY_HR;
    HRESULT SetStatistic(DWORD dwMarker, DWORD dwValue) CE_ONLY_HR;

protected:
    HRESULT GeneratePerfItems() CE_ONLY_HR;
    HRESULT RegisterItems() CE_ONLY_HR;
    HRESULT DeregisterItems() CE_ONLY_HR;

#ifdef UNDER_CE
    virtual HRESULT ItemDescriptors(
        const CEPERFTIMER_BASIC_ITEM ** prgPerfItems, 
        DWORD * pdwCount) = 0;

    CEPERF_BASIC_ITEM_DESCRIPTOR * m_pPerfItems;
    DWORD m_dwPerfItemCount;
    HANDLE m_hSessionHandle;
    TCHAR m_tszNamespace[32];
    TCHAR m_tszSessionName[MAX_PATH];
    TCHAR m_tszScenarioName[MAX_PATH];
    TCHAR m_tszResultFile[MAX_PATH];
    GUID m_guid;
    bool m_fSessionBegun;

    DWORD m_dwMRUMarker;
    HANDLE m_hMRUHandle;
#endif
};

#ifdef UNDER_CE
#define CEPERFTIMER_BASIC_STATISTIC(dwMarker, szName)   \
    { {                                                 \
      INVALID_HANDLE_VALUE,                             \
      CEPERF_TYPE_STATISTIC,                            \
      (szName),                                         \
      CEPERF_STATISTIC_RECORD_MIN },                    \
    dwMarker },

#define CEPERFTIMER_BASIC_DURATION(dwMarker, szName)    \
    { {                                                 \
      INVALID_HANDLE_VALUE,                             \
      CEPERF_TYPE_DURATION,                             \
      (szName),                                         \
      (CEPERF_RECORD_ABSOLUTE_PERFCOUNT |               \
       CEPERF_RECORD_ABSOLUTE_TICKCOUNT |               \
       CEPERF_RECORD_ABSOLUTE_CPUPERFCTR |              \
       CEPERF_DURATION_RECORD_MIN) },                   \
    dwMarker },

#define CEPERFTIMER_BASIC_DURATION_SHARED(dwMarker, szName)    \
    { {                                                 \
      INVALID_HANDLE_VALUE,                             \
      CEPERF_TYPE_DURATION,                             \
      (szName),                                         \
      (CEPERF_RECORD_ABSOLUTE_PERFCOUNT |               \
       CEPERF_RECORD_ABSOLUTE_TICKCOUNT |               \
       CEPERF_RECORD_ABSOLUTE_CPUPERFCTR |              \
       CEPERF_DURATION_RECORD_SHARED |                  \
       CEPERF_DURATION_RECORD_MIN) },                   \
    dwMarker },

#define CEPERFTIMER_BEGIN_ITEM_LIST()\
virtual HRESULT ItemDescriptors(\
    const CEPERFTIMER_BASIC_ITEM ** prgPerfItems, \
    DWORD * pdwCount)\
{\
    static const CEPERFTIMER_BASIC_ITEM rgItems[] = { \

#define CEPERFTIMER_END_ITEM_LIST()\
    }; \
    *prgPerfItems = rgItems;\
    *pdwCount = _countof(rgItems);\
    return S_OK;\
}\

#define CEPERFTIMER_BEGIN(className)    \
class className : public CePerfTimer    \
{                                       \
public:                                 \
    className () : CePerfTimer() {}     \
protected:                              \
    CEPERFTIMER_BEGIN_ITEM_LIST()

#define CEPERFTIMER_END()               \
    CEPERFTIMER_END_ITEM_LIST() };

#else // UNDER_CE
#define CEPERFTIMER_BASIC_STATISTIC(dwMarker, szName)
#define CEPERFTIMER_BASIC_DURATION(dwMarker, szName)
#define CEPERFTIMER_BASIC_DURATION_SHARED(dwMarker, szName)
#define CEPERFTIMER_BEGIN_ITEM_LIST()
#define CEPERFTIMER_END_ITEM_LIST()
#define CEPERFTIMER_BEGIN(className)    \
class className : public CePerfTimer    \
{                                       \
public:                                 \
    className () : CePerfTimer() {}     

#define CEPERFTIMER_END() };

#endif
