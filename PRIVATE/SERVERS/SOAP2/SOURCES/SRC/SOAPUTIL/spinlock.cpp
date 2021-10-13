//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      spinlock.cpp
//
// Contents:
//
//      spinlock implementation file
//
//----------------------------------------------------------------------------------

#include "headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


static WCHAR *rgwszSpinlockTitles [] =
{
    L"Null SpinLock",
    L"Assert File",

    // STEP 2: For adding a new spinlock, add the text name above this line.
};


void CSpinlock::SpinToAcquire()
{
    ULONG ulBackoffs = 0;
    ULONG ulSpins = 0;

    #ifdef DEBUG
        const int x_BackoffLimit = 1000;

        ASSERT(m_eType < x_Last);
    #else
        const int x_BackoffLimit = 10000;
    #endif

    while(1)
        {
        // It is assumed this routine is only called after the inline
        // method has failed the interlocked spinlock test. Therefore we
        // retry using the safe test only after cheaper, unsafe test
        // succeeds.
        //
        for (int i = 0; i < 10000; i++, ulSpins++)
            {
            // Note: Must cast through volatile to ensure the lock is
            // refetched from memory.
            if (*((volatile DWORD*)&m_dwLock) == 0)
                break;
            }

        // Try the inline atomic test again.
        //
        if (GetNoWait())
            break;

        // Yield after hitting the cspinctr limit.
        // Sleep (0) only yields to threads of same priority
        // if hit limit 2 times on uniprocessor, Sleep(1)
        // if hit limit 10000 times, sleep 5sec
        //
        ulBackoffs++;

        if ((ulBackoffs % x_BackoffLimit) == 0)
        {
            Sleep (5000);
        }
        else
            Sleep (1);
        }
    
    return;
    }


