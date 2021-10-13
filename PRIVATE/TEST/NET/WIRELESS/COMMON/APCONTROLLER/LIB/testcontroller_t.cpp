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
// Implementation of the TestController_t class.
//
// ----------------------------------------------------------------------------

#include <TestController_t.hpp>

#include <assert.h>

using namespace ce::qa;

// ----------------------------------------------------------------------------
// 
// Constructor.
//
TestController_t::
TestController_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
TestController_t::
~TestController_t(void)
{
    Stop(1);
}

// ----------------------------------------------------------------------------
//
// Thread call-back procedure.
//
DWORD WINAPI
TestController_t::
ThreadProc(LPVOID pParameter)
{
    TestController_t *testThread = (TestController_t *) pParameter;
    assert(testThread != NULL);
    return (DWORD) testThread->Run();
}

// ----------------------------------------------------------------------------
//
// Determines whether the test has been told to stop.
//
bool
TestController_t::
IsInterrupted(void)
{
    ce::gate<ce::critical_section> locker(m_Locker);

    return (!m_InterruptHandle.valid()
         || WAIT_TIMEOUT != WaitForSingleObject(m_InterruptHandle, 0L));
}

// ----------------------------------------------------------------------------
//    
// Starts the test running in a sub-thread.
//
HRESULT
TestController_t::
Start(void)
{
    ce::gate<ce::critical_section> locker(m_Locker);
    
    Wait(100);

    if (!IsAlive())
    {
        if (m_InterruptHandle.valid())
        {
            ResetEvent(m_InterruptHandle);
        }
        else
        {
		    m_InterruptHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (!m_InterruptHandle.valid())
            {
                DWORD result = GetLastError();
                return HRESULT_FROM_WIN32(result);
            }
        }

        m_ThreadHandle = CreateThread(NULL, 0, TestController_t::ThreadProc,
                                     (PVOID)this, 0, &m_ThreadId);
        if (!IsAlive())
        {
            DWORD result = GetLastError();
            return HRESULT_FROM_WIN32(result);
        }
    }
    
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//    
// Waits the specified time then interrupts the test thread.
// Returns immediately if the thread is already done.
// If grace-time is non-zero, terminates the thread if it ignores
// the interruption too long.
//
DWORD
TestController_t::
Wait(long MaxWaitTimeMS, long MaxGraceTimeMS)
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
                if (WAIT_TIMEOUT == ret
                 && 0L < MaxGraceTimeMS
                 && m_InterruptHandle.valid())
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
HRESULT
TestController_t::
Stop(long MaxGraceTimeMS)
{
    DWORD result = Wait(0, MaxGraceTimeMS);
    return HRESULT_FROM_WIN32(result);
}

// ----------------------------------------------------------------------------
