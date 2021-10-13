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
/*++



Module Name:

    messagemgr.c

Abstract:

    Message Manager for DhcpV6 client.

Author:

    FrancisD

Environment:

    User Level: Windows

Revision History:


--*/

#include "dhcpv6p.h"
#include "intsafe.h"
//#include "precomp.h"
//#include "messagemgr.tmh"


void mul64_32_64(const FILETIME *lpnum1, DWORD num2, LPFILETIME lpres) {
    __int64 num1;
    num1 = (__int64)lpnum1->dwLowDateTime * (__int64)num2;
    num1 += ((__int64)lpnum1->dwHighDateTime * (__int64)num2)<<32;
    lpres->dwHighDateTime = (DWORD)(num1>>32);
    lpres->dwLowDateTime = (DWORD)(num1&0xffffffff);
}

void add64_32_64(const FILETIME *lpnum1, DWORD num2, LPFILETIME lpres) {
    DWORD bottom = lpnum1->dwLowDateTime + num2;
    lpres->dwHighDateTime = lpnum1->dwHighDateTime + (bottom < lpnum1->dwLowDateTime ? 1 : 0);
    lpres->dwLowDateTime = bottom;
}

#ifdef THUMB
// BUGBUG 11310/11387: THUMB doesn't like 32 bit shifts in 64 bit values
#pragma optimize("",off)
#endif
void add64_64_64(const FILETIME *lpnum1, LPFILETIME lpnum2, LPFILETIME lpres) {
    __int64 num1, num2;
    num1 = (((__int64)lpnum1->dwHighDateTime)<<32)+(__int64)lpnum1->dwLowDateTime;
    num2 = (((__int64)lpnum2->dwHighDateTime)<<32)+(__int64)lpnum2->dwLowDateTime;
    num1 += num2;
    lpres->dwHighDateTime = (DWORD)(num1>>32);
    lpres->dwLowDateTime = (DWORD)(num1&0xffffffff);
}

void sub64_64_64(const FILETIME *lpnum1, LPFILETIME lpnum2, LPFILETIME lpres) {
    __int64 num1, num2;
    num1 = (((__int64)lpnum1->dwHighDateTime)<<32)+(__int64)lpnum1->dwLowDateTime;
    num2 = (((__int64)lpnum2->dwHighDateTime)<<32)+(__int64)lpnum2->dwLowDateTime;
    num1 -= num2;
    lpres->dwHighDateTime = (DWORD)(num1>>32);
    lpres->dwLowDateTime = (DWORD)(num1&0xffffffff);
}
// Unsigned divide
// Divides a 64 bit number by a *31* bit number.  Doesn't work for 32 bit divisors!

void div64_32_64(const FILETIME *lpdividend, DWORD divisor, LPFILETIME lpresult) {
    DWORD bitmask;
    DWORD top;
    FILETIME wholetop = *lpdividend;
    top = 0;
    lpresult->dwHighDateTime = 0;
    for (bitmask = 0x80000000; bitmask; bitmask >>= 1) {
        top = (top<<1) + ((wholetop.dwHighDateTime&bitmask) ? 1 : 0);
        if (top >= divisor) {
            top -= divisor;
            lpresult->dwHighDateTime |= bitmask;
        }
    }
    lpresult->dwLowDateTime = 0;
    for (bitmask = 0x80000000; bitmask; bitmask >>= 1) {
        top = (top<<1) + ((wholetop.dwLowDateTime&bitmask) ? 1 : 0);
        if (top >= divisor) {
            top -= divisor;
            lpresult->dwLowDateTime |= bitmask;
        }
    }
}

#ifdef THUMB
// BUGBUG 11310/11387: THUMB doesn't like 32 bit shifts in 64 bit values
#pragma optimize("",on)
#endif

extern DWORD FindMaxTimeout(PDHCPV6_ADAPT pDhcpV6Adapt, DWORD MaxTime, 
    DWORD Time);


VOID
DHCPV6MessageMgrSolicitCallback(
    PVOID pvContext1,
    PVOID pvContext2
    )
{
    DWORD dwError = 0;
    PDHCPV6_ADAPT pDhcpV6Adapt = (PDHCPV6_ADAPT)pvContext1;


    dwError = DHCPV6MessageMgrSolicitMessage(pDhcpV6Adapt);

    return;
}

VOID
DHCPV6MessageMgrRequestCallback(
    PVOID pvContext1,
    PVOID pvContext2
    )
{
    DWORD dwError = 0;
    PDHCPV6_ADAPT pDhcpV6Adapt = (PDHCPV6_ADAPT)pvContext1;


    dwError = DHCPV6MessageMgrRequestMessage(pDhcpV6Adapt);

    return;
}

VOID
DHCPV6MessageMgrTCallback(
    PVOID pvContext1,
    PVOID pvContext2
    )
{
    DWORD dwError = 0;
    PDHCPV6_ADAPT pDhcpV6Adapt = (PDHCPV6_ADAPT)pvContext1;

    DEBUGMSG(1, (TEXT("+DHCPV6MessageMgrTCallback\r\n")));

    if (dhcpv6_state_T1 == pDhcpV6Adapt->DhcpV6State)
        dwError = DHCPV6MessageMgrRenewMessage(pDhcpV6Adapt);
    else if (dhcpv6_state_T2 == pDhcpV6Adapt->DhcpV6State  ||
        dhcpv6_state_rebindconfirm == pDhcpV6Adapt->DhcpV6State)
        dwError = DHCPV6MessageMgrRebindMessage(pDhcpV6Adapt);

    return;
}



VOID
DHCPV6MessageMgrInfoReqCallback(
    PVOID pvContext1,
    PVOID pvContext2
    )
{
    DWORD dwError = 0;
    PDHCPV6_ADAPT pDhcpV6Adapt = (PDHCPV6_ADAPT)pvContext1;


    dwError = DHCPV6MessageMgrInfoRequest(pDhcpV6Adapt);

    return;
}

LONG TimerExpired(PDHCPV6_ADAPT pDhcpV6Adapt, DWORD Time) {
    FILETIME    CurTime, ExpireTime;

    ExpireTime.dwLowDateTime = Time;
    ExpireTime.dwHighDateTime = 0;
    mul64_32_64(&ExpireTime, NANO100_TO_SEC, &ExpireTime);
    add64_64_64(&pDhcpV6Adapt->pPdOption->IAPrefix.IALeaseObtained, 
        &ExpireTime, &ExpireTime);

    GetCurrentFT (&CurTime);
    return CompareFileTime(&CurTime, &ExpireTime);

}   // TimerExpired()


VOID
CALLBACK
DHCPV6MessageMgrTimerCallbackRoutine(
    PVOID pvContext,
    BOOLEAN bWaitTimeout
    )
{
    DWORD dwError = 0;
    PDHCPV6_ADAPT pDhcpV6Adapt = (PDHCPV6_ADAPT)pvContext;
    DWORD   fTimerExpired;


    DhcpV6Trace(DHCPV6_MISC, DHCPV6_LOG_LEVEL_TRACE, ("Begin Timer Callback Fired for Adapt: %d with RefCount: %d", pDhcpV6Adapt->dwIPv6IfIndex, pDhcpV6Adapt->uRefCount));

    AcquireExclusiveLock(&pDhcpV6Adapt->RWLock);
    DEBUGMSG(1, (TEXT("+DHCPV6MessageMgrTimerCallbackRoutine\r\n")));

    if (pDhcpV6Adapt->bEventTimerQueued == FALSE) {
        DhcpV6Trace(DHCPV6_MISC, DHCPV6_LOG_LEVEL_WARN, ("Timer Callback Fired Ignored: has been processed already"));
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        BAIL_ON_WIN32_SUCCESS(dwError);
    }
    pDhcpV6Adapt->bEventTimerQueued = FALSE;

    if(pDhcpV6Adapt->bEventTimerCancelled) {
        DhcpV6Trace(DHCPV6_MISC, DHCPV6_LOG_LEVEL_WARN, ("Timer Callback Cancelled on Adapt: %d with RefCount: %d", pDhcpV6Adapt->dwIPv6IfIndex, pDhcpV6Adapt->uRefCount));
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
        BAIL_ON_WIN32_SUCCESS(dwError);
    }

    switch (pDhcpV6Adapt->DhcpV6State) {
    case dhcpv6_state_solicit:
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        DhcpV6Trace(DHCPV6_MISC, DHCPV6_LOG_LEVEL_WARN, ("WARN: Timer Callback: Request - No reply for InfoRequest on Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));
        dwError = DhcpV6EventAddEvent(
                        gpDhcpV6EventModule,
                        pDhcpV6Adapt->dwIPv6IfIndex,
                        DHCPV6MessageMgrSolicitCallback,
                        pDhcpV6Adapt,
                        NULL
                        );
        BAIL_ON_WIN32_ERROR(dwError);
        break;

    case dhcpv6_state_srequest:

        if (pDhcpV6Adapt->uMaxReXmits && 
            (pDhcpV6Adapt->uReXmits >= pDhcpV6Adapt->uMaxReXmits)) {
            SetInitialTimeout(pDhcpV6Adapt, DHCPV6_SOL_TIMEOUT, 
                DHCPV6_SOL_MAX_RT, 0);
            dwError = DhcpV6EventAddEvent(gpDhcpV6EventModule,
                pDhcpV6Adapt->dwIPv6IfIndex, DHCPV6MessageMgrSolicitCallback,
                pDhcpV6Adapt, NULL);

            ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

            ASSERT(! dwError);
            BAIL_ON_WIN32_ERROR(dwError);
            break;
        }
        
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        DhcpV6Trace(DHCPV6_MISC, DHCPV6_LOG_LEVEL_WARN, ("WARN: Timer Callback: Request - No reply for InfoRequest on Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));
        dwError = DhcpV6EventAddEvent(
                        gpDhcpV6EventModule,
                        pDhcpV6Adapt->dwIPv6IfIndex,
                        DHCPV6MessageMgrRequestCallback,
                        pDhcpV6Adapt,
                        NULL
                        );
        BAIL_ON_WIN32_ERROR(dwError);
        break;

    case dhcpv6_state_request:
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        DhcpV6Trace(DHCPV6_MISC, DHCPV6_LOG_LEVEL_WARN, ("WARN: Timer Callback: Request - No reply for InfoRequest on Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));
        dwError = DhcpV6EventAddEvent(
                        gpDhcpV6EventModule,
                        pDhcpV6Adapt->dwIPv6IfIndex,
                        DHCPV6MessageMgrInfoReqCallback,
                        pDhcpV6Adapt,
                        NULL
                        );
        BAIL_ON_WIN32_ERROR(dwError);
        break;

    case dhcpv6_state_rebindconfirm:

        // 3 reasons to go to solicit mode
        // 1. no prefix option, 2. prefix ValidLifetime has expired
        // 3. DHCPV6_CNF_MAX_RD has expired

        if (pDhcpV6Adapt->pPdOption)
            fTimerExpired = (TimerExpired(pDhcpV6Adapt, 
                pDhcpV6Adapt->pPdOption->IAPrefix.ValidLifetime) >= 0);
        
        if ((! pDhcpV6Adapt->pPdOption) || fTimerExpired ||
            (pDhcpV6Adapt->StartRebindConfirm + (DHCPV6_CNF_MAX_RD * SEC_TO_MS)
            <= GetTickCount())) {

            // prefix has expired go back to solicit mode
            // should we delete the existing pd?

            if (pDhcpV6Adapt->pPdOption) {

                if (fTimerExpired) {
                    // delete existing prefix
                    DHCPV6_IA_PREFIX *pPrefix = &pDhcpV6Adapt->pPdOption->IAPrefix;

                    DHCPv6ManagePrefix(
                        pDhcpV6Adapt->dwIPv6IfIndex,
                        pPrefix->PrefixAddr,
                        pPrefix->cPrefix,
                        0, 0);

                    FreeDHCPV6Mem(pDhcpV6Adapt->pPdOption);
                    pDhcpV6Adapt->pPdOption = NULL;

                }
                DHCPv6ManagePrefixPeriodicCleanup();
                
            }
            DeleteRegistrySettings(pDhcpV6Adapt, DEL_REG_ALL);

            SetInitialTimeout(pDhcpV6Adapt, DHCPV6_SOL_TIMEOUT, 
                DHCPV6_SOL_MAX_RT, 0);
            ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

            dwError = DhcpV6EventAddEvent(gpDhcpV6EventModule,
                pDhcpV6Adapt->dwIPv6IfIndex, DHCPV6MessageMgrSolicitCallback,
                pDhcpV6Adapt, NULL);


            ASSERT(! dwError);
            BAIL_ON_WIN32_ERROR(dwError);
            break;
        }
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        dwError = DhcpV6EventAddEvent(
                        gpDhcpV6EventModule,
                        pDhcpV6Adapt->dwIPv6IfIndex,
                        DHCPV6MessageMgrTCallback,
                        pDhcpV6Adapt,
                        NULL
                        );
        
        BAIL_ON_WIN32_ERROR(dwError);
        break;

    case dhcpv6_state_configured:
    case dhcpv6_state_T1:
    case dhcpv6_state_T2:
        DhcpV6Trace(DHCPV6_MISC, DHCPV6_LOG_LEVEL_WARN, ("Timer Callback: Already Configured on Adapt: %d with RefCount: %d", pDhcpV6Adapt->dwIPv6IfIndex, pDhcpV6Adapt->uRefCount));

        if ((! gbDHCPV6PDEnabled) || (! pDhcpV6Adapt->pPdOption)) {
            ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
            DereferenceDHCPV6Adapt(pDhcpV6Adapt);
            break;
        }

        if (TimerExpired(pDhcpV6Adapt, 
            pDhcpV6Adapt->pPdOption->IAPrefix.ValidLifetime) >= 0) {

            // prefix has expired go back to solicit mode
            
            // delete existing prefix
            DHCPV6_IA_PREFIX *pPrefix = &pDhcpV6Adapt->pPdOption->IAPrefix;

            DHCPv6ManagePrefix(
                pDhcpV6Adapt->dwIPv6IfIndex,
                pPrefix->PrefixAddr,
                pPrefix->cPrefix,
                0, 0);
            DHCPv6ManagePrefixPeriodicCleanup();

            FreeDHCPV6Mem(pDhcpV6Adapt->pPdOption);
            pDhcpV6Adapt->pPdOption = NULL;


            DeleteRegistrySettings(pDhcpV6Adapt, DEL_REG_ALL);

            SetInitialTimeout(pDhcpV6Adapt, DHCPV6_SOL_TIMEOUT, 
                DHCPV6_SOL_MAX_RT, 0);

            dwError = DhcpV6EventAddEvent(gpDhcpV6EventModule,
                pDhcpV6Adapt->dwIPv6IfIndex, DHCPV6MessageMgrSolicitCallback,
                pDhcpV6Adapt, NULL);

            ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

            ASSERT(! dwError);
            BAIL_ON_WIN32_ERROR(dwError);
            break;
        }

        // check T2 time first--it shouldn't normally expire before T1
        if (TimerExpired(pDhcpV6Adapt, pDhcpV6Adapt->pPdOption->T2) >= 0) {
            if (dhcpv6_state_T2 != pDhcpV6Adapt->DhcpV6State) {
                SetInitialTimeout(pDhcpV6Adapt, DHCPV6_REB_TIMEOUT, 
                    DHCPV6_REB_MAX_RT, 0);
            }
            pDhcpV6Adapt->DhcpV6State = dhcpv6_state_T2;
        } else if (TimerExpired(pDhcpV6Adapt, pDhcpV6Adapt->pPdOption->T1) 
            >= 0) {
            if (dhcpv6_state_T1 != pDhcpV6Adapt->DhcpV6State) {
                SetInitialTimeout(pDhcpV6Adapt, DHCPV6_REN_TIMEOUT, 
                    DHCPV6_REN_MAX_RT, 0);
            }
            pDhcpV6Adapt->DhcpV6State = dhcpv6_state_T1;
        } else {
            ULONG       ReXmitTime, MaxTime;
            FILETIME    CurTime, ElapsedTime;
            // strange why did we go off before T1 or T2 expired?

            MaxTime = pDhcpV6Adapt->pPdOption->T1 * SEC_TO_MS;

            // find new time to fire!
            GetCurrentFT (&CurTime);
            if (0 > CompareFileTime(&CurTime, 
                &pDhcpV6Adapt->pPdOption->IAPrefix.IALeaseObtained)) {

                DEBUGMSG(ZONE_WARN, 
                    (TEXT("!DhcpV6: CurTime < LeaseObtained Time!\r\n")));
                ReXmitTime = MaxTime;

            } else {
                sub64_64_64(&CurTime,
                    &pDhcpV6Adapt->pPdOption->IAPrefix.IALeaseObtained, 
                    &ElapsedTime);
                div64_32_64(&ElapsedTime, NANO100_TO_MS, &ElapsedTime);

                if (ElapsedTime.dwHighDateTime)
                    ReXmitTime = 1;
                else if (ElapsedTime.dwLowDateTime >= MaxTime)
                    ReXmitTime = 1;
                else
                    ReXmitTime = MaxTime - ElapsedTime.dwLowDateTime;
            }

            // if time left is still greater than half T1 time, assume that 
            // someone changed the system time & go back to solicit mode
            if (ReXmitTime >= (MaxTime / 2)) {
                DHCPV6_IA_PREFIX *pPrefix = &pDhcpV6Adapt->pPdOption->IAPrefix;

                DEBUGMSG(ZONE_WARN, 
                    (TEXT("!DhcpV6: Timer went off and ReXmitTime > T1 / 2\r\n")));

                // delete existing prefix
                DHCPv6ManagePrefix(
                    pDhcpV6Adapt->dwIPv6IfIndex,
                    pPrefix->PrefixAddr,
                    pPrefix->cPrefix,
                    0, 0);
                DHCPv6ManagePrefixPeriodicCleanup();

                DeleteRegistrySettings(pDhcpV6Adapt, DEL_REG_ALL);

                FreeDHCPV6Mem(pDhcpV6Adapt->pPdOption);
                pDhcpV6Adapt->pPdOption = NULL;

                // go back to solicit mode
                SetInitialTimeout(pDhcpV6Adapt, DHCPV6_SOL_TIMEOUT, 
                    DHCPV6_SOL_MAX_RT, 0);

                dwError = DhcpV6EventAddEvent(gpDhcpV6EventModule,
                    pDhcpV6Adapt->dwIPv6IfIndex,
                    DHCPV6MessageMgrSolicitCallback, pDhcpV6Adapt, NULL);

                ASSERT(! dwError);

            } else {
                dwError = DhcpV6TimerSetTimer(gpDhcpV6TimerModule, 
                    pDhcpV6Adapt, DHCPV6MessageMgrTimerCallbackRoutine,
                    pDhcpV6Adapt, ReXmitTime);

                ASSERT(! dwError);
            }

            ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
            BAIL_ON_WIN32_ERROR(dwError);
            break;
            // don't call the add event function below.

        }
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        dwError = DhcpV6EventAddEvent(
                        gpDhcpV6EventModule,
                        pDhcpV6Adapt->dwIPv6IfIndex,
                        DHCPV6MessageMgrTCallback,
                        pDhcpV6Adapt,
                        NULL
                        );
        
        BAIL_ON_WIN32_ERROR(dwError);
        break;

    case dhcpv6_state_deinit:
        DhcpV6Trace(DHCPV6_MISC, DHCPV6_LOG_LEVEL_WARN, ("Timer Callback: DeInitializing on Adapt: %d with RefCount: %d", pDhcpV6Adapt->dwIPv6IfIndex, pDhcpV6Adapt->uRefCount));
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
        break;

    default:
        ASSERT(0);
    }

success:

    DhcpV6Trace(DHCPV6_MISC, DHCPV6_LOG_LEVEL_TRACE, ("End Timer Callback"));

    return;

error:
    DhcpV6Trace(DHCPV6_MISC, DHCPV6_LOG_LEVEL_ERROR, ("ERROR Timer Callback Fired for Adapt: %d with Error: %!status!", pDhcpV6Adapt->dwIPv6IfIndex, dwError));

    DereferenceDHCPV6Adapt(pDhcpV6Adapt);

    return;
}


#ifdef UNDER_CE


void SetInitialTimeout(PDHCPV6_ADAPT pDhcpV6Adapt, ULONG Time, ULONG MaxTime, 
    ULONG MaxReXmits) {
    DOUBLE  dbRand;

    dbRand = DhcpV6UniformRandom();
    pDhcpV6Adapt->uRetransmissionTimeout = (Time * SEC_TO_MS) + 
        (ULONG)(Time * SEC_TO_MS * dbRand);
    
    pDhcpV6Adapt->uMaxRetransmissionTimeout = MaxTime * SEC_TO_MS;

    pDhcpV6Adapt->uMaxReXmits = MaxReXmits;
    pDhcpV6Adapt->uReXmits = 0;

}   // CalculateRexmitTimeout()


void CalculateRexmitTimeout(PDHCPV6_ADAPT pDhcpV6Adapt) {
    DOUBLE dbRand;
    
    dbRand = DhcpV6UniformRandom();
    pDhcpV6Adapt->uRetransmissionTimeout = 
        2 * pDhcpV6Adapt->uRetransmissionTimeout + 
        (ULONG)(dbRand * pDhcpV6Adapt->uRetransmissionTimeout);
    
    if (pDhcpV6Adapt->uRetransmissionTimeout > 
        pDhcpV6Adapt->uMaxRetransmissionTimeout) {
        
        pDhcpV6Adapt->uRetransmissionTimeout = 
            pDhcpV6Adapt->uMaxRetransmissionTimeout + 
            (ULONG)(dbRand * pDhcpV6Adapt->uMaxRetransmissionTimeout);
    }
}   // CalculateRexmitTimeout()


// takes MaxTime & Time in MS and returns appropriate max time to wait in MS
DWORD FindMaxTimeout(PDHCPV6_ADAPT pDhcpV6Adapt, DWORD MaxTime, 
    DWORD Time) {
    DWORD   Ret;

    // for expired times we return 1 instead of 0.

    if (pDhcpV6Adapt->pPdOption) {
        FILETIME    CurTime, ElapsedTime;

        GetCurrentFT (&CurTime);
#if 1
        if (0 > CompareFileTime(&CurTime, 
            &pDhcpV6Adapt->pPdOption->IAPrefix.IALeaseObtained)) {
            // something is wrong!
            DEBUGMSG(ZONE_WARN, 
                (TEXT("DhcpV6: FindMaxTimeout: CurTime < LeaseObtained Time!\r\n")));
            return 1;
        }
#endif
        sub64_64_64(&CurTime,
            &pDhcpV6Adapt->pPdOption->IAPrefix.IALeaseObtained, &ElapsedTime);
        div64_32_64(&ElapsedTime, NANO100_TO_MS, &ElapsedTime);
        ASSERT(0 == ElapsedTime.dwHighDateTime);

        if (ElapsedTime.dwHighDateTime)
            Ret = 1;
        else if (ElapsedTime.dwLowDateTime >= MaxTime)
            Ret = 1;
        else if (ElapsedTime.dwLowDateTime + Time > MaxTime) {
            Ret = MaxTime - ElapsedTime.dwLowDateTime;
        } else
            Ret = Time;

    } else {
        Ret = Time;
    }

    return Ret;

}

static HRESULT
GetInfoRequestLength(UINT cPhysicalAddr, 
                         UINT usOptionLength, 
                         BOOL bUseIAprefixOption,
                         UINT cServerID,
                         UINT * pLength)
{
    //Helper to do the following unsing intsafe functions for non constants
    //   uInfoRequestLength = sizeof(DHCPV6_MESSAGE_HEADER) +    // Fixed size header
    //                        (sizeof(DHCPV6_OPTION_HEADER) * 4) + // ClientID, ORO, IA_PD, elapsed t
    //                        (4 + pDhcpV6Adapt->cPhysicalAddr) + // DUID (ClientID) length
    //                        usOptionLength + IA_PD_OPTION_LEN + 2;  // ORO + IA_PD + elapsed time
    //   if bUseIAprefixOption
    //     uInfoRequestLength += sizeof(DHCPV6_OPTION_HEADER) + IA_PREFIX_OPTION_LEN;
    //   and if cServerID > 0 
    //        uInfoRequestLength += sizeof(DHCPV6_OPTION_HEADER) + 
    //        pDhcpV6Adapt->cServerID;



    
    HRESULT Res1, Res2 = S_OK;
    UINT TempRes = sizeof(DHCPV6_MESSAGE_HEADER) +    // Fixed size header
                   (sizeof(DHCPV6_OPTION_HEADER) * 4) + // ClientID, ORO, IA_PD, elapsed t
                   4 + IA_PD_OPTION_LEN + 2;

    if(bUseIAprefixOption) {
        TempRes += sizeof(DHCPV6_OPTION_HEADER) + IA_PREFIX_OPTION_LEN;
    }

    Res1 = CeUIntAdd3(TempRes, usOptionLength,
                      cPhysicalAddr, &TempRes);
    
    if(cServerID > 0) {
        Res2 = CeUIntAdd3(sizeof(DHCPV6_OPTION_HEADER), cServerID, 
                          TempRes, &TempRes);
    }

    if(S_OK == Res1 && S_OK == Res2) {
        *pLength = TempRes;
        return S_OK;
    }
    else{
        return INTSAFE_E_ARITHMETIC_OVERFLOW;
    }
}

//
//  Lock: None
//
DWORD
DHCPV6MessageMgrSolicitMessage(
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD dwError = 0;
    ULONG uInfoRequestLength = 0;
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule = &pDhcpV6Adapt->DhcpV6OptionModule;
    PDHCPV6_MESSAGE_HEADER pDhcpV6MessageHeader = NULL;
    PUCHAR pucOptionHeader = NULL;
    USHORT usOptionLength = 0;
    WSABUF WSABuf = { 0 };
    ULONG uByteSent = 0;

    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_TRACE, ("Begin Information Request on Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    AcquireExclusiveLock(&pDhcpV6Adapt->RWLock);
    if (pDhcpV6Adapt->DhcpV6State == dhcpv6_state_deinit) {
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
        return 0;
    }

    if (DHCPV6_MEDIA_DISC_FL & pDhcpV6Adapt->Flags) {
        goto UpdateTime;
    }

    // options: ClientID, ORO (Option-Request-Option), IA_PD, elapsed time
   
    // ORO options: DNS_SERVERS, IA_PD
    usOptionLength = (USHORT)(2 * sizeof(USHORT));
    //usOptionLength = (USHORT)(pDhcpV6OptionModule->uNumOfOptionsEnabled * sizeof(USHORT));

    if(S_OK != GetInfoRequestLength(pDhcpV6Adapt->cPhysicalAddr, 
                                        usOptionLength, FALSE, 0,
                                        &uInfoRequestLength)) {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_LOCK_ERROR(dwError);
    }

    pDhcpV6MessageHeader = AllocDHCPV6Mem(uInfoRequestLength);
    if (!pDhcpV6MessageHeader) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_LOCK_ERROR(dwError);
    }
    memset(pDhcpV6MessageHeader, 0, uInfoRequestLength);

    pDhcpV6MessageHeader->MessageType = DHCPV6_MESSAGE_TYPE_SOLICIT;

    dwError = DhcpV6GenerateRandom(pDhcpV6Adapt->ucTransactionID, sizeof(pDhcpV6Adapt->ucTransactionID));
    BAIL_ON_LOCK_ERROR(dwError);

    memcpy(
        pDhcpV6MessageHeader->TransactionID,
        pDhcpV6Adapt->ucTransactionID,
        sizeof(pDhcpV6MessageHeader->TransactionID)
        );

    pucOptionHeader = (PUCHAR)(&pDhcpV6MessageHeader[1]);

    pDhcpV6Adapt->DhcpV6State = dhcpv6_state_solicit;

    dwError = DhcpV6OptionMgrCreateOptionRequestPD(
                    pDhcpV6Adapt,
                    pDhcpV6OptionModule,
                    pucOptionHeader,
                    sizeof(DHCPV6_OPTION_HEADER) + usOptionLength
                    );
    BAIL_ON_LOCK_ERROR(dwError);

    DEBUGMSG(1, (TEXT("DhcpV6: Sending Solicit message\r\n")));

    WSABuf.len = uInfoRequestLength;
    WSABuf.buf = (PUCHAR)pDhcpV6MessageHeader;
    dwError = WSASendTo(
                pDhcpV6Adapt->Socket,
                &WSABuf,
                1,
                &uByteSent,
                0,
                (LPSOCKADDR)&MCastSockAddr,
                sizeof(MCastSockAddr),
                NULL,
                NULL
                );
    BAIL_ON_LOCK_ERROR(dwError);

UpdateTime:
    dwError = DhcpV6TimerSetTimer(
                gpDhcpV6TimerModule,
                pDhcpV6Adapt,
                DHCPV6MessageMgrTimerCallbackRoutine,
                pDhcpV6Adapt,
                pDhcpV6Adapt->uRetransmissionTimeout
                );
    BAIL_ON_LOCK_ERROR(dwError);

    CalculateRexmitTimeout(pDhcpV6Adapt);

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

success:

    if (pDhcpV6MessageHeader) {
        FreeDHCPV6Mem(pDhcpV6MessageHeader);
    }

    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_TRACE, ("End Information Request"));

    return dwError;

lock:

    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Information Request with Error: %!status!", dwError));

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
    DereferenceDHCPV6Adapt(pDhcpV6Adapt);

    goto success;
}



//
//  Lock: None
//
DWORD
DHCPV6MessageMgrRequestMessage(
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD dwError = 0;
    ULONG uInfoRequestLength = 0;
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule = &pDhcpV6Adapt->DhcpV6OptionModule;
    PDHCPV6_MESSAGE_HEADER pDhcpV6MessageHeader = NULL;
    PUCHAR pucOptionHeader = NULL;
    USHORT usOptionLength = 0;
    WSABUF WSABuf = { 0 };
    ULONG uByteSent = 0;


    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_TRACE, ("Begin Information Request on Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    AcquireExclusiveLock(&pDhcpV6Adapt->RWLock);
    if (pDhcpV6Adapt->DhcpV6State == dhcpv6_state_deinit) {
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
        return 0;
    }

    if (DHCPV6_MEDIA_DISC_FL & pDhcpV6Adapt->Flags) {
        goto UpdateTime;
    }

    // options: ClientID, ORO (Option-Request-Option), IA_PD, elapsed time
   
    // ORO options: DNS_SERVERS, IA_PD
    usOptionLength = (USHORT)(2 * sizeof(USHORT));
    //usOptionLength = (USHORT)(pDhcpV6OptionModule->uNumOfOptionsEnabled * sizeof(USHORT));

    if(S_OK != GetInfoRequestLength(pDhcpV6Adapt->cPhysicalAddr, 
                                    usOptionLength, TRUE,
                                     (pDhcpV6Adapt->pServerID ? pDhcpV6Adapt->cServerID : 0),
                                    &uInfoRequestLength)) {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_LOCK_ERROR(dwError);

    }

    pDhcpV6MessageHeader = AllocDHCPV6Mem(uInfoRequestLength);
    if (!pDhcpV6MessageHeader) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_LOCK_ERROR(dwError);
    }
    memset(pDhcpV6MessageHeader, 0, uInfoRequestLength);

    pDhcpV6MessageHeader->MessageType = DHCPV6_MESSAGE_TYPE_REQUEST;

    dwError = DhcpV6GenerateRandom(pDhcpV6Adapt->ucTransactionID, sizeof(pDhcpV6Adapt->ucTransactionID));
    BAIL_ON_LOCK_ERROR(dwError);

    memcpy(
        pDhcpV6MessageHeader->TransactionID,
        pDhcpV6Adapt->ucTransactionID,
        sizeof(pDhcpV6MessageHeader->TransactionID)
        );

    pucOptionHeader = (PUCHAR)(&pDhcpV6MessageHeader[1]);

    pDhcpV6Adapt->DhcpV6State = dhcpv6_state_srequest;
    
    dwError = DhcpV6OptionMgrCreateOptionRequestPD(
                    pDhcpV6Adapt,
                    pDhcpV6OptionModule,
                    pucOptionHeader,
                    sizeof(DHCPV6_OPTION_HEADER) + usOptionLength
                    );
    BAIL_ON_LOCK_ERROR(dwError);

    DEBUGMSG(1, (TEXT("DhcpV6: Sending Request message\r\n")));

    WSABuf.len = uInfoRequestLength;
    WSABuf.buf = (PUCHAR)pDhcpV6MessageHeader;
    dwError = WSASendTo(
                pDhcpV6Adapt->Socket,
                &WSABuf,
                1,
                &uByteSent,
                0,
                (LPSOCKADDR)&MCastSockAddr,
                sizeof(MCastSockAddr),
                NULL,
                NULL
                );
    BAIL_ON_LOCK_ERROR(dwError);

UpdateTime:
    pDhcpV6Adapt->uReXmits++;
    
    dwError = DhcpV6TimerSetTimer(
                gpDhcpV6TimerModule,
                pDhcpV6Adapt,
                DHCPV6MessageMgrTimerCallbackRoutine,
                pDhcpV6Adapt,
                pDhcpV6Adapt->uRetransmissionTimeout
                );
    BAIL_ON_LOCK_ERROR(dwError);

    CalculateRexmitTimeout(pDhcpV6Adapt);

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

success:

    if (pDhcpV6MessageHeader) {
        FreeDHCPV6Mem(pDhcpV6MessageHeader);
    }

    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_TRACE, ("End Information Request"));

    return dwError;

lock:

    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Information Request with Error: %!status!", dwError));

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
    DereferenceDHCPV6Adapt(pDhcpV6Adapt);

    goto success;
}



//
//  Lock: None
//
DWORD
DHCPV6MessageMgrRenewMessage(
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD   dwError = 0;
    ULONG   uInfoRequestLength = 0;
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule = &pDhcpV6Adapt->DhcpV6OptionModule;
    PDHCPV6_MESSAGE_HEADER pDhcpV6MessageHeader = NULL;
    PUCHAR  pucOptionHeader = NULL;
    USHORT  usOptionLength = 0;
    WSABUF  WSABuf = { 0 };
    ULONG   uByteSent = 0;
    ULONG   ReXmitTime; 


    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_TRACE, ("Begin Information Request on Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    AcquireExclusiveLock(&pDhcpV6Adapt->RWLock);
    if (pDhcpV6Adapt->DhcpV6State == dhcpv6_state_deinit) {
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
        return 0;
    }

    if (DHCPV6_MEDIA_DISC_FL & pDhcpV6Adapt->Flags) {
        goto UpdateTime;
    }

    // options: ClientID, ORO (Option-Request-Option), IA_PD, elapsed time
   
    // ORO options: DNS_SERVERS, IA_PD
    usOptionLength = (USHORT)(2 * sizeof(USHORT));
    //usOptionLength = (USHORT)(pDhcpV6OptionModule->uNumOfOptionsEnabled * sizeof(USHORT));


    if(S_OK != GetInfoRequestLength(pDhcpV6Adapt->cPhysicalAddr, 
                                    usOptionLength, TRUE,
                                     (pDhcpV6Adapt->pServerID ? pDhcpV6Adapt->cServerID : 0),
                                    &uInfoRequestLength)) {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_LOCK_ERROR(dwError);
    }

    pDhcpV6MessageHeader = AllocDHCPV6Mem(uInfoRequestLength);
    if (!pDhcpV6MessageHeader) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_LOCK_ERROR(dwError);
    }
    memset(pDhcpV6MessageHeader, 0, uInfoRequestLength);

    pDhcpV6MessageHeader->MessageType = DHCPV6_MESSAGE_TYPE_RENEW;

    dwError = DhcpV6GenerateRandom(pDhcpV6Adapt->ucTransactionID, sizeof(pDhcpV6Adapt->ucTransactionID));
    BAIL_ON_LOCK_ERROR(dwError);

    memcpy(
        pDhcpV6MessageHeader->TransactionID,
        pDhcpV6Adapt->ucTransactionID,
        sizeof(pDhcpV6MessageHeader->TransactionID)
        );

    pucOptionHeader = (PUCHAR)(&pDhcpV6MessageHeader[1]);

    pDhcpV6Adapt->DhcpV6State = dhcpv6_state_T1;
    
    dwError = DhcpV6OptionMgrCreateOptionRequestPD(
                    pDhcpV6Adapt,
                    pDhcpV6OptionModule,
                    pucOptionHeader,
                    sizeof(DHCPV6_OPTION_HEADER) + usOptionLength
                    );
    BAIL_ON_LOCK_ERROR(dwError);

    DEBUGMSG(1, (TEXT("DhcpV6: Sending Renew message\r\n")));

    WSABuf.len = uInfoRequestLength;
    WSABuf.buf = (PUCHAR)pDhcpV6MessageHeader;
    dwError = WSASendTo(
                pDhcpV6Adapt->Socket,
                &WSABuf,
                1,
                &uByteSent,
                0,
                (LPSOCKADDR)&MCastSockAddr,
                sizeof(MCastSockAddr),
                NULL,
                NULL
                );
    BAIL_ON_LOCK_ERROR(dwError);

UpdateTime:
    if (pDhcpV6Adapt->pPdOption)
        ReXmitTime = FindMaxTimeout(pDhcpV6Adapt, 
            pDhcpV6Adapt->pPdOption->T2 * SEC_TO_MS,
            pDhcpV6Adapt->uRetransmissionTimeout);
    else
        ReXmitTime = pDhcpV6Adapt->uRetransmissionTimeout;

    dwError = DhcpV6TimerSetTimer(
                gpDhcpV6TimerModule,
                pDhcpV6Adapt,
                DHCPV6MessageMgrTimerCallbackRoutine,
                pDhcpV6Adapt,
                ReXmitTime
                );
    BAIL_ON_LOCK_ERROR(dwError);

    CalculateRexmitTimeout(pDhcpV6Adapt);

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

success:

    if (pDhcpV6MessageHeader) {
        FreeDHCPV6Mem(pDhcpV6MessageHeader);
    }

    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_TRACE, ("End Information Request"));

    return dwError;

lock:

    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Information Request with Error: %!status!", dwError));

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
    DereferenceDHCPV6Adapt(pDhcpV6Adapt);

    goto success;
}


//
//  Lock: None
//
DWORD
DHCPV6MessageMgrRebindMessage(
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD dwError = 0;
    ULONG uInfoRequestLength = 0;
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule = &pDhcpV6Adapt->DhcpV6OptionModule;
    PDHCPV6_MESSAGE_HEADER pDhcpV6MessageHeader = NULL;
    PUCHAR pucOptionHeader = NULL;
    USHORT usOptionLength = 0;
    WSABUF WSABuf = { 0 };
    ULONG uByteSent = 0;
    ULONG   ReXmitTime;


    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_TRACE, ("Begin Information Request on Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    AcquireExclusiveLock(&pDhcpV6Adapt->RWLock);
    if (pDhcpV6Adapt->DhcpV6State == dhcpv6_state_deinit) {
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
        return 0;
    }

    if (DHCPV6_MEDIA_DISC_FL & pDhcpV6Adapt->Flags) {
        goto UpdateTime;
    }

    // options: ClientID, ORO (Option-Request-Option), IA_PD, elapsed time
   
    // ORO options: DNS_SERVERS, IA_PD
    usOptionLength = (USHORT)(2 * sizeof(USHORT));
    //usOptionLength = (USHORT)(pDhcpV6OptionModule->uNumOfOptionsEnabled * sizeof(USHORT));
    

    if(S_OK != GetInfoRequestLength(pDhcpV6Adapt->cPhysicalAddr, 
                                    usOptionLength, TRUE, 0,
                                    &uInfoRequestLength)) {
        dwError = ERROR_INVALID_PARAMETER;
        BAIL_ON_LOCK_ERROR(dwError);

    }
    
    pDhcpV6MessageHeader = AllocDHCPV6Mem(uInfoRequestLength);
    if (!pDhcpV6MessageHeader) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_LOCK_ERROR(dwError);
    }
    memset(pDhcpV6MessageHeader, 0, uInfoRequestLength);

    pDhcpV6MessageHeader->MessageType = DHCPV6_MESSAGE_TYPE_REBIND;

    dwError = DhcpV6GenerateRandom(pDhcpV6Adapt->ucTransactionID, sizeof(pDhcpV6Adapt->ucTransactionID));
    BAIL_ON_LOCK_ERROR(dwError);

    memcpy(
        pDhcpV6MessageHeader->TransactionID,
        pDhcpV6Adapt->ucTransactionID,
        sizeof(pDhcpV6MessageHeader->TransactionID)
        );

    pucOptionHeader = (PUCHAR)(&pDhcpV6MessageHeader[1]);

    if (dhcpv6_state_rebindconfirm != pDhcpV6Adapt->DhcpV6State)
        pDhcpV6Adapt->DhcpV6State = dhcpv6_state_T2;

    dwError = DhcpV6OptionMgrCreateOptionRequestPD(
                    pDhcpV6Adapt,
                    pDhcpV6OptionModule,
                    pucOptionHeader,
                    sizeof(DHCPV6_OPTION_HEADER) + usOptionLength
                    );
    BAIL_ON_LOCK_ERROR(dwError);

    DEBUGMSG(1, (TEXT("DhcpV6: Sending Rebind message\r\n")));

    WSABuf.len = uInfoRequestLength;
    WSABuf.buf = (PUCHAR)pDhcpV6MessageHeader;
    dwError = WSASendTo(
                pDhcpV6Adapt->Socket,
                &WSABuf,
                1,
                &uByteSent,
                0,
                (LPSOCKADDR)&MCastSockAddr,
                sizeof(MCastSockAddr),
                NULL,
                NULL
                );
    BAIL_ON_LOCK_ERROR(dwError);

UpdateTime:
    if (pDhcpV6Adapt->pPdOption)
        ReXmitTime = FindMaxTimeout(pDhcpV6Adapt, 
            pDhcpV6Adapt->pPdOption->IAPrefix.ValidLifetime * SEC_TO_MS,
            pDhcpV6Adapt->uRetransmissionTimeout);
    else
        ReXmitTime = pDhcpV6Adapt->uRetransmissionTimeout;

    if ((dhcpv6_state_rebindconfirm == pDhcpV6Adapt->DhcpV6State) &&
        (ReXmitTime > pDhcpV6Adapt->uRetransmissionTimeout)) {
        ReXmitTime = pDhcpV6Adapt->uRetransmissionTimeout;
    }

    dwError = DhcpV6TimerSetTimer(
                gpDhcpV6TimerModule,
                pDhcpV6Adapt,
                DHCPV6MessageMgrTimerCallbackRoutine,
                pDhcpV6Adapt,
                ReXmitTime
                );
    BAIL_ON_LOCK_ERROR(dwError);

    CalculateRexmitTimeout(pDhcpV6Adapt);

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

success:

    if (pDhcpV6MessageHeader) {
        FreeDHCPV6Mem(pDhcpV6MessageHeader);
    }

    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_TRACE, ("End Information Request"));

    return dwError;

lock:

    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Information Request with Error: %!status!", dwError));

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
    DereferenceDHCPV6Adapt(pDhcpV6Adapt);

    goto success;
}


#endif  // UNDER_CE

//
//  Lock: None
//
DWORD
DHCPV6MessageMgrInfoRequest(
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD dwError = 0;
    ULONG uInfoRequestLength = 0;
    PDHCPV6_OPTION_MODULE pDhcpV6OptionModule = &pDhcpV6Adapt->DhcpV6OptionModule;
    PDHCPV6_MESSAGE_HEADER pDhcpV6MessageHeader = NULL;
    PUCHAR pucOptionHeader = NULL;
    USHORT usOptionLength = 0;
    WSABUF WSABuf = { 0 };
    ULONG uByteSent = 0;


    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_TRACE, ("Begin Information Request on Adapt: %d", pDhcpV6Adapt->dwIPv6IfIndex));

    AcquireExclusiveLock(&pDhcpV6Adapt->RWLock);
    if (pDhcpV6Adapt->DhcpV6State == dhcpv6_state_deinit) {
        ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
        return 0;
    }

    if (DHCPV6_MEDIA_DISC_FL & pDhcpV6Adapt->Flags) {
        goto UpdateTime;
    }

    // ORO options: DNS_SERVERS, IA_PD
    usOptionLength = (USHORT)(2 * sizeof(USHORT));
    //usOptionLength = (USHORT)(pDhcpV6OptionModule->uNumOfOptionsEnabled * sizeof(USHORT));

    uInfoRequestLength = sizeof(DHCPV6_MESSAGE_HEADER) +    // Fixed size header
                        sizeof(DHCPV6_OPTION_HEADER) * 4 +     // ElapsedTime, ClientId, VendorClass,  ORO
                        4 + pDhcpV6Adapt->cPhysicalAddr + 4 + 14 +
                        usOptionLength + 2;

    pDhcpV6MessageHeader = AllocDHCPV6Mem(uInfoRequestLength);
    if (!pDhcpV6MessageHeader) {
        dwError = ERROR_OUTOFMEMORY;
        BAIL_ON_LOCK_ERROR(dwError);
    }
    memset(pDhcpV6MessageHeader, 0, uInfoRequestLength);

    pDhcpV6MessageHeader->MessageType = DHCPV6_MESSAGE_TYPE_INFORMATION_REQUEST;

    dwError = DhcpV6GenerateRandom(pDhcpV6Adapt->ucTransactionID, sizeof(pDhcpV6Adapt->ucTransactionID));
    BAIL_ON_LOCK_ERROR(dwError);

    memcpy(
        pDhcpV6MessageHeader->TransactionID,
        pDhcpV6Adapt->ucTransactionID,
        sizeof(pDhcpV6MessageHeader->TransactionID)
        );

    pucOptionHeader = (PUCHAR)(&pDhcpV6MessageHeader[1]);
    dwError = DhcpV6OptionMgrCreateOptionRequest(
                    pDhcpV6Adapt,
                    pDhcpV6OptionModule,
                    pucOptionHeader,
                    sizeof(DHCPV6_OPTION_HEADER) + usOptionLength
                    );
    BAIL_ON_LOCK_ERROR(dwError);

    pDhcpV6Adapt->DhcpV6State = dhcpv6_state_request;

    DEBUGMSG(1, (TEXT("DhcpV6: Sending Info Request message\r\n")));

    WSABuf.len = uInfoRequestLength;
    WSABuf.buf = (PUCHAR)pDhcpV6MessageHeader;
    dwError = WSASendTo(
                pDhcpV6Adapt->Socket,
                &WSABuf,
                1,
                &uByteSent,
                0,
                (LPSOCKADDR)&MCastSockAddr,
                sizeof(MCastSockAddr),
                NULL,
                NULL
                );
    BAIL_ON_LOCK_ERROR(dwError);

    dwError = DhcpV6TimerSetTimer(
                gpDhcpV6TimerModule,
                pDhcpV6Adapt,
                DHCPV6MessageMgrTimerCallbackRoutine,
                pDhcpV6Adapt,
                pDhcpV6Adapt->uRetransmissionTimeout
                );
    BAIL_ON_LOCK_ERROR(dwError);

UpdateTime:

    CalculateRexmitTimeout(pDhcpV6Adapt);

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

success:

    if (pDhcpV6MessageHeader) {
        FreeDHCPV6Mem(pDhcpV6MessageHeader);
    }

    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_TRACE, ("End Information Request"));

    return dwError;

lock:

    DhcpV6Trace(DHCPV6_SEND, DHCPV6_LOG_LEVEL_ERROR, ("FAILED Information Request with Error: %!status!", dwError));

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);
    DereferenceDHCPV6Adapt(pDhcpV6Adapt);

    goto success;
}


DWORD
InitDHCPV6MessageMgr(
    PDHCPV6_ADAPT pDhcpV6Adapt          // Should be ref-counted already by caller
    )
{
    DWORD dwError = 0;

    if (gbDHCPV6PDEnabled && (DHCPV6_PD_ENABLED_FL & pDhcpV6Adapt->Flags))
        dwError = DHCPV6MessageMgrSolicitMessage(pDhcpV6Adapt);
    else
        dwError = DHCPV6MessageMgrInfoRequest(pDhcpV6Adapt);
    BAIL_ON_WIN32_ERROR(dwError);

error:
    return dwError;
}


DWORD
DHCPV6MessageMgrPerformRefresh(
    PDHCPV6_ADAPT pDhcpV6Adapt
    )
{
    DWORD   dwError = 0;
    BOOL    bAdaptReference = FALSE;
    BOOL    bUsePD;
    CHAR    szAdapterName[DHCPV6_MAX_NAME_SIZE];


    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_TRACE, ("Begin Performing Refresh on Adapt: %d with Ref Count: %d", pDhcpV6Adapt->dwIPv6IfIndex, pDhcpV6Adapt->uRefCount));

    AcquireExclusiveLock(&pDhcpV6Adapt->RWLock);
    if (pDhcpV6Adapt->DhcpV6State == dhcpv6_state_deinit) {
        dwError = ERROR_NOT_READY;
        DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_WARN, ("WARN: Cannot refresh on Adapt: %d with Ref Count: %d since not in deinit state", pDhcpV6Adapt->dwIPv6IfIndex, pDhcpV6Adapt->uRefCount));
        BAIL_ON_LOCK_ERROR(dwError);
    }

    
#ifdef UNDER_CE
    wcstombs(szAdapterName, pDhcpV6Adapt->wszAdapterName,DHCPV6_MAX_NAME_SIZE);
    if (IsPDEnabledInterface(szAdapterName, NULL)) {
        pDhcpV6Adapt->Flags |= DHCPV6_PD_ENABLED_FL;
        bUsePD = TRUE;
    } else {
        pDhcpV6Adapt->Flags &= ~DHCPV6_PD_ENABLED_FL;
        bUsePD = FALSE;
    }

    if (! gbDHCPV6PDEnabled)
        bUsePD = FALSE;
    
#endif

    if (bUsePD) {

        if (pDhcpV6Adapt->pPdOption) {
            // set timeouts for confirm message
            SetInitialTimeout(pDhcpV6Adapt, DHCPV6_CNF_TIMEOUT, 
                DHCPV6_CNF_MAX_RT, 0);

            // goto rebind state.
            pDhcpV6Adapt->DhcpV6State = dhcpv6_state_rebindconfirm;

            // we use tickcounts here instead of the filetime
            // a little less hassle
            pDhcpV6Adapt->StartRebindConfirm = GetTickCount();

            dwError = DhcpV6TimerCancel(gpDhcpV6TimerModule, pDhcpV6Adapt);
            ASSERT(! dwError);

            // set timer/event to do work
            dwError = DhcpV6EventAddEvent(gpDhcpV6EventModule,
                pDhcpV6Adapt->dwIPv6IfIndex, DHCPV6MessageMgrTCallback,
                pDhcpV6Adapt, NULL);
            
            ASSERT(! dwError);
        } else {

            // no previous PD, so go to solicit mode
            SetInitialTimeout(pDhcpV6Adapt, DHCPV6_SOL_TIMEOUT, 
                DHCPV6_SOL_MAX_RT, 0);

            dwError = DhcpV6EventAddEvent(gpDhcpV6EventModule,
                pDhcpV6Adapt->dwIPv6IfIndex, DHCPV6MessageMgrSolicitCallback,
                pDhcpV6Adapt, NULL);

            ASSERT(! dwError);
        }

    } else {

        SetInitialTimeout(pDhcpV6Adapt, DHCPV6_INF_TIMEOUT,
            DHCPV6_INF_MAX_RT, 0);

        if (pDhcpV6Adapt->DhcpV6State == dhcpv6_state_request) {

            DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_INFO, ("Adapter in already in request state - fire timer now"));

            dwError = DhcpV6TimerFireNow(gpDhcpV6TimerModule, pDhcpV6Adapt);
            BAIL_ON_LOCK_ERROR(dwError);

        } else {

            DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_INFO, ("Adapter in already in configured state - ReStarting Info Request"));

            pDhcpV6Adapt->DhcpV6State = dhcpv6_state_init;

            if (pDhcpV6Adapt->bEventTimerQueued) {
                dwError = DhcpV6TimerCancel(gpDhcpV6TimerModule, pDhcpV6Adapt);
                ASSERT(! dwError);
            } else {
                ReferenceDHCPV6Adapt(pDhcpV6Adapt);
                bAdaptReference = TRUE;
            }

            dwError = DhcpV6EventAddEvent(
                            gpDhcpV6EventModule,
                            pDhcpV6Adapt->dwIPv6IfIndex,
                            DHCPV6MessageMgrInfoReqCallback,
                            pDhcpV6Adapt,
                            NULL
                            );

            ASSERT(! dwError);
            BAIL_ON_LOCK_ERROR(dwError);

        }

    }

    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

    return dwError;

lock:
    ReleaseExclusiveLock(&pDhcpV6Adapt->RWLock);

    if (bAdaptReference) {
        DereferenceDHCPV6Adapt(pDhcpV6Adapt);
    }

    DhcpV6Trace(DHCPV6_IOCTL, DHCPV6_LOG_LEVEL_ERROR, ("End Performing Refresh with Error: %!status!", dwError));

    return dwError;
}

