//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
ce::list<SMB_PACKET *, CRACKER_PACKETS_ALLOC>    Cracker::g_PacketsToCrack;
ce::list<SMB_PACKET *, CRACKER_PACKETS_ALLOC>    Cracker::g_PacketsToDelayCrack;
CRITICAL_SECTION                                 Cracker::g_csCrackLock;
HANDLE                                           Cracker::g_ToCrackQueueSem = INVALID_HANDLE_VALUE;
HANDLE                                           Cracker::g_hStopEvent = INVALID_HANDLE_VALUE;
HANDLE                                           Cracker::g_hCrackDelaySem = INVALID_HANDLE_VALUE;
HANDLE                                           Cracker::g_hCrackerThread = NULL;
HANDLE                                           Cracker::g_hCrackerDelayThread = NULL;
const UINT                                       Cracker::g_uiMaxPacketsInQueue = 100;
BOOL                                             Cracker::g_fIsRunning = FALSE;
DWORD                                            Cracker::g_dwDelayPacket;

#ifdef DEBUG    
CRITICAL_SECTION                                 Cracker::g_csPerfLock;
DOUBLE                                           Cracker::g_dblPerfAvePacketTime[0xFF];
DWORD                                            Cracker::g_dwPerfPacketsProcessed[0xFF];
BOOL                                             Cracker::g_fAlreadyStarted = FALSE;
#endif

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
    // Wait for the cracker thread to terminate
    ASSERT(NULL != Cracker::g_hCrackerThread);
    if(WAIT_FAILED == WaitForSingleObject(Cracker::g_hCrackerThread, INFINITE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: error waiting for cracker thread to exit!! (%d)", GetLastError()));
        ASSERT(FALSE);
    }

    //
    // Wait for the delay write thread to terminate
    if(WAIT_FAILED == WaitForSingleObject(Cracker::g_hCrackerDelayThread, INFINITE)) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: error waiting for cracker thread to exit!! (%d)", GetLastError()));
        ASSERT(FALSE);
    }
    
    //
    // At this point we are single threaded... so its safe to delete memory
    while(0 != Cracker::g_PacketsToCrack.size()) 
    {
        SMB_PACKET *pToDel = Cracker::g_PacketsToCrack.front();
        Cracker::g_PacketsToCrack.pop_front();
        
        //
        // Return the memory
        SMB_Globals::g_SMB_Pool.Free(pToDel);
        
    }    
    ASSERT(0 == Cracker::g_PacketsToCrack.size());
    
        
#ifdef DEBUG 
    {for(UINT i=0; i<0xFF; i++) {
        if(0 != Cracker::g_dwPerfPacketsProcessed[i]) {
            TRACEMSG(ZONE_STATS, (L"SMBSRV-CRACKER: Perf for packet 0x%x -- %d calls ave time %d", i, Cracker::g_dwPerfPacketsProcessed[i], (UINT)Cracker::g_dblPerfAvePacketTime[i]));
        }
    }
    DeleteCriticalSection(&Cracker::g_csPerfLock);
    }
#endif

    //
    // Clean up handles and critical sections
    CloseHandle(Cracker::g_hCrackerThread);
    Cracker::g_hCrackerThread = NULL; //Createthread returns null, not invalid handle value!

    CloseHandle(Cracker::g_hStopEvent);
    Cracker::g_hStopEvent = INVALID_HANDLE_VALUE;

    DeleteCriticalSection(&Cracker::g_csCrackLock);

    CloseHandle(Cracker::g_ToCrackQueueSem);
    Cracker::g_ToCrackQueueSem = INVALID_HANDLE_VALUE;
        

    hr = S_OK;
    Done:
        if(SUCCEEDED(hr))
            Cracker::g_fIsRunning = FALSE;
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
    // Init data to pre-processing state (so if error we can cleanup)
    //
    // NOTE: these all should be null (or else we have leaked something)
    ASSERT(INVALID_HANDLE_VALUE == Cracker::g_hStopEvent);
    ASSERT(INVALID_HANDLE_VALUE == Cracker::g_ToCrackQueueSem);
    ASSERT(NULL == Cracker::g_hCrackerThread); //(threads are NULL... totally bogus)
    Cracker::g_hStopEvent = NULL;
    Cracker::g_ToCrackQueueSem = NULL;
    Cracker::g_hCrackerThread = NULL;
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
    // Init our semaphore the # in the semaphore represents the # of packets in the queue
    Cracker::g_ToCrackQueueSem = CreateSemaphore(NULL, 0, Cracker::g_uiMaxPacketsInQueue, NULL);
    
    if(NULL == Cracker::g_ToCrackQueueSem) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER:  cant create cracker semaphore!"));
        ASSERT(FALSE);
        hr = E_UNEXPECTED;
        goto Done;
    }   

    //
    // Init our delay semaphore the # in the semaphore represents the # of packets in the delay queue
    Cracker::g_hCrackDelaySem = CreateSemaphore(NULL, 0, Cracker::g_uiMaxPacketsInQueue, NULL);
    
    if(NULL == Cracker::g_hCrackDelaySem) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER:  cant create delay semaphore!"));
        ASSERT(FALSE);
        hr = E_UNEXPECTED;
        goto Done;
    }  
    
    //
    // Startup the Cracker thread (this has to be last so if during failure we dont
    //   have to clean up the thread    
    if(NULL == (Cracker::g_hCrackerThread = CreateThread(NULL, 0, Cracker::SMBSRVR_CrackerThread, 0, CREATE_SUSPENDED, NULL))) {
        TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: cant make cracker thread"));
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }
    TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: cracker thread created 0x%x", (UINT)Cracker::g_hCrackerThread));

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
            if(NULL != Cracker::g_hStopEvent)
                CloseHandle(Cracker::g_hStopEvent);
            if(NULL != Cracker::g_ToCrackQueueSem)
                CloseHandle(Cracker::g_ToCrackQueueSem);
             
            Cracker::g_hStopEvent = INVALID_HANDLE_VALUE;
            Cracker::g_ToCrackQueueSem = INVALID_HANDLE_VALUE;
           
        } else {        
            //
            // Everything went okay, set the running flag
            Cracker::g_fIsRunning = TRUE;
        }
        
        //
        // NOTE: this will start up the cracker thread, and it will
        //   quickly exit during error if the stop event is null
        if(NULL != Cracker::g_hCrackerThread) {
            ResumeThread(Cracker::g_hCrackerThread);
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
            EnterCriticalSection(&Cracker::g_csCrackLock);
                ASSERT(0 != Cracker::g_PacketsToDelayCrack.size());
                pSMB = Cracker::g_PacketsToDelayCrack.front();
                TRACEMSG(ZONE_DETAIL, (L"SMBSRV: Delay Cracker has packet: %x", (UINT)pSMB));
                Cracker::g_PacketsToDelayCrack.pop_front();
                ASSERT(NULL != pSMB);
            LeaveCriticalSection(&Cracker::g_csCrackLock);  

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


DWORD Cracker::SMBSRVR_CrackerThread(LPVOID netnum)
{
    HANDLE hWaitEvents[2];
    HRESULT hr;
    
#ifdef DEBUG   
    if(FALSE == Cracker::g_fAlreadyStarted) {
        Cracker::g_fAlreadyStarted = TRUE;
    } else {
        //THERE CAN ONLY BE ONE CRACKER THREAD!!!
        ASSERT(FALSE);
    }
#endif
    
    //
    // If the stop events are nulled (invalid) we cant run! just exit with failure
    if(NULL == Cracker::g_hStopEvent || NULL == Cracker::g_ToCrackQueueSem) {
        ASSERT(FALSE);
        hr = E_FAIL;
        goto Done;
    }
    
    //
    // The cracker will use 2 events -- the stop event and the 
    //   cracker semaphore.  The stop event will be set when we need
    //   to stop running, and the semaphore represents the # of messages in the
    //   queue
    hWaitEvents[0] = Cracker::g_hStopEvent;
    hWaitEvents[1] = Cracker::g_ToCrackQueueSem;
    
    for(;;) {
        SMB_PACKET *pSMB = NULL; 
        
        
        //
        // Wait for either a packet to come in, or for a STOP event
        DWORD dwRet = WaitForMultipleObjects(2, hWaitEvents, FALSE, INFINITE);
        if(0 == dwRet - WAIT_OBJECT_0) {
            TRACEMSG(ZONE_DETAIL, (L"SMBSRV-CRACKER:  got stop event--exiting thread!"));   
            hr = S_OK;
            goto Done;
        } else if(WAIT_FAILED == dwRet) {
            ASSERT(FALSE);
            TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER:  waiting ERROR (%d)!?!?!?!!", GetLastError()));   
            hr = E_UNEXPECTED;
            goto Done;
        } // else fall through        
        
        //
        // Get an SMB from the queue (we know there is one since we were woken up)
        EnterCriticalSection(&Cracker::g_csCrackLock);        
            ASSERT(0 != Cracker::g_PacketsToCrack.size());
            pSMB = Cracker::g_PacketsToCrack.front();
            TRACEMSG(ZONE_DETAIL, (L"SMBSRV: Cracker has packet: %x", (UINT)pSMB));
            Cracker::g_PacketsToCrack.pop_front();
            ASSERT(NULL != pSMB);
        LeaveCriticalSection(&Cracker::g_csCrackLock);   

        //
        // Make sure not to delay the packet
        Cracker::g_dwDelayPacket = 0;        
        
        //
        // Inspect the packet type to see what we are supposed to do
        //   (meaning -- is this a reg SMB or what?)
        if(SMB_NORMAL_PACKET == pSMB->uiPacketType)
        { 
            BYTE *pResponse = pSMB->OutSMB;            
            UINT uiSize = SMB_Globals::MAX_PACKET_SIZE;        
            UINT uiUsed = sizeof(SMB_HEADER);
            DWORD dwErr;

            //
            // PERFPERF: *ICK*.  we need to make sure we zero or set everything
            //   this will require reviewing most of the code however.  
            //   maybe fix up helper functions to do it?  tough call.  leaving it for now
            memset(pResponse, 0, sizeof(pSMB->OutSMB));    
            
            //
            // Write in SMB header (copy and mask server flag)        
            SMB_HEADER *pResponseSMB = (SMB_HEADER *)pResponse;
            ASSERT(0 == ((UINT)pResponseSMB % 2));
            memcpy(pResponseSMB, pSMB->pInSMB, sizeof(SMB_HEADER)); 
            pResponseSMB->Flags |= SMB_FLAG1_SERVER_TO_REDIR; //SMB_FLAG1_CASELESS | 
            //pResponseSMB->Flags = (SMB_FLAG1_CASELESS | SMB_FLAG1_SERVER_TO_REDIR);
            //pResponseSMB->Flags2 = pSMB->pInSMB->Flags2 & SMB_FLAGS2_UNICODE;
            uiSize -= sizeof(SMB_HEADER);
                        
            //
            // Crack the command
            UINT uiUsedByCracker = 0;
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
            uiUsed += uiUsedByCracker;                 

            //
            // Fill in error codes
            pResponseSMB->StatusFields.Fields.error.ErrorClass = SMB_ERR_CLASS(dwErr);
            pResponseSMB->StatusFields.Fields.error.Error = SMB_ERR_ERR(dwErr);
                    
            ASSERT(NULL == pSMB->pOutSMB);
            ASSERT(0 == pSMB->uiOutSize);
            
            pSMB->pOutSMB = (SMB_HEADER *)pResponse;
            pSMB->uiOutSize = uiUsed;
            
            //
            // Fill in anything specific (flags etc)
            {
                ActiveConnection *pMyConnection = NULL;
                //
                // Find our connection state        
                if(NULL != (pMyConnection = SMB_Globals::g_pConnectionManager->FindConnection(pSMB))) {  
                    /*if(TRUE == pMyConnection->SupportsUnicode(pSMB)) {
                        pSMB->pOutSMB->Flags2 |= SMB_FLAGS2_UNICODE;
                    }*/
                    pSMB->pOutSMB->Uid = pMyConnection->Uid();   
                }
            }
                          
            
            
            
            TRACEMSG(ZONE_SMB, (L"\n\n\n\n--------------------------------------------------------------"));
            
            //     
            // Process the SMB  
            __try {
                if(NULL == pResponse || uiUsed == 0) {
                    ASSERT(FALSE); 
                } else {
                    pSMB->dwDelayBeforeSending = Cracker::g_dwDelayPacket;
                    
                    //
                    // If this packet is to be delayed, put it on the delay list rather than sending
                    //     it now
                    if(0 == pSMB->dwDelayBeforeSending) {
                        if(FAILED(hr = pSMB->pfnQueueFunction(pSMB, TRUE))) {
                            ASSERT(FALSE);
                        }
                    } else {
                        EnterCriticalSection(&Cracker::g_csCrackLock);   
                            ASSERT(pSMB->dwDelayBeforeSending < 5000);
                            Cracker::g_PacketsToDelayCrack.push_front(pSMB);
                            ReleaseSemaphore(Cracker::g_hCrackDelaySem, 1, NULL);
                        LeaveCriticalSection(&Cracker::g_csCrackLock);   
                    }
                } 
            } __except(1) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: FAILURE! -- exception thrown"));
                ASSERT(FALSE);
            }
        }
        //
        //  if the connection has dropped, we need to clean up any memory
        //    that might be outstanding
        else if(SMB_CONNECTION_DROPPED == pSMB->uiPacketType)
        {
            ASSERT(NULL == pSMB->pInSMB &&
                   NULL == pSMB->pOutSMB &&
                   0 == pSMB->uiInSize &&
                   0 == pSMB->uiOutSize);
            // 
            // Remove all connections on this ID from the active list
            if(FAILED(SMB_Globals::g_pConnectionManager->RemoveConnection(pSMB->ulConnectionID, 0xFFFF, 0xFFFF, 0xFFFF))) {
                TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: FAILURE! -- we tried to remove a connection that doesnt exist!"));
            } else {
                TRACEMSG(ZONE_SMB, (L"SMBSRV-CRACKER: removed all info for connectionID 0x%x", pSMB->ulConnectionID));
            }          
                        
            //     
            // Process the SMB (this will cause memory to be deleted if its required) 
            __try {
                if(FAILED(hr = pSMB->pfnQueueFunction(pSMB, TRUE))) {
                    ASSERT(FALSE);
                } 
            } __except(1) {
                TRACEMSG(ZONE_ERROR, (L"SMBSRV-CRACKER: FAILURE! -- exception thrown"));
                ASSERT(FALSE);
            }
        } 
        else {
            ASSERT(FALSE);
        }
    }
    
    Done:
        TRACEMSG(ZONE_DETAIL, (L"SMBSRV-CRACKER:  thread going away!!"));  
        
#ifdef DEBUG 
        ASSERT(TRUE == Cracker::g_fAlreadyStarted);
        Cracker::g_fAlreadyStarted = FALSE;        
#endif
        return hr;
}

HRESULT
CrackPacket(SMB_PACKET *pNewPacket){
    HRESULT hr = S_OK;
    ASSERT(0xFFFFFFFF != pNewPacket->ulConnectionID);
    ASSERT(SMB_NORMAL_PACKET == pNewPacket->uiPacketType ||
           SMB_CONNECTION_DROPPED == pNewPacket->uiPacketType);
 
    if(SMB_CONNECTION_DROPPED != pNewPacket->uiPacketType) {    
        //
        // Proper SMB signature
        char pSig[4];
        pSig[0] = (char)0xFF;
        pSig[1] = (char)'S';
        pSig[2] = (char)'M';
        pSig[3] = (char)'B';
    
        //
        // Verify the SMB signature is correct (it should be 0xFF,S,M,B)
        if(0 != memcmp(pNewPacket->pInSMB->Protocol, pSig, 4)) {
            TRACEMSG(ZONE_SMB,(L"CRACKPACKET: Error with SMB- protocol header is incorrect!"));
            ASSERT(FALSE);
            return E_FAIL;    
        }
    }
    
    //
    // Add the SMB to a queue for processing
    EnterCriticalSection(&Cracker::g_csCrackLock);
        //if there are TOO many packets in the queue, Sleep, then retry
        while(Cracker::g_PacketsToCrack.size() >= Cracker::g_uiMaxPacketsInQueue) {
            LeaveCriticalSection(&Cracker::g_csCrackLock);
            Sleep(500);
            EnterCriticalSection(&Cracker::g_csCrackLock);
        }
        TRACEMSG(ZONE_DETAIL, (L"SMBSRV: Giving Cracker  packet: %x", (UINT)pNewPacket));
        Cracker::g_PacketsToCrack.push_back(pNewPacket);  
    LeaveCriticalSection(&Cracker::g_csCrackLock);   
    
    // Set the semaphore saying there is a packet in the queue (this will wake
    //   up CrackThread()
    ReleaseSemaphore(Cracker::g_ToCrackQueueSem, 1, NULL);    
   
    return hr;
}
