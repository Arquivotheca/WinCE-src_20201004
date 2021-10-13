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
#include "SMB_Globals.h"
#include "PC_NET_PROG.h"
#include "SMBErrors.h"
#include "PktHandler.h"
#include "CriticalSection.h"
#include "ShareInfo.h"
#include "FileServer.h"
#include "ConnectionManager.h"

//
// Init global variables defined in PktHandler.h
ce::list<SMB_PACKET *, PktHandler_PACKETS_ALLOC>    PktHandler::g_PacketsToDelayPktHandle;
CRITICAL_SECTION                                 PktHandler::g_csPktHandleLock;
HANDLE                                           PktHandler::g_hStopEvent = NULL;
HANDLE                                           PktHandler::g_hPktHandleDelaySem = NULL;
HANDLE                                           PktHandler::g_hPktHandlerDelayThread = NULL;
const UINT                                       PktHandler::g_uiMaxPacketsInQueue = 100;
BOOL                                             PktHandler::g_fIsRunning = FALSE;

#ifdef DEBUG    
CRITICAL_SECTION                                 PktHandler::g_csPerfLock;
DOUBLE                                           PktHandler::g_dblPerfAvePacketTime[0xFF];
DWORD                                            PktHandler::g_dwPerfPacketsProcessed[0xFF];
#endif


//
// Forward Declared fn's()
extern HRESULT CloseConnectionTransport(ULONG ulConnectionID);

HRESULT
StopPktHandler() {
    HRESULT hr = E_FAIL;
    
    //
    // If the PktHandler isnt running just return failure
    if(FALSE == PktHandler::g_fIsRunning) {        
        TRACEMSG(ZONE_DETAIL, (L"PktHandler:  not running, so stopping doesnt do anything"));
        return S_OK;    
    }
    
    //
    // First things first set a stop event
    if(0 == SetEvent(PktHandler::g_hStopEvent)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-PktHandler: setting stop event FAILED!!"));
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }

    //
    // Wait for the delay write thread to terminate
    if(WAIT_FAILED == WaitForSingleObject(PktHandler::g_hPktHandlerDelayThread, INFINITE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-PktHandler: error waiting for PktHandler thread to exit!! (%d)", GetLastError()));
        ASSERT(FALSE);
    }
       

    //
    // Delete our PktHandling pool
    ASSERT(SMB_Globals::g_pPktHandlingPool);
    if(SMB_Globals::g_pPktHandlingPool) {
        delete SMB_Globals::g_pPktHandlingPool;
        SMB_Globals::g_pPktHandlingPool = NULL;
    }

    
#ifdef DEBUG 
    {for(UINT i=0; i<0xFF; i++) {
        if(0 != PktHandler::g_dwPerfPacketsProcessed[i]) {
            TRACEMSG(ZONE_STATS, (L"SMBSRV-PktHandler: Perf for packet (0x%x -- %10s) -- %d calls ave time %d", i, GetCMDName(i) ,PktHandler::g_dwPerfPacketsProcessed[i], (UINT)PktHandler::g_dblPerfAvePacketTime[i]));
        }
    }
    DeleteCriticalSection(&PktHandler::g_csPerfLock);
    }
#endif

    //
    // Clean up handles and critical sections
    CloseHandle(PktHandler::g_hStopEvent);
    PktHandler::g_hStopEvent = NULL;
    
    DeleteCriticalSection(&PktHandler::g_csPktHandleLock);

    hr = S_OK;
    Done:
        if(SUCCEEDED(hr)) {
            PktHandler::g_fIsRunning = FALSE;
        }
        return hr;
}


HRESULT 
StartPktHandler() {
    HRESULT hr = E_FAIL;
    WORD    wErr = 0;
         
    //
    // If the PktHandler is running just return okay
    if(TRUE == PktHandler::g_fIsRunning) {
        ASSERT(FALSE);       
        TRACEMSG(ZONE_DETAIL, (L"PktHandler:  is already running, so starting dosent do anything"));
        return S_OK;        
    }
    
    //
    // Init our critical section
    InitializeCriticalSection(&(PktHandler::g_csPktHandleLock));    

    //
    // Create a thread pool for packets
    if(NULL == SMB_Globals::g_pPktHandlingPool) {
        if(!(SMB_Globals::g_pPktHandlingPool = new SVSThreadPool(SMB_Globals::g_uiMaxPktHandlingThreads))) {
            hr = E_UNEXPECTED;
            goto Done;
        }
        if(NULL == (SMB_Globals::g_PktHandlingSem = CreateSemaphore(NULL, SMB_Globals::g_uiMaxPktHandlingThreads, SMB_Globals::g_uiMaxPktHandlingThreads, NULL))) {
            hr = E_OUTOFMEMORY;
            goto Done;
        }        
    }

   
    //
    // Init data to pre-processing state (so if error we can cleanup)
    //
    // NOTE: these all should be null (or else we have leaked something)
    ASSERT(NULL == PktHandler::g_hStopEvent);
    PktHandler::g_hStopEvent = NULL;
    PktHandler::g_hPktHandlerDelayThread = NULL;
    PktHandler::g_hPktHandleDelaySem = NULL;
    PktHandler::g_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if(NULL == PktHandler::g_hStopEvent) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-PktHandler:  cant create stop event!"));
        ASSERT(FALSE);
        hr = E_UNEXPECTED;
        goto Done;
    }    
    
#ifdef DEBUG
    //
    // Init perf measurements
    {
    for(UINT i=0; i<0xFF; i++) {
        PktHandler::g_dblPerfAvePacketTime[i] = 0;
        PktHandler::g_dwPerfPacketsProcessed[i] = 0;
    }
    InitializeCriticalSection(&PktHandler::g_csPerfLock);  
    }   
#endif
    
    //
    // Init our delay semaphore the # in the semaphore represents the # of packets in the delay queue
    PktHandler::g_hPktHandleDelaySem = CreateSemaphore(NULL, 0, PktHandler::g_uiMaxPacketsInQueue, NULL);
    
    if(NULL == PktHandler::g_hPktHandleDelaySem) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-PktHandler:  cant create delay semaphore!"));
        ASSERT(FALSE);
        hr = E_UNEXPECTED;
        goto Done;
    }  
    
    if(NULL == (PktHandler::g_hPktHandlerDelayThread = CreateThread(NULL, 0, PktHandler::SMBSRVR_PktHandlerDelayThread, 0, CREATE_SUSPENDED, NULL))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-PktHandler: cant make PktHandler delay thread"));
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }    
    TRACEMSG(ZONE_ERROR, (L"SMBSRV-PktHandler: PktHandler delay thread created 0x%x", (UINT)PktHandler::g_hPktHandlerDelayThread));

      
    hr = S_OK;
    
    Done:
        if(FAILED(hr)) {
            if(SMB_Globals::g_pPktHandlingPool) 
                delete SMB_Globals::g_pPktHandlingPool;                    
            if(PktHandler::g_hStopEvent)
                CloseHandle(PktHandler::g_hStopEvent);    
            if(SMB_Globals::g_PktHandlingSem) 
                CloseHandle(SMB_Globals::g_PktHandlingSem);

            SMB_Globals::g_pPktHandlingPool = NULL;
            PktHandler::g_hStopEvent = NULL;
            SMB_Globals::g_PktHandlingSem = NULL;
            //Reset the running flag (FALSE)
            PktHandler::g_fIsRunning = FALSE;
        } else {        
            //
            // Everything went okay, set the running flag
            PktHandler::g_fIsRunning = TRUE;
        }
        
        if(NULL != PktHandler::g_hPktHandlerDelayThread) {
            ResumeThread(PktHandler::g_hPktHandlerDelayThread);
        }
        return hr;
    
}


DWORD PktHandler::SMBSRVR_PktHandlerDelayThread(LPVOID unused) 
{
    HANDLE hWaitEvents[2];
    HRESULT hr;
    ce::list<SMB_PACKET *, PktHandler_PACKETS_ALLOC> OrderedDelayList;
    ce::list<SMB_PACKET *, PktHandler_PACKETS_ALLOC >::iterator it;
    DWORD dwSleepTime = INFINITE;
        
    //
    // If the stop events are nulled (invalid) we cant run! just exit with failure
    if(NULL == PktHandler::g_hStopEvent || NULL == PktHandler::g_hPktHandleDelaySem) {
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }
    
    //   
    /// This thread does the following:
    //  
    //  1. Waits for one of these 3 things:
    //     a. timeout (meaning a packet is ready to go)
    //     b. a new packet (at which time it will be inserted in order of delay on the ORderedDelayList)
    //     c. shutdown
    //  2. When it wakes up for:
    //     a. timeout -- the first packet will be sent, and the timer will be set for the next
    //        closest value
    //     b. a new packet comes in, it will be inserted in order and the timer will be set 
    //        to be what it should for the next newest packet
    //     c. shutdown -- we just exit
    hWaitEvents[0] = PktHandler::g_hStopEvent;
    hWaitEvents[1] = PktHandler::g_hPktHandleDelaySem;
    
    for(;;) {
        SMB_PACKET *pSMB = NULL;
        ce::list<SMB_PACKET *, PktHandler_PACKETS_ALLOC >::iterator itDelayAdjVar;
        
        //
        // Wait for either a packet to come in, or for a STOP event
        DWORD dwStartAsleepAt = GetTickCount();
        DWORD dwStopAsleepFor;
        DWORD dwRet = WaitForMultipleObjects(2, hWaitEvents, FALSE, dwSleepTime);
          
        if(dwRet == WAIT_TIMEOUT) {
            if(0 != OrderedDelayList.size()) {
                //
                // Get the first node off the list
                pSMB = OrderedDelayList.front();
                OrderedDelayList.pop_front();

                //
                // Send the packet to its transport
                hr = pSMB->pfnQueueFunction(pSMB, TRUE);   
                ASSERT(SUCCEEDED(hr)); 
            } else {
                ASSERT(FALSE);
            }
        }
        else if(1 == dwRet - WAIT_OBJECT_0) {           
            //
            // Get an SMB from the queue (we know there is one since we were woken up)
            LockPktHandler();
                ASSERT(0 != PktHandler::g_PacketsToDelayPktHandle.size());
                pSMB = PktHandler::g_PacketsToDelayPktHandle.front();
                TRACEMSG(ZONE_DETAIL, (L"SMBSRV: Delay PktHandler has packet: %x", (UINT)pSMB));
                PktHandler::g_PacketsToDelayPktHandle.pop_front();
                ASSERT(NULL != pSMB);
            UnlockPktHandler(); 

            //
            // Adjust everyones timings to reflect our sleeping (so order is preserved)      
            dwStopAsleepFor = GetTickCount() - dwStartAsleepAt;
            dwStartAsleepAt = GetTickCount();
            for(itDelayAdjVar = OrderedDelayList.begin(); itDelayAdjVar != OrderedDelayList.end(); ++itDelayAdjVar) {
                if(dwStopAsleepFor <= (*itDelayAdjVar)->dwDelayBeforeSending) {
                    (*itDelayAdjVar)->dwDelayBeforeSending -= dwStopAsleepFor;
                } else {
                    (*itDelayAdjVar)->dwDelayBeforeSending = 0;
                }
            }   
          
            //
            // Search out the correct location for our node and insert it         
            for(it = OrderedDelayList.begin(); it != OrderedDelayList.end(); ++it) {           
                if(pSMB->dwDelayBeforeSending > (*it)->dwDelayBeforeSending) {            
                    break;
                }    
            }   
            OrderedDelayList.insert(it, pSMB);
       } else if(0 == dwRet - WAIT_OBJECT_0) {
            TRACEMSG(ZONE_DETAIL, (L"SMBSRV-DELAYPktHandler:  got stop event--exiting thread!"));   
            hr = S_OK;
            goto Done;
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-DELAYPktHandler:  unknown response from waitformultipleobjects -- trying to continue!"));   
            ASSERT(FALSE);
        } 

        //
        // Adjust everyones timings to reflect our sleeping (so order is preserved)      
        dwStopAsleepFor = GetTickCount() - dwStartAsleepAt;
        dwStartAsleepAt = GetTickCount();
        for(itDelayAdjVar = OrderedDelayList.begin(); itDelayAdjVar != OrderedDelayList.end(); ++itDelayAdjVar) {
            if(dwStopAsleepFor <= (*itDelayAdjVar)->dwDelayBeforeSending) {
                (*itDelayAdjVar)->dwDelayBeforeSending -= dwStopAsleepFor;
            } else {
                (*itDelayAdjVar)->dwDelayBeforeSending = 0;
            }
        }  

        #ifdef DEBUG
            //
            // Perform a sanity check on the list to make sure its still in order
            ce::list<SMB_PACKET *, PktHandler_PACKETS_ALLOC >::iterator itPrev = OrderedDelayList.begin();
            ce::list<SMB_PACKET *, PktHandler_PACKETS_ALLOC >::iterator itVar = OrderedDelayList.begin();
            itVar ++;
            for(; itVar != OrderedDelayList.end(); ++itVar) {
                ASSERT((*itPrev)->dwDelayBeforeSending >= (*itVar)->dwDelayBeforeSending);
                itPrev = itVar;
            }
        #endif
        //
        // If we are here, we need to go back to sleep -- get the first packet off the list and 
        //   sleep for that long,  if the list is empty -- sleep forever    
        if(0 != OrderedDelayList.size()) {
            pSMB = OrderedDelayList.front();
            dwSleepTime = pSMB->dwDelayBeforeSending;
        
            //
            // Dont allow sleeping for more than 5 seconds (most likely an error!)
            if(5000 <= dwSleepTime) {
                dwSleepTime = 5000;
                ASSERT(FALSE);
            }       
        } else {
            dwSleepTime = INFINITE;
        }
    }

    Done:
        //
        // Purge the delayed list
        while(0 != OrderedDelayList.size()) 
        {
            SMB_PACKET *pToDel = OrderedDelayList.front();
            OrderedDelayList.pop_front();
            
            //
            // Return the memory
            SMB_Globals::g_SMB_Pool.Free(pToDel);
            
        }    
        ASSERT(0 == OrderedDelayList.size());
        return 0;
}



VOID
SendSMBResponse(SMB_PACKET *pSMB, UINT uiUsed, DWORD dwErr)
{
    SMB_HEADER *pResponseSMB = (SMB_HEADER *)(pSMB->OutSMB);
    
    //
    // Fill in error codes
    /*pResponseSMB->StatusFields.Fields.error.ErrorClass = SMB_ERR_CLASS(dwErr);
    pResponseSMB->StatusFields.Fields.error.Error = SMB_ERR_ERR(dwErr);*/
    pResponseSMB->StatusFields.Fields.dwError = dwErr;
            
    ASSERT(NULL == pSMB->pOutSMB);
    ASSERT(0 == pSMB->uiOutSize);
    
    pSMB->pOutSMB = (SMB_HEADER *)(pSMB->OutSMB);
    pSMB->uiOutSize = uiUsed;
    
    //
    // Fill in anything specific (flags etc)
    { 
        //
        // Find our connection state   
        ce::smart_ptr<ActiveConnection> pMyConnection = NULL;
        if((pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {  
            pSMB->pOutSMB->Uid = pMyConnection->Uid();   
        }
    }

    //
    //  send sent protocol message to output
    //
    TRACE_PROTOCOL_SEND_SMB_RESPONSE( pSMB );
    TRACEMSG(ZONE_SMB | ZONE_PROTOCOL, (L"\n\n\n\n------------------------------------------------------"));
      
    //
    // If this packet is to be delayed, put it on the delay list rather than sending
    //     it now
    if(0 == pSMB->dwDelayBeforeSending) {
        
        //
        // Send out the packet, if that fails there isnt anything we can do :(
        IFDBG(pSMB->PerfPrintStatus(L"Queuing send"));
        pSMB->pfnQueueFunction(pSMB, TRUE);       
    } else {
        PktHandler::LockPktHandler();
            ASSERT(pSMB->dwDelayBeforeSending < 5000);
            PktHandler::g_PacketsToDelayPktHandle.push_front(pSMB);
            ReleaseSemaphore(PktHandler::g_hPktHandleDelaySem, 1, NULL);
        PktHandler::UnlockPktHandler();   
    }   
}



VOID  
PktHandler::LockPktHandler()
{    
    EnterCriticalSection(&PktHandler::g_csPktHandleLock);
}

VOID  
PktHandler::UnlockPktHandler()
{
    LeaveCriticalSection(&PktHandler::g_csPktHandleLock);
}


DWORD WINAPI SMBHandlePacket_Helper(LPVOID _pSMB)
{  
    HRESULT hr=E_FAIL;
    SMB_PACKET *pSMB = (SMB_PACKET *)_pSMB;    
    
    ASSERT(SMB_NORMAL_PACKET == pSMB->uiPacketType ||
           SMB_CONNECTION_DROPPED == pSMB->uiPacketType ||
           SMB_CLIENT_IDLE_TIMEOUT == pSMB->uiPacketType);

    //
    // Check for proper SMB signature
    if(SMB_NORMAL_PACKET == pSMB->uiPacketType) {    
        
        char pSig[4];
        pSig[0] = (char)0xFF;
        pSig[1] = (char)'S';
        pSig[2] = (char)'M';
        pSig[3] = (char)'B';
    
        //
        // Verify the SMB signature is correct (it should be 0xFF,S,M,B)
        if(0 != memcmp(pSMB->pInSMB->Protocol, pSig, 4)) {
            TRACEMSG(ZONE_SMB,(L"SMBHandlePacket: Error with SMB- protocol header is incorrect!"));
            ASSERT(FALSE);
            hr = E_FAIL;
            goto Done;
        }
    }
    
    //
    // Inspect the packet type to see what we are supposed to do
    //   (meaning -- is this a reg SMB or what?)
    if(SMB_NORMAL_PACKET == pSMB->uiPacketType) {                    
        UINT uiSize = SMB_Globals::MAX_PACKET_SIZE;        
        UINT uiUsed = sizeof(SMB_HEADER);
        DWORD dwErr;

        //
        // PERFPERF: *ICK*.  we need to make sure we zero or set everything
        //   this will require reviewing most of the code however.  
        //   maybe fix up helper functions to do it?  tough call.  leaving it for now

        //
        // NOTE: removing this *FOR SURE* will break 9x clients in FindFirstFile.. there prob
        //  are others as well, be careful removing this!
        memset(pSMB->OutSMB, 0, sizeof(pSMB->OutSMB));    
        
        //
        // Write in SMB header (copy and mask server flag)        
        SMB_HEADER *pResponseSMB = (SMB_HEADER *)(pSMB->OutSMB);
        ASSERT(0 == ((UINT)pResponseSMB % 2));
        memcpy(pResponseSMB, pSMB->pInSMB, sizeof(SMB_HEADER)); 
        pResponseSMB->Flags |= SMB_FLAG1_SERVER_TO_REDIR; //SMB_FLAG1_CASELESS | 
        //pResponseSMB->Flags = (SMB_FLAG1_CASELESS | SMB_FLAG1_SERVER_TO_REDIR);
        //pResponseSMB->Flags2 = pSMB->pInSMB->Flags2 & SMB_FLAGS2_UNICODE;
        uiSize -= sizeof(SMB_HEADER);
                    
        //
        // PktHandle the command
        UINT uiUsedByPktHandler = 0;

        //
        //  send received protocol message to output
        //
        TRACE_PROTOCOL_RECEIVE_SMB_REQUEST( pSMB );
	
        IFDBG(pSMB->PerfPrintStatus(L"Going to PktHandler"));
        if (0 == (dwErr = PC_NET_PROG::PktHandleSMB(pSMB->pInSMB, 
                                   pSMB->uiInSize, 
                                   pResponseSMB, 
                                   uiSize, 
                                   &uiUsedByPktHandler, 
                                   pSMB))) {
            TRACEMSG(ZONE_SMB, (L"SMBSRV-PktHandler: SUCCESS! -- PktHandleed by PC_NETWORK_PROGRAM_1.0"));                   
        } 
        else {
            TRACEMSG(ZONE_SMB, (L"SMBSRV-PktHandler: FAILURE! -- sending error message"));              
        }
        IFDBG(pSMB->PerfPrintStatus(L"back from PktHandler"));
        if(SMB_ERR(ERRInternal, PACKET_QUEUED) != dwErr) {                
            uiUsed += uiUsedByPktHandler;  
            SendSMBResponse(pSMB, uiUsed, dwErr);                
        }
    }
    //
    //  if the connection has dropped, we need to clean up any memory
    //    that might be outstanding
    else if(SMB_CONNECTION_DROPPED == pSMB->uiPacketType) {
        ASSERT(NULL == pSMB->pInSMB &&
               NULL == pSMB->pOutSMB &&
               0 == pSMB->uiInSize &&
               0 == pSMB->uiOutSize);  
        
        // 
        // Remove all connections on this ID from the active list           
        if(FAILED(SMB_Globals::g_pConnectionManager->RemoveConnection(pSMB->ulConnectionID, 0xFFFF))) {
            TRACEMSG(ZONE_SMB, (L"SMBSRV-PktHandler: FAILURE! -- we tried to remove a connection that doesnt exist!"));
        } else {
            TRACEMSG(ZONE_SMB, (L"SMBSRV-PktHandler: removed all info for connectionID 0x%x", pSMB->ulConnectionID));
        } 
                               
        //     
        // Process the SMB (this will cause memory to be deleted if its required) 
        hr = pSMB->pfnQueueFunction(pSMB, TRUE);
        ASSERT(SUCCEEDED(hr));
    } else if(SMB_CLIENT_IDLE_TIMEOUT == pSMB->uiPacketType) {
        //
        // If this is an idle timeout we may not close up here (b/c they
        //   may have connections opened already)            
        ce::list<ce::smart_ptr<ActiveConnection>, ACTIVE_CONN_PTR_LIST > ConnList;
        BOOL fInUse = FALSE;
        
        //
        // Find our connection state        
        SMB_Globals::g_pConnectionManager->FindAllConnections(pSMB->ulConnectionID, ConnList);
         
        while(ConnList.size()) {
            ce::smart_ptr<ActiveConnection> pConn = ConnList.front();
            ConnList.pop_front();

            ASSERT(pConn->ConnectionID() == pSMB->ulConnectionID);

            if(pConn->HasOpenedResources()) {
                fInUse = TRUE;
                break;
            }
        }
        
        if(!fInUse) {
            CloseConnectionTransport(pSMB->ulConnectionID);                            
        } 

        //     
        // Process the SMB (this will cause memory to be deleted if its required) 
        hr = pSMB->pfnQueueFunction(pSMB, TRUE);
        ASSERT(SUCCEEDED(hr));
        
    } else {
        ASSERT(FALSE);
    }
    
    
    Done:
        ReleaseSemaphore(SMB_Globals::g_PktHandlingSem, 1, NULL);
        return hr;
}


HRESULT SMBHandlePacket(SMB_PACKET *pSMB)
{
    WaitForSingleObject(SMB_Globals::g_PktHandlingSem, INFINITE);
       
    if(!SMB_Globals::g_pPktHandlingPool->ScheduleEvent(SMBHandlePacket_Helper, (LPVOID)pSMB)) {
        return E_FAIL;
    } else {
        return S_OK;
    }
}
