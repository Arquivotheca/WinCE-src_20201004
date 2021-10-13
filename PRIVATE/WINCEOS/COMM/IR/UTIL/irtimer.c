//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <irda.h>
#include <irlap.h>
#include <irlmp.h>

int         TimerInit;
CTEEvent    IrdaTimerExpEvent;

VOID
IrdaTimerExpFunc(struct CTEEvent *Event, void *Arg);


void
IrdaTimerInitialize(PIRDA_TIMER     pTimer,
                    VOID            (*ExpFunc)(PVOID Context),
                    UINT            Timeout,
                    PVOID           Context,
                    PIRDA_LINK_CB   pIrdaLinkCb)
{
    
#if DBG
    DEBUGMSG(DBG_TIMER, (TEXT("%hs timer initialized, context %x\n"),
                         pTimer->pName, pTimer));
#endif
    
    if (!TimerInit)
    {
        TimerInit = 1;
        CTEInitEvent(&IrdaTimerExpEvent, IrdaTimerExpFunc);
    }
    
    CTEInitTimer(&pTimer->CteTimer);
    pTimer->ExpFunc = ExpFunc;
    pTimer->Context = Context;
    pTimer->Timeout = Timeout;
    pTimer->pIrdaLinkCb = pIrdaLinkCb;
}

void
TimerFuncAtDpcLevel(CTEEvent *Event, void *Arg)
{
    PIRDA_TIMER pIrdaTimer = (PIRDA_TIMER) Arg;

#if DBG    
    DEBUGMSG(DBG_TIMER, (TEXT("%hs timer expired at DPC, context %x\n"),
                         pIrdaTimer->pName, pIrdaTimer));
#endif
    
#ifdef UNDER_CE
    // Should not get called.
    ASSERT(FALSE);
    IrdaTimerExpFunc(&IrdaTimerExpEvent, Arg);
#else
    CTEScheduleEvent(&IrdaTimerExpEvent, Arg);
#endif 
    
    return;
}

VOID
IrdaTimerExpFunc(struct CTEEvent *Event, void *Arg)
{
    PIRDA_TIMER     pIrdaTimer = (PIRDA_TIMER) Arg;
    // SH - get a pointer to the control block. For the response timer
    //      we may delete the LsapCb which deletes the response timer
    //      and then we try to access the timer. Instead we know that
    //      we maitain a reference to the IrdaLinkCb that we added
    //      in IrdaTimerStart.
    PIRDA_LINK_CB   pIrdaLinkCb = pIrdaTimer->pIrdaLinkCb;

#if DBG    
    DEBUGMSG(DBG_TIMER, (TEXT("%hs timer expired, context %x\n"),
              pIrdaTimer->pName, pIrdaTimer ));
#endif    

    if (pIrdaLinkCb)
        LOCK_LINK(pIrdaLinkCb);
            
    if (pIrdaTimer->Late != TRUE)    
        pIrdaTimer->ExpFunc(pIrdaTimer->Context);
    else
    {
        DEBUGMSG(DBG_WARN | DBG_TIMER,
             (TEXT("IRDA TIMER LATE, ignoring\r\n")));
        pIrdaTimer->Late = FALSE;
    }
   
    if (pIrdaLinkCb)
    {
        UNLOCK_LINK(pIrdaLinkCb);
        REFDEL(&pIrdaLinkCb->RefCnt,'RMIT');
    }
    
    return;
}

VOID
IrdaTimerStart(PIRDA_TIMER pIrdaTimer)
{
    PVOID pTimer;

    if (pIrdaTimer->pIrdaLinkCb)
        REFADD(&pIrdaTimer->pIrdaLinkCb->RefCnt, 'RMIT');
    
    pIrdaTimer->Late = FALSE;

    CTEStopTimer(&pIrdaTimer->CteTimer);
#ifdef UNDER_CE
    // sh - CE is not at DPC, go straight to ExpFunc instead of scheduling
    //      an event.
    pTimer = CTEStartTimer(&pIrdaTimer->CteTimer, pIrdaTimer->Timeout,
                  IrdaTimerExpFunc, (PVOID)pIrdaTimer);

#else
    pTimer = CTEStartTimer(&pIrdaTimer->CteTimer, pIrdaTimer->Timeout,
                  TimerFuncAtDpcLevel, (PVOID) pIrdaTimer);
#endif 

#if DBG    
    if (pTimer != NULL)
    {
        DEBUGMSG(DBG_TIMER,
             (TEXT("Start timer %hs (%dms) context %x\n"),
              pIrdaTimer->pName,
              pIrdaTimer->Timeout,
              pIrdaTimer));
    }
    else
    {
        DEBUGMSG(1, (TEXT("%hs timer FAILED to start\r\n"),
              pIrdaTimer->pName));
    }
#endif    
    return;
}

VOID
IrdaTimerStop(PIRDA_TIMER pIrdaTimer)
{
    if (pIrdaTimer->pIrdaLinkCb)
        LOCK_LINK(pIrdaTimer->pIrdaLinkCb);

    if (CTEStopTimer(&pIrdaTimer->CteTimer) == 0)
    {
        pIrdaTimer->Late = TRUE;
    }
    else
    {
        if (pIrdaTimer->pIrdaLinkCb)
            REFDEL(&pIrdaTimer->pIrdaLinkCb->RefCnt,'RMIT');    
    }
#if DBG    
    DEBUGMSG(DBG_TIMER, (TEXT("Timer %hs stopped, late %d\n"), pIrdaTimer->pName,
              pIrdaTimer->Late));
#endif

    if (pIrdaTimer->pIrdaLinkCb)
    {
        UNLOCK_LINK(pIrdaTimer->pIrdaLinkCb);
    }

    return;
}

VOID
IrdaTimerRestart(PIRDA_TIMER pIrdaTimer)
{
    IrdaTimerStop(pIrdaTimer);
    IrdaTimerStart(pIrdaTimer);
}
    
