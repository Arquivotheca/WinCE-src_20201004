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
// Definitions and declarations for the ThreadController_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_ThreadController_t_
#define _DEFINED_ThreadController_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>
#include <auto_xxx.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides a simple sub-thread abstract base-class. Contrete clsses are
// created by extended this class and implementing the Run method.
//
class ThreadController_t
{
public:
   
    // Constructor and destructor:
    //
    ThreadController_t(void);
    virtual
   ~ThreadController_t(void);

    // Starts the thread:
    //
    DWORD
    Start(void);

    // Determines whether the thread is still running:
    //
    bool
    IsAlive(void) const { return m_ThreadHandle.valid() != (BOOL)0; }

    // Determines whether the thread has been told to stop:
    //
    bool
    IsInterrupted(void);
    
    // Waits the specified time then interrupts the thread:
    // Returns immediately if the thread is already done.
    // If grace-time is non-zero, terminates the thread if it ignores
    // the interruption too long.
    //
    DWORD
    Wait(long MaxWaitTimeMS, long MaxGraceTimeMS = 0);

    // Interrupts the thread:
    // If grace-time is non-zero, terminates the thread if it ignores
    // the interruption too long.
    //
    DWORD
    Stop(long MaxGraceTimeMS = 0);

protected:
            
    // Runs the sub-thread:
    // Derived classes implement the actual thread procedure here.
    //
    virtual DWORD
    Run(void) = 0;

    // Synchronization object:
    //
    ce::critical_section m_Locker;

    // Interruption signal:
    //
    ce::auto_handle m_InterruptHandle;
    
private:

    // Thread call-back procedure:
    //
    static DWORD WINAPI
    ThreadProc(LPVOID pParameter);

    // Sub-thread handle and id:
    //
    ce::auto_handle m_ThreadHandle;
    DWORD           m_ThreadId;

    // Sub-thread's result-code:
    //
    DWORD m_ThreadResult;
    
    // Copy and assignment are deliberately disabled:
    //
    ThreadController_t(const ThreadController_t &src);
    ThreadController_t &operator = (const ThreadController_t &src);
};

};
};
#endif /* _DEFINED_ThreadController_t_ */
// ----------------------------------------------------------------------------
