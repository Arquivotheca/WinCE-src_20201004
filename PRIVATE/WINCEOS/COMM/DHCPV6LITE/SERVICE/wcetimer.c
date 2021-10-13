//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "dhcpv6p.h"
#include "wcetim.h"


////////////////////////////////////////////////////////////////////////////////
//	CE_ScheduleWorkItem()
//
//	Routine Description:
//
//		This function uses cxport to schedule work item..
//
//	Arguments:
//		
//		None.
//
//	Return Value:
//
//		TRUE if successful, FALSE otherwise..
//
//	Note:
//
//		The called function ** MUST ** clear the CTE_EVENT_DATA struct 
//		allocated here..
//	

BOOL
CE_ScheduleWorkItem(
    CTEEventRtn pFunction,
    PVOID       pvArg)
{

    PCTE_EVENT_DATA     pCteEventData;

    pCteEventData = MemCAlloc(sizeof(CTE_EVENT_DATA));

    if (pCteEventData)
    {
        CTEInitEvent(
            &(pCteEventData->hCTEEvent), 
            pFunction);

        if (!CTEScheduleEvent(
                &(pCteEventData->hCTEEvent), 
                pvArg))
        {
            ASSERT(0);
        }

        return TRUE;
    }
    else
    {
        DEBUGMSG (ZONE_ERROR,
        (TEXT("ZCF:: Alloc() failed in CE_ScheduleWorkItem().\r\n")));

        return FALSE;
    }

}	//	CE_ScheduleWorkItem()



//////////////////////////////////////////////////////////////////////////////
//  CE emulation of TimerQueue 
///////////////////////////////////////////////////////////////////////////////


LIST_ENTRY          g_TimerQueueList;
CRITICAL_SECTION    g_csTimerQueueList;
HANDLE		        g_hDefaultTimerQueue = NULL;

#ifdef DEBUG
DWORD               g_dwTotalTQ     = 0x00;
DWORD               g_dwTotalTQT    = 0x00;
#endif


///////////////////////////////////////////////////////////////////////////////
//	PerformCallback()
//
//	Routine Description:
//
//	    CTE calls this, and we call the actual function.
//
//	Arguments:
//		
//		pCteTimer   ::  * Not used *.
//      pvData      ::  Should point to PTIMER_QUEUE_TIMER.
//
//	Return Value:
//
//		None.
//

void
PerformCallback(CTETimer *pCteTimer, PVOID pvData)
{
    PTIMER_QUEUE_TIMER  pTimerQueueTimer = (PTIMER_QUEUE_TIMER) pvData;    
    
    //
    //  The actual callback..
    //

    ASSERT(pTimerQueueTimer->dwSig == CE_TIMERTIMER_SIG);

    EnterCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);


    DEBUGMSG (ZONE_TIMER,
        (TEXT("ZCF:: PerformCallback() TQT[0x%x]..\r\n"),
        pTimerQueueTimer));

    //
    //  bTimerRunning == FALSE indicates we need to quit now..
    //

    if (pTimerQueueTimer->bTimerRunning == FALSE)
    {
        DEBUGMSG (ZONE_TIMER,
            (TEXT("ZCF:: PerformCallback() TQT[0x%x] forced terminated..\r\n"),
            pTimerQueueTimer));

        CTESignal(&pTimerQueueTimer->TimerBlockStruc, ERROR_SUCCESS);
        LeaveCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);
        TQT_DELREF(pTimerQueueTimer);   //  Taken in CE_CreateTimerQueueTimer() for completion.
        return;
    }

    //
    //  Arriving here means this is timer expiration..
    //  Mark it that we are going into callback function.
    //  The reason is because call back function may call ChangeTimer() in which
    //  we should just update the TimerQueueTimer structure and restart the timer
    //  in this function (i.e instead of in the ChangeTimer() function).
    //

    pTimerQueueTimer->bTimerInCallBack = TRUE;
    LeaveCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);

    pTimerQueueTimer->Callback(pTimerQueueTimer->Parameter, TRUE);

    EnterCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);
    pTimerQueueTimer->bTimerInCallBack = FALSE;
    if (pTimerQueueTimer->bTimerRunning == FALSE)
    {
        DEBUGMSG (ZONE_TIMER,
            (TEXT("ZCF:: PerformCallback() TQT[0x%x] forced terminated..\r\n"),
            pTimerQueueTimer));

        CTESignal(&pTimerQueueTimer->TimerBlockStruc, ERROR_SUCCESS);
        LeaveCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);
        TQT_DELREF(pTimerQueueTimer);   //  Taken in CE_CreateTimerQueueTimer() for completion.
        return;
    }

    if (pTimerQueueTimer->DueTime)
    {
        //
        //  The call back may call changetimer to restart with another due time, honor it.
        //
        
        CTEStartTimer(
            &(pTimerQueueTimer->hTimer),
            pTimerQueueTimer->DueTime,
            (CTEEventRtn)PerformCallback,
            pTimerQueueTimer);        
        
        LeaveCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);
    }
    else
    if (pTimerQueueTimer->Period)
    {   
        //
        //  Restart the timer..
        //

        DEBUGMSG (ZONE_TIMER,
            (TEXT("ZCF:: PerformCallback() TQT[0x%x] Restarted for period [%d].\r\n"),
            pTimerQueueTimer,
            pTimerQueueTimer->Period));
        
        
        CTEStartTimer(
            &(pTimerQueueTimer->hTimer),
            pTimerQueueTimer->Period,
            (CTEEventRtn)PerformCallback,
            pTimerQueueTimer);

        LeaveCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);
    }
    else
    {
        //
        //  One shot timer.. 
        //

        DEBUGMSG (ZONE_TIMER,
            (TEXT("ZCF:: PerformCallback() TQT[0x%x] 1 time timer terminated.\r\n"),
            pTimerQueueTimer));
        
        pTimerQueueTimer->bTimerRunning = FALSE;
        LeaveCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);
        TQT_DELREF(pTimerQueueTimer);   //  Taken in CE_CreateTimerQueueTimer() for completion.
    }
}   //  PerformCallback()


///////////////////////////////////////////////////////////////////////////////
//	WCE_Initialize()
//
//	Routine Description:
//
//		Called during initialization..
//
//	Return Value:
//
//		TRUE if successful, FALSE otherwise..
//

BOOL
WCE_Initialize()
{
#if 0
    BOOL    bStatus = FALSE;    
    
    do
    {
        //
        //  Async IO support.
        //
        
        g_AsyncWorkerWakeUpEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        if (g_AsyncWorkerWakeUpEvent == NULL)
            break;
            
        InitializeCriticalSection (&g_csAsyncWorkItem);

        InitializeListHead(&g_pAsyncWorkItemQueue);
        g_dwTotalPendingAsyncWorkItem   = 0x00;
        
        InitializeListHead(&g_pFreeAsyncWorkItem);
        g_dwTotalFreeAsyncWorkItem      = 0x00;
        
        InitializeListHead(&g_pHandleMappingList);        
        InitializeCriticalSection(&g_csHandleMappingList);

        InitializeCriticalSection (&g_csThreads);
        g_dwTotalAsyncWorkerThread      = 0x00;
        g_dwTotalFreeAsyncWorkerThread  = 0x00;
        g_bGlobalAsyncThreadQuit        = FALSE;


        //
        //  RegisterWaitForSingleObject() support.
        //

        InitializeCriticalSection(&g_csWaitForSingleObjectThread);
        InitializeListHead(&g_WaitObjectList);

        g_ReenumEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        if (g_ReenumEvent == NULL)
            break;
        
#endif
        //
        //  TimerQueue support.
        //

        InitializeListHead(&g_TimerQueueList);
        InitializeCriticalSection(&g_csTimerQueueList);
        return TRUE;

#if 0
#ifdef  TEST_CE_SPECIFIC_CODE
{
        extern void            
        CETestThread(PVOID Param1);
        
        HANDLE  hThread;
        DWORD   dwThreadId;
        
        hThread = CreateThread(
                            NULL, 
                            0, 
                            (LPTHREAD_START_ROUTINE) CETestThread,
                            NULL,
                            0,
                            &dwThreadId);

        if (hThread != NULL)
            CloseHandle(hThread);
}
#endif
        
        bStatus = TRUE;    
    }
    while (FALSE);

    if (!bStatus)
    {

        DEBUGMSG (ZONE_ERR,
            (TEXT("IPv6Hlp:: ** Failed ** WCE_Initialize.\r\n")));
            
        if (g_AsyncWorkerWakeUpEvent)
            CloseHandle(g_AsyncWorkerWakeUpEvent);

        if (g_ReenumEvent)
            CloseHandle(g_ReenumEvent);
        
    }
    return bStatus;
#endif

}



///////////////////////////////////////////////////////////////////////////////
//	CE_CreateTimerQueue()
//
//	Routine Description:
//
//		This function prepares the structures for subsequent calls to
//      CE_CreateTimerQueueTimer.
//
//	Arguments:
//		
//		None.
//
//	Return Value:
//
//		NULL if fail.
//

HANDLE
CE_CreateTimerQueue(void)
{
    PTIMER_QUEUE    pTimerQueue;

    pTimerQueue = (PTIMER_QUEUE) MALLOC(sizeof(TIMER_QUEUE));   
    
    if (pTimerQueue == NULL)
        return (HANDLE)INVALID_HANDLE_VALUE;

    memset(pTimerQueue, 0x00, sizeof(TIMER_QUEUE));

#ifdef DEBUG
	pTimerQueue->dwSig = CE_TIMER_SIG;
#endif

    InitializeCriticalSection(&pTimerQueue->csTimerQueue);

    InitializeListHead(&pTimerQueue->TimerQueueTimerList);

    //
    //  Derefed in CE_DeleteTimerQueueTimer()
    //

    TQ_ADDREF(pTimerQueue);

    //
    //  Queue this to global timer queue..
    //

    EnterCriticalSection(&g_csTimerQueueList);

    InsertHeadList(&g_TimerQueueList, (PLIST_ENTRY)pTimerQueue);

    LeaveCriticalSection(&g_csTimerQueueList);

    DEBUGMSG(
        ZONE_TIMER,
        (TEXT("ZCF:: CreateTimerQueue() returning handle [0x%x], TotalTQ[%d]\r\n"),
        pTimerQueue,
        InterlockedIncrement(&g_dwTotalTQ)));

    return (HANDLE)pTimerQueue;

}   //  CE_CreateTimerQueue()



///////////////////////////////////////////////////////////////////////////////
//	CE_DeleteTimerQueue()
//
//	Routine Description:
//
//		This function tries to FREE the TIMER_QUEUE structure if ref reaches
//      zero.   Regardless, it will wipe out the signature so the next
//      CreateTimerQueueTimer() on it will fail.
//      
//
//	Arguments:
//		
//		hTimerQueue :: Should be the handle returned by CE_CreateTimerQueue()
//
//	Return Value:
//
//		TRUE if successful, FALSE otherwise..
//

BOOL
CE_DeleteTimerQueue(HANDLE  hTimerQueue)
{
    PTIMER_QUEUE            pTimerQueue;


    DEBUGMSG (ZONE_TIMER,
        (TEXT("ZCF:: Deleting TimerQueue [0x%x]\r\n"),
        hTimerQueue));

    //
    //  Find it in our list..
    //

    EnterCriticalSection(&g_csTimerQueueList);

    pTimerQueue = (PTIMER_QUEUE)g_TimerQueueList.Flink;

    while (pTimerQueue != NULL)
    {
        if (pTimerQueue == (PTIMER_QUEUE)hTimerQueue)
            break;

        pTimerQueue = (PTIMER_QUEUE)pTimerQueue->ListEntry.Flink;
    }

    if (pTimerQueue == NULL)
    {
        //
        //  This should not happen in our ZCF code.
        //
        
        ASSERT(0);
        LeaveCriticalSection(&g_csTimerQueueList);
        return FALSE;
    }    

    EnterCriticalSection(&pTimerQueue->csTimerQueue);

    ASSERT(pTimerQueue->dwSig == CE_TIMER_SIG);

    //
    //  Cancel and delete all existing TimerQueueTimer
    //

    while (!IsListEmpty(&pTimerQueue->TimerQueueTimerList))
    {
        DEBUGMSG (ZONE_TIMER,
            (TEXT("ZCF:: Force deleting TimerQueueTimer[0x%x].\r\n"),
            pTimerQueue->TimerQueueTimerList.Flink));
            
        DeleteTimerQueueTimer(
            pTimerQueue,
            (PTIMER_QUEUE_TIMER)(pTimerQueue->TimerQueueTimerList.Flink),
            INVALID_HANDLE_VALUE);
    }

    LeaveCriticalSection(&pTimerQueue->csTimerQueue);    


    //
    //  Take it out from the list..
    //

    RemoveEntryList((PLIST_ENTRY)pTimerQueue);

    LeaveCriticalSection(&g_csTimerQueueList);

    TQ_DELREF(pTimerQueue);     //  Taken in CE_CreateTimerQueue()
    
    return TRUE;

}   //  CE_DeleteTimerQueue()



///////////////////////////////////////////////////////////////////////////////
//	FindAndRefTimerQueue()
//
//	Routine Description:
//
//		This function tries to find pTimerQueue in g_TimerQueueList list.
//      
//
//	Return Value:
//
//		NULL if not found, otherwise point to TimerQueue that's refed.
//      Caller needs to unref.
//

PTIMER_QUEUE
FindAndRefTimerQueue(PTIMER_QUEUE  pTargetTimerQueue)
{
    PTIMER_QUEUE            pTimerQueue;

    //
    //  Find it in our list..
    //

    EnterCriticalSection(&g_csTimerQueueList);

    pTimerQueue = (PTIMER_QUEUE)(g_TimerQueueList.Flink);

    while ((PLIST_ENTRY)pTimerQueue != &g_TimerQueueList)
    {
        if (pTimerQueue == pTargetTimerQueue)
            break;

        pTimerQueue = (PTIMER_QUEUE)(pTimerQueue->ListEntry.Flink);
    }

    if ((PLIST_ENTRY)pTimerQueue == &g_TimerQueueList)
    {
        LeaveCriticalSection(&g_csTimerQueueList);
        return NULL;
    }

    //
    //  Take the REF, caller needs to deref.
    //

    TQ_ADDREF(pTimerQueue);    

    LeaveCriticalSection(&g_csTimerQueueList);

    return pTimerQueue;

}   //  FindAndLockTimerQueue()


///////////////////////////////////////////////////////////////////////////////
//	FindTimerQueueTimer()
//
//	Routine Description:
//
//		Find the pTQT in the given pTQ's TimerQueueTimerList.
//
//      
//	Return Value:
//
//		NULL if it's not found, PTIMER_QUEUE_TIMER if found.
//
//
//  Note:
//
//      Caller should hold TIMER_QUEUE lock.
//

PTIMER_QUEUE_TIMER
FindTimerQueueTimer(PTIMER_QUEUE  pTQ, PTIMER_QUEUE_TIMER pTargetTQT)
{
    PTIMER_QUEUE_TIMER  pTQT = (PTIMER_QUEUE_TIMER)(pTQ->TimerQueueTimerList.Flink);

    while((PLIST_ENTRY)pTQT != &(pTQ->TimerQueueTimerList))
    {
        if (pTQT == pTargetTQT)
            break;

        pTQT = (PTIMER_QUEUE_TIMER)pTQT->ListEntry.Flink;
    }

    if ((PLIST_ENTRY)pTQT == &pTQ->ListEntry)
        return NULL;

    return pTQT;
    
}   //  FindAndLockTimerQueue()



///////////////////////////////////////////////////////////////////////////////
//	CE_CreateTimerQueueTimer()
//
//	Routine Description:
//
//		Implement the CreateTimerQueueTimer() Win32 API.
//
//	Return Value:
//
//		TRUE if successful, FALSE otherwise..
//

BOOL 
CE_CreateTimerQueueTimer(
  PHANDLE               phNewTimer,     // handle to timer
  HANDLE                TimerQueue,     // handle to timer queue
  WAITORTIMERCALLBACK   Callback,       // timer callback function
  PVOID                 Parameter,      // callback parameter
  DWORD                 DueTime,        // timer due time
  DWORD                 Period,         // timer period
  ULONG                 Flags)          // options *Ignored*
{
    PTIMER_QUEUE_TIMER  pTimerQueueTimer;
    PTIMER_QUEUE        pTimerQueue;    

    DEBUGMSG (ZONE_TIMER,
        (TEXT("ZCF:: CE_CreateTimerQueueTimer(). Due[%d]s Period[%d]s\r\n"),
        DueTime/1000,
        Period/1000));

    pTimerQueueTimer = (PTIMER_QUEUE_TIMER) MALLOC(sizeof(TIMER_QUEUE_TIMER));    
    
    if (pTimerQueueTimer == NULL)
    {
        DEBUGMSG(
            ZONE_ERROR,
            (TEXT("ZCF:: Failed CE_CreateTimerQueueTimer() OOM.\r\n")));
        return FALSE;
    }

    memset(pTimerQueueTimer, 0x00, sizeof(TIMER_QUEUE_TIMER));

#ifdef DEBUG
    pTimerQueueTimer->dwSig       = CE_TIMERTIMER_SIG;
#endif
    pTimerQueueTimer->Callback    = Callback; 
    pTimerQueueTimer->Parameter   = Parameter;
    pTimerQueueTimer->DueTime     = 0x00;       //  <-- due time is good for one go only..
    pTimerQueueTimer->Period      = Period;

    InitializeCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);

    TQT_ADDREF(pTimerQueueTimer);       //  For deletion.
    TQT_ADDREF(pTimerQueueTimer);       //  For completion.


    CTEInitBlockStruc(&pTimerQueueTimer->TimerBlockStruc);

    //
    //  The TimerQueue better be in our list..
    //

    if (TimerQueue == NULL)
    {
        //
		//	Use default timer queue, create one if there isn't one.
		//

		if (g_hDefaultTimerQueue == NULL)
		{
		    g_hDefaultTimerQueue = CreateTimerQueue();			
			if (g_hDefaultTimerQueue == INVALID_HANDLE_VALUE)
				goto ErrorReturn;
		}

		TimerQueue = (HANDLE)g_hDefaultTimerQueue;	
    }

    //
    //  For each TimerQueueTimer, we'll take a ref in TimerQueue.
    //  We deref it in CE_DeleteTimerQueueTimer()
    //
    
    pTimerQueue = FindAndRefTimerQueue((PTIMER_QUEUE)TimerQueue);

    if (pTimerQueue == NULL)
        goto ErrorReturn;

    EnterCriticalSection(&pTimerQueue->csTimerQueue);

    ASSERT(pTimerQueue->dwSig == CE_TIMER_SIG);
    
    //
    //  Queue TimerQueueTimer..
    //

    InsertTailList(&pTimerQueue->TimerQueueTimerList, (PLIST_ENTRY)pTimerQueueTimer);

    LeaveCriticalSection(&pTimerQueue->csTimerQueue);

    //
    //  Start it..
    //

    *phNewTimer = (PHANDLE)pTimerQueueTimer;	

    DEBUGMSG (ZONE_TIMER,
        (TEXT("ZCF:: CE_CreateTimerQueueTimer() TQT[0x%x] created.   Due[%d]s Period[%d]s.   TotalTQT[%d]"),
        pTimerQueueTimer,
        DueTime/1000,
        Period/1000,
        InterlockedIncrement(&g_dwTotalTQT)));
    
    pTimerQueueTimer->bTimerRunning = TRUE;

    CTEStartTimer(
		&(pTimerQueueTimer->hTimer),
        DueTime,
		(CTEEventRtn)PerformCallback,
		pTimerQueueTimer);	

    return TRUE;

ErrorReturn:

    DEBUGMSG(ZONE_ERROR,
        (TEXT("ZCF:: Failed CE_CreateTimerQueueTimer().\r\n")));
        
    CTEDeinitBlockStruc(&pTimerQueueTimer->TimerBlockStruc);
    FREE(pTimerQueueTimer);
    return FALSE;

}   //  CE_CreateTimerQueueTimer()




///////////////////////////////////////////////////////////////////////////////
//	CE_DeleteTimerQueueTimer()
//
//	Routine Description:
//
//		Implement the CreateTimerQueueTimer() Win32 API.
//
//	Return Value:
//
//		TRUE if successful, FALSE otherwise..
//

BOOL CE_DeleteTimerQueueTimer(
  HANDLE TimerQueue,        // handle to timer queue
  HANDLE Timer,             // handle to timer
  HANDLE CompletionEvent)   // handle to completion event
{

    PTIMER_QUEUE        pTimerQueue;
    PTIMER_QUEUE_TIMER  pTimerQueueTimer;

    DEBUGMSG (ZONE_TIMER,
        (TEXT("ZCF:: CE_DeleteTimerQueueTimer() TQT[0x%x] TQ[0x%x]\r\n"),
        Timer,
        TimerQueue));         

    if (TimerQueue == NULL)
    {
        if (g_hDefaultTimerQueue == NULL)
        {
            DEBUGMSG (ZONE_TIMER,
                (TEXT("ZCF:: CE_ChangeTimerQueueTimer() Default Timer not started.\r\n")));

            return FALSE;
        }

        TimerQueue = g_hDefaultTimerQueue;
    }


    //
    //  FindAndRefTimerQueue() will ref TimerQueue in this function.
    //
    
    pTimerQueue =  FindAndRefTimerQueue(TimerQueue);

    if (pTimerQueue == NULL)
    {
        //
        //  This should not happen in ZCF code..
        //
        
        ASSERT(0);        
        return FALSE;
    }

    EnterCriticalSection(&pTimerQueue->csTimerQueue);

    pTimerQueueTimer =  FindTimerQueueTimer(
                            pTimerQueue, 
                            (PTIMER_QUEUE_TIMER)Timer);

    if (pTimerQueueTimer == NULL)
    {
        DEBUGMSG (ZONE_TIMER,
            (TEXT("ZCF:: CE_DeleteTimerQueueTimer() invalid TQT[0x%x]\r\n"),
            Timer));

        //
        //  This should not happen in IPv6 code..
        //        
        ASSERT(0);
        
        LeaveCriticalSection(&pTimerQueue->csTimerQueue);
        TQ_DELREF(pTimerQueue); //  Taken in FindAndRefTimerQueue()
        return FALSE;
    }

    //
    //  Remove it from TimerQueueTimerList list.
    //

    RemoveEntryList((PLIST_ENTRY)pTimerQueueTimer);

    LeaveCriticalSection(&pTimerQueue->csTimerQueue);

    TQ_DELREF(pTimerQueue);     //  Taken in FindAndRefTimerQueue()
    TQ_DELREF(pTimerQueue);     //  Taken in CE_CreateTimerQueueTimer()


    //
    //  By now pTimerQueueTimer is a stand alone cell..
    //

    EnterCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);

    pTimerQueueTimer->hFinishEvent = CompletionEvent;
    pTimerQueueTimer->DueTime      = 0x00;
    pTimerQueueTimer->Period       = 0x00;
 
    if (pTimerQueueTimer->bTimerRunning == TRUE)        
    {   
        //
        //  For the timer still running case, completion event will be
        //  set by the callback function.  If caller asks to be blocked
        //  do it.
        //  
        
        pTimerQueueTimer->bTimerRunning = FALSE;
        
        if (!CTEStopTimer(&(pTimerQueueTimer->hTimer)))
        {                            
            DEBUGMSG(ZONE_TIMER,
                (TEXT("ZCF:: CE_DeleteTimerQueueTimer().  TQT[0x%x] pending.\r\n"),
                pTimerQueueTimer));

            //
            //  Potentially we may get here from the callback itself.
            //  So, don't block if that's the case.
            //            
            
            if (CompletionEvent == INVALID_HANDLE_VALUE  &&
                pTimerQueueTimer->bTimerInCallBack == FALSE)
            {                
                LeaveCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);
                CTEBlock(&pTimerQueueTimer->TimerBlockStruc);
    			ResetEvent(&pTimerQueueTimer->TimerBlockStruc.cbs_event);
                EnterCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);
            }

            //
            //  Do not need to deref TQT because the PerformCallBack() must have done 
            //  or will be doing that.
            //
        }
        else
        {
            DEBUGMSG(ZONE_TIMER,
                (TEXT("ZCF:: CE_DeleteTimerQueueTimer().  Timer successfully stopped.\r\n")));            
            
            TQT_DELREF(pTimerQueueTimer);       //  Taken in CE_CreateTimerQueueTimer() for completion.
        }
    }   
    else
    {
        DEBUGMSG(ZONE_TIMER,
            (TEXT("ZCF:: CE_DeleteTimerQueueTimer().  Timer was not running.\r\n")));
        
        //
        //  Ditto here..
        //  Do not need to deref TQT because the PerformCallBack() must have
        //  done that.
        //
    }

    LeaveCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);
    TQT_DELREF(pTimerQueueTimer);   // Taken in CE_CreateTimerQueueTimer() for deletion.    
    return TRUE;

}   //  CE_DeleteTimerQueueTimer()



///////////////////////////////////////////////////////////////////////////////
//  CE_ChangeTimerQueueTimer()
//
//  Routine Description:
//
//      Implement the ChangeTimerQueueTimer() Win32 API.
//
//  Return Value:
//
//      TRUE if successful, FALSE otherwise..
//

BOOL
CE_ChangeTimerQueueTimer(
    HANDLE  TimerQueue,     // handle to timer queue
    HANDLE  Timer,          // handle to timer
    ULONG   DueTime,        // timer due time
    ULONG   Period)         // timer period
{

    PTIMER_QUEUE_TIMER  pTimerQueueTimer;
    PTIMER_QUEUE        pTimerQueue;
    BOOL                bRestartTimer = TRUE;


    DEBUGMSG (ZONE_TIMER,
        (TEXT("ZCF:: CE_ChangeTimerQueueTimer() TQT[0x%x] TQ[0x%x] Due[%d]s Period[%d]s\r\n"),
        Timer,
        TimerQueue,
        DueTime/1000,
        Period/1000));

    //
    //  This will hold REF for TIMER_QUEUE during in this function.
    //

    if (TimerQueue == NULL)
    {
        if (g_hDefaultTimerQueue == NULL)
        {
            DEBUGMSG (ZONE_TIMER,
                (TEXT("ZCF:: CE_ChangeTimerQueueTimer() Default Timer not started.\r\n")));

            return FALSE;
        }

        TimerQueue = g_hDefaultTimerQueue;
    }

    //
    //  TimerQueue is REFed once here.
    //
    
    pTimerQueue =  FindAndRefTimerQueue(TimerQueue);

    if (pTimerQueue == NULL)
    {
        //
        //  This should not happen in ZCF code.
        //
        
        ASSERT(0);
        return FALSE;
    }

    EnterCriticalSection(&pTimerQueue->csTimerQueue);

    pTimerQueueTimer =  FindTimerQueueTimer(
                            pTimerQueue, 
                            (PTIMER_QUEUE_TIMER)Timer);

    if (pTimerQueueTimer == NULL)
    {
        DEBUGMSG (ZONE_TIMER,
            (TEXT("ZCF:: CE_ChangeTimerQueueTimer() invalid TQT[0x%x]\r\n"),
            Timer));

        //
        //  Should not happen in ZCF code.
        //
        ASSERT(0);
        
        LeaveCriticalSection(&pTimerQueue->csTimerQueue);
        TQ_DELREF(pTimerQueue); //  Taken in FindAndRefTimerQueue()
        return FALSE;
    }

    //
    //  REF hold during this function only..
    //
    
    TQT_ADDREF(pTimerQueueTimer);

    LeaveCriticalSection(&pTimerQueue->csTimerQueue);


    EnterCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);

    //
    //  Attempt to stop the timer if it is currently ticking..
    // 
    
    if (pTimerQueueTimer->bTimerRunning == TRUE)
    {
        if (!CTEStopTimer(&(pTimerQueueTimer->hTimer)))
        {
            if (pTimerQueueTimer->bTimerInCallBack)
            {
                bRestartTimer = FALSE;
            }
            else
            {
                //
                //  Timer must have fired..   
                //  And the call back is waiting to get in while we are holding
                //  the CS.
                //  Tell it to go away..
                //           

                pTimerQueueTimer->bTimerRunning = FALSE;

                LeaveCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);

                //
                //  Wait for it to tell us that it has gone away..
                //

                CTEBlock(&pTimerQueueTimer->TimerBlockStruc);
                ResetEvent(&pTimerQueueTimer->TimerBlockStruc.cbs_event);

                //
                //  Cool, all done we are clear to go..
                //

                EnterCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);
                
            }
        }
        else
        {
            TQT_DELREF(pTimerQueueTimer);   //  Taken in CE_CreateTimerQueueTimer() for completion.
                                            //  Since we now cancel it.

        }
    }
    
    //
    //  By now the timer has successfully been stopped.
    //  Per MSDN, 
    //  If you call ChangeTimerQueueTimer on a one-shot timer (its period is 
    //  zero) that has already expired, the timer is not updated.     
    //
    
    if (pTimerQueueTimer->Period)
    {
        pTimerQueueTimer->DueTime       = DueTime;
        pTimerQueueTimer->Period        = Period;         

        if (bRestartTimer)
        {
            pTimerQueueTimer->bTimerRunning = TRUE;        

            TQT_ADDREF(pTimerQueueTimer);

            CTEStartTimer(
                &(pTimerQueueTimer->hTimer),
                pTimerQueueTimer->DueTime,
                (CTEEventRtn)PerformCallback,
                pTimerQueueTimer);            

            pTimerQueueTimer->DueTime = 0x00;

        }
    }

    LeaveCriticalSection(&pTimerQueueTimer->csTimerQueueTimer);
    
    TQT_DELREF(pTimerQueueTimer);       //  REF hold in this function.
    TQ_DELREF(pTimerQueue);             //  REF hold in this function.
    return TRUE;

}   //  CE_ChangeTimerQueueTimer()


