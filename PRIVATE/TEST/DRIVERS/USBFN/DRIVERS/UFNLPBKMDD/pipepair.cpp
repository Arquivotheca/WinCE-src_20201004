//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

// 
// Module Name:  
//     Pipepair.cpp
// 
// Abstract:  This file implements a class which manage a shared transfer descriptor list
//            between a pair of IN/OUT pipe, and does loopback functionality  
//            
//     
// Notes: 
//

#include <usbfn.h>
#include <usbfntypes.h>
#include "pipepair.h"


#define  THREAD_IDLE_TIME    500    // .5 sec. wait time
#define  TASK_EXIT_TIME    500    // .5 sec. wait time


//----------------------------------------------------------------------------------
// Init: Initialize this pipe management instance
//----------------------------------------------------------------------------------

BOOL 
CSinglePipe::Init(){
    SETFNAME(_T("Init"));
    FUNCTION_ENTER_MSG();
    BOOL  bRet = TRUE;

    if(m_hDevice == NULL || m_pUfnFuncs == NULL){
        ERRORMSG(1, (_T("%s invalid Device handle or function table!!! \r\n"), pszFname));
        return FALSE;	
    }

    //initialize critical sections
    InitializeCriticalSection(&m_Transcs);
    InitializeCriticalSection(&m_TransCompletioncs);
	
    //initialize semaphore
    m_hSemaphore = CreateSemaphore(NULL, m_uQueueDepth, m_uQueueDepth, NULL);
    if (m_hSemaphore == NULL) {
        ERRORMSG(1, (_T("%s Error creating semaphore. Error = %d\r\n"), 
            					pszFname, GetLastError()));
        bRet = FALSE;
        goto EXIT;
    }
    		
    //create task ready notify events
    m_hTaskReadyEvent = CreateEvent(0, FALSE, FALSE, NULL);
    if(m_hTaskReadyEvent == NULL ){
        ERRORMSG(1, (_T("%s failed to create event(s)\r\n"), pszFname));
        bRet = FALSE;
        goto EXIT;
    }

    //Task read/completion notify events
    m_hCompleteEvent = CreateEvent(0, FALSE, FALSE, NULL);
    m_hAckReadyEvent = CreateEvent(0, FALSE, FALSE, NULL);
    if((m_hCompleteEvent == NULL) ||( m_hAckReadyEvent == NULL)){
        ERRORMSG(1, (_T("%s failed to create event(s)\r\n"), pszFname));
        bRet = FALSE;
        goto EXIT;
    }

    //create completion notify events
    m_hTransCompleteEvent = CreateEvent(0, FALSE, FALSE, NULL);
    if(m_hTransCompleteEvent == NULL){
        ERRORMSG(1, (_T("%s failed to create event(s)\r\n"), pszFname));
        bRet = FALSE;
        goto EXIT;
    }

    //reset buffer
    m_pBuffer = NULL;

    //clear transnotifications.
    m_pTransNoti = NULL;

EXIT:

    if(bRet == FALSE){
        Cleanup();
    }
    FUNCTION_LEAVE_MSG();
    return bRet;
}


//----------------------------------------------------------------------------------
// Cleanup: Cleanup this pipe management instance
//----------------------------------------------------------------------------------

VOID 
CSinglePipe::Cleanup(){
    SETFNAME(_T("Cleanup"));
    FUNCTION_ENTER_MSG();
    BOOL  bRet = TRUE;
    DWORD dwWait, dwExitCode;

    //notify all threads to exit
    m_bThreadsExit = TRUE;

    //now try to close all those 4 threads
    if(m_hTransCompleteThread){
        dwWait = WaitForSingleObject(m_hTransCompleteThread, THREAD_IDLE_TIME);
        if(dwWait != WAIT_OBJECT_0){//we have trouble on this thread
            ERRORMSG(1, (_T("%s Transfer complete process thread will be terminated!!!\r\n"), pszFname));
            ::GetExitCodeThread(m_hTransCompleteThread, &dwExitCode);
            ::TerminateThread(m_hTransCompleteThread, dwExitCode);
	 }
        else{
            CloseHandle(m_hTransCompleteThread);
        }
        m_hTransCompleteThread = NULL;
    }
    if(m_hTransThread){
        dwWait = WaitForSingleObject(m_hTransThread, THREAD_IDLE_TIME);
        if(dwWait != WAIT_OBJECT_0){//we have trouble on this thread
            ERRORMSG(1, (_T("%s Transfer process thread will be terminated!!!\r\n"), pszFname));
            ::GetExitCodeThread(m_hTransThread, &dwExitCode);
            ::TerminateThread(m_hTransThread, dwExitCode);
	 }
        else{
            CloseHandle(m_hTransThread);
        }
        m_hTransThread = NULL;
    }

    //free critical sections
    DeleteCriticalSection(&m_Transcs);
    DeleteCriticalSection(&m_TransCompletioncs);
	
    //Close semaphores
    if(m_hSemaphore != NULL){
        CloseHandle(m_hSemaphore);
    }    		

    //close all events
    if(m_hTransCompleteEvent != NULL){
        CloseHandle(m_hTransCompleteEvent);
    }    		
    if(m_hTaskReadyEvent != NULL){
        CloseHandle(m_hTaskReadyEvent);
    }    		
    if(m_hCompleteEvent != NULL){
        CloseHandle(m_hCompleteEvent);
    }    		
    if(m_hAckReadyEvent != NULL){
        CloseHandle(m_hAckReadyEvent);
    }    		

    //clear buffer and transcount
    m_usTransLeft = 0;
    if(m_pBuffer != NULL){
        delete[] m_pBuffer;
    }
    if(m_pTransNoti != NULL){
        delete[] m_pTransNoti;
        m_pTransNoti = NULL;
    }
    
    FUNCTION_LEAVE_MSG();
}


//----------------------------------------------------------------------------------
// StartThreads:Launch Out transfer thread and In/Out transfer complete threads
//----------------------------------------------------------------------------------

BOOL 
CSinglePipe::StartThreads(){
    SETFNAME(_T("StartThreads"));
    FUNCTION_ENTER_MSG();
    BOOL  bRet = TRUE;

    m_bThreadsExit = FALSE;

    //start transfer process thread
    if(m_hTransThread == NULL){	
        m_hTransThread = ::CreateThread(NULL, 0, CSinglePipe::TransThreadProc, (LPVOID)this, 0, NULL);
        if (m_hTransThread == NULL) {
            ERRORMSG(1, (_T("%s Out process thread creation failed\r\n"), pszFname));
    	      bRet = FALSE;
    	      goto EXIT;
        }
    }

    //start transfer completion thread
    if(m_hTransCompleteThread == NULL){	
        m_hTransCompleteThread = CreateThread(NULL, 0, CSinglePipe::CompleteThreadProc, (LPVOID)this, 0, NULL);
        if (m_hTransCompleteThread == NULL) {
            ERRORMSG(1, (_T("%s Out completion process thread creation failed\r\n"), pszFname));
    	      bRet = FALSE;
    	      goto EXIT;
        }
    }

EXIT:
   if(bRet == FALSE)
   	Cleanup();
   FUNCTION_LEAVE_MSG();
   return bRet;
}


//----------------------------------------------------------------------------------
// StartNewTransfers:Notify IN/OUT transfer thread to start to execute new request from host side
//----------------------------------------------------------------------------------

BOOL 
CSinglePipe::StartNewTransfers(DWORD dwTransLen, USHORT usTransNum, USHORT usAlignment, DWORD dwStartValue){
    SETFNAME(_T("StartNewTransfers"));
    FUNCTION_ENTER_MSG();

    if((dwTransLen == 0) || (usTransNum == 0) || usAlignment == 0 || usAlignment == 3){
        ERRORMSG(1, (_T("%s invalid parameter!\r\n"), pszFname));
        FUNCTION_LEAVE_MSG();
	  return FALSE;
    }

    //previous transfer not done yet
    if(m_usTransLeft > 0){

        DWORD dwWait = WaitForSingleObject(m_hCompleteEvent, TASK_EXIT_TIME);
        if(dwWait != WAIT_OBJECT_0){
            //oops, we are in trouble now
            ERRORMSG(1, (_T("%s error occurred in the last transfer(total %d transfers left) task, pipes will be forced to reset!!!\r\n"), pszFname, m_usTransLeft));
            Cleanup(); //terminate all threads and clean up everything
            ResetPipe(); //clear pipes
            //re-initialize buffers, events and other sync objects.
            if(Init() == FALSE){ 
                ERRORMSG(1, (_T("%s can not start new transfers!!!\r\n"), pszFname));
                FUNCTION_LEAVE_MSG();
                return FALSE;				
            }

            //re-start those 4 threads
            if(StartThreads() == FALSE){ 
                ERRORMSG(1, (_T("%s can not start new transfers!!!\r\n"), pszFname));
                Cleanup();
                FUNCTION_LEAVE_MSG();
                return FALSE;				
            }
	 }
    }

    ResetEvent(m_hCompleteEvent);
    m_usTransRounds = m_usTransLeft = usTransNum;
    m_dwTransLen = dwTransLen;

    //prepare buffer
    DWORD cbBuf = ((dwTransLen*usTransNum)/usAlignment +1)*usAlignment;
    m_pBuffer = new BYTE[cbBuf];
    if(m_pBuffer == NULL){
        ERRORMSG(1, (_T("%s OOM!!!\r\n"), pszFname));
        Cleanup();
        FUNCTION_LEAVE_MSG();
        return FALSE;				
    }

    //prepar transfer notifications
    m_pTransNoti = new STRANSNOTI[usTransNum];
    if(m_pTransNoti == NULL){
        ERRORMSG(1, (_T("%s OOM!!!\r\n"), pszFname));
        Cleanup();
        FUNCTION_LEAVE_MSG();
        return FALSE;				
    }
    memset(m_pTransNoti, 0, sizeof(STRANSNOTI)*usTransNum);
    
    //if this is in pipe, we need to make data ready first
    if(m_bIn == TRUE){
        MakeInData(cbBuf, dwStartValue, usAlignment);
    }
    
    //notify issue-transfer thread and request handler
    SetEvent(m_hAckReadyEvent);
    SetEvent(m_hTaskReadyEvent);
    FUNCTION_LEAVE_MSG();
    return TRUE;
}

VOID
CSinglePipe::MakeInData(DWORD dwTotalLen, DWORD dwStartVal, USHORT usAlignment){
    if(dwTotalLen <=0)
        return;

    switch(usAlignment){
        case (sizeof(BYTE)):{ 
            BYTE StartVal = (BYTE)dwStartVal;
            PBYTE pTemp = m_pBuffer;
            for(DWORD i = 0; i < dwTotalLen/usAlignment; i++){
                *pTemp = StartVal;
                pTemp++;
                StartVal++;
            }
            break;
        }
        case (sizeof(WORD)): {
            WORD wStartVal = (WORD)dwStartVal;
            PWORD pwTemp = (PWORD)m_pBuffer;
            for(DWORD i = 0; i < dwTotalLen/usAlignment; i++){
                *pwTemp = wStartVal;
                pwTemp++;
                wStartVal++;
            }
            break;
        }
        case (sizeof(DWORD)): {
            DWORD dwlStartVal = dwStartVal;
            PDWORD pdwTemp = (PDWORD)m_pBuffer;
            for(DWORD i = 0; i < dwTotalLen/usAlignment; i++){
                *pdwTemp = dwlStartVal;
                pdwTemp++;
                dwlStartVal++;
            }
            break;
        }
        default:
            break;
    }

}

//----------------------------------------------------------------------------------
// ResetPipes: Clear IN/OUT pipe
//----------------------------------------------------------------------------------

VOID 
CSinglePipe::ResetPipe(){
    SETFNAME(_T("ResetPipe"));
    FUNCTION_ENTER_MSG();
	
    //Reset pipe is done by stall/clear pipe twice
    if(m_hPipe != NULL){
        DEBUGMSG(ZONE_PIPE, (_T("%s, Reset non-paired PIPE, hPipe = 0x%x \r\n"),
                                                    pszFname, m_hPipe));
        m_pUfnFuncs->lpStallPipe(m_hDevice, m_hPipe);
        m_pUfnFuncs->lpClearPipeStall(m_hDevice, m_hPipe);
        m_pUfnFuncs->lpStallPipe(m_hDevice, m_hPipe);
        m_pUfnFuncs->lpClearPipeStall(m_hDevice, m_hPipe);
    }
	
    FUNCTION_LEAVE_MSG();
}

//----------------------------------------------------------------------------------
// CloseOutPipe:Close OUT pipe
//----------------------------------------------------------------------------------

DWORD 
CSinglePipe::ClosePipe(){
    SETFNAME(_T("ClosePipe"));
    FUNCTION_ENTER_MSG();
    DWORD dwRet = ERROR_SUCCESS;
    if(m_hPipe != NULL){
        dwRet = m_pUfnFuncs->lpClosePipe(m_hDevice, m_hPipe);
        m_hPipe = NULL; 
        DEBUGMSG(ZONE_PIPE, (_T("%s, Close non-paired PIPE, hPipe = 0x%x \r\n"),
                                                    pszFname, m_hPipe));
    }
	
    FUNCTION_LEAVE_MSG();
    return dwRet;
}

//----------------------------------------------------------------------------------
// OutThreadProc:Thread that executes OUT-transfer function
//----------------------------------------------------------------------------------

DWORD
WINAPI
CSinglePipe::TransThreadProc(LPVOID dParam){
	CSinglePipe* threadPtr= (CSinglePipe *)dParam;
	DWORD exitCode=threadPtr-> RunTransThreadProc();
	::ExitThread(exitCode);
	return exitCode;
}


//----------------------------------------------------------------------------------
// RunOutThreadProc:Thread body that issues Out transfers
//----------------------------------------------------------------------------------

DWORD
CSinglePipe::RunTransThreadProc(){
    SETFNAME(_T("RunTransThreadProc"));
    FUNCTION_ENTER_MSG();
    DWORD  dwRet = 0;
    DWORD  dwWait = WAIT_OBJECT_0;
    BOOL     bError = FALSE;

    while ((m_bThreadsExit == FALSE) && (bError == FALSE)) {
        //wait for the next transfer task
        dwWait = WaitForSingleObject(m_hTaskReadyEvent, THREAD_IDLE_TIME);
        if(dwWait != WAIT_OBJECT_0){
            continue; //to check whether we should exit
	 }
        else{
            ResetEvent(m_hTaskReadyEvent);
        }
        DEBUGMSG(ZONE_TRANSFER, (_T("%s start executing %s transfer request, size = %d, rounds = %d\r\n"), 
			                                pszFname, (m_bIn == TRUE)?_T("IN"):_T("OUT"), m_dwTransLen, m_usTransRounds));
        USHORT usRounds = m_usTransRounds;
        USHORT usWrittenLen = 1;
        PBYTE   pCurBuf = m_pBuffer;
        UCHAR uIndex = 0;
	  //issue out transfers 
	  while(usRounds > 0 && m_bThreadsExit == FALSE){

             //guarantee we do not issue transfers more than function driver can handle
	      dwWait = WaitForSingleObject(m_hSemaphore, INFINITE);
            if(dwWait != WAIT_OBJECT_0){//sth. wrong
                bError = TRUE;
                break;
    	      }

            //issue transfer
            EnterCriticalSection(&m_Transcs);
            DEBUGMSG(ZONE_TRANSFER, (_T("%s transfer No. %d ready to issue\r\n"), pszFname, uIndex));
            m_pTransNoti[uIndex].pClass = (LPVOID)this;
            m_pTransNoti[uIndex].uTransIndex = uIndex;
                
            DWORD dwErr = ERROR_GEN_FAILURE;
            
            dwErr = m_pUfnFuncs->lpIssueTransfer(m_hDevice, m_hPipe, 
                                                  &CompleteCallback, (LPVOID)(&(m_pTransNoti[uIndex])), (m_bIn == TRUE)?USB_IN_TRANSFER:USB_OUT_TRANSFER,
                                                  (m_dwTransLen != -1)?m_dwTransLen:usWrittenLen, pCurBuf,  0, NULL, &(m_pTransNoti[uIndex].hTransfer));
            LeaveCriticalSection(&m_Transcs);

		DEBUGMSG(ZONE_TRANSFER, (_T("%s Isssue transfer: uIndex = %d, size = %d"), pszFname,  uIndex, (m_dwTransLen != -1)?m_dwTransLen:usWrittenLen));
	      if(dwErr != ERROR_SUCCESS ){//sth. wrong
	          ERRORMSG(1, (_T("%s Transfer failed!, error = 0x%x\r\n"), pszFname, dwErr));
                bError = TRUE;
		   break;
	      }

            DEBUGMSG(ZONE_TRANSFER, (_T("%s issue transfer index = %d, phTransfer = %p, *phTrasnfer = %p\r\n"), 
		                  pszFname, uIndex,  &(m_pTransNoti[uIndex].hTransfer), m_pTransNoti[uIndex].hTransfer));
			
	      usRounds --;
	      usWrittenLen ++;
	      uIndex++;
	      if(m_dwTransLen == -1){
                pCurBuf += usWrittenLen-1;
	      }
	      else{
	          pCurBuf += m_dwTransLen;
	      }
	      DEBUGMSG(ZONE_TRANSFER, (_T("%s Transfer  rounds left = %d\r\n"), pszFname, usRounds));
		  
	  }
    }

   FUNCTION_LEAVE_MSG();
   return 0;
}


//----------------------------------------------------------------------------------
// CompleteNotify: Transfer completion notify function
//----------------------------------------------------------------------------------

DWORD
WINAPI
CSinglePipe::CompleteCallback(LPVOID dParam){

	if(dParam == NULL)
	    return 0;
	PSTRANSNOTI  pNotiPtr = (PSTRANSNOTI)dParam;
	CSinglePipe* CSinglePipePtr = (CSinglePipe*)(pNotiPtr->pClass);
	DWORD exitCode=CSinglePipePtr-> CompleteNotify(pNotiPtr->uTransIndex);
	return exitCode;
}

//----------------------------------------------------------------------------------
// CompleteNotify: Transfer completion notify function
//----------------------------------------------------------------------------------

DWORD
CSinglePipe::CompleteNotify(UCHAR uTransIndex)
{   
    EnterCriticalSection(&m_TransCompletioncs);
    m_uCurIndex = uTransIndex;
    SetEvent(m_hTransCompleteEvent);
    LeaveCriticalSection(&m_TransCompletioncs);

    return ERROR_SUCCESS;
}

//----------------------------------------------------------------------------------
// CompleteThreadProc: Thread that executes Transfer completion function
//----------------------------------------------------------------------------------

DWORD
WINAPI
CSinglePipe::CompleteThreadProc(LPVOID dParam){
	if(dParam == NULL)
	    return -1;
	    
	CSinglePipe* threadPtr= (CSinglePipe *)dParam;
	DWORD exitCode=threadPtr-> RunCompleteThreadProc();
	::ExitThread(exitCode);
	return exitCode;
}


//----------------------------------------------------------------------------------
// RunCompleteThreadProc:Close transfer and cleanup buffer settings when  
// transfer is complete
//----------------------------------------------------------------------------------

DWORD 
CSinglePipe::RunCompleteThreadProc(){
    SETFNAME(_T("CompleteThreadProc"));
    FUNCTION_ENTER_MSG();
    DWORD  dwRet = 0;
    DWORD  dwWait = WAIT_OBJECT_0;
    UCHAR   uExpected = 0;
    DWORD  cbTransferred;
    DWORD  dwUsbError;
	
    while (m_bThreadsExit == FALSE) {
        //wait for the next transfer task
        dwWait = WaitForSingleObject(m_hTransCompleteEvent, THREAD_IDLE_TIME);
        if(dwWait != WAIT_OBJECT_0){
            continue; //to check whether we should exit
	 }

        //this is a little strange: what I am doing is to wait issuetransfer to be done
        //otherwise we may get an invalid transfer handle
        // TODO: may use event instead?
        EnterCriticalSection(&m_Transcs);
        LeaveCriticalSection(&m_Transcs);

        //following two statements have to be atomic, otherwise we may
        //miss some transfers
        EnterCriticalSection(&m_TransCompletioncs);
        UCHAR uIndexNow = m_uCurIndex;
        ResetEvent(m_hTransCompleteEvent);
        LeaveCriticalSection(&m_TransCompletioncs);

        //we may have more than one transfer done here		
        SHORT cbDone = (SHORT)((uIndexNow + 1) - uExpected);

        UCHAR uIndex = uExpected;
        DEBUGMSG(ZONE_TRANSFER, (_T("%s: num of done transfers = %d, TD start with index %d\r\n"), pszFname,  cbDone, uIndex));
        //complete these done transfers
        while(cbDone > 0){
            PUFN_TRANSFER phTransfer = &(m_pTransNoti[uIndex].hTransfer);
            DEBUGMSG(ZONE_TRANSFER, (_T("%s Transfer done  index = %d, phTransfer = %p, *phTrasnfer = %p\r\n"), 
		                               pszFname, uIndex, phTransfer, *phTransfer));

            if(*phTransfer != NULL){	
                //get transfer status and close pipe
                DWORD dwErr = m_pUfnFuncs->lpGetTransferStatus(m_hDevice, *phTransfer, &cbTransferred, &dwUsbError);
                if(dwErr != ERROR_SUCCESS){//failed, but need to close it anyway
                    ERRORMSG(1, (_T("%s ERROR: Transfer hTrasnfer = %p got USB error = 0x%x\r\n"), 
    		                               pszFname, *phTransfer, dwUsbError));
                }
                else{
                    DEBUGMSG(ZONE_TRANSFER, (_T("%s %u bytes received!\r\n"), pszFname, cbTransferred));
                   if((m_dwTransLen != -1) && cbTransferred != m_dwTransLen){//we may lost data?
                        ERRORMSG(1, (_T("%s ERROR: Expected bytes received is %u, some data may lost !!!\r\n"), pszFname, m_dwTransLen));
                    }
    				
                }

                dwErr = m_pUfnFuncs->lpCloseTransfer(m_hDevice, *phTransfer);
                if(dwErr != ERROR_SUCCESS){//failed, but need to close it anyway
                    ERRORMSG(1, (_T("%s ERROR: Transfer hTrasnfer = %p failed to close\r\n"), 
    		                               pszFname, *phTransfer));
                }

                //clear transfer handle in TD
                *phTransfer = NULL;
            }
            
             //allow another transfer to proceed
	       ReleaseSemaphore(m_hSemaphore, 1, NULL);
		//head to the next done transfer	 
	      uIndex = uIndex +1;
            cbDone --;
            m_usTransLeft --; 
            if(m_usTransLeft == 0){
                delete[] m_pBuffer;
                m_pBuffer = NULL;
                delete[] m_pTransNoti;
                m_pTransNoti = NULL;
                uIndex = 0;
                SetEvent(m_hCompleteEvent);
            }
        }
        //next expected TD with done transfer.
        uExpected = uIndex;
    }

   FUNCTION_LEAVE_MSG();
   return 0;
}




//----------------------------------------------------------------------------------
// TD_Init:  Allocates memory for TD list and initialize events and othe variables in TDs
//----------------------------------------------------------------------------------
BOOL 
CPipePair::TD_Init(){

    SETFNAME(_T("TD_Init"));
    FUNCTION_ENTER_MSG();
    BOOL  bRet = TRUE;
    int	i = 0;

    DEBUGCHK(m_uQueueSize > 0);
    DEBUGCHK(m_dwBufLen > 0);

    //try to allocate transfer descriptor list
    m_pTDList = NULL;
    m_pTDList = (PTRANSDESCRIPTOR)LocalAlloc(LPTR, sizeof(TRANSDESCRIPTOR)*m_uQueueSize);
    if(m_pTDList == NULL){//failed
        ERRORMSG(1, (_T("%s out of memory\r\n"), pszFname));
        bRet = FALSE;
        goto EXIT;
    }

    //initialize TD list	
    memset(m_pTDList, 0, sizeof(TRANSDESCRIPTOR)*m_uQueueSize);
	
     //setup each TD   
    for(i = 0; i < m_uQueueSize; i ++){
        //allocate data buffer
        m_pTDList[i].pBuff = (PBYTE)LocalAlloc(LPTR, m_dwBufLen);
        if(m_pTDList[i].pBuff == NULL){
            ERRORMSG(1, (_T("%s out of memory\r\n"), pszFname));
            bRet = FALSE;
            goto EXIT;
        }
        //initilize members
        m_pTDList[i].dwBytes = 0;
        m_pTDList[i].uStatus = TD_STATUS_WRITABLE; //buffers are initially empty

        //create read/write events
        m_pTDList[i].hReadEvent= CreateEvent(0, FALSE, FALSE, NULL);
        m_pTDList[i].hWriteEvent= CreateEvent(0, FALSE, FALSE, NULL);
        if((m_pTDList[i].hReadEvent == NULL) || (m_pTDList[i].hWriteEvent == NULL) ){
            ERRORMSG(1, (_T("%s failed to create event(s)\r\n"), pszFname));
            bRet = FALSE;
            goto EXIT;
        }
   }
	
EXIT:

    if(bRet == FALSE){//something wrong 
        TD_Cleanup();
    }
	
    FUNCTION_LEAVE_MSG();
    return bRet;
}


//----------------------------------------------------------------------------------
// TD_WaitOutBuffReady:  Waits the next TD ready for OUT transfer, return buffer pointer, size and
// TD index. These values are needed for issuing an OUT transfer
//----------------------------------------------------------------------------------
BOOL 
CPipePair::TD_WaitOutTDReady(PBYTE* ppData, PUCHAR puIndex){

    if(ppData == NULL || puIndex == NULL)
        return FALSE;
        
    SETFNAME(_T("TD_WaitOutTDReady"));
    FUNCTION_ENTER_MSG();
    BOOL  bRet = TRUE;
    UCHAR uIndex = m_uNextWrIndex;

    //Check TD status, if it is not ready, then wait 
    if( m_pTDList[uIndex].uStatus != TD_STATUS_WRITABLE){
        DEBUGMSG(ZONE_COMMENT, (_T("%s wait for write event %d\r\n"), pszFname, uIndex));
        DWORD dwWait = WaitForSingleObject(m_pTDList[uIndex].hWriteEvent, INFINITE);
        if (dwWait != WAIT_OBJECT_0) {
            ERRORMSG(1, (_T("%s Error waiting write ready event. Error = %d\r\n"), 
            					pszFname, dwWait));
            bRet = FALSE;
            goto EXIT;
        }
    }

    //now retrieve those needed values
    EnterCriticalSection(&m_TDcs);
    ResetEvent(m_pTDList[uIndex].hWriteEvent);
    *ppData = m_pTDList[uIndex].pBuff;
    *puIndex = uIndex;
    m_pTDList[uIndex].uStatus = TD_STATUS_WRITING;  //change status
    m_uNextWrIndex = ((uIndex+1) == m_uQueueSize)?0:(uIndex+1); //point to TD for the next  out
    LeaveCriticalSection(&m_TDcs);

EXIT:
    FUNCTION_LEAVE_MSG();
    return bRet;
	
}

BOOL 
CPipePair::TD_WaitInTDReady(PBYTE* ppData, PUCHAR puIndex){
    if(ppData == NULL || puIndex == NULL)
        return FALSE;
        
    SETFNAME(_T("TD_WaitInTDReady"));
    FUNCTION_ENTER_MSG();
    BOOL  bRet = TRUE;
    UCHAR uIndex = m_uNextRdIndex;

    //Check TD status, if it is not ready, then wait 
    if( m_pTDList[uIndex].uStatus != TD_STATUS_READABLE){
        DEBUGMSG(ZONE_COMMENT, (_T("%s wait for read event %d\r\n"), pszFname, uIndex));
        DWORD dwWait = WaitForSingleObject(m_pTDList[uIndex].hReadEvent, INFINITE);
        if (dwWait != WAIT_OBJECT_0) {
            ERRORMSG(1, (_T("%s Error waiting write ready event. Error = %d\r\n"), 
            					pszFname, dwWait));
            bRet = FALSE;
            goto EXIT;
        }
    }

    //now retrieve those needed values
    EnterCriticalSection(&m_TDcs);
    ResetEvent(m_pTDList[uIndex].hReadEvent);
    *ppData = m_pTDList[uIndex].pBuff;
    *puIndex = uIndex;
    m_pTDList[uIndex].uStatus = TD_STATUS_READING;  //change status
    m_uNextRdIndex = ((uIndex+1) == m_uQueueSize)?0:(uIndex+1); //point to TD for the next  out
    LeaveCriticalSection(&m_TDcs);

EXIT:
    FUNCTION_LEAVE_MSG();
    return bRet;
}

//----------------------------------------------------------------------------------
// TD_OutTransferComplete: make nessecary status and setup changes for the TD that 
// its related Out transfer has done
//----------------------------------------------------------------------------------

VOID 
CPipePair::TD_OutTransferComplete(UCHAR uIndex, DWORD dwSize){
    SETFNAME(_T("TD_OutTransferComplete"));
    FUNCTION_ENTER_MSG();

    DEBUGCHK(dwSize >= 0);
    EnterCriticalSection(&m_TDcs);
    m_pTDList[uIndex].uStatus = TD_STATUS_READABLE; //contents ready to send back
    m_pTDList[uIndex].dwBytes = dwSize; 

    //notify TD read ready event (for OUT)
    SetEvent(m_pTDList[uIndex].hReadEvent);
    DEBUGMSG(ZONE_COMMENT, (_T("%s set for read event %d\r\n"), pszFname, uIndex));

    LeaveCriticalSection(&m_TDcs);

    FUNCTION_LEAVE_MSG();
}


//----------------------------------------------------------------------------------
// TD_InTransferComplete: make nessecary status and setup changes for the TD that 
// its related In transfer has done
//----------------------------------------------------------------------------------

VOID 
CPipePair::TD_InTransferComplete(UCHAR uIndex, DWORD dwSize){
    SETFNAME(_T("TD_InTransferComplete"));
    FUNCTION_ENTER_MSG();

    DEBUGCHK(dwSize >= 0);
    EnterCriticalSection(&m_TDcs);
    m_pTDList[uIndex].uStatus = TD_STATUS_WRITABLE; //TD ready to get data
    m_pTDList[uIndex].dwBytes = 0;

    //notify TD write ready event (for OUT)
    SetEvent(m_pTDList[uIndex].hWriteEvent);
    DEBUGMSG(ZONE_COMMENT, (_T("%s set for write event %d\r\n"), pszFname, uIndex));
	
    LeaveCriticalSection(&m_TDcs);

    FUNCTION_LEAVE_MSG();
}


//----------------------------------------------------------------------------------
// TD_Cleanup: Free all memory allocated for TD list and close all event handles 
// its related In transfer has done
//----------------------------------------------------------------------------------

VOID 
CPipePair::TD_Cleanup(){
    SETFNAME(_T("TD_Cleanup"));
    FUNCTION_ENTER_MSG();

    if(m_pTDList == NULL)
        return;

    for(int i = 0; i < m_uQueueSize; i++){
        if(m_pTDList[i].hTransfer != NULL){//clean unfinishsed transfers
            m_pUfnFuncs->lpAbortTransfer(m_hDevice, m_pTDList[i].hTransfer);
            m_pTDList[i].hTransfer = NULL;
        }
        if(m_pTDList[i].pBuff != NULL){
            LocalFree(m_pTDList[i].pBuff);
        }
        if(m_pTDList[i].hReadEvent != NULL){
            CloseHandle(m_pTDList[i].hReadEvent);
        }
        if(m_pTDList[i].hWriteEvent != NULL){
            CloseHandle(m_pTDList[i].hWriteEvent);
        }
    }

    LocalFree(m_pTDList);
	
    FUNCTION_LEAVE_MSG();
}


//----------------------------------------------------------------------------------
// Init: Initialize this pipe management instance
//----------------------------------------------------------------------------------

BOOL 
CPipePair::Init(){
    SETFNAME(_T("Init"));
    FUNCTION_ENTER_MSG();
    BOOL  bRet = TRUE;

    if(m_hDevice == NULL || m_pUfnFuncs == NULL){
        ERRORMSG(1, (_T("%s invalid Device handle or function table!!! \r\n"), pszFname));
        return FALSE;	
    }

    if(TD_Init() == FALSE){//failed to initialize TD list
        ERRORMSG(1, (_T("%s Error in TD initialization r\n"), pszFname));
        return FALSE;	
    }

    //initialize critical sections
    InitializeCriticalSection(&m_TDcs);
    InitializeCriticalSection(&m_Incs);
    InitializeCriticalSection(&m_Outcs);
    InitializeCriticalSection(&m_InCompletioncs);
    InitializeCriticalSection(&m_OutCompletioncs);
	
    //initialize semaphores
    m_hInSemaphore = CreateSemaphore(NULL, m_uQueueDepth, m_uQueueDepth, NULL);
    m_hOutSemaphore = CreateSemaphore(NULL, m_uQueueDepth, m_uQueueDepth, NULL);
    if (m_hOutSemaphore == NULL || m_hInSemaphore == NULL ) {
        ERRORMSG(1, (_T("%s Error creating semaphore. Error = %d\r\n"), 
            					pszFname, GetLastError()));
        bRet = FALSE;
        goto EXIT;
    }
    		
    //create in/out task ready notify events
    m_hInTaskReadyEvent = CreateEvent(0, FALSE, FALSE, NULL);
    m_hOutTaskReadyEvent = CreateEvent(0, FALSE, FALSE, NULL);
    if((m_hInTaskReadyEvent == NULL) || (m_hOutTaskReadyEvent == NULL) ){
        ERRORMSG(1, (_T("%s failed to create event(s)\r\n"), pszFname));
        bRet = FALSE;
        goto EXIT;
    }

    //Task read/completion notify events
    m_hCompleteEvent = CreateEvent(0, FALSE, FALSE, NULL);
    m_hAckReadyEvent = CreateEvent(0, FALSE, FALSE, NULL);
    if((m_hCompleteEvent == NULL) ||( m_hAckReadyEvent == NULL)){
        ERRORMSG(1, (_T("%s failed to create event(s)\r\n"), pszFname));
        bRet = FALSE;
        goto EXIT;
    }

    //create in/out completion notify events
    m_hInCompleteEvent = CreateEvent(0, FALSE, FALSE, NULL);
    m_hOutCompleteEvent = CreateEvent(0, FALSE, FALSE, NULL);
    if((m_hInCompleteEvent == NULL) || (m_hOutCompleteEvent == NULL) ){
        ERRORMSG(1, (_T("%s failed to create event(s)\r\n"), pszFname));
        bRet = FALSE;
        goto EXIT;
    }

EXIT:
    if(bRet == FALSE){
        Cleanup();
    }
    FUNCTION_LEAVE_MSG();
    return bRet;
}


//----------------------------------------------------------------------------------
// Cleanup: Cleanup this pipe management instance
//----------------------------------------------------------------------------------

VOID 
CPipePair::Cleanup(){
    SETFNAME(_T("Cleanup"));
    FUNCTION_ENTER_MSG();
    BOOL  bRet = TRUE;
    DWORD dwWait, dwExitCode;

    //notify all threads to exit
    m_bThreadsExit = TRUE;

    //now try to close all those 4 threads
    if(m_hOutCompleteThread){
        dwWait = WaitForSingleObject(m_hOutCompleteThread, THREAD_IDLE_TIME);
        if(dwWait != WAIT_OBJECT_0){//we have trouble on this thread
            ERRORMSG(1, (_T("%s OUT transfer complete process thread will be terminated!!!\r\n"), pszFname));
            ::GetExitCodeThread(m_hOutCompleteThread, &dwExitCode);
            ::TerminateThread(m_hOutCompleteThread, dwExitCode);
	 }
        else{
            CloseHandle(m_hOutCompleteThread);
        }
        m_hOutCompleteThread = NULL;
    }
    if(m_hOutThread){
        dwWait = WaitForSingleObject(m_hOutThread, THREAD_IDLE_TIME);
        if(dwWait != WAIT_OBJECT_0){//we have trouble on this thread
            ERRORMSG(1, (_T("%s OUT transfer process thread will be terminated!!!\r\n"), pszFname));
            ::GetExitCodeThread(m_hOutThread, &dwExitCode);
            ::TerminateThread(m_hOutThread, dwExitCode);
	 }
        else{
            CloseHandle(m_hOutThread);
        }
        m_hOutThread = NULL;
    }
    if(m_hInCompleteThread){
        dwWait = WaitForSingleObject(m_hInCompleteThread, THREAD_IDLE_TIME);
        if(dwWait != WAIT_OBJECT_0){//we have trouble on this thread
            ERRORMSG(1, (_T("%s IN transfer complete process thread will be terminated!!!\r\n"), pszFname));
            ::GetExitCodeThread(m_hInCompleteThread, &dwExitCode);
            ::TerminateThread(m_hInCompleteThread, dwExitCode);
	 }
        else{
            CloseHandle(m_hInCompleteThread);
        }
        m_hInCompleteThread = NULL;
    }
    if(m_hInThread){
        dwWait = WaitForSingleObject(m_hInThread, THREAD_IDLE_TIME);
        if(dwWait != WAIT_OBJECT_0){//we have trouble on this thread
            ERRORMSG(1, (_T("%s IN transfer process thread will be terminated!!!\r\n"), pszFname));
            ::GetExitCodeThread(m_hInThread, &dwExitCode);
            ::TerminateThread(m_hInThread, dwExitCode);
	 }
        else{
            CloseHandle(m_hInThread);
        }
        m_hInThread = NULL;
    }

    //Free TD list
    TD_Cleanup();

    //free critical sections
    DeleteCriticalSection(&m_TDcs);
    DeleteCriticalSection(&m_Incs);
    DeleteCriticalSection(&m_Outcs);
    DeleteCriticalSection(&m_InCompletioncs);
    DeleteCriticalSection(&m_OutCompletioncs);
	
    //Close semaphores
    if(m_hInSemaphore != NULL){
        CloseHandle(m_hInSemaphore);
    }    		
    if(m_hOutSemaphore != NULL){
        CloseHandle(m_hOutSemaphore);
    }
	
    //close all events
    if(m_hInCompleteEvent != NULL){
        CloseHandle(m_hInCompleteEvent);
    }    		
    if(m_hOutCompleteEvent != NULL){
        CloseHandle(m_hOutCompleteEvent);
    }    		
    if(m_hInTaskReadyEvent != NULL){
        CloseHandle(m_hInTaskReadyEvent);
    }    		
    if(m_hOutTaskReadyEvent != NULL){
        CloseHandle(m_hOutTaskReadyEvent);
    }    		
    if(m_hCompleteEvent != NULL){
        CloseHandle(m_hCompleteEvent);
    }    		
    if(m_hAckReadyEvent != NULL){
        CloseHandle(m_hAckReadyEvent);
    }    		

    m_usTransLeft = 0;
    
    FUNCTION_LEAVE_MSG();
}


//----------------------------------------------------------------------------------
// StartThreads:Launch Out transfer thread and In/Out transfer complete threads
//----------------------------------------------------------------------------------

BOOL 
CPipePair::StartThreads(){
    SETFNAME(_T("StartThreads"));
    FUNCTION_ENTER_MSG();
    BOOL  bRet = TRUE;

    m_bThreadsExit = FALSE;

    //start out transfer process thread
    if(m_hOutThread == NULL){	
        m_hOutThread = ::CreateThread(NULL, 0, CPipePair::OutThreadProc, (LPVOID)this, 0, NULL);
        if (m_hOutThread == NULL) {
            ERRORMSG(1, (_T("%s Out process thread creation failed\r\n"), pszFname));
    	      bRet = FALSE;
    	      goto EXIT;
        }
    }
    //start in transfer process thread
    if(m_hInThread == NULL){	
        m_hInThread = ::CreateThread(NULL, 0, CPipePair::InThreadProc, (LPVOID)this, 0, NULL);
        if (m_hInThread == NULL) {
            ERRORMSG(1, (_T("%s in process thread creation failed\r\n"), pszFname));
      	      bRet = FALSE;
            goto EXIT;
        }
    }
	
    //start Out transfer completion thread
    if(m_hOutCompleteThread == NULL){	
        m_hOutCompleteThread = CreateThread(NULL, 0, CPipePair::OutCompleteThreadProc, (LPVOID)this, 0, NULL);
        if (m_hOutCompleteThread == NULL) {
            ERRORMSG(1, (_T("%s Out completion process thread creation failed\r\n"), pszFname));
    	      bRet = FALSE;
    	      goto EXIT;
        }
    }
    //start In transfer completion thread
    if(m_hInCompleteThread == NULL){	
        m_hInCompleteThread = CreateThread(NULL, 0, CPipePair::InCompleteThreadProc, (LPVOID)this, 0, NULL);
        if (m_hInCompleteThread == NULL) {
            ERRORMSG(1, (_T("%s Out completion process thread creation failed\r\n"), pszFname));
    	      bRet = FALSE;
        }
    }
EXIT:
   if(bRet == FALSE)
   	Cleanup();
   FUNCTION_LEAVE_MSG();
   return bRet;
}


//----------------------------------------------------------------------------------
// StartNewTransfers:Notify IN/OUT transfer thread to start to execute new request from host side
//----------------------------------------------------------------------------------

BOOL 
CPipePair::StartNewTransfers(DWORD dwTransLen, USHORT usTransNum){
    SETFNAME(_T("StartNewTransfers"));
    FUNCTION_ENTER_MSG();

    if((dwTransLen == 0) || (usTransNum == 0) ){
        ERRORMSG(1, (_T("%s invalid parameter!\r\n"), pszFname));
        FUNCTION_LEAVE_MSG();
	  return FALSE;
    }

    //previous transfer not done yet
    if(m_usTransLeft > 0){

        DWORD dwWait = WaitForSingleObject(m_hCompleteEvent, TASK_EXIT_TIME);
        if(dwWait != WAIT_OBJECT_0){
            //oops, we are in trouble now
            ERRORMSG(1, (_T("%s error occurred in the last transfer task, pipes will be forced to reset!!!\r\n"), pszFname));
            Cleanup(); //terminate all threads and clean up everything
            ResetPipes(); //clear pipes
            //re-initialize buffers, events and other sync objects.
            if(Init() == FALSE){ 
                ERRORMSG(1, (_T("%s can not start new transfers!!!\r\n"), pszFname));
                FUNCTION_LEAVE_MSG();
                return FALSE;				
            }

            //re-start those 4 threads
            if(StartThreads() == FALSE){ 
                ERRORMSG(1, (_T("%s can not start new transfers!!!\r\n"), pszFname));
                Cleanup();
                FUNCTION_LEAVE_MSG();
                return FALSE;				
            }
            m_uNextRdIndex = m_uNextWrIndex = 0;
	 }
    }

    ResetEvent(m_hCompleteEvent);
    m_usTransRounds = m_usTransLeft = usTransNum;
    m_dwTransLen = dwTransLen;

    //notify issue-transfer thread and request handler
    SetEvent(m_hAckReadyEvent);
    SetEvent(m_hOutTaskReadyEvent);
    SetEvent(m_hInTaskReadyEvent);
    FUNCTION_LEAVE_MSG();
    return TRUE;
}

//----------------------------------------------------------------------------------
// ResetPipes: Clear IN/OUT pipe
//----------------------------------------------------------------------------------

VOID 
CPipePair::ResetPipes(){
    SETFNAME(_T("ResetPipes"));
    FUNCTION_ENTER_MSG();
	
    //Reset pipe is done by stall/clear pipe twice
    if(m_hOutPipe != NULL){
        DEBUGMSG(ZONE_PIPE, (_T("%s, Reset OUT PIPE, hPipe = 0x%x \r\n"),
                                                    pszFname, m_hOutPipe));
        m_pUfnFuncs->lpStallPipe(m_hDevice, m_hOutPipe);
        m_pUfnFuncs->lpClearPipeStall(m_hDevice, m_hOutPipe);
        m_pUfnFuncs->lpStallPipe(m_hDevice, m_hOutPipe);
        m_pUfnFuncs->lpClearPipeStall(m_hDevice, m_hOutPipe);
    }
	
    if(m_hInPipe != NULL){
        DEBUGMSG(ZONE_PIPE, (_T("%s, Reset IN PIPE, hPipe = 0x%x \r\n"),
                                                    pszFname, m_hInPipe));
        m_pUfnFuncs->lpStallPipe(m_hDevice, m_hInPipe);
        m_pUfnFuncs->lpClearPipeStall(m_hDevice, m_hInPipe);
        m_pUfnFuncs->lpStallPipe(m_hDevice, m_hInPipe);
        m_pUfnFuncs->lpClearPipeStall(m_hDevice, m_hInPipe);
    }
	
    FUNCTION_LEAVE_MSG();
}

//----------------------------------------------------------------------------------
// CloseOutPipe:Close OUT pipe
//----------------------------------------------------------------------------------

DWORD 
CPipePair::CloseOutPipe(){
    SETFNAME(_T("CloseOutPipe"));
    FUNCTION_ENTER_MSG();
    DWORD dwRet = ERROR_SUCCESS;
    if(m_hOutPipe != NULL){
        dwRet = m_pUfnFuncs->lpClosePipe(m_hDevice, m_hOutPipe);
        DEBUGMSG(ZONE_PIPE, (_T("%s, Close OUT PIPE, hPipe = 0x%x \r\n"),
                                                    pszFname, m_hOutPipe));
        m_hOutPipe = NULL; 
    }
	
    FUNCTION_LEAVE_MSG();
    return dwRet;
}

//----------------------------------------------------------------------------------
// CloseInPipe:Close IN pipe
//----------------------------------------------------------------------------------

DWORD 
CPipePair::CloseInPipe(){
    SETFNAME(_T("CloseInPipe"));
    FUNCTION_ENTER_MSG();
    DWORD dwRet = ERROR_SUCCESS;
    if(m_hInPipe != NULL){
        dwRet = m_pUfnFuncs->lpClosePipe(m_hDevice, m_hInPipe);
        DEBUGMSG(ZONE_PIPE, (_T("%s, Close IN PIPE, hPipe = 0x%x \r\n"),
                                                    pszFname, m_hInPipe));
        m_hInPipe = NULL; 
    }
	
    FUNCTION_LEAVE_MSG();
    return dwRet;
}



//----------------------------------------------------------------------------------
// OutThreadProc:Thread that executes OUT-transfer function
//----------------------------------------------------------------------------------

DWORD
WINAPI
CPipePair::OutThreadProc(LPVOID dParam){
	CPipePair* threadPtr= (CPipePair *)dParam;
	DWORD exitCode=threadPtr-> RunOutThreadProc();
	::ExitThread(exitCode);
	return exitCode;
}


//----------------------------------------------------------------------------------
// RunOutThreadProc:Thread body that issues Out transfers
//----------------------------------------------------------------------------------

DWORD
CPipePair::RunOutThreadProc(){
    SETFNAME(_T("OutThreadProc"));
    FUNCTION_ENTER_MSG();
    DWORD  dwRet = 0;
    DWORD  dwWait = WAIT_OBJECT_0;
    BOOL     bError = FALSE;

    while ((m_bThreadsExit == FALSE) && (bError == FALSE)) {
        //wait for the next transfer task
        dwWait = WaitForSingleObject(m_hOutTaskReadyEvent, THREAD_IDLE_TIME);
        if(dwWait != WAIT_OBJECT_0){
            continue; //to check whether we should exit
	 }
        else{
            ResetEvent(m_hOutTaskReadyEvent);
        }
        DEBUGMSG(ZONE_TRANSFER, (_T("%s start executing OUT transfer request, size = %d, rounds = %d\r\n"), 
			                                pszFname, m_dwTransLen, m_usTransRounds));
        USHORT usRounds = m_usTransRounds;
        USHORT usWrittenLen = 1;
	  //issue out transfers 
	  while(usRounds > 0 && m_bThreadsExit == FALSE){
	      UCHAR	uIndex = 0;
	      PBYTE	pData = NULL;
	      BOOL      bRet = FALSE;

             //guarantee we do not issue transfers more than function driver can handle
	      dwWait = WaitForSingleObject(m_hOutSemaphore, INFINITE);
            if(dwWait != WAIT_OBJECT_0){//sth. wrong
                bError = TRUE;
                break;
    	      }

            //get next TD			
	      DEBUGMSG(ZONE_TRANSFER, (_T("%s Out transfer before check TD  ready\r\n"), pszFname));
            bRet = TD_WaitOutTDReady(&pData, &uIndex);
	      if(bRet == FALSE){//sth. wrong
	          ERRORMSG(1, (_T("%s Out transfer check TD  ready failed!\r\n"), pszFname));
                bError = TRUE;
		   break;
	      }

            //issue OUT transfer
            EnterCriticalSection(&m_Outcs);
            DEBUGMSG(ZONE_TRANSFER, (_T("%s Out transfer ready to issue, using TD %d\r\n"), pszFname, uIndex));
            m_pTDList[uIndex].TransNoti.pClass = (LPVOID)this;
            m_pTDList[uIndex].TransNoti.uTransIndex = uIndex;
            if(m_dwTransLen == -1)
                m_pTDList[uIndex].usLen = usWrittenLen;
                
            DWORD dwErr = ERROR_GEN_FAILURE;
            
            if(m_hOutPipe == NULL || m_bOutFaked == TRUE){//we need to "create" OUT data here
                //fill in data
                PBYTE pCur = m_pTDList[uIndex].pBuff;
                DWORD dwTempLen = (m_dwTransLen != (DWORD) -1)?m_dwTransLen:(DWORD)usWrittenLen;
                for(DWORD dwi = 0; dwi < dwTempLen; dwi++){
                    pCur[dwi] = m_StartByte++;
                }

                //notify completion function
                OutCompleteNotify(uIndex);

                dwErr = ERROR_SUCCESS;
            }
            else{
                dwErr = m_pUfnFuncs->lpIssueTransfer(m_hDevice, m_hOutPipe, 
                                                      &OutCompleteCallback, (LPVOID)(&(m_pTDList[uIndex].TransNoti)), USB_OUT_TRANSFER,
                                                      (m_dwTransLen != -1)?m_dwTransLen:usWrittenLen, pData,  0, NULL, &(m_pTDList[uIndex].hTransfer));
            }
            LeaveCriticalSection(&m_Outcs);
			
	      if(dwErr != ERROR_SUCCESS ){//sth. wrong
	          ERRORMSG(1, (_T("%s Out transfer failed!, error = 0x%x\r\n"), pszFname, dwErr));
                bError = TRUE;
		   break;
	      }

            DEBUGMSG(ZONE_TRANSFER, (_T("%s issue out transfer index = %d, phTransfer = %p, *phTrasnfer = %p\r\n"), 
		                  pszFname, uIndex,  &(m_pTDList[uIndex].hTransfer), m_pTDList[uIndex].hTransfer));
			
	      usRounds --;
	      usWrittenLen ++;
	      DEBUGMSG(ZONE_TRANSFER, (_T("%s out transfer  rounds left = %d\r\n"), pszFname, usRounds));
		  
	  }
    }

   FUNCTION_LEAVE_MSG();
   return 0;
}


//----------------------------------------------------------------------------------
// OutCompleteNotify: OUT transfer completion notify function
//----------------------------------------------------------------------------------

DWORD
WINAPI
CPipePair::OutCompleteCallback(LPVOID dParam){

	if(dParam == NULL)
	    return 0;
	PTRANSNOTI  pNotiPtr = (PTRANSNOTI)dParam;
	CPipePair* cPipePairPtr = (CPipePair*)(pNotiPtr->pClass);
	DWORD exitCode=cPipePairPtr-> OutCompleteNotify(pNotiPtr->uTransIndex);
	return exitCode;
}

//----------------------------------------------------------------------------------
// OutCompleteNotify: OUT transfer completion notify function
//----------------------------------------------------------------------------------

DWORD
CPipePair::OutCompleteNotify(UCHAR uTransIndex)
{   
    EnterCriticalSection(&m_OutCompletioncs);
    m_uCurOutIndex = uTransIndex;
    SetEvent(m_hOutCompleteEvent);
    LeaveCriticalSection(&m_OutCompletioncs);

    return ERROR_SUCCESS;
}

//----------------------------------------------------------------------------------
// OutCompleteThreadProc: Thread that executes OUT transfer completion function
//----------------------------------------------------------------------------------

DWORD
WINAPI
CPipePair::OutCompleteThreadProc(LPVOID dParam){
	if(dParam == NULL)
	    return 0;
	CPipePair* threadPtr= (CPipePair *)dParam;
	DWORD exitCode=threadPtr-> RunOutCompleteThreadProc();
	::ExitThread(exitCode);
	return exitCode;
}


//----------------------------------------------------------------------------------
// RunOutCompleteThreadProc:Close transfer and cleanup buffer settings when OUT 
// transfer is complete
//----------------------------------------------------------------------------------

DWORD 
CPipePair::RunOutCompleteThreadProc(){
    SETFNAME(_T("OutCompleteThreadProc"));
    FUNCTION_ENTER_MSG();
    DWORD  dwRet = 0;
    DWORD  dwWait = WAIT_OBJECT_0;
    UCHAR   uExpected = 0;
    DWORD  cbTransferred;
    DWORD  dwUsbError;
	
    while (m_bThreadsExit == FALSE) {
        //wait for the next transfer task
        dwWait = WaitForSingleObject(m_hOutCompleteEvent, THREAD_IDLE_TIME);
        if(dwWait != WAIT_OBJECT_0){
            continue; //to check whether we should exit
	 }

        //this is a little strange: what I am doing is to wait issuetransfer to be done
        //otherwise we may get an invalid transfer handle
        EnterCriticalSection(&m_Outcs);
        LeaveCriticalSection(&m_Outcs);

        //following two statements have to be atomic, otherwise we may
        //miss some transfers
        EnterCriticalSection(&m_OutCompletioncs);
        UCHAR uIndexNow = m_uCurOutIndex;
        ResetEvent(m_hOutCompleteEvent);
        LeaveCriticalSection(&m_OutCompletioncs);

        //we may have more than one transfer done here		
        SHORT cbDone = (SHORT)((uIndexNow + 1) - uExpected);
        if(cbDone <= 0) 
		cbDone += m_uQueueSize;
        UCHAR uIndex = uExpected;
        DEBUGMSG(ZONE_TRANSFER, (_T("%s: num of done transfers = %d, TD start with index %d\r\n"), pszFname,  cbDone, uIndex));
        //complete these done transfers
        while(cbDone > 0){
            PUFN_TRANSFER phTransfer = &(m_pTDList[uIndex].hTransfer);
            DEBUGMSG(ZONE_TRANSFER, (_T("%s out transfer done  index = %d, phTransfer = %p, *phTrasnfer = %p\r\n"), 
		                               pszFname, uIndex, phTransfer, *phTransfer));

            if(*phTransfer != NULL){	
                //get transfer status and close pipe
                DWORD dwErr = m_pUfnFuncs->lpGetTransferStatus(m_hDevice, *phTransfer, &cbTransferred, &dwUsbError);
                if(dwErr != ERROR_SUCCESS){//failed, but need to close it anyway
                    ERRORMSG(1, (_T("%s ERROR: out transfer hTrasnfer = %p got USB error = 0x%x\r\n"), 
    		                               pszFname, *phTransfer, dwUsbError));
                }
                else{
                    DEBUGMSG(ZONE_TRANSFER, (_T("%s %u bytes received!\r\n"), pszFname, cbTransferred));
                    if((m_dwTransLen != -1) && cbTransferred != m_dwTransLen){//we may lost data?
                        ERRORMSG(1, (_T("%s ERROR: pipe: 0x%x, Expected bytes received from host is %u, actual recieved %u, some data may lost !!!\r\n"), pszFname, m_hOutPipe, m_dwTransLen, cbTransferred));
                    }
    				
                }

                dwErr = m_pUfnFuncs->lpCloseTransfer(m_hDevice, *phTransfer);
                if(dwErr != ERROR_SUCCESS){//failed, but need to close it anyway
                    ERRORMSG(1, (_T("%s ERROR: out transfer hTrasnfer = %p failed to close\r\n"), 
    		                               pszFname, *phTransfer));
                }

                DEBUGMSG(ZONE_TRANSFER, (_T("Got Out data, uindex = %d, buffer[0] = %d"), uIndex, m_pTDList[uIndex].pBuff[0]));
                //clear transfer handle in TD
                *phTransfer = NULL;
            }
            else if(m_bOutFaked == TRUE || m_hOutPipe == NULL){//simulated OUT transfer
                if(m_dwTransLen != -1)
                    cbTransferred = m_dwTransLen;
            }
            else{//this should not happen
                DEBUGCHK(FALSE);
            }
            
             //update TD contents
             TD_OutTransferComplete(uIndex, cbTransferred);
             //allow another transfer to proceed
	       ReleaseSemaphore(m_hOutSemaphore, 1, NULL);
		//head to the next done transfer	 
	       uIndex = (uIndex +1) % m_uQueueSize;
	       cbDone --;
        }
        //next expected TD with done transfer.
        uExpected = uIndex;
    }

   FUNCTION_LEAVE_MSG();
   return 0;
}

//----------------------------------------------------------------------------------
// InThreadProc:Thread that executes IN-transfer function
//----------------------------------------------------------------------------------

DWORD
WINAPI
CPipePair::InThreadProc(LPVOID dParam){
	DEBUGCHK(dParam != NULL);
	CPipePair* threadPtr= (CPipePair *)dParam;
	DWORD exitCode=threadPtr-> RunInThreadProc();
	::ExitThread(exitCode);
	return exitCode;
}

//----------------------------------------------------------------------------------
// RunInThreadProc:Real thread body that issues IN transfers
//----------------------------------------------------------------------------------


DWORD 
CPipePair::RunInThreadProc(){
    SETFNAME(_T("InThreadProc"));
    FUNCTION_ENTER_MSG();
    DWORD  dwRet = 0;
    DWORD  dwWait = WAIT_OBJECT_0;
    BOOL     bError = FALSE;

    while ((m_bThreadsExit == FALSE) && (bError == FALSE)) {
        //wait for the next transfer task
        dwWait = WaitForSingleObject(m_hInTaskReadyEvent, THREAD_IDLE_TIME);
        if(dwWait != WAIT_OBJECT_0){
            continue; //to check whether we should exit
	 }
        else{
            ResetEvent(m_hInTaskReadyEvent);
        }

        USHORT usRounds = m_usTransRounds;
	  //issue out transfers 
	  while(usRounds > 0){
	      UCHAR	uIndex = 0;
	      PBYTE	pData = NULL;
	      BOOL      bRet = FALSE;

             //guarantee we do not issue transfers more than function driver can handle
	      dwWait = WaitForSingleObject(m_hInSemaphore, INFINITE);
            if(dwWait != WAIT_OBJECT_0){//sth. wrong
                bError = TRUE;
                break;
    	      }

            //get next TD			
	      DEBUGMSG(ZONE_TRANSFER, (_T("%s In transfer before check TD  ready\r\n"), pszFname));
            bRet = TD_WaitInTDReady(&pData, &uIndex);
	      if(bRet == FALSE){//sth. wrong
	          ERRORMSG(1, (_T("%s In transfer check TD  ready failed!\r\n"), pszFname));
                bError = TRUE;
		   break;
	      }

            //issue IN transfer
            EnterCriticalSection(&m_Incs);
            DEBUGMSG(ZONE_TRANSFER, (_T("%s In transfer ready to issue, using TD %d\r\n"), pszFname, uIndex));
            DWORD dwErr = ERROR_GEN_FAILURE;
            m_pTDList[uIndex].TransNoti.pClass = (LPVOID)this;
            m_pTDList[uIndex].TransNoti.uTransIndex = uIndex;
            if(m_bInFaked == TRUE || m_hInPipe == NULL){//no real IN actiion needed
                InCompleteNotify(uIndex);
                dwErr = ERROR_SUCCESS;
            }
            else {
                dwErr = m_pUfnFuncs->lpIssueTransfer(m_hDevice, m_hInPipe, 
                                                          &InCompleteCallback, (LPVOID)(&(m_pTDList[uIndex].TransNoti)), USB_IN_TRANSFER,
                                                          (m_dwTransLen != -1)?m_dwTransLen:m_pTDList[uIndex].usLen, pData,  0, NULL, &(m_pTDList[uIndex].hTransfer));
            }
            LeaveCriticalSection(&m_Incs);

	      if(dwErr != ERROR_SUCCESS ){//sth. wrong
	          ERRORMSG(1, (_T("%s In transfer failed!, error = 0x%x\r\n"), pszFname, dwErr));
                bError = TRUE;
		   break;
	      }

            DEBUGMSG(ZONE_TRANSFER, (_T("%s issue In transfer index = %d, phTransfer = %p, *phTrasnfer = %p\r\n"), 
		                  pszFname, uIndex,  &(m_pTDList[uIndex].hTransfer), m_pTDList[uIndex].hTransfer));
			
	      usRounds --;
	      DEBUGMSG(ZONE_TRANSFER, (_T("%s In transfer rounds left = %d\r\n"), pszFname, usRounds));
		  
	  }
    }

   FUNCTION_LEAVE_MSG();
   return 0;
}

DWORD
WINAPI
CPipePair::InCompleteCallback(LPVOID dParam){
	if(dParam == NULL)
	    return 0;
	PTRANSNOTI  pNotiPtr = (PTRANSNOTI)dParam;
	CPipePair* cPipePairPtr = (CPipePair*)(pNotiPtr->pClass);
	DWORD exitCode=cPipePairPtr-> InCompleteNotify(pNotiPtr->uTransIndex);
	return exitCode;
}


//----------------------------------------------------------------------------------
// InCompleteNotify: In transfer completion notify function
//----------------------------------------------------------------------------------

DWORD
CPipePair::InCompleteNotify(UCHAR uTransIndex)
{   
    EnterCriticalSection(&m_InCompletioncs);
    m_uCurInIndex = uTransIndex;
    SetEvent(m_hInCompleteEvent);
    LeaveCriticalSection(&m_InCompletioncs);

    return ERROR_SUCCESS;
}


//----------------------------------------------------------------------------------
// InCompleteThreadProc: Thread that execute IN transfer completion function
//----------------------------------------------------------------------------------

DWORD
WINAPI
CPipePair::InCompleteThreadProc(LPVOID dParam){
	DEBUGCHK(dParam != NULL);
	CPipePair* threadPtr= (CPipePair *)dParam;
	DWORD exitCode=threadPtr-> RunInCompleteThreadProc();
	::ExitThread(exitCode);
	return exitCode;
}


//----------------------------------------------------------------------------------
// RunInCompleteThreadProc:Close transfer and cleanup buffer settings when IN 
// transfer is complete
//----------------------------------------------------------------------------------

DWORD 
CPipePair::RunInCompleteThreadProc(){
    SETFNAME(_T("InCompleteThreadProc"));
    FUNCTION_ENTER_MSG();
    DWORD  dwRet = 0;
    DWORD  dwWait = WAIT_OBJECT_0;
    UCHAR   uExpected = 0;
    DWORD  cbTransferred;
    DWORD  dwUsbError;
	
    while (m_bThreadsExit == FALSE) {
        //wait for the next transfer task
        dwWait = WaitForSingleObject(m_hInCompleteEvent, THREAD_IDLE_TIME);
        if(dwWait != WAIT_OBJECT_0){
            continue; //to check whether we should exit
	 }

        //this is a little strange: what I am doing is to wait issuetransfer to be done
        //otherwise we may get an invalid transfer handle
        // TODO: may use event instead?
        EnterCriticalSection(&m_Incs);
        LeaveCriticalSection(&m_Incs);

        //following two statements have to be atomic, otherwise we may
        //miss some transfers
        EnterCriticalSection(&m_InCompletioncs);
        UCHAR uIndexNow = m_uCurInIndex;
        ResetEvent(m_hInCompleteEvent);
        LeaveCriticalSection(&m_InCompletioncs);

        //we may have more than one transfer done here		
        SHORT cbDone = (SHORT)((uIndexNow + 1) - uExpected);
        if(cbDone <= 0) 
		cbDone += m_uQueueSize;
        UCHAR uIndex = uExpected;
        DEBUGMSG(ZONE_TRANSFER, (_T("%s: num of done transfers = %d, TD start with index %d\r\n"), pszFname,  cbDone, uIndex));
        //complete these done transfers
        while(cbDone > 0){
            PUFN_TRANSFER phTransfer = &(m_pTDList[uIndex].hTransfer);
            DEBUGMSG(ZONE_TRANSFER, (_T("%s In transfer done  index = %d, phTransfer = %p, *phTrasnfer = %p\r\n"), 
		                               pszFname, uIndex, phTransfer, *phTransfer));

            if(*phTransfer != NULL){
                //get transfer status and close pipe
                DWORD dwErr = m_pUfnFuncs->lpGetTransferStatus(m_hDevice, *phTransfer, &cbTransferred, &dwUsbError);
                if(dwErr != ERROR_SUCCESS){//failed, but need to close it anyway
                    ERRORMSG(1, (_T("%s ERROR: In transfer hTrasnfer = %p got USB error = 0x%x\r\n"), 
    		                               pszFname, *phTransfer, dwUsbError));
                }
                else{
                    DEBUGMSG(ZONE_TRANSFER, (_T("%s %u bytes sent!\r\n"), pszFname, cbTransferred));
                    if(m_dwTransLen != -1 && cbTransferred != m_dwTransLen){//we may lost data?
                        ERRORMSG(1, (_T("%s ERROR: pipe 0x%x, Expected bytes sent to host is %u, actual sent ou %u, some data may lost !!!\r\n"), pszFname, m_hInPipe, m_dwTransLen, cbTransferred));
                    }
    				
                }

                dwErr = m_pUfnFuncs->lpCloseTransfer(m_hDevice, *phTransfer);
                if(dwErr != ERROR_SUCCESS){//failed, but need to close it anyway
                    ERRORMSG(1, (_T("%s ERROR: In transfer hTrasnfer = %p failed to close\r\n"), 
    		                               pszFname, *phTransfer));
                }

                //clear transfer handle in TD
                *phTransfer = NULL;
            }
            else if(m_bInFaked == TRUE || m_hInPipe == NULL){//no real IN transfer occurred
                cbTransferred = 0;
            }
            else{//should not happen
                DEBUGCHK(FALSE);
            }
             //update TD contents
            TD_InTransferComplete(uIndex, cbTransferred);
             //allow another transfer to proceed
	      ReleaseSemaphore(m_hInSemaphore, 1, NULL);
		//head to the next done transfer	 
	      uIndex = (uIndex +1) % m_uQueueSize;
	      cbDone --;
            m_usTransLeft --; 
            if(m_usTransLeft == 0){
                SetEvent(m_hCompleteEvent);
            }
        }
        //next expected TD with done transfer.
        uExpected = uIndex;
    }

   FUNCTION_LEAVE_MSG();
   return 0;
}


//----------------------------------------------------------------------------------
// RunOutThreadProc:Thread body that issues Out transfers
//----------------------------------------------------------------------------------

DWORD
CPipePair::WaitPipesReady(DWORD	dwWaitTime){
    SETFNAME(_T("WaitPipesReady"));
    FUNCTION_ENTER_MSG();
    DWORD  dwRet = 0;
    DWORD  dwWait = WAIT_OBJECT_0;

    //wait for the pipes ready for the next task
    dwWait = WaitForSingleObject(m_hAckReadyEvent, dwWaitTime);
    if(dwWait != WAIT_OBJECT_0){
        ERRORMSG(1, (_T("%s FATAL ERROR: can not start executing transfer request, host side should quit testing!!!\r\n"), 
	                                                                   pszFname));
        dwRet = (DWORD)-1;
    }

    ResetEvent(m_hAckReadyEvent);
    FUNCTION_LEAVE_MSG();
    return dwRet;
}

//*************************************************************************
//member function implementations of class CPipePairManager
//*************************************************************************
BOOL
CPipePairManager::Init(){
    if(m_fInitialized == TRUE){//already done
        return TRUE;
    }   
    
    SETFNAME(_T("CPipePairManager.Init"));
    FUNCTION_ENTER_MSG();

    BOOL bRet = FALSE;
    
    if(m_pDevConfig == NULL || m_hDevice == NULL || m_pUfnFuncs == NULL ){
        ERRORMSG(1, (_T("%s invalid value exsit! please check: !!\r\n"), pszFname));
        ERRORMSG(1, (_T("%s m_pDevConfig = 0x%x m_hDevice =0x%x m_pUfnFuncs = 0x%x\r\n"), pszFname, 
                                    m_pDevConfig,  m_hDevice,  m_pUfnFuncs));
        
        goto EXIT;
    }

    m_pFSConfig = m_pDevConfig->GetCurFSConfig();
    m_pHSConfig = m_pDevConfig->GetCurHSConfig();
    m_pDevConfig->SetBusSpeed(m_curSpeed);
    if(m_pFSConfig == NULL || m_pHSConfig == NULL){
        ERRORMSG(1, (_T("%s invalid configuration instance!!!\r\n"), pszFname));
        goto EXIT;
    }

    m_uNumofPairs = m_pDevConfig->GetNumofPairs();
    m_uNumofEPs = m_pDevConfig->GetNumofEPs();
    if(m_uNumofPairs == 0 || m_uNumofEPs == 0){
        ERRORMSG(1, (_T("%s no endpoints or pipe pair avaliable!!!\r\n"), pszFname));
        goto EXIT;
    }
    
    m_pPairsInfo = (PPIPEPAIR_INFO) new PIPEPAIR_INFO[m_uNumofPairs];
    m_pPipePairs = (PCPipePair *) new PCPipePair [m_uNumofPairs];
    PUCHAR  puChecked = (PUCHAR) new UCHAR[m_uNumofEPs];
    if(m_pPairsInfo == NULL ||m_pPipePairs == NULL || puChecked == NULL){
        ERRORMSG(1, (_T("%s Out of memory!!!\r\n"), pszFname));
        goto EXIT;
    }
    else{
        memset(m_pPairsInfo, 0, sizeof(PIPEPAIR_INFO)*m_uNumofPairs);
        memset(m_pPipePairs, 0, sizeof(PVOID)*m_uNumofPairs);
        memset(puChecked, 0, sizeof(UCHAR)*m_uNumofEPs);
    }

    m_uNumofSPipes = m_uNumofEPs - m_uNumofPairs*2;
    if(m_uNumofSPipes > 0){
        m_pSinglesInfo = (PSINGLEPIPE_INFO) new SINGLEPIPE_INFO[m_uNumofSPipes];
        m_pSinglePipes = (PCSinglePipe *) new PCSinglePipe[m_uNumofSPipes];
        if(m_pSinglePipes == NULL || m_pSinglesInfo == NULL){
            ERRORMSG(1, (_T("%s Out of memory!!!\r\n"), pszFname));
            goto EXIT;
        }
        else{
            memset(m_pSinglesInfo, 0, sizeof(SINGLEPIPE_INFO)*m_uNumofSPipes);
            memset(m_pSinglePipes, 0, sizeof(PVOID)*m_uNumofSPipes);
        }
    }

    DEBUGMSG(ZONE_INIT, (_T("%s total endpoints = %d, ep pairs = %d, single ep = %d\r\n"), 
                                 pszFname, m_uNumofEPs, m_uNumofPairs, m_uNumofSPipes));
    PUCHAR  pPairs = m_pDevConfig->GetEPPairs();
    if(pPairs == NULL){
        ERRORMSG(1, (_T("%s no valid pipe pair informaton!!!\r\n"), pszFname));
        goto EXIT;
    }

    PUFN_ENDPOINT pEndPoints = NULL;
    if(m_curSpeed == BS_HIGH_SPEED){
        pEndPoints = m_pHSConfig->pInterfaces->pEndpoints;
        NKDbgPrintfW(_T("%s Current host is a HighSpeed host!\r\n"), pszFname);
    }
    else{
        pEndPoints = m_pFSConfig->pInterfaces->pEndpoints;
        NKDbgPrintfW(_T("%s Current host is a FullSpeed host!\r\n"), pszFname);
    }

    UCHAR uIndex = 0; 
    UCHAR uSIndex = 0;    

    UCHAR uQueueSize = QUEUE_SIZE;
    UCHAR uQueueDepth = QUEUE_DEPTH;
    
    if(m_uNumofPairs > 6 && m_uNumofPairs < 12){
        uQueueDepth = uQueueDepth / 2;
        uQueueSize = uQueueSize / 2;
    }
    else if ( m_uNumofPairs >= 12){
        uQueueDepth = uQueueDepth / 8;
        uQueueSize = uQueueSize / 8;
    }
    
    for(UCHAR uEp = 0; uEp < m_uNumofEPs; uEp ++){
        if(puChecked[uEp] == 1)
            continue;
            
        if(pPairs[uEp] == 0xFF){
            m_pSinglesInfo[uSIndex].bmAttribute = pEndPoints[uEp].Descriptor.bmAttributes;
            m_pSinglesInfo[uSIndex].bEPAddress = pEndPoints[uEp].Descriptor.bEndpointAddress;
            BOOL bIn = ((m_pSinglesInfo[uSIndex].bEPAddress & 0x80) != 0);
            CSinglePipe* pNewPipe = NULL;
            switch(m_pSinglesInfo[uSIndex].bmAttribute){
                case USB_ENDPOINT_TYPE_CONTROL:
                    DEBUGMSG(ZONE_INIT, (_T("%s Single EP: addr=0x%x, USB_ENDPOINT_TYPE_CONTROL \r\n"),
                                        pszFname, m_pSinglesInfo[uSIndex].bEPAddress));
                    pNewPipe = new CSinglePipe(m_hDevice, m_pUfnFuncs, uQueueDepth, TRANS_TYPE_CONTROL, bIn);
                    break;
                case USB_ENDPOINT_TYPE_BULK:
                    DEBUGMSG(ZONE_INIT, (_T("%s Single EP: addr=0x%x, USB_ENDPOINT_TYPE_BULK \r\n"),
                                        pszFname, m_pSinglesInfo[uSIndex].bEPAddress));
                    pNewPipe = new CSinglePipe(m_hDevice, m_pUfnFuncs, uQueueDepth, TRANS_TYPE_BULK, bIn);
                    break;
                case USB_ENDPOINT_TYPE_INTERRUPT:
                    DEBUGMSG(ZONE_INIT, (_T("%s Single EP: addr=0x%x, USB_ENDPOINT_TYPE_INTERRUPT \r\n"),
                                        pszFname, m_pSinglesInfo[uSIndex].bEPAddress));
                    pNewPipe = new CSinglePipe(m_hDevice, m_pUfnFuncs, uQueueDepth, TRANS_TYPE_INT, bIn);
                    break;
                case USB_ENDPOINT_TYPE_ISOCHRONOUS:
                     DEBUGMSG(ZONE_INIT, (_T("%s Single EP: addr=0x%x, USB_ENDPOINT_TYPE_ISOCH \r\n"),
                                        pszFname, m_pSinglesInfo[uSIndex].bEPAddress));
                   pNewPipe = new CSinglePipe(m_hDevice, m_pUfnFuncs, uQueueDepth, TRANS_TYPE_ISOCH, bIn);
                    break;
                default:
                    ERRORMSG(1, (_T("%s invalid descriptor type!!!\r\n"), pszFname));
                    goto EXIT;
            }
            if(pNewPipe == NULL){
                ERRORMSG(1, (_T("%s Out of memory!!!\r\n"), pszFname));
                goto EXIT;
            }
            if(pNewPipe->Init() == FALSE){
                ERRORMSG(1, (_T("%s Pipe initialization failed!!!\r\n"), pszFname));
                goto EXIT;
            }
            m_pSinglePipes[uSIndex++] = pNewPipe;
            puChecked[uEp] = 1;
            continue;
        }
        
        m_pPairsInfo[uIndex].bmAttribute = pEndPoints[uEp].Descriptor.bmAttributes;
        if((pEndPoints[uEp].Descriptor.bEndpointAddress & 0x80) != 0){//in address
            m_pPairsInfo[uIndex].bInEPAddress = pEndPoints[uEp].Descriptor.bEndpointAddress;
            m_pPairsInfo[uIndex].bOutEPAddress = pEndPoints[pPairs[uEp]].Descriptor.bEndpointAddress;
        }
        else{
            m_pPairsInfo[uIndex].bOutEPAddress = pEndPoints[uEp].Descriptor.bEndpointAddress;
            m_pPairsInfo[uIndex].bInEPAddress = pEndPoints[pPairs[uEp]].Descriptor.bEndpointAddress;
        }

        CPipePair* pNewPipe = NULL;
        switch(m_pPairsInfo[uIndex].bmAttribute){
            case USB_ENDPOINT_TYPE_CONTROL:
                DEBUGMSG(ZONE_INIT, (_T("%s Paired EPs: IN addr=0x%x, OUT addr = 0x%x, USB_ENDPOINT_TYPE_CONTROL \r\n"),
                                    pszFname, m_pPairsInfo[uIndex].bInEPAddress, m_pPairsInfo[uIndex].bOutEPAddress));
                pNewPipe = new CPipePair(m_hDevice, m_pUfnFuncs, CTRL_TRANSFER_SIZE, uQueueSize, uQueueDepth, TRANS_TYPE_CONTROL);
                break;
            case USB_ENDPOINT_TYPE_BULK:
                DEBUGMSG(ZONE_INIT, (_T("%s Paired EPs: IN addr=0x%x, OUT addr = 0x%x, USB_ENDPOINT_TYPE_BULK \r\n"),
                                    pszFname, m_pPairsInfo[uIndex].bInEPAddress, m_pPairsInfo[uIndex].bOutEPAddress));
                pNewPipe = new CPipePair(m_hDevice, m_pUfnFuncs, BULK_TRANSFER_SIZE, uQueueSize, uQueueDepth, TRANS_TYPE_BULK);
                break;
            case USB_ENDPOINT_TYPE_INTERRUPT:
                DEBUGMSG(ZONE_INIT, (_T("%s Paired EPs: IN addr=0x%x, OUT addr = 0x%x, USB_ENDPOINT_TYPE_INTERRUPT \r\n"),
                                    pszFname, m_pPairsInfo[uIndex].bInEPAddress, m_pPairsInfo[uIndex].bOutEPAddress));
                pNewPipe = new CPipePair(m_hDevice, m_pUfnFuncs, INT_TRANSFER_SIZE, uQueueSize, uQueueDepth, TRANS_TYPE_INT);
                break;
            case USB_ENDPOINT_TYPE_ISOCHRONOUS:
                DEBUGMSG(ZONE_INIT, (_T("%s Paired EPs: IN addr=0x%x, OUT addr = 0x%x, USB_ENDPOINT_TYPE_ISOCH \r\n"),
                                    pszFname, m_pPairsInfo[uIndex].bInEPAddress, m_pPairsInfo[uIndex].bOutEPAddress));
                pNewPipe = new CPipePair(m_hDevice, m_pUfnFuncs, ISOCH_TRANSFER_SIZE, uQueueSize, uQueueDepth, TRANS_TYPE_ISOCH);
                break;
            default:
                ERRORMSG(1, (_T("%s invalid descriptor type!!!\r\n"), pszFname));
                goto EXIT;
        }
        if(pNewPipe == NULL){
            ERRORMSG(1, (_T("%s Out of memory!!!\r\n"), pszFname));
            goto EXIT;
        }
        if(pNewPipe->Init() == FALSE){
            ERRORMSG(1, (_T("%s Pipe initialization failed!!!\r\n"), pszFname));
            goto EXIT;
        }
        m_pPipePairs[uIndex++] = pNewPipe;
        puChecked[uEp] = 1;
        if(pPairs[uEp] <=  m_uNumofEPs)
            puChecked[pPairs[uEp]] = 1;
    }

    if(uIndex != m_uNumofPairs){
            ERRORMSG(1, (_T("%s expected %d pairs, actually got %d\r\n"), pszFname, m_uNumofPairs, uIndex));
            goto EXIT;
    }

    bRet = TRUE;
    
EXIT: 

    if(bRet == FALSE){
        if(m_pPipePairs){
            for(UCHAR u = 0; u < m_uNumofPairs; u++){
                if(m_pPipePairs[u])
                    delete m_pPipePairs[u];
            }
            delete[] m_pPipePairs;
        }
        if(m_pPairsInfo)
            delete[] m_pPairsInfo;

        if(m_pSinglePipes){
            for(UCHAR u = 0; u < m_uNumofSPipes; u++){
                if(m_pSinglePipes[u])
                    delete m_pSinglePipes[u];
            }
            delete[] m_pSinglePipes;
        }
        if(m_pSinglesInfo)
            delete[] m_pSinglesInfo;
    }
    else{
        m_fInitialized = TRUE;
    }

    delete[] puChecked;
    
    FUNCTION_LEAVE_MSG();
    return bRet;
}

VOID
CPipePairManager::SendDataLoopbackRequest(UCHAR uOutEP, USHORT wBlockLen, USHORT wNumofBlocks, USHORT usAlign, DWORD dwStartVal){

    if(wBlockLen == 0 || wNumofBlocks == 0)
        return;

    //check paired pipes first
    for(UCHAR uIndex = 0; uIndex < m_uNumofPairs; uIndex ++){
        if(m_pPairsInfo[uIndex].bOutEPAddress == uOutEP){
            m_pPipePairs[uIndex]->StartNewTransfers(wBlockLen, wNumofBlocks);
            return;
        }
    }

    //check single pipes 
    for(UCHAR uIndex = 0; uIndex < m_uNumofSPipes; uIndex ++){
        if(m_pSinglesInfo[uIndex].bEPAddress == uOutEP){
            m_pSinglePipes[uIndex]->StartNewTransfers(wBlockLen, wNumofBlocks, usAlign, dwStartVal);
            return;
        }
    }
}

VOID
CPipePairManager::SendShortStressRequest(UCHAR uOutEP, USHORT wNumofBlocks){

    if(wNumofBlocks == 0)
        return;

    //check paired pipes first
    for(UCHAR uIndex = 0; uIndex < m_uNumofPairs; uIndex ++){
        if(m_pPairsInfo[uIndex].bOutEPAddress == uOutEP){
            m_pPipePairs[uIndex]->StartNewTransfers((DWORD)-1, wNumofBlocks);
            return;
        }
    }

    //check single pipes 
    for(UCHAR uIndex = 0; uIndex < m_uNumofSPipes; uIndex ++){
        if(m_pSinglesInfo[uIndex].bEPAddress == uOutEP){
            m_pSinglePipes[uIndex]->StartNewTransfers((DWORD)-1, wNumofBlocks, 1, 0);
            return;
        }
    }
    
}

BOOL
CPipePairManager::OpenAllPipes(){
    SETFNAME(_T("CPipePairManager.OpenAllPipes"));
    FUNCTION_ENTER_MSG();

    for(UCHAR uIndex = 0; uIndex < m_uNumofPairs; uIndex ++){
        //open out pipe
        UCHAR uEPAddr = m_pPairsInfo[uIndex].bOutEPAddress;
        PUFN_PIPE pPipe = m_pPipePairs[uIndex]->GetOutPipe();
        if(pPipe == NULL){
            ERRORMSG(1, (_T("%s Null pipe found!!!\r\n"), pszFname));
            return FALSE;
        }
        DEBUGMSG(ZONE_PIPE, (_T("%s, OPEN OUT PIPE, addr = 0x%x \r\n"),
                                                    pszFname, uEPAddr));
        DWORD dwRet = m_pUfnFuncs->lpOpenPipe(m_hDevice, uEPAddr, pPipe);
        if (dwRet != ERROR_SUCCESS) {
            ERRORMSG(1, (_T("%s Failed to open  OUT pipe (0x%x)\r\n"), pszFname, uEPAddr));
            return FALSE;
        }

        //open in pipe
        uEPAddr = m_pPairsInfo[uIndex].bInEPAddress;
        pPipe = m_pPipePairs[uIndex]->GetInPipe();
        if(pPipe == NULL){
            ERRORMSG(1, (_T("%s Null pipe found!!!\r\n"), pszFname));
            return FALSE;
        }
        dwRet = m_pUfnFuncs->lpOpenPipe(m_hDevice, uEPAddr, pPipe);
        if (dwRet != ERROR_SUCCESS) {
            ERRORMSG(1, (_T("%s Failed to open IN pipe (0x%x)\r\n"), pszFname, uEPAddr));
            return FALSE;
        }
        DEBUGMSG(ZONE_PIPE, (_T("%s, OPEN IN PIPE, addr = 0x%x \r\n"),
                                                    pszFname, uEPAddr));

        if (m_pPipePairs[uIndex]->StartThreads() == FALSE) {
            ERRORMSG(1, (_T("%s Failed to start pipe thread No. %d!\r\n"), pszFname, uIndex));
            return FALSE;
        }
        
        m_pPairsInfo[uIndex].bOpened = TRUE;
    }

    for(UCHAR uIndex = 0; uIndex < m_uNumofSPipes; uIndex++){
        //open out pipe
        UCHAR uEPAddr = m_pSinglesInfo[uIndex].bEPAddress;
        PUFN_PIPE pPipe = m_pSinglePipes[uIndex]->GetPipe();
        if(pPipe == NULL){
            ERRORMSG(1, (_T("%s Null pipe found!!!\r\n"), pszFname));
            return FALSE;
        }

        DEBUGMSG(ZONE_PIPE, (_T("%s, OPEN non-paired PIPE, addr = 0x%x \r\n"),
                                                    pszFname, uEPAddr));

        DWORD dwRet = m_pUfnFuncs->lpOpenPipe(m_hDevice, uEPAddr, pPipe);
        if (dwRet != ERROR_SUCCESS) {
            ERRORMSG(1, (_T("%s Failed to open  pipe (0x%x)\r\n"), pszFname, uEPAddr));
            return FALSE;
        }
        if (m_pSinglePipes[uIndex]->StartThreads() == FALSE) {
            ERRORMSG(1, (_T("%s Failed to start pipe thread No. %d!\r\n"), pszFname, uIndex));
            return FALSE;
        }

        m_pSinglesInfo[uIndex].bOpened = TRUE;
        
    }

    FUNCTION_LEAVE_MSG();
    return TRUE;    

}

BOOL
CPipePairManager::CloseAllPipes(){

    for(UCHAR uIndex = 0; uIndex < m_uNumofPairs; uIndex ++){
        if(m_pPairsInfo[uIndex].bOpened == TRUE){
            m_pPipePairs[uIndex]->CloseInPipe();
            m_pPipePairs[uIndex]->CloseOutPipe();
            m_pPairsInfo[uIndex].bOpened = FALSE;
        }
    }

    for(UCHAR uIndex = 0; uIndex < m_uNumofSPipes; uIndex ++){
        if(m_pSinglesInfo[uIndex].bOpened == TRUE){
            m_pSinglePipes[uIndex]->ClosePipe();
            m_pSinglesInfo[uIndex].bOpened = FALSE;
        }
    }
    
    return TRUE;    
}

BOOL
CPipePairManager::CleanAllPipes(){

    for(UCHAR uIndex = 0; uIndex < m_uNumofPairs; uIndex ++){
        if(m_pPipePairs[uIndex]){
            delete m_pPipePairs[uIndex];
        }
    }
    delete[] m_pPipePairs;
    m_pPipePairs = NULL;

    delete[] m_pPairsInfo;
    m_pPairsInfo = NULL;

    for(UCHAR uIndex = 0; uIndex < m_uNumofSPipes; uIndex ++){
        if(m_pSinglePipes[uIndex]){
            delete m_pSinglePipes[uIndex];
        }
    }
    if(m_pSinglePipes){
        delete[] m_pSinglePipes;
        m_pSinglePipes = NULL;
    }

    if(m_pSinglesInfo){
        delete[] m_pSinglesInfo;
        m_pSinglesInfo = NULL;
    }

    m_fInitialized = FALSE;
    return TRUE;    
}

