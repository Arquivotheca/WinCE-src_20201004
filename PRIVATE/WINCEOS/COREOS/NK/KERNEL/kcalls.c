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
//    kcalls.c - implementations of all KCalls
//
#include <windows.h>

#include <kernel.h>

#ifndef _PREFAST_
#pragma warning(disable: 4068) // Disable pragma warnings
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// IMPORTANT NOTE FOR CRITICAL SECTION:
//
//  Kernel and user mode CS are handled differently. The hCrit member of CS is a pointer when CS is created by
//  kernel (ONLY KERNEL, NOT INCLUDING OTHER DLL LOADED INTO KERNEL). For the others, hCrit member is a handle.
//
//  Therefore, the hCrit member SHOULD NEVER BE REFERENCED WITHIN CS HANDLING CODE. An PCRIT argument is passed
//  along all the CS functions to access the CRIT structure.
//
//




RunList_t   RunList;
PTHREAD     SleepList;

DWORD       dwPrevReschedTime;
DWORD       currmaxprio = MAX_CE_PRIORITY_LEVELS - 1;
DWORD       CurrTime;
PTHREAD     pOOMThread;
BOOL        bLastIdle;
DWORD g_PerfCounterHigh;

// get the lowest priority interrupt bit position
BYTE GetHighPos (DWORD);

__inline BOOL KC_IsThreadTerminated (PTHREAD pth)
{
    return GET_DYING(pth) && !GET_DEAD(pth)     // being terminated, but not yet exited
        && GET_USERBLOCK(pth)                   // a blocking call from user
        && !KC_IsThrdInPSL (pth);               // not in PSL
}

//------------------------------------------------------------------------------
// make a thread runnable
//------------------------------------------------------------------------------
VOID MakeRun (PTHREAD pth) 
{
    DWORD prio, prio2;
    PTHREAD pth2, pth3;
    if (!pth->bSuspendCnt) {
        SET_RUNSTATE(pth,RUNSTATE_RUNNABLE);
        CELOG_KCThreadRunnable(pth);

        prio = GET_CPRIO(pth);
        prio2 = prio/PRIORITY_LEVELS_HASHSCALE;
        if (!(pth2 = RunList.pRunnable) || (prio < GET_CPRIO(pth2))) {
            // either there is no runnable or we're the highest priority thread
            // make pth the head of runable and update the hash table
            pth->pPrevSleepRun = 0;
            if (pth->pNextSleepRun = pth2) {
                pth2->pPrevSleepRun = pth;
            }
            pth->pUpRun = pth->pDownRun = RunList.pHashThread[prio2] = pth;
            RunList.pRunnable = pth;

            // see if we need to reschedule
            if (!RunList.pth || (prio < GET_CPRIO(RunList.pth)))
                SetReschedule();

        } else {
            // there are higher priority thread than pth, find the right
            // place to enqueue it.

            if (!(pth2 = RunList.pHashThread[prio2])) {
                // find the preceeding non-zero hash table entry
                // bounded by PRIORITY_LEVELS_HASHSIZE
                RunList.pHashThread[prio2] = pth;
                while (!(pth2 = RunList.pHashThread[-- prio2]))
                    ;

            }
            
            if (prio < GET_CPRIO(pth2)) {
                // insert into runlist and replace the hash table entry
                DEBUGCHK (prio/PRIORITY_LEVELS_HASHSCALE == prio2);
                pth->pPrevSleepRun = pth2->pPrevSleepRun;
                pth->pNextSleepRun = pth2;
                pth->pUpRun = pth->pDownRun = RunList.pHashThread[prio2] = pth->pPrevSleepRun->pNextSleepRun = pth2->pPrevSleepRun = pth;

            } else {
                
                // find the appropriate place to insert pth
                // bounded by MAX_PRIORITY_HASHSCALE
                while ((pth3 = pth2->pNextSleepRun) && (prio >= GET_CPRIO(pth3)))
                    pth2 = pth3;
                DEBUGCHK (!pth3 || (prio < GET_CPRIO(pth3)));
                DEBUGCHK (GET_CPRIO (pth2) <= prio);
                if (prio == GET_CPRIO(pth2)) {
                    pth->pUpRun = pth2->pUpRun;
                    pth->pUpRun->pDownRun = pth2->pUpRun = pth;
                    pth->pDownRun = pth2;
                    pth->pPrevSleepRun = pth->pNextSleepRun = 0;
                } else {
                    // insert between pth2 and pth3
                    if (pth->pNextSleepRun = pth3) {
                        pth3->pPrevSleepRun = pth;
                    }
                    pth->pPrevSleepRun = pth2;
                    pth2->pNextSleepRun = pth->pUpRun = pth->pDownRun = pth;
                }
            }
        }
        
    } else {
        DEBUGCHK(!((pth->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
        SET_RUNSTATE(pth,RUNSTATE_BLOCKED);
    }
}


//------------------------------------------------------------------------------
// PrioEnqueue - add a proxy to the 2-d priority queue
//------------------------------------------------------------------------------
static void PrioEnqueue (PEVENT pevent, DWORD prio, PPROXY pProx)
{
    DWORD prio2;
    PPROXY pProx2, pProx3;
    prio2 = prio/PRIORITY_LEVELS_HASHSCALE;
    if (!pevent->pProxList) {
        DEBUGCHK(!pProx->pQPrev && !pProx->pQNext);
        pProx->pQUp = pProx->pQDown = pevent->pProxHash[prio2] = pevent->pProxList = pProx;
    } else if (prio < pevent->pProxList->prio) {
        DEBUGCHK(!pProx->pQPrev);
        pProx->pQUp = pProx->pQDown = pevent->pProxHash[prio2] = pevent->pProxList->pQPrev = pProx;
        pProx->pQNext = pevent->pProxList;
        pevent->pProxList = pProx;
    } else if (pProx2 = pevent->pProxHash[prio2]) {
        if (prio < pProx2->prio) {
            pProx->pQPrev = pProx2->pQPrev;
            pProx->pQNext = pProx2;
            pProx->pQUp = pProx->pQDown = pevent->pProxHash[prio2] = pProx->pQPrev->pQNext = pProx2->pQPrev = pProx;
        } else {
FinishEnqueue:
            // bounded by MAX_PRIORITY_HASHSCALE
            while ((pProx3 = pProx2->pQNext) && (prio > pProx3->prio))
                pProx2 = pProx3;
            if (prio == pProx2->prio) {
                pProx->pQUp = pProx2->pQUp;
                pProx->pQUp->pQDown = pProx2->pQUp = pProx;
                pProx->pQDown = pProx2;
                DEBUGCHK(!pProx->pQPrev && !pProx->pQNext);
            } else if (!pProx3) {
                DEBUGCHK(!pProx->pQNext);
                pProx->pQPrev = pProx2;
                pProx->pQUp = pProx->pQDown = pProx2->pQNext = pProx;
            } else {
                 if (prio == pProx3->prio) {
                    pProx->pQUp = pProx3->pQUp;
                    pProx->pQUp->pQDown = pProx3->pQUp = pProx;
                    pProx->pQDown = pProx3;
                    DEBUGCHK(!pProx->pQPrev && !pProx->pQNext);
                 } else {
                    pProx->pQUp = pProx->pQDown = pProx;
                    pProx->pQPrev = pProx2;
                    pProx2->pQNext = pProx3->pQPrev = pProx;
                    pProx->pQNext = pProx3;
                 }
            }
        }
    } else {
        pevent->pProxHash[prio2] = pProx;
        // bounded by PRIORITY_LEVELS_HASHSIZE
        while (!(pProx2 = pevent->pProxHash[--prio2]))
            ;
        goto FinishEnqueue;
    }
}




//------------------------------------------------------------------------------
// FlatEnqueue - add a proxy to a flat queue
//------------------------------------------------------------------------------
static void FlatEnqueue (PTHREAD pth, PPROXY pProx) 
{
    PPROXY pProx2;
    if (!(pProx2 = pth->pProxList)) {
        pProx->pQUp = pProx->pQDown = pProx;
        pProx->pQPrev = (PPROXY)pth;
        pth->pProxList = pProx;
    } else {
        DEBUGCHK(!pProx->pQPrev);
        pProx->pQUp = pProx2->pQUp;
        pProx->pQDown = pProx2;
        pProx2->pQUp = pProx->pQUp->pQDown = pProx;
    }
}




//------------------------------------------------------------------------------
// remove a thread from Run Queue
//------------------------------------------------------------------------------
static void RunqDequeue (PTHREAD pth, DWORD cprio)
{
    PTHREAD pDown, pNext;
    DWORD prio = cprio/PRIORITY_LEVELS_HASHSCALE;

    DEBUGCHK (!GET_SLEEPING (pth));
    DEBUGCHK (!pth->pUpSleep);
    // check if there is a hanging tail of the thread...
    if (pDown = pth->pDownSleep) {
        DEBUGCHK (!GET_NEEDSLEEP(pDown) && GET_SLEEPING (pDown));
#ifdef DEBUG
        if (cprio > GET_CPRIO (pDown)) {
            DEBUGMSG (1, (L"pth->bprio = %d, pth->cprio = %d, pDown->bprio = %d, pDown->cprio = %d\r\n",
                pth->bBPrio, cprio, pDown->bBPrio, GET_CPRIO(pDown)));
            if (pth->pOwnedList) {
                DEBUGMSG (1, (L"pth->pOwnedList->bListedPrio = %dlx\r\n", pth->pOwnedList->bListedPrio));
            }
            if (pDown->pOwnedList) {
                DEBUGMSG (1, (L"pDown->pOwnedList->bListedPrio = %dlx\r\n", pDown->pOwnedList->bListedPrio));
            }
            DEBUGMSG (1, (L"pth->wInfo = %8.8lx, pDown->wInfo = %8.8lx\r\n", pth->wInfo, pDown->wInfo));
            DEBUGCHK (0);
        }
#endif
        DEBUGCHK (!pDown->bSuspendCnt);
        pDown->wCount ++;
        pDown->wCount2 ++;
        CLEAR_SLEEPING (pDown);
        pDown->pUpSleep = pth->pDownSleep = 0;
        // don't worry about it if NEEDRUN flag is set
        if (GET_RUNSTATE(pDown) != RUNSTATE_NEEDSRUN) {

            // setup proxy for cleanup if not pure sleeping 
            DEBUGCHK(GET_RUNSTATE(pDown) == RUNSTATE_BLOCKED);
            if (pDown->lpce) { // not set if purely sleeping
                pDown->lpce->base = pDown->lpProxy;
                pDown->lpce->size = (DWORD)pDown->lpPendProxy;
                pDown->lpProxy = 0;
            } else if (pDown->lpProxy) {
                // must be an interrupt event - special case it!
                PPROXY pprox = pDown->lpProxy;
                PEVENT lpe = (PEVENT)pprox->pObject;
                DEBUGCHK(pprox->bType == HT_MANUALEVENT);
                DEBUGCHK(pprox->pQDown == pprox);
                DEBUGCHK(pprox->pQPrev == (PPROXY)lpe);
                DEBUGCHK(pprox->dwRetVal == WAIT_OBJECT_0);
                lpe->pProxList = 0;
                pprox->pQDown = 0;
                pDown->lpProxy = 0;
            }

            // replace pth's slot if of same priority
            if (cprio == GET_CPRIO (pDown)) {
                if (pth == pth->pDownRun) {
                    // only node in this priority
                    pDown->pUpRun = pDown->pDownRun = pDown;
                } else {
                    // fixup the links
                    if (pDown->pUpRun = pth->pUpRun)
                        pDown->pUpRun->pDownRun = pDown;
                    if (pDown->pDownRun = pth->pDownRun)
                        pDown->pDownRun->pUpRun = pDown;
                }
                // fix up next node
                if (pDown->pNextSleepRun = pth->pNextSleepRun)
                    pDown->pNextSleepRun->pPrevSleepRun = pDown;

                // fix up prev node, update pRunnable if necessary
                if (pDown->pPrevSleepRun = pth->pPrevSleepRun) {
                    pDown->pPrevSleepRun->pNextSleepRun = pDown;
                } else if (RunList.pRunnable == pth) {
                    RunList.pRunnable = pDown;
                }
                // update hash table if necessary
                if (RunList.pHashThread[prio] == pth)
                    RunList.pHashThread[prio] = pDown;
                SET_RUNSTATE (pDown, RUNSTATE_RUNNABLE);
                return;
            }
            // not of the same priority, just call MakeRun
            // might want to save an instruction or two by
            // handling the logic here (don't have to check
            // suspend/pRunnable, etc.
            MakeRun (pDown);
        } 
    }

    // remove pth from the run queue
    pDown = pth->pDownRun;
    pNext = pth->pNextSleepRun;
    if (RunList.pHashThread[prio] == pth) {
        RunList.pHashThread[prio] = ((pDown != pth) ? pDown :
            (pNext && (GET_CPRIO(pNext)/PRIORITY_LEVELS_HASHSCALE == (WORD)prio)) ? pNext : 0);
    }
    if (pDown == pth) {
        if (!pth->pPrevSleepRun) {
            DEBUGCHK(RunList.pRunnable == pth);
            if (RunList.pRunnable = pNext)
                pNext->pPrevSleepRun = 0;
        } else {
            if (pth->pPrevSleepRun->pNextSleepRun = pNext)
                pNext->pPrevSleepRun = pth->pPrevSleepRun;
        }
    } else {
        pDown->pUpRun = pth->pUpRun;
        pth->pUpRun->pDownRun = pDown;
        if (pth->pPrevSleepRun) {
            pth->pPrevSleepRun->pNextSleepRun = pDown;
            pDown->pPrevSleepRun = pth->pPrevSleepRun;
            goto FinishDequeue;
        } else if (pth == RunList.pRunnable) {
            RunList.pRunnable = pDown;
            DEBUGCHK(!pDown->pPrevSleepRun);
FinishDequeue:            
            if (pNext) {
                pNext->pPrevSleepRun = pDown;
                pDown->pNextSleepRun = pNext;
            }
        }
    }
}



//------------------------------------------------------------------------------
// remove a thread from sleep list
//------------------------------------------------------------------------------
static void SleepqDequeue (PTHREAD pth) 
{
    PTHREAD pth2;
    DEBUGCHK(pth && GET_SLEEPING(pth));
    pth->wCount2++;

    if (pth2 = pth->pUpSleep) {
        DEBUGCHK (pth != SleepList);
        DEBUGCHK (!pth->pNextSleepRun && !pth->pPrevSleepRun);
        if (pth2->pDownSleep = pth->pDownSleep) {
            pth2->pDownSleep->pUpSleep = pth2;
            pth->pDownSleep = 0;
        }
        pth->pUpSleep = 0;
    } else if (pth2 = pth->pDownSleep) {
        DEBUGCHK (!pth2->pNextSleepRun && !pth2->pPrevSleepRun);
        if (pth2->pNextSleepRun = pth->pNextSleepRun) {
            pth2->pNextSleepRun->pPrevSleepRun = pth2;
        }
        if (pth2->pPrevSleepRun = pth->pPrevSleepRun) {
            pth2->pPrevSleepRun->pNextSleepRun = pth2;
        } else {
            DEBUGCHK (pth == SleepList);
            SleepList = pth2;
        }
        pth2->pUpSleep = pth->pDownSleep = 0;

    } else if (pth2 = pth->pPrevSleepRun) {
        if (pth2->pNextSleepRun = pth->pNextSleepRun) {
            pth2->pNextSleepRun->pPrevSleepRun = pth2;
        }
    } else {
        DEBUGCHK (pth == SleepList);
        // update SleepList
        if (SleepList = pth->pNextSleepRun) {
            SleepList->pPrevSleepRun = 0;
        }
    }

    CLEAR_SLEEPING(pth);
}


//------------------------------------------------------------------------------
// SleepOneMore
// Returns:
//        0    thread is put to sleep, or no need to sleep
//        1    continue search
//------------------------------------------------------------------------------
#pragma prefast(disable: 11, "pth cannot be NULL when wDirection is non-zero")
static int SleepOneMore (sleeper_t *pSleeper) 
{
    PTHREAD pth, pth2;
    int nDiff;

    if ((pth = pSleeper->pth) 
        && (pth->wCount2 != pSleeper->wCount2)) {
        // restart if pth changes its position in the sleepq, or removed from sleepq
        pSleeper->pth = 0;
        pSleeper->wDirection = 0;
        return 1;
    }

    if (pSleeper->wDirection) {
        // veritcal
        DEBUGCHK (pth && GET_SLEEPING(pth));
        DEBUGCHK (pSleeper->dwWakeupTime == pth->dwWakeupTime);
        DEBUGCHK((GET_RUNSTATE(pth) == RUNSTATE_BLOCKED) || (GET_RUNSTATE(pth) == RUNSTATE_NEEDSRUN));

        if (GET_CPRIO (pCurThread) < GET_CPRIO (pth)) {
            // rare case when we're priority inverted while traversing down, restart
            pSleeper->pth = 0;
            pSleeper->wDirection = 0;
            return 1;
        }

        if ((pth2 = pth->pDownSleep) && (GET_CPRIO (pCurThread) >= GET_CPRIO (pth2))) {
            // continue vertical search
            pSleeper->pth = pth2;
            pSleeper->wCount2 = pth2->wCount2;
            return 1;
        }
        // insert between pth and pth2 vertically
        pCurThread->pNextSleepRun = pCurThread->pPrevSleepRun = 0;
        pCurThread->pUpSleep = pth;
        pCurThread->pDownSleep = pth2;
        pth->pDownSleep = pCurThread;
        if (pth2) {
            pth2->pUpSleep = pCurThread;
        }

    } else {
        // horizontal
        pth2 = pth? pth->pNextSleepRun : SleepList;

        if (pCurThread->pNextSleepRun = pth2) {
            nDiff = pth2->dwWakeupTime - pSleeper->dwWakeupTime;
            if (0 > nDiff) {
                // continue search horizontally
                pSleeper->pth = pth2;
                pSleeper->wCount2 = pth2->wCount2;
                return 1;
            }
            if (0 == nDiff) {
                if (GET_CPRIO (pCurThread) >= GET_CPRIO (pth2)) {
                    // start vertical search
                    pSleeper->wDirection = 1;
                    pSleeper->pth = pth2;
                    pSleeper->wCount2 = pth2->wCount2;
                    return 1;
                }
                // replace pth2 as head of vertical list
                pth2->wCount2 ++;
                pCurThread->pDownSleep = pth2;
                pCurThread->pNextSleepRun = pth2->pNextSleepRun;
                pth2->pUpSleep = pCurThread;
                pth2->pNextSleepRun = pth2->pPrevSleepRun = 0;
                if (pth2 = pCurThread->pNextSleepRun)
                    pth2->pPrevSleepRun = pCurThread;
            } else {
                // insert between pth and pth2
                pth2->pPrevSleepRun = pCurThread;
            }
        }

        // common part
        if (pCurThread->pPrevSleepRun = pth) {
            DEBUGCHK((GET_RUNSTATE(pth) == RUNSTATE_BLOCKED) || (GET_RUNSTATE(pth) == RUNSTATE_NEEDSRUN));
            pth->pNextSleepRun = pCurThread;
        } else {

            // update reschedule time if need to wake up earlier
            if ((int) (dwReschedTime - pSleeper->dwWakeupTime) > 0) {
                // if the platform support variable tick scheduling, tell OAL to 
                // update the next interrupt tick
                dwReschedTime = pSleeper->dwWakeupTime;
                if (g_pOemGlobal->pfnUpdateReschedTime) {
                    g_pOemGlobal->pfnUpdateReschedTime (dwReschedTime);
                }
            }
            // update SleepList
            SleepList = pCurThread;

        }
    }

    pCurThread->dwWakeupTime = pSleeper->dwWakeupTime;
    pSleeper->pth = 0;
    pSleeper->wDirection = 0;
    RunList.pth = 0;
    SetReschedule();

    DEBUGCHK(!((pCurThread->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
    SET_RUNSTATE(pCurThread,RUNSTATE_BLOCKED);
    SET_SLEEPING(pCurThread);
    return 0;

}
#pragma prefast(pop)


//------------------------------------------------------------------------------
// DoLinkCritMut - Worker function to add a CS/Mutex to the 'owned object list' a thread
//------------------------------------------------------------------------------
static void DoLinkCritMut (PCRIT lpcrit, PTHREAD pth, BYTE prio) 
{
    PCRIT lpcrit2, lpcrit3;
    BYTE prio2;
    prio2 = prio/PRIORITY_LEVELS_HASHSCALE;
    DEBUGCHK(prio2 < PRIORITY_LEVELS_HASHSIZE);
    DEBUGCHK(IsKernelVa (lpcrit));
    if (!pth->pOwnedList || (prio < pth->pOwnedList->bListedPrio)) {
        // either pth doesn't own any critical section or lpcrit has higher priority
        // than the ones in its 'owned list'. Make lpcrit the head of its 'owned list' 
        // and update its hash table
        lpcrit->pPrevOwned = 0;
        if (lpcrit->pNextOwned = pth->pOwnedList) {
            lpcrit->pNextOwned->pPrevOwned = lpcrit;
        }
        lpcrit->pUpOwned = lpcrit->pDownOwned = pth->pOwnedHash[prio2] = pth->pOwnedList = lpcrit;
    } else {
        // pth owns a CS that has a higer priority thread blocked on it, find the right
        // place to enqueue lpcrit.
        if (!(lpcrit2 = pth->pOwnedHash[prio2])) {
            // find the preceeding non-zero hash table entry
            // bounded by PRIORITY_LEVELS_HASHSIZE
            pth->pOwnedHash[prio2] = lpcrit;
            while (!(lpcrit2 = pth->pOwnedHash[--prio2]))
                ;
        }
            
        if (prio < lpcrit2->bListedPrio) {
            // insert in front of lpcrit2 and replace the hash table entry
            DEBUGCHK (prio/PRIORITY_LEVELS_HASHSCALE == prio2);
            lpcrit->pPrevOwned = lpcrit2->pPrevOwned;
            lpcrit->pNextOwned = lpcrit2;
            lpcrit->pUpOwned = lpcrit->pDownOwned = pth->pOwnedHash[prio2] = lpcrit->pPrevOwned->pNextOwned = lpcrit2->pPrevOwned = lpcrit;
            
        } else {    

            // find the appropriate place to insert pth
            // bounded by MAX_PRIORITY_HASHSCALE
            while ((lpcrit3 = lpcrit2->pNextOwned) && (prio >= lpcrit3->bListedPrio))
                lpcrit2 = lpcrit3;

            DEBUGCHK (!lpcrit3 || (prio < lpcrit3->bListedPrio));
            DEBUGCHK (lpcrit2->bListedPrio <= prio);

            if (prio == lpcrit2->bListedPrio) {
                // insert at the end of the veritcal list of lpcrit2
                lpcrit->pUpOwned = lpcrit2->pUpOwned;
                lpcrit->pUpOwned->pDownOwned = lpcrit2->pUpOwned = lpcrit;
                lpcrit->pDownOwned = lpcrit2;
                lpcrit->pPrevOwned = lpcrit->pNextOwned = 0;
            } else {
                // insert between lpcrit2 and lpcrit3
                if (lpcrit->pNextOwned = lpcrit3)
                    lpcrit3->pPrevOwned = lpcrit;
                lpcrit->pPrevOwned = lpcrit2;
                lpcrit->pUpOwned = lpcrit->pDownOwned = lpcrit2->pNextOwned = lpcrit;
            }
        }
    }
    lpcrit->bListed = 1;
}

//------------------------------------------------------------------------------
// LinkCritMut - add a CS/Mutex to the 'owned object list' a thread
//------------------------------------------------------------------------------
static void LinkCritMut (PCRIT lpcrit, PTHREAD pth, BOOL bIsCrit) 
{ 
    BYTE prio;
    DEBUGCHK(IsKernelVa (lpcrit));
    DEBUGCHK(lpcrit->bListed != 1);
    if (!bIsCrit || (lpcrit->lpcs->needtrap && (!GET_BURIED(pth) || IsKernelVa (lpcrit->lpcs)))) {
        prio = lpcrit->bListedPrio = (lpcrit->pProxList ? lpcrit->pProxList->prio : MAX_CE_PRIORITY_LEVELS-1);
        DoLinkCritMut(lpcrit,pth,prio);
    }
}


//------------------------------------------------------------------------------
// UnlinkCritMut - remove a CS/Mutex from the 'owned object list' a thread
//------------------------------------------------------------------------------
static void UnlinkCritMut (PCRIT lpcrit, PTHREAD pth) 
{ 
    PCRIT pDown, pNext;
    WORD prio;
    if (lpcrit->bListed == 1) {
        prio = lpcrit->bListedPrio/PRIORITY_LEVELS_HASHSCALE;
        DEBUGCHK(prio < PRIORITY_LEVELS_HASHSIZE);
        pDown = lpcrit->pDownOwned;
        pNext = lpcrit->pNextOwned;
        if (pth->pOwnedHash[prio] == lpcrit) {
            pth->pOwnedHash[prio] = ((pDown != lpcrit) ? pDown :
                (pNext && (pNext->bListedPrio/PRIORITY_LEVELS_HASHSCALE == prio)) ? pNext : 0);
        }
        if (pDown == lpcrit) {
            if (!lpcrit->pPrevOwned) {
                DEBUGCHK(pth->pOwnedList == lpcrit);
                if (pth->pOwnedList = pNext)
                    pNext->pPrevOwned = 0;
            } else {
                if (lpcrit->pPrevOwned->pNextOwned = pNext)
                    pNext->pPrevOwned = lpcrit->pPrevOwned;
            }
        } else {
            pDown->pUpOwned = lpcrit->pUpOwned;
            lpcrit->pUpOwned->pDownOwned = pDown;
            if (lpcrit->pPrevOwned) {
                lpcrit->pPrevOwned->pNextOwned = pDown;
                pDown->pPrevOwned = lpcrit->pPrevOwned;
                goto FinishDequeue;
            } else if (lpcrit == pth->pOwnedList) {
                pth->pOwnedList = pDown;
                DEBUGCHK(!pDown->pPrevOwned);
    FinishDequeue:            
                if (pNext) {
                    pNext->pPrevOwned = pDown;
                    pDown->pNextOwned = pNext;
                }
            }
        }
        lpcrit->bListed = 0;
    }
}

//------------------------------------------------------------------------------
// ReprioCritMut - worker function to re-prio a CS/Mutex in a thread's 'owned object list'
//------------------------------------------------------------------------------
static void ReprioCritMut (PCRIT lpcrit, PTHREAD pth)
{
    BYTE prio;
    if (lpcrit->bListed == 1) {
        UnlinkCritMut(lpcrit,pth);
        prio = lpcrit->bListedPrio = (lpcrit->pProxList ? lpcrit->pProxList->prio : MAX_CE_PRIORITY_LEVELS-1);
        DoLinkCritMut(lpcrit,pth,prio);
    }
}

//------------------------------------------------------------------------------
// BoostCPrio - Boost the current priority of a thread
//------------------------------------------------------------------------------
static void BoostCPrio (PTHREAD pth, DWORD prio)
{
    DWORD oldcprio;
    oldcprio = GET_CPRIO(pth);
    SET_CPRIO(pth,prio);
    if (GET_RUNSTATE(pth) == RUNSTATE_RUNNABLE) {
        RunqDequeue (pth, oldcprio);
        MakeRun(pth);
    } else if (GET_SLEEPING (pth) && pth->pUpSleep && (GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN)) {
        SleepqDequeue (pth);
        SET_NEEDSLEEP (pth);
        if (pth->lpce || pth->lpProxy) {
            pth->bWaitState = WAITSTATE_PROCESSING; // re-process the wait
        }
        MakeRun (pth);
    }
}


//------------------------------------------------------------------------------
// WakeOneThreadInterruptDelayed - wake up the thread blocked on an interrupt event
//------------------------------------------------------------------------------
static PTHREAD WakeOneThreadInterruptDelayed (PEVENT lpe) 
{
    PTHREAD pth, pRet = NULL;
    PPROXY pprox;

    pprox = lpe->pProxList;
    DEBUGCHK(pprox->pObject == (LPBYTE)lpe);
    DEBUGCHK(pprox->bType == HT_MANUALEVENT);
    DEBUGCHK(pprox->pQDown == pprox);
    DEBUGCHK(pprox->pQPrev == (PPROXY)lpe);
    DEBUGCHK(pprox->dwRetVal == WAIT_OBJECT_0);
    lpe->pProxList = 0;
    pth = pprox->pTh;
    DEBUGCHK(!pth->lpce);
    pth->wCount++;
    DEBUGCHK(pth->lpProxy == pprox);
    pth->lpProxy = 0;
    if (pth->bWaitState == WAITSTATE_BLOCKED) {
        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
        pth->retValue = WAIT_OBJECT_0;
        pRet = pth;
        SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
    } else {
        DEBUGCHK(!GET_SLEEPING(pth));
        DEBUGCHK(pth->bWaitState == WAITSTATE_PROCESSING);
        pth->dwPendReturn = WAIT_OBJECT_0;
        pth->bWaitState = WAITSTATE_SIGNALLED;
    }
    return pRet;
}





//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
//
// KCalls exposed to external
//
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// ThreadSuspend: suspend a thread. If fLateSuspend is true, we'll mark the thread
//                structure and the thread would suspend itself when exiting PSL.
//------------------------------------------------------------------------------
DWORD ThreadSuspend (PTHREAD pth, BOOL fLateSuspend) 
{
    DWORD retval = 0xfffffffd;
    KCALLPROFON(5);

    // validate parameter 
    if (!KC_IsValidThread (pth)) {
        retval = 0xfffffffe;

    // okay to suspend current running thread as long as it's not in the middle of dying
    } else if (pth == pCurThread) {
    
        if (GET_DYING(pCurThread) && !GET_DEAD(pCurThread)) {
            retval = 0xfffffffe;
        }

    // always okay to suspend on the spot if suspend count is non-zero.
    } else if (!pth->bSuspendCnt) {

        // Only kernel would ever call ThreadSuspend with fLateSuspend == False. Currently
        // the only case is for stack scavanging, where we know it'll resume the thread quickly.
        // We'll suspend the thread as long as it's not inside kernel or it's blocked.
        if (!fLateSuspend) {

            if ((RUNSTATE_BLOCKED != GET_RUNSTATE(pth)) && (GetThreadIP (pth) & 0x80000000)) {
                retval = 0xfffffffe;
            }

        // fLateSuspend == TRUE, check where we are, late suspend if in PSL
        } else if ((GetThreadIP (pth) & 0x80000000) || KC_IsThrdInPSL (pth)) {
        
            if (pth->bPendSusp != MAX_SUSPEND_COUNT) {
                retval = pth->bPendSusp;
                pth->bPendSusp ++;
            } else {
                retval = 0xffffffff;
            }
        }
    }

    // okay to suspend on the spot?
    if (0xfffffffd == retval) {

        retval = pth->bSuspendCnt;
        if (pth->bSuspendCnt) {
            if (pth->bSuspendCnt != MAX_SUSPEND_COUNT)
                pth->bSuspendCnt++;
            else {
                retval = 0xffffffff;
            }
        } else {
            pth->bSuspendCnt = 1;
            switch (GET_RUNSTATE(pth)) {
                case RUNSTATE_RUNNING:
                    DEBUGCHK(RunList.pth == pth);
                    RunList.pth = 0;
                    SetReschedule();
                    break;
                case RUNSTATE_RUNNABLE:
                    RunqDequeue(pth, GET_CPRIO(pth));
                    break;
//                case RUNSTATE_NEEDSRUN:
                default:
                    if (GET_SLEEPING(pth)) {
                        SleepqDequeue(pth);
                        if (GET_RUNSTATE(pth) == RUNSTATE_NEEDSRUN) {
                            pth->wCount ++;
                        } else {
                            //DEBUGCHK (0);   // we should always be late suspended if we're sleeping
                            SET_NEEDSLEEP (pth);
                            if (pth->lpce) {
                                pth->bWaitState = WAITSTATE_PROCESSING; // re-process the wait
                            }
                        }
                    }
                    break;
            }
//            DEBUGCHK(!((pth->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
            SET_RUNSTATE(pth,RUNSTATE_BLOCKED);
        }
    }
    
    if ((int)retval >= 0) {
        CELOG_KCThreadSuspend(pth);
    }
    
    KCALLPROFOFF(5);
    return retval;
}


void SuspendSelfIfNeeded (void)
{
    KCALLPROFON (64);
    DEBUGCHK (!pCurThread->bSuspendCnt);

    if (pCurThread->bPendSusp) {
        pCurThread->bSuspendCnt = pCurThread->bPendSusp;
        pCurThread->bPendSusp = 0;
        SET_RUNSTATE (pCurThread, RUNSTATE_BLOCKED);
        SET_USERBLOCK (pCurThread);     // okay to terminate the thread
        RunList.pth = 0;
        SetReschedule ();
    }
    KCALLPROFOFF (64);
}



//------------------------------------------------------------------------------
// resume a thread
//------------------------------------------------------------------------------
DWORD ThreadResume (PTHREAD pth) 
{
    DWORD retval = 0;
    KCALLPROFON(47);
    if (KC_IsValidThread (pth)) {
        if (retval = pth->bSuspendCnt) {
            if (!--pth->bSuspendCnt && (GET_RUNSTATE(pth) == RUNSTATE_BLOCKED) && (!pth->lpProxy || GET_NEEDSLEEP(pth))) {
                DEBUGCHK (!GET_SLEEPING(pth));
                MakeRun(pth);
            }
        } else if (retval = pth->bPendSusp) {
            // if the suspend is pending, just decrement the count and return
            pth->bPendSusp --;
        }
        if (retval) {
            CELOG_KCThreadResume(pth);
        }
    }
    KCALLPROFOFF(47);
    return retval;
}



//------------------------------------------------------------------------------
// current thread yield
//------------------------------------------------------------------------------
void ThreadYield (void) 
{
    KCALLPROFON(35);
    DEBUGCHK (GET_RUNSTATE(pCurThread) == RUNSTATE_RUNNING);
    DEBUGCHK (RunList.pth == pCurThread);
    RunList.pth = 0;
    SetReschedule();
    MakeRun (pCurThread);
    KCALLPROFOFF(35);
}

//------------------------------------------------------------------------------
// change base priority of a thread
//------------------------------------------------------------------------------
BOOL SetThreadBasePrio (PTHREAD pth, DWORD nPriority)
{
    BOOL bRet;
    DWORD oldb, oldc, prio;
    KCALLPROFON(28);
    bRet = FALSE;

    if (KC_IsValidThread (pth)) {
        oldb = GET_BPRIO(pth);
        if (oldb != nPriority) {
            oldc = GET_CPRIO(pth);
            SET_BPRIO(pth,(WORD)nPriority);

            CELOG_KCThreadSetPriority(pth, nPriority);

            // update process priority watermark
            if (nPriority < pth->pprcOwner->bPrio)
                pth->pprcOwner->bPrio = (BYTE) nPriority;
                
            if (pCurThread == pth) {
                // thread is running, can't be in sleep queue.
                if (pCurThread->pOwnedList && ((prio = pCurThread->pOwnedList->bListedPrio) < nPriority))
                    nPriority = prio;
                if (nPriority != GET_CPRIO(pCurThread)) {
                    SET_CPRIO(pCurThread,nPriority);
                    if (RunList.pRunnable && (nPriority > GET_CPRIO(RunList.pRunnable)))
                        SetReschedule();
                }
            } else {
                if (nPriority < oldc) {
                    SET_CPRIO(pth,nPriority);
                    if (GET_RUNSTATE(pth) == RUNSTATE_RUNNABLE) {
                        RunqDequeue(pth,oldc);
                        MakeRun(pth);
                    } else if (GET_SLEEPING (pth) && pth->pUpSleep && (GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN)) {
                        SleepqDequeue (pth);
                        SET_NEEDSLEEP (pth);
                        if (pth->lpce) {
                            pth->bWaitState = WAITSTATE_PROCESSING; // re-process the wait
                        }
                        MakeRun (pth);
                    }
                }
            }
        }
        bRet = TRUE;
    }

    KCALLPROFOFF(28);
    return bRet;
}


//------------------------------------------------------------------------------
// terminate a thread
//------------------------------------------------------------------------------
PTHREAD SetThreadToDie (PTHREAD pth, DWORD dwExitCode, sttd_t *psttd) 
{
    PTHREAD pthNext = NULL;
    PPROXY pprox;

    KCALLPROFON(17);

    // terminating a kernel thread is not allowed.
    if (KC_IsValidThread (pth) && (pth->pprcOwner != g_pprcNK)) {
        pthNext = (PTHREAD) pth->thLink.pFwd;
        if (!GET_DYING(pth) && !GET_DEAD(pth)) {
            SET_DYING(pth);
            pth->tlsSecure[TLSSLOT_KERNEL] |= TLSKERN_TRYINGTODIE;
            pth->dwExitCode = dwExitCode;
            psttd->pThNeedBoost = pth;
            pth->bPendSusp = 0;     // clear pending suspend bit, no longer need to suspend since we're dying
            
            switch (GET_RUNSTATE(pth)) {
            case RUNSTATE_RUNNABLE:
                if (!KC_IsThrdInPSL (pth) && !(GetThreadIP(pth) & 0x80000000)) {
                    DEBUGCHK (pth->bWaitState != WAITSTATE_PROCESSING);
                    if (!pth->pcstkTop) {
                        RunqDequeue(pth,GET_CPRIO(pth));
                        SetThreadIP(pth, pExitThread);
#ifdef THUMBSUPPORT
                        if ((DWORD)pExitThread & 1)
                            pth->ctx.Psr |= THUMB_STATE;
                        else
                            pth->ctx.Psr &= ~THUMB_STATE;
#endif
                        SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
                        psttd->pThNeedRun = pth;
                    }
                }
                break;
                
            case RUNSTATE_BLOCKED:
                if (!KC_IsThrdInPSL (pth) && 
                    (GET_USERBLOCK(pth) || !(GetThreadIP(pth) & 0x80000000))) {
                    if (!GET_USERBLOCK(pth)) {
                        SetThreadIP(pth, pExitThread);
#ifdef THUMBSUPPORT
                        if ((DWORD)pExitThread & 1)
                            pth->ctx.Psr |= THUMB_STATE;
                        else
                            pth->ctx.Psr &= ~THUMB_STATE;
#endif
                    } else if (pprox = pth->lpProxy) {
                        if (pth->lpce) {
                            pth->lpce->base = pprox;
                            pth->lpce->size = (DWORD)pth->lpPendProxy;
                        } else {
                            // must be an interrupt event - special case it!
                            DEBUGCHK(pprox->bType == HT_MANUALEVENT);
                            DEBUGCHK(((PEVENT)pprox->pObject)->pProxList == pprox);
                            ((PEVENT)pprox->pObject)->pProxList = 0;
                            pprox->pQDown = 0;
                        }
                        pth->lpProxy = 0;
                        pth->bWaitState = WAITSTATE_SIGNALLED;
                    }
                    pth->bSuspendCnt = 0;
                    pth->wCount++;
                    SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
                    psttd->pThNeedRun = pth;
                }
                break;
            case RUNSTATE_RUNNING:
            case RUNSTATE_NEEDSRUN:
                // can't kill on the spot
                break;
            default:
                DEBUGCHK(0);
                break;
            }
        }
    }
    KCALLPROFOFF(17);
    return pthNext;
}


//------------------------------------------------------------------------------
// change thread quantum
//------------------------------------------------------------------------------
BOOL SetThreadQuantum (PTHREAD pth, DWORD dwQuantum)
{
    BOOL fRet;
    KCALLPROFON(17);
    if (fRet = KC_IsValidThread (pth)) {
        pth->dwQuantum = dwQuantum;
    }
    KCALLPROFON(17);
    return fRet;
}


//------------------------------------------------------------------------------
// get kernel time of current running thread
//------------------------------------------------------------------------------
DWORD GetCurThreadKTime (void) 
{
    DWORD retval;
    KCALLPROFON(48);
    retval = pCurThread->dwKTime;
    if (GET_TIMEMODE(pCurThread))
        retval += (GETCURRTICK () - CurrTime);
    KCALLPROFOFF(48);
    return retval;
}


//------------------------------------------------------------------------------
// get user time of current running thread
//------------------------------------------------------------------------------
DWORD GetCurThreadUTime (void) 
{
    DWORD retval;
    KCALLPROFON(65);
    retval = pCurThread->dwUTime;
    if (!GET_TIMEMODE(pCurThread))
        retval += (GETCURRTICK () - CurrTime);
    KCALLPROFOFF(65);
    return retval;
}

//------------------------------------------------------------------------------
// GetOwnedSyncObject - called on thread exit to get/release owned sync objects
//------------------------------------------------------------------------------
PCRIT GetOwnedSyncObject (void) 
{
    KCALLPROFON(67);
    if (pCurThread->pOwnedList)
        return pCurThread->pOwnedList;
    SET_BURIED(pCurThread);
    KCALLPROFOFF(67);
    return 0;
}


//------------------------------------------------------------------------------
// NextThread - part I of scheduler. Set interrupt event on interrupt, 
//              and make sleeping thread runnable when sleep time expired.
//------------------------------------------------------------------------------
void NextThread (void) 
{
    PTHREAD pth;
    PEVENT lpe;
    PPROXY pprox;
    DWORD pend1, pend2, intr;
    KCALLPROFON(10);
    /* Handle interrupts */
    do {

        pend1 = InterlockedExchange (&PendEvents1, 0);
        pend2 = InterlockedExchange (&PendEvents2, 0);

        while (pend1 || pend2) {
            if (pend1) {
                intr = GetHighPos(pend1);
                pend1 &= ~(1<<intr);
            } else {
                intr = GetHighPos(pend2);
                pend2 &= ~(1<<intr);
                intr += 32;
            }
            if (lpe = IntrEvents[intr]) {
                if (!lpe->pProxList)
                    lpe->state = 1;
                else {
                    DEBUGCHK(!lpe->state);
                    pprox = lpe->pProxList;
                    DEBUGCHK(pprox->pObject == (LPBYTE)lpe);
                    DEBUGCHK(pprox->bType == HT_MANUALEVENT);
                    DEBUGCHK(pprox->pQDown == pprox);
                    DEBUGCHK(pprox->pQPrev == (PPROXY)lpe);
                    DEBUGCHK(pprox->dwRetVal == WAIT_OBJECT_0);
                    lpe->pProxList = 0;
                    pth = pprox->pTh;
                    DEBUGCHK(!pth->lpce);
                    pth->wCount++;
                    DEBUGCHK(pth->lpProxy == pprox);
                    DEBUGCHK(lpe->phdIntr);
                    pth->lpProxy = 0;
                    if (pth->bWaitState == WAITSTATE_BLOCKED) {
                        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
                        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
                        DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
                        pth->retValue = WAIT_OBJECT_0;
                        if (GET_SLEEPING(pth)) {
                            SleepqDequeue(pth);
                        }
                        MakeRun(pth);
                    } else {
                        DEBUGCHK(!GET_SLEEPING(pth));
                        DEBUGCHK(pth->bWaitState == WAITSTATE_PROCESSING);
                        pth->dwPendReturn = WAIT_OBJECT_0;
                        pth->bWaitState = WAITSTATE_SIGNALLED;
                    }
                }
            }
        }
    } while (PendEvents1 || PendEvents2);

///////////////////////////////////////////////////////////
// We can not return here or the real-time SleepQueue
// will not work as expected. i.e. if timer tick and 
// other interrupts come at the same time, we'll lose 
// the timer tick and the thread at the beginning of
// the SleepList will not be waked up until the next
// tick.

    /* Handle timeouts */
    if ((int) (CurMSec - dwReschedTime) >= 0) {
        SetReschedule ();
    }
    if ((pth = SleepList) && ((int) (CurMSec - pth->dwWakeupTime) >= 0)) {

        DEBUGCHK((int) (CurMSec - dwReschedTime) >= 0);
        DEBUGCHK(GET_SLEEPING(pth));
        // We only update CurMSec on timer interrupt or in/out of Idle.
        // Sinc the timer ISR should return SYSINTR_RESCHED in this case, and we 
        // always reschedule after Idle, there is no need to update dwReschedTime
        // It'll be updated in KCNextThread
        CLEAR_SLEEPING(pth);
        pth->wCount++;
        pth->wCount2 ++;

        if (SleepList = pth->pNextSleepRun) {
            SleepList->pPrevSleepRun = 0;
        }
        if (GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN) {
            DEBUGCHK(GET_RUNSTATE(pth) == RUNSTATE_BLOCKED);
            if (pth->lpce) { // not set if purely sleeping
                pth->lpce->base = pth->lpProxy;
                pth->lpce->size = (DWORD)pth->lpPendProxy;
                pth->lpProxy = 0;
            } else if (pth->lpProxy) {
                // must be an interrupt event - special case it!
                pprox = pth->lpProxy;
                lpe = (PEVENT)pprox->pObject;
                DEBUGCHK(pprox->bType == HT_MANUALEVENT);
                DEBUGCHK(pprox->pQDown == pprox);
                DEBUGCHK(pprox->pQPrev == (PPROXY)lpe);
                DEBUGCHK(pprox->dwRetVal == WAIT_OBJECT_0);
                lpe->pProxList = 0;
                pprox->pQDown = 0;
                pth->lpProxy = 0;
            }
            MakeRun(pth);
        } 
    }

    if (!KCResched && pCurThread) {
        g_pOemGlobal->pfnNotifyReschedule (dwCurThId, GET_CPRIO(pCurThread), pCurThread->dwQuantum, 0);
    }

    KCALLPROFOFF(10);
}

//------------------------------------------------------------------------------
// KCNextThread - part 2 of scheduler, find the highest prioity runnable thread
//------------------------------------------------------------------------------
void KCNextThread (void) 
{
    DWORD prio, prio2, NewTime, NewReschedTime;
    BOOL bQuantExpired;
    PTHREAD pth, pth2, pth3, pOwner;
    PCRIT pCrit;
    LPCRITICAL_SECTION lpcs;
    KCALLPROFON(44);
    NewTime = GETCURRTICK();

    if (pth = RunList.pth) {
        // calculate quantum left
        if (bQuantExpired = (0 != pth->dwQuantum)) {
            if ((int) (pth->dwQuantLeft -= NewTime - dwPrevReschedTime) <= 0) {
                pth->dwQuantLeft = pth->dwQuantum;
                CELOG_KCThreadQuantumExpire();
            } else {
                bQuantExpired = FALSE;
            }
        }

        // if there is any other runable thread, check to see if we need to context switch
        if (pth2 = RunList.pRunnable) {
            if (((prio = GET_CPRIO (pth)) == (prio2 = GET_CPRIO (pth2))) && bQuantExpired) {
                // Current thread quantum expired and next runnable thread have same priority, same run list.  
                // Use runnable thread pointer (pth2, first on list) to place current thread (pth) 
                // last on list.
                pth->pUpRun = pth2->pUpRun;
                pth->pUpRun->pDownRun = pth2->pUpRun = pth;
                pth->pDownRun = pth2;
                pth->pPrevSleepRun = pth->pNextSleepRun = 0;

                SET_RUNSTATE(pth,RUNSTATE_RUNNABLE);
                RunList.pth = 0;

            } else if (prio > prio2) {
                // there is a runable thread of higher priority, enqueue the current thread
                prio2 = prio/PRIORITY_LEVELS_HASHSCALE;
                if (!(pth2 = RunList.pHashThread[prio2])) {
                    // find the preceeding non-zero hash table entry
                    // bounded by PRIORITY_LEVELS_HASHSIZE
                    RunList.pHashThread[prio2] = pth;
                    while (!(pth2 = RunList.pHashThread[-- prio2]))
                        ;
                }
                
                if (prio < GET_CPRIO(pth2)) {
                    // insert into runlist and replace the hash table entry
                    DEBUGCHK (prio/PRIORITY_LEVELS_HASHSCALE == prio2);
                    pth->pPrevSleepRun = pth2->pPrevSleepRun;
                    pth->pNextSleepRun = pth2;
                    pth->pUpRun = pth->pDownRun = RunList.pHashThread[prio2] = pth->pPrevSleepRun->pNextSleepRun = pth2->pPrevSleepRun = pth;

                } else {
                    
                    // find the appropriate place to insert current thread
                    // bounded by MAX_PRIORITY_HASHSCALE
                    while ((pth3 = pth2->pNextSleepRun) && (prio >= GET_CPRIO(pth3)))
                        pth2 = pth3;
                    DEBUGCHK (!pth3 || (prio < GET_CPRIO(pth3)));
                    DEBUGCHK (GET_CPRIO (pth2) <= prio);
                    DEBUGCHK (pth);

                    if (prio == GET_CPRIO(pth2)) {
                        // Run list for current thread priority exists, make current thread first if time
                        // left in quantum or if quantum is 0 (run to completion).
                        if (bQuantExpired) {
                            pth->pPrevSleepRun = pth->pNextSleepRun = 0;
                        } else {
                            // become the head of the list
                            if (pth->pNextSleepRun = pth3)
                                pth3->pPrevSleepRun = pth;
                            if (pth->pPrevSleepRun = pth2->pPrevSleepRun)
                                pth2->pPrevSleepRun->pNextSleepRun = pth;
                            pth2->pNextSleepRun = pth2->pPrevSleepRun = 0;
                            // Update hash table entry if replacing first thread on list.
                            if (pth2 == RunList.pHashThread[prio2]) {
                                DEBUGCHK (prio/PRIORITY_LEVELS_HASHSCALE == prio2);
                                RunList.pHashThread[prio2] = pth;
                            }
                        }

                        // make pth "before" pth2
                        pth->pUpRun = pth2->pUpRun;
                        pth->pUpRun->pDownRun = pth2->pUpRun = pth;
                        pth->pDownRun = pth2;

                    } else {
                        // insert between pth2 and pth3
                        if (pth->pNextSleepRun = pth3) {
                            pth3->pPrevSleepRun = pth;
                        }
                        pth->pPrevSleepRun = pth2;
                        pth2->pNextSleepRun = pth->pUpRun = pth->pDownRun = pth;
                    }
                }
                SET_RUNSTATE(pth,RUNSTATE_RUNNABLE);
                RunList.pth = 0;
            }
        }
    } else if (!bLastIdle) {
        // pCurThread yeild, receive full quantum again
        pCurThread->dwQuantLeft = pCurThread->dwQuantum;
    }

    if (!RunList.pth && (pth = RunList.pRunnable)) {
        // context switch occurs -- check "max runable priority" (set by OOM)
        pOwner = 0;
        if ((prio = GET_CPRIO(pth)) > currmaxprio) {
            pOwner = pOOMThread;
            pth = 0;
            while (pOwner && !pth && !PendEvents1 && !PendEvents2) {
                switch (GET_RUNSTATE(pOwner)) {
                case RUNSTATE_NEEDSRUN:
                    if (GET_SLEEPING(pOwner)) {
                        SleepqDequeue(pOwner);
                        pOwner->wCount ++;
                    }
                    MakeRun(pOwner);
                    DEBUGCHK (GET_RUNSTATE (pOwner) == RUNSTATE_RUNNABLE);
                    // fall thru
                    __fallthrough;
                    
                case RUNSTATE_RUNNABLE:
                    pth = pOwner;
                    prio = GET_CPRIO(pOwner);
                    break;

                case RUNSTATE_BLOCKED:
                    if (!GET_SLEEPING(pOwner) && pOwner->lpProxy && pOwner->lpProxy->pQDown
                        && !pOwner->lpProxy->pThLinkNext && (pOwner->lpProxy->bType == HT_CRITSEC)) {
                        pCrit = (PCRIT)pOwner->lpProxy->pObject;
                        DEBUGCHK(pCrit);
                        lpcs = pCrit->lpcs;
                        DEBUGCHK(lpcs);
                        DEBUGCHK(lpcs->OwnerThread);
                        pOwner = KC_PthFromId (((DWORD)lpcs->OwnerThread & ~1));
                        DEBUGCHK(pOwner);
                        break;
                    }
                    pOwner = 0;
                    break;

                default:
                    DEBUGCHK (0);    // the thread can't be running!! something wrong
                }
            }
        }

        // check if there is a thread to run
        if (RunList.pth = pth) {
            SET_RUNSTATE(pth,RUNSTATE_RUNNING);
            RunqDequeue(pth,prio);
            pth->pLastCrit = 0;
            pth->lpCritProxy = 0;

            if (!GET_NOPRIOCALC(pth)) {
                // recalculate priority based on sync object pth owns
                prio = GET_BPRIO(pth);
                if (pth->pOwnedList && ((prio2 = pth->pOwnedList->bListedPrio) < prio))
                    prio = prio2;

                // make sure we don't reschedule forever in OOM condition
                if (pOwner && (prio > currmaxprio)) {
                    prio = currmaxprio;
                }

                if (prio > GET_CPRIO(pth)) {
                    SET_CPRIO(pth,prio);
                    CELOG_KCThreadPriorityInvert(pth);
                    if (RunList.pRunnable && (prio > GET_CPRIO(RunList.pRunnable)))
                        SetReschedule();
                }
            }
        }
    }

    randdw2 = ((randdw2<<5) | (randdw2>>27)) ^ ((NewTime>>5) & 0x1f);
    
    if (!bLastIdle) {
        if (GET_TIMEMODE(pCurThread) == TIMEMODE_USER)
            pCurThread->dwUTime += NewTime - dwPrevReschedTime;
        else
            pCurThread->dwKTime += NewTime - dwPrevReschedTime;

        // save the debug register for the thread being swap out.
        // NOTE: we use the pSwapStack field to save the debug registers
        //       since a thread will not use the swap stack unless it's dying,
        //       we don't have worry about the contents being destroyed.
        //       The cleanup thread doesn't have a swap stack but it makes
        //       perfect sense that OEM didn't see them.
        if (g_pOemGlobal->fSaveCoProcReg && pCurThread->pCoProcSaveArea) {
            DEBUGCHK (g_pOemGlobal->pfnSaveCoProcRegs);
            g_pOemGlobal->pfnSaveCoProcRegs (pCurThread->pCoProcSaveArea);
        }
    }

    // update perf counter high in case tick count wrapped around
    if (CurrTime > NewTime) {
        g_PerfCounterHigh++;
    }

    // update current time
    CurrTime = dwPrevReschedTime = NewTime;
    bLastIdle = !(pth = RunList.pth);
    NewReschedTime = SleepList? SleepList->dwWakeupTime : (NewTime + 0x7fffffff);

    if (!bLastIdle) {
        // update NewReschedTime
        if (pth->dwQuantum && ((int) ((NewTime += pth->dwQuantLeft) - NewReschedTime) < 0)) {
            NewReschedTime = NewTime;
        }

        CELOG_KCThreadSwitch(pth);
        g_pOemGlobal->pfnNotifyReschedule (pth->dwId, GET_CPRIO(pth), pth->dwQuantum, 0);

    } else {
        CELOG_KCThreadSwitch(NULL);
        g_pOemGlobal->pfnNotifyReschedule (0, 0, 0, 0);
    }

    // if the platform support variable tick scheduling, tell OAL to 
    // update the next interrupt tick
    if (dwReschedTime != NewReschedTime) {
        dwReschedTime = NewReschedTime;
        if (g_pOemGlobal->pfnUpdateReschedTime) {
            g_pOemGlobal->pfnUpdateReschedTime (NewReschedTime);
        }
    }

    
//#ifndef SHIP_BUILD
    if (pCurThread != pth) {
        KInfoTable[KINX_DEBUGGER]++;
    }
//#endif
#ifdef SH4
    if (pth) {
        if (g_CurFPUOwner != pth)
            pth->ctx.Psr |= 0x8000;
        else
            pth->ctx.Psr &= ~0x8000;
    }
#endif
#ifdef SH3
    if (pth) {
        if (g_CurDSPOwner != pth)
            pth->ctx.Psr &= ~0x1000;
        else
            pth->ctx.Psr |= 0x1000;
    }
#endif

    if (PendEvents1 || PendEvents2) {
        SetReschedule ();
        ReschedFlag = 1;
    }

    // DEBUGMSG (ZONE_SCHEDULE, (L"KCNextThread pth = %8.8lx\r\n", pth));
    KCALLPROFOFF(44);
}


//------------------------------------------------------------------------------
// FinalRemoveThread - final stage of thread/process cleanup.
//------------------------------------------------------------------------------
void FinalRemoveThread (PHDATA phdThrd)
{
    PTHREAD pth = GetThreadPtr (phdThrd);
    KCALLPROFON(36);

    DEBUGCHK (pth == pCurThread);
    DEBUGCHK (!pth->pOwnedList);
    DEBUGCHK (RunList.pth == pth);
    DEBUGCHK (pth->tlsPtr == pth->tlsSecure);
    DEBUGCHK (dwCurThId == pth->dwId);
    DEBUGCHK (!pth->lpce);
    DEBUGCHK (!pth->lpProxy);
    DEBUGCHK (!pth->pProxList);
    DEBUGCHK (!pth->pOwnedList);
    
    RunList.pth = 0;
    SetReschedule ();

    // set bLastIdle = TRUE so KCNextThread will not update the statistic of pCurThread
    bLastIdle = TRUE;

    // cache the thread's secure stack in kernel's stack list
    VMCacheStack (pth->pStkForExit, (DWORD) pth->tlsSecure & ~VM_BLOCK_OFST_MASK, KRN_STACK_SIZE);

    // the thread handle can no longer get to the thread
    phdThrd->pvObj  = NULL;

    // Add the HDATA to delay free list
    KC_AddDelayFreeThread (phdThrd);

    g_pOemGlobal->pfnNotifyThreadExit (pth->dwId, pth->dwExitCode);

    DEBUGMSG (ZONE_SCHEDULE, (L"Thread %8.8lx complete terminated\r\n", dwCurThId));

    KCALLPROFOFF(36);
    
}


//------------------------------------------------------------------------------
// LaterLinkCritMut - current thread gets the ownership of a CS/Mutex
//------------------------------------------------------------------------------
void LaterLinkCritMut (PCRIT lpcrit, BOOL bIsCrit) 
{ 
    BYTE prio;
    KCALLPROFON(50);
    if (!lpcrit->bListed && (!bIsCrit || (lpcrit->lpcs->needtrap && (!GET_BURIED(pCurThread) || IsKernelVa (lpcrit->lpcs))))) {
        prio = lpcrit->bListedPrio = (lpcrit->pProxList ? lpcrit->pProxList->prio : MAX_CE_PRIORITY_LEVELS-1);
        DoLinkCritMut (lpcrit, pCurThread, prio);
    }
    KCALLPROFOFF(50);
}



//------------------------------------------------------------------------------
// LaterLinkCritMut - a thread gets the ownership of a Mutex
//------------------------------------------------------------------------------
void LaterLinkMutOwner (PMUTEX lpm) 
{ 
    BYTE prio;
    KCALLPROFON(55);
    if (!lpm->bListed) {
        prio = lpm->bListedPrio = (lpm->pProxList ? lpm->pProxList->prio : MAX_CE_PRIORITY_LEVELS-1);
        DoLinkCritMut ((PCRIT)lpm, lpm->pOwner, prio);
    }
    KCALLPROFOFF(55);
}


//------------------------------------------------------------------------------
// PreUnlinkMutex - step 1 of releasing the ownership of a Mutex
//------------------------------------------------------------------------------
void PreUnlinkMutex (PCRIT lpcrit) 
{
    KCALLPROFON(51);
    UnlinkCritMut(lpcrit,pCurThread);
    SET_NOPRIOCALC(pCurThread);
    KCALLPROFOFF(51);
}



//------------------------------------------------------------------------------
// PostUnlinkCritMut - final step of releasing the ownership of a CS/Mutex
//------------------------------------------------------------------------------
void PostUnlinkCritMut (void) 
{
    WORD prio, prio2;
    KCALLPROFON(52);
    CLEAR_NOPRIOCALC(pCurThread);
    prio = GET_BPRIO(pCurThread);
    if (pCurThread->pOwnedList && (prio > (prio2 = pCurThread->pOwnedList->bListedPrio)))
        prio = prio2;
    if (prio != GET_CPRIO(pCurThread)) {
        SET_CPRIO(pCurThread,prio);
        CELOG_KCThreadPriorityInvert(pCurThread);
        if (RunList.pRunnable && (prio > GET_CPRIO(RunList.pRunnable)))
            SetReschedule();
    }
    KCALLPROFOFF(52);
}



//------------------------------------------------------------------------------
// MakeRunIfNeeded - make a thread runnable if needed
//------------------------------------------------------------------------------
void MakeRunIfNeeded (PTHREAD pth)
{
    KCALLPROFON(39);
    if (GET_RUNSTATE(pth) == RUNSTATE_NEEDSRUN) {
        if (GET_SLEEPING(pth)) {
            SleepqDequeue(pth);
            pth->wCount ++;
        }
        MakeRun(pth);
    }
    KCALLPROFOFF(39);
}

//------------------------------------------------------------------------------
// DequeueFlatProxy - remove a proxy from a flat queue (not a 2-d priority queue)
//------------------------------------------------------------------------------
void DequeueFlatProxy (PPROXY pprox) 
{
    PPROXY pDown;
    PEVENT pObject;
    KCALLPROFON(54);
    DEBUGCHK((pprox->bType == SH_CURTHREAD) || (pprox->bType == HT_MANUALEVENT));
    if (pDown = pprox->pQDown) { // not already dequeued
        if (pprox->pQPrev) { // we're first
            pObject = ((PEVENT)pprox->pQPrev);
            DEBUGCHK(pObject->pProxList == pprox);
            if (pDown == pprox) { // we're alone
                pObject->pProxList = 0;
            } else {
                pDown->pQUp = pprox->pQUp;
                pprox->pQUp->pQDown = pObject->pProxList = pDown;
                pDown->pQPrev = (PPROXY)pObject;
            }
        } else {
            pDown->pQUp = pprox->pQUp;
            pprox->pQUp->pQDown = pDown;
        }
        pprox->pQDown = 0;
    }
    KCALLPROFOFF(54);
}



//------------------------------------------------------------------------------
// DequeuePrioProxy - remove a proxy form a 2-d priority queue
//------------------------------------------------------------------------------
BOOL DequeuePrioProxy (PPROXY pprox) 
{
    PCRIT lpcrit;
    PPROXY pDown, pNext;
    WORD prio;
    BOOL bRet;
    KCALLPROFON(31);
    DEBUGCHK((pprox->bType == HT_EVENT) || (pprox->bType == HT_CRITSEC) || (pprox->bType == HT_MUTEX) || (pprox->bType == HT_SEMAPHORE));
    bRet = FALSE;
    if (pDown = pprox->pQDown) { // not already dequeued
        lpcrit = (PCRIT)pprox->pObject;
        prio = pprox->prio/PRIORITY_LEVELS_HASHSCALE;
        pDown = pprox->pQDown;
        pNext = pprox->pQNext;
        if (lpcrit->pProxHash[prio] == pprox) {
            lpcrit->pProxHash[prio] = ((pDown != pprox) ? pDown :
                (pNext && (pNext->prio/PRIORITY_LEVELS_HASHSCALE == prio)) ? pNext : 0);
        }
        if (pDown == pprox) {
            if (!pprox->pQPrev) {
                DEBUGCHK(lpcrit->pProxList == pprox);
                if (lpcrit->pProxList = pNext)
                    pNext->pQPrev = 0;
                bRet = TRUE;
            } else {
                if (pprox->pQPrev->pQNext = pNext)
                    pNext->pQPrev = pprox->pQPrev;
            }
        } else {
            pDown->pQUp = pprox->pQUp;
            pprox->pQUp->pQDown = pDown;
            if (pprox->pQPrev) {
                pprox->pQPrev->pQNext = pDown;
                pDown->pQPrev = pprox->pQPrev;
                goto FinishDequeue;
            } else if (pprox == lpcrit->pProxList) {
                lpcrit->pProxList = pDown;
                DEBUGCHK(!pDown->pQPrev);
FinishDequeue:            
                if (pNext) {
                    pNext->pQPrev = pDown;
                    pDown->pQNext = pNext;
                }
            }
        }
        pprox->pQDown = 0;
    }
    KCALLPROFOFF(31);
    return bRet;
}


//------------------------------------------------------------------------------
// DoReprioCrit - re-prio a CS in the owner thread's 'owned object list'
//------------------------------------------------------------------------------
void DoReprioCrit (PCRIT lpcrit)
{
    HANDLE hth;
    PTHREAD pth;
    KCALLPROFON(4);
    if ((hth = lpcrit->lpcs->OwnerThread) && (pth = KC_PthFromId (((DWORD)hth & ~1))))
        ReprioCritMut(lpcrit,pth);
    KCALLPROFOFF(4);
}



//------------------------------------------------------------------------------
// DoReprioMutex - re-prio a mutex in the owner thread's 'owned object list'
//------------------------------------------------------------------------------
void DoReprioMutex (PMUTEX lpm) 
{
    KCALLPROFON(29);
    if (lpm->pOwner)
        ReprioCritMut((PCRIT)lpm,lpm->pOwner);
    KCALLPROFOFF(29);
}


//------------------------------------------------------------------------------
// PostBoostMut - boost the priority of the owner of a mutex (current thread is
//                acquiring a mutex that is owned by another thread)
//------------------------------------------------------------------------------
void PostBoostMut (PMUTEX lpm)
{
    PTHREAD pth;
    WORD prio;
    KCALLPROFON(56);
    if (pCurThread->bWaitState == WAITSTATE_PROCESSING) {
        pth = lpm->pOwner;
        DEBUGCHK(pth);
        prio = GET_CPRIO (pCurThread);
        if (prio < GET_CPRIO (pth)) {
            BoostCPrio (pth,prio);
            CELOG_KCThreadPriorityInvert(pth);
        }
        if (!GET_NOPRIOCALC (pth))
            LaterLinkMutOwner(lpm);
        
        ReprioCritMut ((PCRIT)lpm, pth);
    }
    KCALLPROFOFF(56);
}



//------------------------------------------------------------------------------
// PostBoostCrit1 - part 1 of priority inheritance handling of CS.
//                  (re-prio the cs in the owner thread's 'owned object list'
//------------------------------------------------------------------------------
void PostBoostCrit1 (PCRIT pcrit)
{
    PTHREAD pth;
    KCALLPROFON(57);
    if (pCurThread->bWaitState == WAITSTATE_PROCESSING) {
        pth = KC_PthFromId (((DWORD)pcrit->lpcs->OwnerThread & ~1));
        DEBUGCHK(pth);
        ReprioCritMut (pcrit, pth);
    }
    KCALLPROFOFF(57);
}

//------------------------------------------------------------------------------
// PostBoostCrit12 - part 2 of priority inheritance handling of CS.
//                  (boost the priority of the owner thread)
//------------------------------------------------------------------------------
void PostBoostCrit2 (PCRIT pcrit)
{
    PTHREAD pth;
    BYTE prio;
    KCALLPROFON(60);
    if (pCurThread->bWaitState == WAITSTATE_PROCESSING) {
        pth = KC_PthFromId (((DWORD)pcrit->lpcs->OwnerThread & ~1));
        DEBUGCHK(pth);
        if (pcrit->pProxList && ((prio = pcrit->pProxList->prio) < GET_CPRIO(pth))) {
            BoostCPrio (pth,prio);
            CELOG_KCThreadPriorityInvert(pth);
        }
    }
    KCALLPROFOFF(60);
}

//------------------------------------------------------------------------------
// CritFinalBoost - final step of priority inheritance handling of CS.
//                  (boost the priority of the new owner)
//------------------------------------------------------------------------------
void CritFinalBoost (PCRIT pCrit, LPCRITICAL_SECTION lpcs)
{
    DWORD prio;
    KCALLPROFON(59);
    DEBUGCHK (lpcs->OwnerThread == (HANDLE)dwCurThId);
    DEBUGCHK (pCrit->lpcs == lpcs);
    if (!pCrit->bListed && pCrit->pProxList)
        LinkCritMut(pCrit,pCurThread,1);
    if (pCrit->pProxList && ((prio = pCrit->pProxList->prio) < GET_CPRIO(pCurThread))) {
        SET_CPRIO(pCurThread,prio);
        CELOG_KCThreadPriorityInvert(pCurThread);
    }
    KCALLPROFOFF(59);
}


//------------------------------------------------------------------------------
// WakeOneThreadFlat - wake up a thread that is blocked on thread/process/manual-event
//                      (object linked on a flat-queue)
//------------------------------------------------------------------------------
void WakeOneThreadFlat (PEVENT pObject, PTHREAD *ppTh) 
{
    PTHREAD pth;
    PPROXY pprox, pDown;
    KCALLPROFON(41);
    if (pprox = pObject->pProxList) {
        DEBUGCHK((pprox->bType == SH_CURTHREAD) || (pprox->bType == HT_MANUALEVENT));
        DEBUGCHK(pprox->pQPrev == (PPROXY)pObject);
        pDown = pprox->pQDown;
        DEBUGCHK(pDown);
        if (pDown == pprox) { // we're alone
            pObject->pProxList = 0;
        } else {
            pDown->pQUp = pprox->pQUp;
            pprox->pQUp->pQDown = pObject->pProxList = pDown;
            pDown->pQPrev = (PPROXY)pObject;
        }
        pprox->pQDown = 0;
        pth = pprox->pTh;
        DEBUGCHK(pth);
        if (pth->wCount == pprox->wCount) {
            DEBUGCHK(pth->lpce);
            pth->lpce->base = pth->lpProxy;
            pth->lpce->size = (DWORD)pth->lpPendProxy;
            pth->lpProxy = 0;
            pth->wCount++;
            if (pth->bWaitState == WAITSTATE_BLOCKED) {
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
                pth->retValue = pprox->dwRetVal;
                SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
                *ppTh = pth;
            } else {
                DEBUGCHK(!GET_SLEEPING(pth));
                DEBUGCHK(pth->bWaitState == WAITSTATE_PROCESSING);
                pth->dwPendReturn = pprox->dwRetVal;
                pth->bWaitState = WAITSTATE_SIGNALLED;
            }
        }
    }
    KCALLPROFOFF(41);
}



//------------------------------------------------------------------------------
// EventModMan - set/pulse a manual reset event
//------------------------------------------------------------------------------
DWORD EventModMan (PEVENT lpe, PSTUBEVENT lpse, DWORD action)
{
    DWORD prio;
    KCALLPROFON(15);
    prio = lpe->bMaxPrio;
    lpe->bMaxPrio = THREAD_RT_PRIORITY_IDLE;
    if (lpse->pProxList = lpe->pProxList) {
        lpse->pProxList->pQPrev = (PPROXY)lpse;
        DEBUGCHK(lpse->pProxList->pQDown);
        lpe->pProxList = 0;
    }
    lpe->state = (action == EVENT_SET);
    KCALLPROFOFF(15);
    return prio;
}

//------------------------------------------------------------------------------
// EventModAuto - set/pulse a auto-reset event
//------------------------------------------------------------------------------
BOOL EventModAuto (PEVENT lpe, DWORD action, PTHREAD *ppTh)
{
    BOOL bRet;
    PTHREAD pth;
    PPROXY pprox, pDown, pNext;
    BYTE prio;
    KCALLPROFON(16);
    bRet = 0;

    if (!(pprox = lpe->pProxList)) {
        lpe->state = (action == EVENT_SET);
    } else {
        pDown = pprox->pQDown;
        if (lpe->phdIntr) {
            // interrupt event, special case it.
            DEBUGCHK(pprox->pQPrev = (PPROXY)lpe);
            if (pDown == pprox) { // we're alone
                lpe->pProxList = 0;
            } else {
                pDown->pQUp = pprox->pQUp;
                pprox->pQUp->pQDown = lpe->pProxList = pDown;
                pDown->pQPrev = (PPROXY)lpe;
            }
        } else {
            prio = pprox->prio/PRIORITY_LEVELS_HASHSCALE;
            pNext = pprox->pQNext;
            if (lpe->pProxHash[prio] == pprox) {
                lpe->pProxHash[prio] = ((pDown != pprox) ? pDown :
                    (pNext && (pNext->prio/PRIORITY_LEVELS_HASHSCALE == prio)) ? pNext : 0);
            }
            if (pDown == pprox) {
                if (lpe->pProxList = pNext)
                    pNext->pQPrev = 0;
            } else {
                pDown->pQUp = pprox->pQUp;
                pprox->pQUp->pQDown = pDown;
                lpe->pProxList = pDown;
                DEBUGCHK(!pDown->pQPrev);
                if (pNext) {
                    pNext->pQPrev = pDown;
                    pDown->pQNext = pNext;
                }
            }
        }
        pprox->pQDown = 0;
        pth = pprox->pTh;
        DEBUGCHK(pth);
        if (pth->wCount != pprox->wCount)
            bRet = 1;
        else {
            DEBUGCHK(pth->lpce);
            pth->lpce->base = pth->lpProxy;
            pth->lpce->size = (DWORD)pth->lpPendProxy;
            pth->lpProxy = 0;
            pth->wCount++;
            if (pth->bWaitState == WAITSTATE_BLOCKED) {
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
                pth->retValue = pprox->dwRetVal;
                SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
                *ppTh = pth;
            } else {
                DEBUGCHK(!GET_SLEEPING(pth));
                DEBUGCHK(pth->bWaitState == WAITSTATE_PROCESSING);
                pth->dwPendReturn = pprox->dwRetVal;
                pth->bWaitState = WAITSTATE_SIGNALLED;
            }
            lpe->state = 0;
        }
    }
    KCALLPROFOFF(16);
    return bRet;
}

//------------------------------------------------------------------------------
// ResetProcEvent -- reset a process event if the process hasn't exited yet
//------------------------------------------------------------------------------
void ResetProcEvent (PEVENT pEvt)
{
    KCALLPROFON(16);
    if (!(pEvt->manualreset & PROCESS_EXITED)) {
        pEvt->state = 0;
    }
    KCALLPROFOFF(16);
}


//------------------------------------------------------------------------------
// DoFreeMutex -- clean up before deleting a mutex object
//------------------------------------------------------------------------------
void DoFreeMutex (PMUTEX lpm)
{
    KCALLPROFON(34);
    UnlinkCritMut((PCRIT)lpm,lpm->pOwner);
    KCALLPROFOFF(34);
}


//------------------------------------------------------------------------------
// LeaveMutex - release the ownership of a mutex
//------------------------------------------------------------------------------
BOOL LeaveMutex (PMUTEX lpm, PTHREAD *ppTh) 
{
    BOOL bRet;
    PTHREAD pth;
    PPROXY pprox, pDown, pNext;
    WORD prio;
    KCALLPROFON(21);
    bRet = 0;
    DEBUGCHK(pCurThread == lpm->pOwner);
    if (!(pprox = lpm->pProxList)) {
        lpm->pOwner = 0;
    } else {
        // dequeue proxy
        prio = pprox->prio/PRIORITY_LEVELS_HASHSCALE;
        pDown = pprox->pQDown;
        pNext = pprox->pQNext;
        DEBUGCHK(lpm->pProxHash[prio] == pprox);
        lpm->pProxHash[prio] = ((pDown != pprox) ? pDown :
            (pNext && (pNext->prio/PRIORITY_LEVELS_HASHSCALE == prio)) ? pNext : 0);
        if (pDown == pprox) {
            if (lpm->pProxList = pNext)
                pNext->pQPrev = 0;
        } else {
            pDown->pQUp = pprox->pQUp;
            pprox->pQUp->pQDown = lpm->pProxList = pDown;
            DEBUGCHK(!pDown->pQPrev);
            if (pNext) {
                pNext->pQPrev = pDown;
                pDown->pQNext = pNext;
            }
        }
        pprox->pQDown = 0;
        // wake up new owner
        pth = pprox->pTh;
        DEBUGCHK(pth);
        if (pth->wCount != pprox->wCount)
            bRet = 1;
        else {
            DEBUGCHK(pth->lpProxy);
            DEBUGCHK(pth->lpce);
            pth->lpce->base = pth->lpProxy;
            pth->lpce->size = (DWORD)pth->lpPendProxy;
            pth->wCount++;
            pth->lpProxy = 0;
            if (pth->bWaitState == WAITSTATE_BLOCKED) {
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
                DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
                pth->retValue = pprox->dwRetVal;
                SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
                *ppTh = pth;
            } else {
                DEBUGCHK(!GET_SLEEPING(pth));
                pth->dwPendReturn = pprox->dwRetVal;
                pth->bWaitState = WAITSTATE_SIGNALLED;
            }
            lpm->LockCount = 1;
            lpm->pOwner = pth;
            LinkCritMut((PCRIT)lpm,pth,0);
        }
    }
    KCALLPROFOFF(21);
    return bRet;
}



//------------------------------------------------------------------------------
// DoFreeCrit -- clean up before deleting a CS
//------------------------------------------------------------------------------
void DoFreeCrit (PCRIT lpcrit)
{
    PTHREAD pth;
    KCALLPROFON(45);
    if (lpcrit->bListed == 1) {
        pth = KC_PthFromId ((DWORD) lpcrit->lpcs->OwnerThread);
        DEBUGCHK(pth);
        UnlinkCritMut(lpcrit,pth);
    }
    KCALLPROFOFF(45);
}

//------------------------------------------------------------------------------
// EventModIntr -- set/pulse a interrupt event
//------------------------------------------------------------------------------
PTHREAD EventModIntr (PEVENT lpe, DWORD type) 
{
    PTHREAD pth = NULL;
    KCALLPROFON(42);
    if (!lpe->pProxList) {
        lpe->state = (type == EVENT_SET);
    } else {
        lpe->state = 0;
        pth = WakeOneThreadInterruptDelayed(lpe);
    }
    DEBUGCHK(!lpe->manualreset);
    KCALLPROFOFF(42);
    return pth;
}



//------------------------------------------------------------------------------
// AdjustPrioDown -- adjust the priority of current running thread while set/pulse 
//                   manual-reset events.
//------------------------------------------------------------------------------
void AdjustPrioDown (void) 
{
    DWORD dwPrio, dwPrio2;
    KCALLPROFON(66);
    dwPrio = GET_BPRIO(pCurThread);
    if (pCurThread->pOwnedList && ((dwPrio2 = pCurThread->pOwnedList->bListedPrio) < dwPrio))
        dwPrio = dwPrio2;
    SET_CPRIO(pCurThread,dwPrio);
    if (RunList.pRunnable && (dwPrio > GET_CPRIO(RunList.pRunnable)))
        SetReschedule();
    KCALLPROFOFF(66);
}

//------------------------------------------------------------------------------
// SamAdd - increment Semaphore count by lReleaseCount,
//          returns previous count (-1 if exceed max)
//------------------------------------------------------------------------------
LONG SemAdd (PSEMAPHORE lpsem, LONG lReleaseCount) 
{
    LONG prev;
    KCALLPROFON(3);
    if ((lReleaseCount <= 0) ||
        (lpsem->lCount + lpsem->lPending + lReleaseCount > lpsem->lMaxCount) ||
        (lpsem->lCount + lpsem->lPending + lReleaseCount < lpsem->lCount + lpsem->lPending)) {
        KCALLPROFOFF(3);
        return -1;
    }
    prev = lpsem->lCount + lpsem->lPending;
    lpsem->lPending += lReleaseCount;
    KCALLPROFOFF(3);
    return prev;
}

//------------------------------------------------------------------------------
// relase all threads, up to the current semaphore count, blocked on the semaphore
//------------------------------------------------------------------------------
BOOL SemPop (PSEMAPHORE lpsem, LPLONG pRemain, PTHREAD *ppTh) 
{
    PTHREAD pth;
    PPROXY pprox, pDown, pNext;
    WORD prio;
    BOOL bRet;
    KCALLPROFON(37);
    bRet = 0;
    if (*pRemain) {
        DEBUGCHK(*pRemain <= lpsem->lPending);
        if (!(pprox = lpsem->pProxList)) {
            lpsem->lCount += *pRemain;
            lpsem->lPending -= *pRemain;
        } else {
            bRet = 1;
            // dequeue proxy
            prio = pprox->prio/PRIORITY_LEVELS_HASHSCALE;
            pDown = pprox->pQDown;
            pNext = pprox->pQNext;
            DEBUGCHK(lpsem->pProxHash[prio] == pprox);
            lpsem->pProxHash[prio] = ((pDown != pprox) ? pDown :
                (pNext && (pNext->prio/PRIORITY_LEVELS_HASHSCALE == prio)) ? pNext : 0);
            if (pDown == pprox) {
                if (lpsem->pProxList = pNext)
                    pNext->pQPrev = 0;
            } else {
                pDown->pQUp = pprox->pQUp;
                pprox->pQUp->pQDown = lpsem->pProxList = pDown;
                DEBUGCHK(!pDown->pQPrev);
                if (pNext) {
                    pNext->pQPrev = pDown;
                    pDown->pQNext = pNext;
                }
            }
            pprox->pQDown = 0;
            // wake up thread        
            pth = pprox->pTh;
            DEBUGCHK(pth);
            if (pth->wCount == pprox->wCount) {
                DEBUGCHK(pth->lpProxy);
                if (pth->lpce) {
                    pth->lpce->base = pth->lpProxy;
                    pth->lpce->size = (DWORD)pth->lpPendProxy;
                }
                pth->wCount++;
                pth->lpProxy = 0;
                if (pth->bWaitState == WAITSTATE_BLOCKED) {
                    DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNABLE);
                    DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_RUNNING);
                    DEBUGCHK(GET_RUNSTATE(pth) != RUNSTATE_NEEDSRUN);
                    pth->retValue = pprox->dwRetVal;
                    SET_RUNSTATE(pth,RUNSTATE_NEEDSRUN);
                    *ppTh = pth;
                } else {
                    DEBUGCHK(!GET_SLEEPING(pth));
                    pth->dwPendReturn = pprox->dwRetVal;
                    pth->bWaitState = WAITSTATE_SIGNALLED;
                }
                lpsem->lPending--;
                (*pRemain)--;
            }
        }
    }
    KCALLPROFOFF(37);
    return bRet;
}


//------------------------------------------------------------------------------
// PutThreadToSleep - put a thread into the sleep queue
//------------------------------------------------------------------------------
int PutThreadToSleep (sleeper_t *pSleeper) 
{
    int nRet;
    KCALLPROFON(2);
    DEBUGCHK(!GET_SLEEPING(pCurThread));
    DEBUGCHK(!pCurThread->pUpSleep && !pCurThread->pDownSleep);

    CLEAR_NEEDSLEEP (pCurThread);

    if (GET_DYING(pCurThread) && !GET_DEAD(pCurThread) && GET_USERBLOCK(pCurThread)) {
        // thread dying
        KCALLPROFOFF(2);
        return 0;
    }

    if ((int) (pSleeper->dwWakeupTime - GETCURRTICK()) <= 0) {
        // time to wakeup already
        KCALLPROFOFF(2);
        return 0;
    }

    nRet = SleepOneMore (pSleeper);

    KCALLPROFOFF(2);
    return nRet;
}


//------------------------------------------------------------------------------
// WaitConfig - setup before start processing a blocking operation (Wait/ECS)
//------------------------------------------------------------------------------
void WaitConfig (PPROXY pProx, CLEANEVENT *lpce) 
{
    KCALLPROFON(1);
    DEBUGCHK(RunList.pth == pCurThread);
    DEBUGCHK(!pCurThread->lpProxy);
    pCurThread->lpPendProxy = pProx;
    pCurThread->lpce = lpce;
    pCurThread->bWaitState = WAITSTATE_PROCESSING;
    pCurThread->pCrabPth = 0;
    pCurThread->wCrabDir = 0;
    DEBUGCHK(RunList.pth == pCurThread);
    KCALLPROFOFF(1);
}


//------------------------------------------------------------------------------
// WaitOneMore - process one synchronization object in Wait funciton
//------------------------------------------------------------------------------
DWORD WaitOneMore (LPDWORD pdwContinue, PCRIT *ppCritMut) 
{
    PPROXY pProx;
    DWORD retval;
    BOOL noenqueue;
    KCALLPROFON(18);
    CLEAR_NEEDSLEEP (pCurThread);
    noenqueue = !pCurThread->lpce && !pCurThread->dwPendTime;
    DEBUGCHK(RunList.pth == pCurThread);
    if (pCurThread->bWaitState == WAITSTATE_SIGNALLED) {
        *pdwContinue = 0;
        retval = pCurThread->dwPendReturn;
    } else if (KC_IsThreadTerminated (pCurThread)) {
        KSetLastError(pCurThread,ERROR_OPERATION_ABORTED);
        retval = WAIT_FAILED;
        goto exitfunc;
    } else if (pProx = pCurThread->lpPendProxy) {
        DEBUGCHK(pCurThread->bWaitState == WAITSTATE_PROCESSING);
        DEBUGCHK(pCurThread == pProx->pTh);
        DEBUGCHK(pProx->pTh == pCurThread);
        DEBUGCHK(pProx->wCount == pCurThread->wCount);

        switch (pProx->bType) {
            case SH_CURTHREAD: {
                PTHREAD pthWait;
                if (!(pthWait = (PTHREAD)pProx->pObject) || GET_DEAD(pthWait))
                    goto waitok;
                if (!noenqueue)
                    FlatEnqueue(pthWait,pProx);
                break;
            }
            case HT_EVENT: {
                PEVENT pevent;
                BYTE prio;
                if (!(pevent = (PEVENT)pProx->pObject))
                    goto waitbad;
                if (pevent->state) {    // event is already signalled
                    DEBUGCHK(!pevent->phdIntr || !pevent->manualreset);
                    pevent->state = pevent->manualreset;
                    goto waitok;
                }
                if (!noenqueue) {
                    prio = pProx->prio;

                    if (pevent->phdIntr) {
                        // interrupt event
                        if (pevent->pProxList) {
                            // more than one threads waiting on the same interrupt event.
                            // can't DEBUGCHK here, do it ouside KCall, so system doesn't 
                            // crash on debug build
                            // DEBUGCHK (0);
                            goto waitbad;
                        }

                        pProx->bType = HT_MANUALEVENT;
                        pProx->pQUp = pProx->pQDown = pProx;
                        pProx->pQPrev = (PPROXY) pevent;
                        pevent->pProxList = pProx;
                        
                    } else if (pevent->manualreset) {
                        // manual reset event
                        pProx->bType = HT_MANUALEVENT;
                        FlatEnqueue((PTHREAD)pevent,pProx);
                        if (prio < pevent->bMaxPrio)
                            pevent->bMaxPrio = prio;
                        
                    } else {
                        // auto-reset event
                        PrioEnqueue(pevent,prio,pProx);
                    }
                }
                break;
            }
            case HT_SEMAPHORE: {
                PSEMAPHORE psem;
                BYTE prio;
                if (!(psem = (PSEMAPHORE)pProx->pObject))
                    goto waitbad;
                if (psem->lCount) {
                    psem->lCount--;
                    goto waitok;
                }
                if (!noenqueue) {
                    prio = pProx->prio;
                    PrioEnqueue((PEVENT)psem,prio,pProx);
                }
                break;
            }
            case HT_MUTEX: {
                PMUTEX pmutex;
                BYTE prio;
                if (!(pmutex = (PMUTEX)pProx->pObject))
                    goto waitbad;
                if (!pmutex->pOwner) {
                    pmutex->LockCount = 1;
                    pmutex->pOwner = pCurThread;
                    *ppCritMut = (PCRIT)pmutex;
                    goto waitok;
                }
                if (pmutex->pOwner == pCurThread) {
                    if (pmutex->LockCount == MUTEX_MAXLOCKCNT) {
                        goto waitbad;
                    }
                    pmutex->LockCount++;
                    goto waitok;
                }
                if (!noenqueue) {
                    prio = pProx->prio;
                    if (!pmutex->pProxList || (prio < pmutex->pProxList->prio))
                        *ppCritMut = (PCRIT)pmutex;
                    PrioEnqueue((PEVENT)pmutex,prio,pProx);
                }
                break;
            }
            default:
                goto waitbad;
        }
        if (!noenqueue) {
            pCurThread->lpPendProxy = pProx->pThLinkNext;
            pProx->pThLinkNext = pCurThread->lpProxy;
            pCurThread->lpProxy = pProx;
            retval = 0; // doesn't matter, we're continuing
        } else {
            retval = WAIT_TIMEOUT;
            goto exitfunc;
        }
    } else if (pCurThread->dwPendTime == INFINITE) {
        *pdwContinue = 0;
        pCurThread->bWaitState = WAITSTATE_BLOCKED;
        DEBUGCHK(!((pCurThread->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
        SET_RUNSTATE(pCurThread,RUNSTATE_BLOCKED);
        RunList.pth = 0;
        SetReschedule();
        retval = 0; // doesn't matter, only an event can wake us
    } else {
        if (!pCurThread->dwPendTime || (((int) (GETCURRTICK () - pCurThread->dwPendWakeup)) >= 0)) {
            retval = WAIT_TIMEOUT;
            goto exitfunc;
        }
        if (SleepOneMore ((sleeper_t *) &pCurThread->pCrabPth)) {
            KCALLPROFOFF(18);
            return 0;    // doesn't matter, we're continueing
        }
        DEBUGCHK(!pCurThread->lpPendProxy);
        pCurThread->bWaitState = WAITSTATE_BLOCKED;
        *pdwContinue = 0;
        retval = WAIT_TIMEOUT;
    }
    KCALLPROFOFF(18);
    return retval;
waitbad:
    KSetLastError(pCurThread,ERROR_INVALID_HANDLE);
    retval = WAIT_FAILED;
    goto exitfunc;
waitok:
    retval = pProx->dwRetVal;
exitfunc:
    if (pCurThread->lpce) {
        pCurThread->lpce->base = pCurThread->lpProxy;
        pCurThread->lpce->size = (DWORD)pCurThread->lpPendProxy;
    }
    pCurThread->lpProxy = 0;
    pCurThread->wCount++;
    pCurThread->bWaitState = WAITSTATE_SIGNALLED;
    *pdwContinue = 0;
    KCALLPROFOFF(18);
    return retval;
}


//------------------------------------------------------------------------------
// WaitForOneManualNonInterruptEvent - process one synchronization object 
// in Wait funciton; optionally call user specified function before the wait 
// begins. If the user specified function returns TRUE, then return from the 
// wait function with the same return value as if the wait object is signaled.
//------------------------------------------------------------------------------
DWORD WaitForOneManualNonInterruptEvent (PPROXY pProx, PFN_WAITFUNC pfnUserFunc, DWORD dwUserData) 
{
    DWORD retval; 
    PEVENT pevent;

    KCALLPROFON(76);        
    CLEAR_NEEDSLEEP (pCurThread);
    DEBUGCHK(RunList.pth == pCurThread);
  
    if (!pProx || !(pevent = (PEVENT)pProx->pObject) || !pevent->manualreset) {
        // this api supports only manual reset events
        KSetLastError(pCurThread,ERROR_INVALID_PARAMETER);
        pCurThread->wCount++;
        pCurThread->bWaitState = WAITSTATE_SIGNALLED;
        retval = WAIT_FAILED;
    
    } else if (KC_IsThreadTerminated (pCurThread)) {
        // Is the thread terminated?
        KSetLastError(pCurThread,ERROR_OPERATION_ABORTED);
        pCurThread->wCount++;
        pCurThread->bWaitState = WAITSTATE_SIGNALLED;        
        retval = WAIT_FAILED;

    } else if (pfnUserFunc && pfnUserFunc(dwUserData)) {
        // Does user specified function return TRUE?
        pCurThread->wCount++;
        pCurThread->bWaitState = WAITSTATE_SIGNALLED;        
        retval = WAIT_OBJECT_0;

    } else if (pevent->state){
        // Is the event already signalled?
        pCurThread->wCount++;
        pCurThread->bWaitState = WAITSTATE_SIGNALLED;        
        retval = pProx->dwRetVal;

    } else if (!pCurThread->dwPendTime){
        // zero time left?
        pCurThread->wCount++;
        pCurThread->bWaitState = WAITSTATE_SIGNALLED;        
        retval = WAIT_TIMEOUT;
            
    } else {
        // add the proxy to the proxy list for the event
        FlatEnqueue((PTHREAD)pevent,pProx);
        if (pProx->prio < pevent->bMaxPrio)
            pevent->bMaxPrio = pProx->prio;
            
        if (pCurThread->dwPendTime == INFINITE) {
            // The current thread will wake up only when the event is signaled
            // after this kcall returns.
            pCurThread->bWaitState = WAITSTATE_BLOCKED;
            DEBUGCHK(!((pCurThread->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
            SET_RUNSTATE(pCurThread,RUNSTATE_BLOCKED);
            RunList.pth = 0;
            SetReschedule();
            retval = 0;
            
        } else {
            if (((int) (GETCURRTICK () - pCurThread->dwPendWakeup)) >= 0) {
                // timed out?
                pCurThread->wCount++;
                pCurThread->bWaitState = WAITSTATE_SIGNALLED;        
                retval = WAIT_TIMEOUT;
                
            } else if (SleepOneMore ((sleeper_t *) &pCurThread->pCrabPth)) {
                // could not add the current thread to the sleep list; caller will retry
                DequeueFlatProxy(pProx);
                pCurThread->bWaitState = WAITSTATE_PROCESSING;               
                retval = 0;
                
            } else {
                // current thread will be put to sleep when the kcall returns
                // current thread will wake up when time expires or event is signaled
                pCurThread->bWaitState = WAITSTATE_BLOCKED;
                retval = WAIT_TIMEOUT;
            }
        }
    }

    KCALLPROFOFF(76);
    return retval;
}


//------------------------------------------------------------------------------
// CSWaitPart1 - part I of grabbing a CS.
//------------------------------------------------------------------------------
DWORD CSWaitPart1 (PCRIT *ppCritMut, PPROXY pProx, PCRIT pcrit) 
{
    BOOL retval;
    LPCRITICAL_SECTION lpcs;
    PTHREAD pOwner;
    BYTE prio;
    KCALLPROFON(19);
    DEBUGCHK(RunList.pth == pCurThread);
    DEBUGCHK(GET_RUNSTATE(pCurThread) != RUNSTATE_BLOCKED);
    if (pCurThread->bWaitState == WAITSTATE_SIGNALLED) {
        retval = CSWAIT_TAKEN;
        DEBUGCHK(RunList.pth == pCurThread);
    } else if (KC_IsThreadTerminated (pCurThread)) {
        retval = CSWAIT_ABORT;
        DEBUGCHK(RunList.pth == pCurThread);
        goto exitcs1;
    } else if (!IsValidKPtr (pcrit)) {
        retval = CSWAIT_ABORT;
        NKDbgPrintfW (L"Critical section deleted while other threads are still using it\r\n");
        goto exitcs1;
    } else {
        DEBUGCHK(RunList.pth == pCurThread);
        DEBUGCHK(pProx == pCurThread->lpPendProxy);
        DEBUGCHK(pCurThread->bWaitState == WAITSTATE_PROCESSING);
        DEBUGCHK(pCurThread == pProx->pTh);
        DEBUGCHK(pProx->pTh == pCurThread);
        DEBUGCHK(pProx->wCount == pCurThread->wCount);
        DEBUGCHK(pProx->bType == HT_CRITSEC);
        DEBUGCHK(pcrit == (PCRIT)pProx->pObject);
        lpcs = pcrit->lpcs;
        if (!lpcs->OwnerThread
            || (!lpcs->needtrap && ((DWORD)lpcs->OwnerThread & 1))
            || !(pOwner = KC_PthFromId ((DWORD)lpcs->OwnerThread & ~1))
            || (!lpcs->needtrap && (pcrit->bListed != 1) && GET_BURIED(pOwner) && !IsKernelVa (lpcs))) {
            DEBUGCHK(RunList.pth == pCurThread);
            lpcs->OwnerThread = (HANDLE) dwCurThId;
            lpcs->LockCount = 1;
            DEBUGCHK(!pcrit->pProxList);
            DEBUGCHK(pcrit->bListed != 1);
            retval = CSWAIT_TAKEN;
            DEBUGCHK(RunList.pth == pCurThread);
            goto exitcs1;
        } else if ((pOwner->pLastCrit == pcrit) && (GET_CPRIO(pOwner) > GET_CPRIO(pCurThread))) {
            DEBUGCHK(RunList.pth == pCurThread);
            DEBUGCHK(lpcs->LockCount == 1);
            DEBUGCHK(pOwner->lpCritProxy);
            DEBUGCHK(!pOwner->lpProxy);
            DEBUGCHK(pOwner->wCount == (unsigned short)(pOwner->lpCritProxy->wCount + 1));
            lpcs->OwnerThread = (HANDLE) dwCurThId;
            DEBUGCHK(pcrit->bListed != 1);
            if (pcrit->pProxList) {
                *ppCritMut = pcrit;
                lpcs->needtrap = 1;
            } else
                DEBUGCHK(!lpcs->needtrap);
            retval = CSWAIT_TAKEN;
            DEBUGCHK(RunList.pth == pCurThread);
            goto exitcs1;
        } else {
            DEBUGCHK(RunList.pth == pCurThread);
            prio = pProx->prio;
            DEBUGCHK(!pcrit->pProxList || lpcs->needtrap);
            if (!pcrit->pProxList || (prio < pcrit->pProxList->prio))
                *ppCritMut = pcrit;
            PrioEnqueue((PEVENT)pcrit,prio,pProx);
            lpcs->needtrap = 1;
            DEBUGCHK(RunList.pth == pCurThread);
            // get here if blocking
            DEBUGCHK(!pProx->pThLinkNext);
            pCurThread->lpPendProxy = 0;
            DEBUGCHK(!pCurThread->lpProxy);
            pProx->pThLinkNext = 0;
            pCurThread->lpProxy = pProx;
            retval = CSWAIT_PEND;
            DEBUGCHK(RunList.pth == pCurThread);
        }
    }
    DEBUGCHK(RunList.pth == pCurThread);
    DEBUGCHK(GET_RUNSTATE(pCurThread) != RUNSTATE_BLOCKED);
    KCALLPROFOFF(19);
    return retval;
exitcs1:
    DEBUGCHK(pCurThread->lpce);
    DEBUGCHK(!pCurThread->lpProxy);
    DEBUGCHK(pCurThread->lpPendProxy);
    pCurThread->lpce->base = 0;
    pCurThread->lpce->size = (DWORD)pCurThread->lpPendProxy;
    pCurThread->wCount++;
    pCurThread->bWaitState = WAITSTATE_SIGNALLED;
    KCALLPROFOFF(19);
    DEBUGCHK(RunList.pth == pCurThread);
    DEBUGCHK(GET_RUNSTATE(pCurThread) != RUNSTATE_BLOCKED);
    return retval;
}



//------------------------------------------------------------------------------
// CSWaitPart2 - part 2 of grabbing a CS.
//------------------------------------------------------------------------------
void CSWaitPart2 (void) 
{
    KCALLPROFON(58);
    if (pCurThread->bWaitState != WAITSTATE_SIGNALLED) {
        if (KC_IsThreadTerminated (pCurThread)) {
            DEBUGCHK(pCurThread->lpce);
            DEBUGCHK(pCurThread->lpProxy);
            pCurThread->lpce->base = pCurThread->lpProxy;
            DEBUGCHK(!pCurThread->lpPendProxy);
            pCurThread->lpce->size = 0;
            pCurThread->lpProxy = 0;
            pCurThread->wCount++;
            pCurThread->bWaitState = WAITSTATE_SIGNALLED;
        } else {
            PCRIT pCrit;
            PTHREAD pth;
            pCrit = (PCRIT)pCurThread->lpProxy->pObject;
            if (!pCrit->bListed) {
                pth = KC_PthFromId ((DWORD)pCrit->lpcs->OwnerThread & ~1);
                DEBUGCHK(pth);
                pth->pLastCrit = 0;
                LinkCritMut(pCrit,pth,1);
            }
            DEBUGCHK(pCrit->lpcs->needtrap);
            pCurThread->bWaitState = WAITSTATE_BLOCKED;
            DEBUGCHK(!((pCurThread->wInfo >> DEBUG_LOOPCNT_SHIFT) & 1));
            SET_RUNSTATE(pCurThread,RUNSTATE_BLOCKED);
            RunList.pth = 0;
            SetReschedule();
        }
    }
    KCALLPROFOFF(58);
}

//------------------------------------------------------------------------------
// PreLeaveCrit - step 1 of leaving a CS
//          remove the CS from the thread's "owned object list"
//------------------------------------------------------------------------------
BOOL PreLeaveCrit (PCRIT pCrit, LPCRITICAL_SECTION lpcs) 
{
    BOOL bRet;
    KCALLPROFON(53);
    DEBUGCHK (pCrit->lpcs == lpcs);
    if (((DWORD)lpcs->OwnerThread != ((DWORD)dwCurThId | 1)))
        bRet = FALSE;
    else {
        lpcs->OwnerThread = (HANDLE) dwCurThId;
        bRet = TRUE;
        SET_NOPRIOCALC (pCurThread);
        UnlinkCritMut (pCrit, pCurThread);
        pCrit->bListed = 2;
    }
    KCALLPROFOFF(53);
    return bRet;
}

//------------------------------------------------------------------------------
// LeaveCrit - final stop to leave a CS
//          give the CS to new owner or set owner to 0 if there is no one waiting
//------------------------------------------------------------------------------
BOOL LeaveCrit (PCRIT pCrit, LPCRITICAL_SECTION lpcs, PTHREAD *ppTh)
{
    PTHREAD pNewOwner;
    PPROXY pprox, pDown, pNext;
    WORD prio;
    BOOL bRet;
    KCALLPROFON(20);
    bRet = FALSE;
    DEBUGCHK(lpcs->OwnerThread == (HANDLE) dwCurThId);

    DEBUGCHK(lpcs->LockCount == 1);
    if (!(pprox = pCrit->pProxList)) {
        DEBUGCHK(pCrit->bListed != 1);
        lpcs->OwnerThread = 0;
        lpcs->needtrap = 0;
    } else {
        // dequeue proxy
        prio = pprox->prio/PRIORITY_LEVELS_HASHSCALE;
        pDown = pprox->pQDown;
        pNext = pprox->pQNext;
        DEBUGCHK(pCrit->pProxHash[prio] == pprox);
        pCrit->pProxHash[prio] = ((pDown != pprox) ? pDown :
            (pNext && (pNext->prio/PRIORITY_LEVELS_HASHSCALE == prio)) ? pNext : 0);
        if (pDown == pprox) {
            if (pCrit->pProxList = pNext)
                pNext->pQPrev = 0;
        } else {
            pDown->pQUp = pprox->pQUp;
            pprox->pQUp->pQDown = pCrit->pProxList = pDown;
            DEBUGCHK(!pDown->pQPrev);
            if (pNext) {
                pNext->pQPrev = pDown;
                pDown->pQNext = pNext;
            }
        }
        pprox->pQDown = 0;
        // wake up new owner
        pNewOwner = pprox->pTh;
        DEBUGCHK(pNewOwner);
        if (pNewOwner->wCount != pprox->wCount)
            bRet = 1;
        else {
            DEBUGCHK(!GET_SLEEPING(pNewOwner));
            DEBUGCHK(pNewOwner->lpce);
            DEBUGCHK(pNewOwner->lpProxy == pprox);
            DEBUGCHK(!pNewOwner->lpPendProxy);
            pNewOwner->lpce->base = pprox;
            pNewOwner->lpce->size = 0;
            pNewOwner->wCount++;
            pNewOwner->lpProxy = 0;
            pNewOwner->lpCritProxy = pprox; // since we can steal it back, don't lose proxy
            if (pNewOwner->bWaitState == WAITSTATE_BLOCKED) {
                DEBUGCHK(GET_RUNSTATE(pNewOwner) != RUNSTATE_RUNNABLE);
                DEBUGCHK(GET_RUNSTATE(pNewOwner) != RUNSTATE_RUNNING);
                DEBUGCHK(GET_RUNSTATE(pNewOwner) != RUNSTATE_NEEDSRUN);
                SET_RUNSTATE(pNewOwner,RUNSTATE_NEEDSRUN);
                *ppTh = pNewOwner;
            } else {
                DEBUGCHK(GET_RUNSTATE(pNewOwner) != RUNSTATE_BLOCKED);
                pNewOwner->bWaitState = WAITSTATE_SIGNALLED;
            }
            if (!pCrit->pProxList)
                lpcs->needtrap = 0;
            else
                DEBUGCHK(lpcs->needtrap);
            lpcs->LockCount = 1;
            lpcs->OwnerThread = (HANDLE) pNewOwner->dwId;
            DEBUGCHK(!pNewOwner->pLastCrit);
            pNewOwner->pLastCrit = pCrit;
      }
    }
    if (!bRet) {
        DEBUGCHK(pCrit->bListed != 1);
        pCrit->bListed = 0;
    }
    KCALLPROFOFF(20);
    return bRet;
}

//------------------------------------------------------------------------------
// ClearThreadStruct
//          preserve wCount and wCount2 and set all other fields of THREAD to 0
//------------------------------------------------------------------------------
void ClearThreadStruct (PTHREAD pth)
{
    WORD wCount, wCount2;

    KCALLPROFON(13);

    // need to preserve wCount and wCount2 or SleepList and sync-object handling can mess up.
    wCount = pth->wCount;
    wCount2 = pth->wCount2;

    memset (pth, 0, sizeof (THREAD));

    pth->wCount = wCount;
    pth->wCount2 = wCount2;

    KCALLPROFOFF(13);
}


