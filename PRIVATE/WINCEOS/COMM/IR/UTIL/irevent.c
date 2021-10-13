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

VOID
IrdaEventCallback(struct CTEEvent *Event, PVOID Arg)
{
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) Arg;
    PIRDA_EVENT     pIrdaEvent = (PIRDA_EVENT) Event;
    
    if (pIrdaLinkCb)
        REFADD(&pIrdaLinkCb->RefCnt, 'TNVE');
    
    pIrdaEvent->Callback(pIrdaLinkCb);
    
    if (pIrdaLinkCb)
        REFDEL(&pIrdaLinkCb->RefCnt, 'TNVE');
}    

VOID
IrdaEventInitialize(PIRDA_EVENT pIrdaEvent,
                    VOID        (*Callback)(PVOID Context))
{
    CTEInitEvent(&pIrdaEvent->CteEvent, IrdaEventCallback);
    pIrdaEvent->Callback = Callback;
}   
    
VOID
IrdaEventSchedule(PIRDA_EVENT pIrdaEvent, PVOID Arg)
{
    PIRDA_LINK_CB pIrdaLinkCb = (PIRDA_LINK_CB) Arg;
        
    CTEStopTimer(&pIrdaEvent->CteEvent);
    if (CTEScheduleEvent(&pIrdaEvent->CteEvent, pIrdaLinkCb) == FALSE)
    {
        DEBUGMSG(1, (TEXT("CTEScheduleEvent failed\n")));
    //    ASSERT(0);
    }
}

