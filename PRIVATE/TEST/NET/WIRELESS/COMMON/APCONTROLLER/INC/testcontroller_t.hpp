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
// Definitions and declarations for the TestController_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_TestController_t_
#define _DEFINED_TestController_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>
#include <inc/auto_xxx.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides multi-threaded control of a test scenario. Individual test
// scenarios are created by extended this class and implementing the Run
// method.
//
class TestController_t
{
private:

    // Thread call-back procedure:
    static DWORD WINAPI
    ThreadProc(LPVOID pParameter);

    // Sub-thread handle and id:
    ce::auto_handle m_ThreadHandle;
    DWORD           m_ThreadId;

    // Interruption signal:
    ce::auto_handle m_InterruptHandle;

    // Sub-thread's result-code:
    DWORD m_ThreadResult;
    
    // Copy and assignment are deliberately disabled:
    TestController_t(const TestController_t &src);
    TestController_t &operator = (const TestController_t &src);

protected:

    // Synchronization object:
    ce::critical_section m_Locker;

public:
   
    // Constructor and destructor:
    TestController_t(void);
    virtual
   ~TestController_t(void);

    // Starts the test running in a sub-thread:
    HRESULT
    Start(void);

    // Determines whether the test thread is still running:
    bool
    IsAlive(void) const { return m_ThreadHandle.valid() != (BOOL)0; }

    // Determines whether the test has been told to stop:
    bool
    IsInterrupted(void);
    
    // Waits the specified time then interrupts the test thread:
    // Returns immediately if the thread is already done.
    // If grace-time is non-zero, terminates the thread if it ignores
    // the interruption too long.
    DWORD
    Wait(long MaxWaitTimeMS, long MaxGraceTimeMS = 0);

    // Interrupts the test thread:
    // If grace-time is non-zero, terminates the thread if it ignores
    // the interruption too long.
    HRESULT
    Stop(long MaxGraceTimeMS = 0);

protected:
            
    // Runs the sub-thread:
    // Derived classes implement the actual test procedure here.
    virtual DWORD
    Run(void) = 0;
};

};
};
#endif /* _DEFINED_TestController_t_ */
// ----------------------------------------------------------------------------
