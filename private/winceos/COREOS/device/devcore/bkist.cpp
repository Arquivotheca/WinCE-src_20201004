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
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:  
    BkIst.cpp
Abstract:
    Backgroud IST implementation.

Notes: 
--*/

#include <windows.h>
#include <cmthread.h>
#include <devload.h>
#include <nkintr.h>
#include <pkfuncs.h>

#include "Devzones.h"

class CBackgroundIST : public CMiniThread {
public:
    CBackgroundIST() 
    :   CMiniThread(0, TRUE) 
    ,   m_hISTEvent(NULL)
    ,   m_fIntInitialized(FALSE)
    {
    };
    ~CBackgroundIST() {
        m_bTerminated=TRUE;
        ThreadStart();
        if (m_hISTEvent) {
            SetEvent(m_hISTEvent);
            VERIFY(ThreadTerminated(1000));
        };
        if (m_fIntInitialized) {
            InterruptDisable(SYSINTR_IST_BGT);
        }
        if (m_hISTEvent) {
            CloseHandle(m_hISTEvent);
        }
    }
    BOOL Init() {
        m_hISTEvent= CreateEvent(0,FALSE,FALSE,NULL);
        if (m_hISTEvent) {
            m_fIntInitialized = InterruptInitialize(SYSINTR_IST_BGT,m_hISTEvent,NULL, 0 );
        }
        if (m_fIntInitialized) {
            ThreadStart();
        }
        return m_fIntInitialized;
    }
private:
    DWORD  ThreadRun() {
        KernelLibIoControl((HANDLE)KMOD_OAL,IOCTL_HAL_UNKNOWN_IRQ_HANDLER,NULL,0,NULL,0,NULL);
        if (!m_bTerminated && m_hISTEvent!=NULL) {
            BOOL fRet= TRUE;
            while (!m_bTerminated && fRet ) {
                switch (WaitForSingleObject(m_hISTEvent,INFINITE)){
                  case WAIT_OBJECT_0:
                    fRet = KernelLibIoControl((HANDLE)KMOD_OAL,IOCTL_HAL_UNKNOWN_IRQ_HANDLER,NULL,0,NULL,0,NULL);
                    DEBUGMSG((ZONE_WARNING && !fRet), (TEXT("Waring!!CBackgroundIST: platform does not support IOCTL_HAL_UNKNOWN_IRQ_HANDLER, Quiting \r\n")));
                    break;
                  default:
                    ASSERT(FALSE);
                    fRet = FALSE;
                    break;
                }
            }
        }
        return 0;
    }
    HANDLE  m_hISTEvent;
    BOOL    m_fIntInitialized;
};


extern "C"
PVOID   CreateBackgroundIST()
{
    CBackgroundIST * pBkIst = new CBackgroundIST;
    if (pBkIst && !pBkIst->Init()) {
        delete pBkIst;
        pBkIst = NULL;
    }
    return (PVOID)pBkIst;
}
extern "C" 
VOID    DeleteBackgroundIST(PVOID pObject) 
{
    if (pObject) {
        delete (CBackgroundIST *)pObject;
    }
}


