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

