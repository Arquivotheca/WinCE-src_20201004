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
#include "Cracker.h"
#include "CriticalSection.h"
#include "ShareInfo.h"
#include "FileServer.h"
#include "ConnectionManager.h"

//
// Init global variables defined in Cracker.h
ce::list<SMB_PACKET *, CRACKER_PACKETS_ALLOC>    Cracker::g_PacketsToDelayCrack;
CRITICAL_SECTION                                 Cracker::g_csCrackLock;
HANDLE                                           Cracker::g_hStopEvent = NULL;
HANDLE                                           Cracker::g_hCrackDelaySem = NULL;
HANDLE                                           Cracker::g_hCrackerDelayThread = NULL;
const UINT                                       Cracker::g_uiMaxPacketsInQueue = 100;
BOOL                                             Cracker::g_fIsRunning = FALSE;

#ifdef DEBUG    
CRITICAL_SECTION                                 Cracker::g_csPerfLock;
DOUBLE                                           Cracker::g_dblPerfAvePacketTime[0xFF];
DWORD                                            Cracker::g_dwPerfPacketsProcessed[0xFF];
#endif


//
// Forward Declared fn's()
extern HRESULT CloseConnectionTransport(ULONG ulConnectionID);

HRESULT
StopCracker() {
    HRESULT hr = E_FAIL;
    
    //
    // If the cracker isnt running just return failure
    if(FALSE == Cracker::g_fIsRunning) {        
        TRACEMSG(ZONE_DETAIL, (L"CRACKER:  not running, so stopping doesnt do anything"));
        return S_OK;    
    }
    
    //
    // First things first set a stop event
    if(0 == SetEvent(Cracker::g_hStopEvent)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: setting stop event FAILED!!"));
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }

    //
    // Wait for the delay write thread to terminate
    if(WAIT_FAILED == WaitForSingleObject(Cracker::g_hCrackerDelayThread, INFINITE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: error waiting for cracker thread to exit!! (%d)", GetLastError()));
        ASSERT(FALSE);
    }
       

    //
    // Delete our cracking pool
    ASSERT(SMB_Globals::g_pCrackingPool);
    if(SMB_Globals::g_pCrackingPool) {
        delete SMB_Globals::g_pCrackingPool;
        SMB_Globals::g_pCrackingPool = NULL;
    }

    
#ifdef DEBUG 
    {for(UINT i=0; i<0xFF; i++) {
        if(0 != Cracker::g_dwPerfPacketsProcessed[i]) {
            TRACEMSG(ZONE_STATS, (L"SMBSRV-CRACKER: Perf for packet (0x%x -- %10s) -- %d calls ave time %d", i, GetCMDName(i) ,Cracker::g_dwPerfPacketsProcessed[i], (UINT)Cracker::g_dblPerfAvePacketTime[i]));
        }
    }
    DeleteCriticalSection(&Cracker::g_csPerfLock);
    }
#endif

    //
    // Clean up handles and critical sections
    CloseHandle(Cracker::g_hStopEvent);
    Cracker::g_hStopEvent = NULL;
    
    DeleteCriticalSection(&Cracker::g_csCrackLock);

    hr = S_OK;
    Done:
        if(SUCCEEDED(hr)) {
            Cracker::g_fIsRunning = FALSE;
        }
        return hr;
}


HRESULT 
StartCracker() {
    HRESULT hr = E_FAIL;
    WORD    wErr = 0;
         
    //
    // If the cracker is running just return okay
    if(TRUE == Cracker::g_fIsRunning) {
        ASSERT(FALSE);       
        TRACEMSG(ZONE_DETAIL, (L"CRACKER:  is already running, so starting dosent do anything"));
        return E_FAIL;        
    }
    
    //
    // Init our critical section
    InitializeCriticalSection(&(Cracker::g_csCrackLock));    

    //
    // Create a thread pool for packets
    if(NULL == SMB_Globals::g_pCrackingPool) {
        if(!(SMB_Globals::g_pCrackingPool = new SVSThreadPool(SMB_Globals::g_uiMaxCrackingThreads))) {
            hr = E_UNEXPECTED;
            goto Done;
        }
        if(NULL == (SMB_Globals::g_CrackingSem = CreateSemaphore(NULL, SMB_Globals::g_uiMaxCrackingThreads, SMB_Globals::g_uiMaxCrackingThreads, NULL))) {
            hr = E_OUTOFMEMORY;
            goto Done;
        }        
    }

   
    //
    // Init data to pre-processing state (so if error we can cleanup)
    //
    // NOTE: these all should be null (or else we have leaked something)
    ASSERT(NULL == Cracker::g_hStopEvent);
    Cracker::g_hStopEvent = NULL;
    Cracker::g_hCrackerDelayThread = NULL;
    Cracker::g_hCrackDelaySem = NULL;
    Cracker::g_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if(NULL == Cracker::g_hStopEvent) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER:  cant create stop event!"));
        ASSERT(FALSE);
        hr = E_UNEXPECTED;
        goto Done;
    }    
    
#ifdef DEBUG
    //
    // Init perf measurements
    {
    for(UINT i=0; i<0xFF; i++) {
        Cracker::g_dblPerfAvePacketTime[i] = 0;
        Cracker::g_dwPerfPacketsProcessed[i] = 0;
    }
    InitializeCriticalSection(&Cracker::g_csPerfLock);  
    }   
#endif
    
    //
    // Init our delay semaphore the # in the semaphore represents the # of packets in the delay queue
    Cracker::g_hCrackDelaySem = CreateSemaphore(NULL, 0, Cracker::g_uiMaxPacketsInQueue, NULL);
    
    if(NULL == Cracker::g_hCrackDelaySem) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER:  cant create delay semaphore!"));
        ASSERT(FALSE);
        hr = E_UNEXPECTED;
        goto Done;
    }  
    
    if(NULL == (Cracker::g_hCrackerDelayThread = CreateThread(NULL, 0, Cracker::SMBSRVR_CrackerDelayThread, 0, CREATE_SUSPENDED, NULL))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: cant make cracker delay thread"));
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }    
    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: cracker delay thread created 0x%x", (UINT)Cracker::g_hCrackerDelayThread));

      
    hr = S_OK;
    
    Done:
        if(FAILED(hr)) {
            if(SMB_Globals::g_pCrackingPool) 
                delete SMB_Globals::g_pCrackingPool;                    
            if(Cracker::g_hStopEvent)
                CloseHandle(Cracker::g_hStopEvent);    
            if(SMB_Globals::g_CrackingSem) 
                CloseHandle(SMB_Globals::g_CrackingSem);

            SMB_Globals::g_pCrackingPool = NULL;
            Cracker::g_hStopEvent = NULL;
            SMB_Globals::g_CrackingSem = NULL;
        } else {        
            //
            // Everything went okay, set the running flag
            Cracker::g_fIsRunning = TRUE;
        }
        
        if(NULL != Cracker::g_hCrackerDelayThread) {
            ResumeThread(Cracker::g_hCrackerDelayThread);
        }
        return hr;
    
}


DWORD Cracker::SMBSRVR_CrackerDelayThread(LPVOID unused) 
{
    HANDLE hWaitEvents[2];
    HRESULT hr;
    ce::list<SMB_PACKET *, CRACKER_PACKETS_ALLOC> OrderedDelayList;
    ce::list<SMB_PACKET *, CRACKER_PACKETS_ALLOC >::iterator it;
    DWORD dwSleepTime = INFINITE;
        
    //
    // If the stop events are nulled (invalid) we cant run! just exit with failure
    if(NULL == Cracker::g_hStopEvent || NULL == Cracker::g_hCrackDelaySem) {
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
    hWaitEvents[0] = Cracker::g_hStopEvent;
    hWaitEvents[1] = Cracker::g_hCrackDelaySem;
    
    for(;;) {
        SMB_PACKET *pSMB = NULL;
        ce::list<SMB_PACKET *, CRACKER_PACKETS_ALLOC >::iterator itDelayAdjVar;
        
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
            LockCracker();
                ASSERT(0 != Cracker::g_PacketsToDelayCrack.size());
                pSMB = Cracker::g_PacketsToDelayCrack.front();
                TRACEMSG(ZONE_DETAIL, (L"SMBSRV: Delay Cracker has packet: %x", (UINT)pSMB));
                Cracker::g_PacketsToDelayCrack.pop_front();
                ASSERT(NULL != pSMB);
            UnlockCracker(); 

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
            TRACEMSG(ZONE_DETAIL, (L"SMBSRV-DELAYCRACKER:  got stop event--exiting thread!"));   
            hr = S_OK;
            goto Done;
        } else {
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-DELAYCRACKER:  unknown response from waitformultipleobjects -- trying to continue!"));   
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
            ce::list<SMB_PACKET *, CRACKER_PACKETS_ALLOC >::iterator itPrev = OrderedDelayList.begin();
            ce::list<SMB_PACKET *, CRACKER_PACKETS_ALLOC >::iterator itVar = OrderedDelayList.begin();
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
        Cracker::LockCracker();
            ASSERT(pSMB->dwDelayBeforeSending < 5000);
            Cracker::g_PacketsToDelayCrack.push_front(pSMB);
            ReleaseSemaphore(Cracker::g_hCrackDelaySem, 1, NULL);
        Cracker::UnlockCracker();   
    }   
}



VOID  
Cracker::LockCracker()
{    
    EnterCriticalSection(&Cracker::g_csCrackLock);
}

VOID  
Cracker::UnlockCracker()
{
    LeaveCriticalSection(&Cracker::g_csCrackLock);
}


DWORD WINAPI CrackPacket_Helper(LPVOID _pSMB)
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
            TRACEMSG(ZONE_SMB,(L"CRACKPACKET: Error with SMB- protocol header is incorrect!"));
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
        // Crack the command
        UINT uiUsedByCracker = 0;

        //
        //  send received protocol message to output
        //
        TRACE_PROTOCOL_RECEIVE_SMB_REQUEST( pSMB );
	
        IFDBG(pSMB->PerfPrintStatus(L"Going to cracker"));
        if (0 == (dwErr = PC_NET_PROG::CrackSMB(pSMB->pInSMB, 
                                   pSMB->uiInSize, 
                                   pResponseSMB, 
                                   uiSize, 
                                   &uiUsedByCracker, 
                                   pSMB))) {
            TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: SUCCESS! -- cracked by PC_NETWORK_PROGRAM_1.0"));                   
        } 
        else {
            TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: FAILURE! -- sending error message"));              
        }
        IFDBG(pSMB->PerfPrintStatus(L"back from cracker"));
        if(SMB_ERR(ERRInternal, PACKET_QUEUED) != dwErr) {                
            uiUsed += uiUsedByCracker;  
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
            TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: FAILURE! -- we tried to remove a connection that doesnt exist!"));
        } else {
            TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: removed all info for connectionID 0x%x", pSMB->ulConnectionID));
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
        ReleaseSemaphore(SMB_Globals::g_CrackingSem, 1, NULL);
        return hr;
}


HRESULT CrackPacket(SMB_PACKET *pSMB)
{
    WaitForSingleObject(SMB_Globals::g_CrackingSem, INFINITE);
       
    if(!SMB_Globals::g_pCrackingPool->ScheduleEvent(CrackPacket_Helper, (LPVOID)pSMB)) {
        return E_FAIL;
    } else {
        return S_OK;
    }
}
