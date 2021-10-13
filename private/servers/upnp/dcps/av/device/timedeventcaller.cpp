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

#include "av_upnp.h"

using namespace av_upnp;
using namespace av_upnp::details;

/////////////////////////////////////////////////////////////////////////////
// TimedEventCaller

DWORD TimedEventCaller::RegisterCallee(DWORD nTimeout, ITimedEventCallee* pCallee)
{
    if(!pCallee)
    {
        return ERROR_AV_POINTER;
    }

    ce::gate<ce::critical_section_with_copy> gate(m_csMapCalleeGroups);

    CalleeGroupMap::iterator it = m_mapCalleeGroups.find(nTimeout);

    if(it == m_mapCalleeGroups.end())
    {
        // Create CalleeGroup for this timeout
        it = m_mapCalleeGroups.insert(nTimeout, CalleeGroup(nTimeout));

        if(it == m_mapCalleeGroups.end())
        {
            return ERROR_AV_OOM;
        }
    }

    return it->second.RegisterCallee(pCallee);
}


DWORD TimedEventCaller::UnregisterCallee(DWORD nTimeout, ITimedEventCallee* pCallee)
{
    if(!pCallee)
    {
        return ERROR_AV_POINTER;
    }

    ce::gate<ce::critical_section_with_copy> gate(m_csMapCalleeGroups);

    const CalleeGroupMap::iterator it = m_mapCalleeGroups.find(nTimeout);

    if(it == m_mapCalleeGroups.end())
    {
        return ERROR_AV_INVALID_INSTANCE;
    }

    if(it->second.UnregisterCallee(pCallee))
    {
        // erase empty callee group
        m_mapCalleeGroups.erase(it);
    }

    return SUCCESS_AV;
}


void TimedEventCaller::AddRef(DWORD nTimeout)
{
    ce::gate<ce::critical_section_with_copy> gate(m_csMapCalleeGroups);

    CalleeGroupMap::iterator it = m_mapCalleeGroups.find(nTimeout);

    assert(it != m_mapCalleeGroups.end());

    it->second.AddRef();
}


void TimedEventCaller::Release(DWORD nTimeout)
{
    ce::gate<ce::critical_section_with_copy> gate(m_csMapCalleeGroups);

    CalleeGroupMap::iterator it = m_mapCalleeGroups.find(nTimeout);

    assert(it != m_mapCalleeGroups.end());

    it->second.Release();
}


//
// EventCallerThread
//
DWORD WINAPI TimedEventCaller::CalleeGroup::EventCallerThread(LPVOID lpvthis)
{
    assert(lpvthis);

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    // This function loops on the for statement every pThis->m_nTimeout ms to call the callees,
    // also waking up when it has been signaled to exit. If m_hEventRun is not signaled,
    // it blocks until m_hEventRun is again signaled or this thread has been signaled to exit.

    CalleeGroup* pThis = reinterpret_cast<CalleeGroup*>(lpvthis);

    HANDLE aEvents[] = {pThis->m_hEventRun, pThis->m_hEventExit};

    for(;;)
    {
        DWORD dw;

        if(WAIT_TIMEOUT != (dw = WaitForSingleObject(pThis->m_hEventExit, pThis->m_nTimeout)))
        {
            assert(dw == WAIT_OBJECT_0);
            CoUninitialize();
            return 0;
        }

        if(WAIT_OBJECT_0 == (dw = WaitForMultipleObjects(sizeof(aEvents)/sizeof(aEvents[0]), aEvents, FALSE, INFINITE)))
        {    
            ce::gate<ce::critical_section_with_copy> gate(pThis->m_cs);

            for(CalleeSet::iterator it = pThis->m_setCallees.begin(), itEnd = pThis->m_setCallees.end(); itEnd != it; ++it)
            {
                (*it)->TimedEventCall();
            }
        }
        else
        {
            assert(dw == WAIT_OBJECT_0 + 1);
        }
    }
}




/////////////////////////////////////////////////////////////////////////////
// CalleeGroup

TimedEventCaller::CalleeGroup::CalleeGroup(DWORD nTimeout)
    : m_nTimeout(nTimeout),
      m_nRefs(0)
{
}


TimedEventCaller::CalleeGroup::~CalleeGroup()
{
    assert(m_setCallees.empty());
}


// RegisterCallee
DWORD TimedEventCaller::CalleeGroup::RegisterCallee(ITimedEventCallee* pCallee)
{
    ce::gate<ce::critical_section_with_copy> gate(m_cs);

    assert(m_setCallees.end() == m_setCallees.find(pCallee));

    if(m_setCallees.end() == m_setCallees.insert(pCallee))
    {
        return ERROR_AV_OOM;
    }

    if(1 == m_setCallees.size())
    {
        assert(!m_hEventRun.valid());
        assert(!m_hEventExit.valid());

        m_hEventRun = CreateEvent(NULL, TRUE, FALSE, NULL);
        m_hEventExit = CreateEvent(NULL, TRUE, FALSE, NULL);

        if(!m_hEventRun.valid() || !m_hEventExit.valid())
        {
            m_setCallees.erase(pCallee);
            return ERROR_AV_OOM;
        }

        // Create the thread for this no-longer-callee-empty CalleeGroup
        m_hCallerThread = CreateThread(NULL,
                                       0,
                                       TimedEventCaller::CalleeGroup::EventCallerThread,
                                       static_cast<LPVOID>(this),
                                       0,
                                       NULL);

        if(!m_hCallerThread.valid())
        {
            m_setCallees.erase(pCallee);
            return ERROR_AV_UPNP_ACTION_FAILED;
        }
    }
    
    return SUCCESS_AV;
}


// UnregisterCallee
BOOL TimedEventCaller::CalleeGroup::UnregisterCallee(ITimedEventCallee* pCallee)
{
    ce::gate<ce::critical_section_with_copy> gate(m_cs);

    size_t nItemsErased = m_setCallees.erase(pCallee);

    assert(nItemsErased);

    if(0 == m_setCallees.size())
    {
        // must leave lock as EventCallerThread() may need to obtain this lock before exiting
        gate.leave(); 

        assert(m_hEventExit.valid());
        assert(m_hCallerThread.valid());
        
        BOOL bRet = SetEvent(m_hEventExit);
        
        assert(bRet);

        DWORD dw = WaitForSingleObject(m_hCallerThread, INFINITE);

        assert(dw == WAIT_OBJECT_0);

        return TRUE;
    }

    return FALSE;
}


// AddRef
void TimedEventCaller::CalleeGroup::AddRef()
{
    assert(ULONG_MAX != m_nRefs);

    if(1 == InterlockedIncrement(&m_nRefs))
    {
        SetEvent(m_hEventRun);
    }
}


// Release
void TimedEventCaller::CalleeGroup::Release()
{
    assert(m_nRefs > 0);

    LONG refCnt = InterlockedDecrement(&m_nRefs);
    if(0 == refCnt)
    {
        ResetEvent(m_hEventRun);
    }
}
