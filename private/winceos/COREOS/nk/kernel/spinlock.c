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
#include <windows.h>
#include "kernel.h"
#include "spinlock.h"

void MDReleaseSpinlockAndReschedule (volatile SPINLOCK *psplock);

void InitializeSpinLock (volatile SPINLOCK *psplock)
{
    psplock->nestcount = psplock->owner_cpu = psplock->next_owner = 0;
}


void AcquireSpinLock (volatile SPINLOCK *psplock)
{
    if (1 == g_pKData->nCpus) {
        if (!psplock->owner_cpu) {
            PcbIncOwnSpinlockCount ();
            psplock->owner_cpu = 1;
        }
        
    } else {
        DataMemoryBarrier ();
        if (InSysCall ()) {
            
            DWORD dwCurCpu = PcbGetCurCpu ();
            DWORD dwSpinlockOwner;
            if (dwCurCpu != psplock->owner_cpu) {

                do {

                    for ( ; ; ) {
                        dwSpinlockOwner = psplock->owner_cpu;
                        if (!dwSpinlockOwner || (dwCurCpu == dwSpinlockOwner)) {
                            break;
                        }
                        MDSpin ();
                    }
                    
                } while (InterlockedCompareExchange ((PLONG) &psplock->owner_cpu, dwCurCpu, dwSpinlockOwner) != (LONG) dwSpinlockOwner);

                PcbIncOwnSpinlockCount ();      // now we holds an extra spinlock
            }

        } else {
            
            for ( ; ; ) {        
                if (!psplock->owner_cpu) {
                    
                    PcbIncOwnSpinlockCount ();      // stop reschedule

                    if (!InterlockedCompareExchange ((PLONG) &psplock->owner_cpu, PcbGetCurCpu (), 0)) {
                        break;
                    }
                    
                    PcbDecOwnSpinlockCount ();      // enable reschedule

                    // The chance of getting here should be pretty small - the lock must be
                    // taken by other CPU between the "if" test and the InterlockedCompareExchange.
                    // KCall to a dummy function to reschedule if needed. We can do more optimization if this proves to
                    // be a perf issue.
                    KCall ((PKFN) ReturnFalse);
                }        
                MDSpin ();
            }
        }
        DataMemoryBarrier ();
        DEBUGCHK (PcbGetCurCpu () == psplock->owner_cpu);
    }
    psplock->nestcount ++;
}

#pragma optimize( "", off )
void ReleaseSpinLock (volatile SPINLOCK *psplock)
{
    if (! --psplock->nestcount) {
        PPCB ppcb;

        DataSyncBarrier ();
        DataMemoryBarrier ();
        DEBUGCHK (PcbGetCurCpu () == psplock->owner_cpu);

        ppcb = GetPCB ();

        if (InPrivilegeCall () || (ppcb->ownspinlock > 1)) {
            DataMemoryBarrier ();
            psplock->owner_cpu = psplock->next_owner;
            MDSignal ();
            ppcb->ownspinlock --;

        } else if (ppcb->dwKCRes) {
            MDReleaseSpinlockAndReschedule (psplock);

        } else {
            DEBUGCHK (1 == ppcb->ownspinlock);
            DataMemoryBarrier ();
            psplock->owner_cpu = psplock->next_owner;
            MDSignal ();
            ppcb->ownspinlock  = 0;
            if (ppcb->bResched) {
                // KCall to a dummy function to reschedule
                KCall ((PKFN) ReturnFalse);
            }
        }
    }
}
#pragma optimize( "", on )

//
// while holding spin-lock, enumerating through a data strucutre, we usually need to release the lock to enable
// preemption (in the old code, it's usually a serious a KCalls). In the case where we don't got interrupted, it's
// okay to kepp enumerating the structure without releasing the lock.
//
void EnablePremption (volatile SPINLOCK *psplock)
{
    PPCB ppcb = GetPCB ();
    if ((ppcb->dwKCRes || ppcb->bResched) && !InPrivilegeCall()) {
        ReleaseSpinLock (psplock);
        AcquireSpinLock (psplock);
    }
}

