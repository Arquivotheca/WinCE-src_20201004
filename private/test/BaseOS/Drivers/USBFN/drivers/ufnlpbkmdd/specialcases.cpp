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
#include "utility.h"
#include "specialcases.h"


VOID               
SP_StartDataTransWithStalls(PUFL_CONTEXT pContext,
PIPEPAIR_INFO PairInfo, 
UFN_PIPE OutPipe,  UFN_PIPE InPipe, 
UCHAR uStallRepeat, USHORT wBlockSize)
{
    if(pContext == NULL || OutPipe == NULL || InPipe == NULL)//actually this should not happen
        return;

    if(wBlockSize <= PairInfo.wCurPacketSize){
        ERRORMSG(1,(_T("SP_StartDataTransWithStalls: block size %d is not bigger than packet size %d, can not run test"),
                             wBlockSize, PairInfo.wCurPacketSize));
        return;
    }

    UCHAR uThread = 0;
    if(FindVacantThreadPos(pContext->spThreads, &uThread) == FALSE){//this should not happen either
        ERRORMSG(1,(_T("SP_StartDataTransWithStalls: there are already 8 threads running special tests, please run this test later")));
        return;
    }

    //now we are able to create test thread
    PSTALLDATATRANS pStallTrans = new STALLDATATRANS;
    if(pStallTrans == NULL){
        ERRORMSG(1,(_T("SP_StartDataTransWithStalls: OOM!")));
        return;
    }

    pStallTrans->pThread = &(pContext->spThreads[uThread]);
    pStallTrans->hDevice = pContext->hDevice;
    pStallTrans->pUfnFuncs = pContext->pUfnFuncs;
    pStallTrans->OutPipe = OutPipe;
    pStallTrans->InPipe = InPipe;
    pStallTrans->uStallRepeat = uStallRepeat;
    pStallTrans->wBlockSize = wBlockSize;
    memcpy(&pStallTrans->PairInfo, &PairInfo, sizeof(PIPEPAIR_INFO));

    DWORD dwThreadId = 0;
    pContext->spThreads[uThread].hThread = CreateThread(NULL, 0, StallDataTransThread, (LPVOID)pStallTrans, 0, &dwThreadId);
    if(pContext->spThreads[uThread].hThread == NULL){
        ERRORMSG(1,(_T("SP_StartDataTransWithStalls: Create test thread failed!")));
        return;
    }
    else{
        pContext->spThreads[uThread].fRunning = TRUE;
    }

    return;
}

DWORD WINAPI
StallDTCallback(LPVOID dParam){
    HANDLE hEvent = (HANDLE)dParam;

    if(hEvent != NULL)
        SetEvent(hEvent);

    return 0;
}

#define TRANSFER_WAIT_COMPLETE_TIME     30*1000  //30 secounds wait time

BOOL
IssueSynchronizedTransfer(
PUFN_FUNCTIONS pUfnFuncs, UFN_HANDLE hDevice, UFN_PIPE hPipe, 
PBYTE  pBuf, USHORT  cbSize, DWORD dwFlags){

    if(pUfnFuncs == NULL || hDevice == NULL || hPipe == NULL || pBuf == NULL)
        return FALSE;

    SETFNAME(_T("IssueSynchronizedTransfer"));
    FUNCTION_ENTER_MSG();

    HANDLE hCompEvent = CreateEvent(0, FALSE, FALSE, NULL);
    UFN_TRANSFER    hTransfer = NULL;
    BOOL fRet = FALSE;
    
    DWORD dwErr = pUfnFuncs->lpIssueTransfer(hDevice, hPipe, 
                                                  &StallDTCallback, (LPVOID)hCompEvent, dwFlags,
                                                  cbSize, pBuf,  0, NULL, &hTransfer);

    if(dwErr != ERROR_SUCCESS || hTransfer == INVALID_HANDLE_VALUE || hTransfer == NULL ){//sth. wrong
        ERRORMSG(1, (_T("%s Issue transfer failed!, error = 0x%x\r\n"), pszFname, dwErr));
        goto IT_EXIT;
    }

    //Wait for transfer to complete
    DWORD dwWait = WaitForSingleObject(hCompEvent, TRANSFER_WAIT_COMPLETE_TIME);
    if(dwWait != WAIT_OBJECT_0){//sth. wrong
        ERRORMSG(1, (_T("%s OUT transfer not completed with 30 secs, test will quit!\r\n"), pszFname));
        pUfnFuncs->lpAbortTransfer(hDevice, hTransfer);
        goto IT_EXIT;
    }        

    //check transfer status and close transfer
    DWORD cbTransferred  = 0; 
    DWORD dwUSBError = 0;
    dwErr = pUfnFuncs->lpGetTransferStatus(hDevice, hTransfer, &cbTransferred, &dwUSBError);   
    if(dwErr != ERROR_SUCCESS){//failed, but need to close it anyway
        ERRORMSG(1, (_T("%s ERROR: out transfer hTrasnfer = %p got USB error = 0x%x\r\n"), 
                               pszFname, hTransfer, dwUSBError));
        pUfnFuncs->lpCloseTransfer(hDevice, hTransfer);
        goto IT_EXIT;
    }
    else{
        DEBUGMSG(ZONE_TRANSFER, (_T("%s %u bytes received!\r\n"), pszFname, cbTransferred));
        if(cbTransferred != cbSize){//we may lost data?
            ERRORMSG(1, (_T("%s ERROR: pipe: 0x%x, Expected bytes received from host is %u, actual recieved %u, some data may lost !!!\r\n"), 
                                pszFname, hPipe,  cbSize, cbTransferred));
            pUfnFuncs->lpCloseTransfer(hDevice, hTransfer);
            goto IT_EXIT;
        }
        pUfnFuncs->lpCloseTransfer(hDevice, hTransfer);
        hTransfer = NULL;
    }

    fRet = TRUE;
    
IT_EXIT:

    CloseHandle(hCompEvent);
    FUNCTION_LEAVE_MSG();
    return fRet;

}



DWORD WINAPI 
StallDataTransThread (LPVOID lpParameter) {

    PSTALLDATATRANS pStallTrans = (PSTALLDATATRANS)lpParameter;
    if(pStallTrans == NULL){//this should not happen
        ExitThread((DWORD)-1);
        return (DWORD)-1;
    }

    SETFNAME(_T("StallDataTransThread"));
    FUNCTION_ENTER_MSG();

    DWORD dwRet = (DWORD)-1;
    
    PBYTE pBuf = new BYTE[pStallTrans->wBlockSize];
    if(pBuf == NULL){
        ERRORMSG(1, (_T("%s OOM!! \r\n"), pszFname));
        goto THREAD_EXIT;
    }

    PUFN_FUNCTIONS pUfnFuncs = pStallTrans->pUfnFuncs;
    UFN_HANDLE  hDevice = pStallTrans->hDevice;
    BOOL    fTrans;

    Sleep(500);
    pUfnFuncs->lpStallPipe(hDevice, pStallTrans->OutPipe);    
    NKDbgPrintfW(_T("Device side OUT  pipe 0x%x stalled!"), pStallTrans->OutPipe);
    Sleep(2000);
    
    //now receive the OUT data
    memset(pBuf, 0, sizeof(pStallTrans->wBlockSize));
    fTrans = IssueSynchronizedTransfer(pUfnFuncs, hDevice, pStallTrans->OutPipe, pBuf, pStallTrans->wBlockSize, USB_OUT_TRANSFER);
    if(fTrans == FALSE){
        ERRORMSG(1, (_T("%s IssueSynchronizedTransfer OUT call failed!!! \r\n"), pszFname));
        goto THREAD_EXIT;
    }

    Sleep(500);
    NKDbgPrintfW(_T("Device side IN pipe 0x%x stalled!"), pStallTrans->InPipe);
    pUfnFuncs->lpStallPipe(hDevice, pStallTrans->InPipe);    
    Sleep(6000);

    //now *formally* send the IN data
    fTrans = IssueSynchronizedTransfer(pUfnFuncs, hDevice, pStallTrans->InPipe, pBuf, pStallTrans->wBlockSize, USB_IN_TRANSFER);
    if(fTrans == FALSE){
        ERRORMSG(1, (_T("%s IssueSynchronizedTransfer call failed!!! \r\n"), pszFname));
        goto THREAD_EXIT;
    }

    dwRet = 0;

THREAD_EXIT:

    if(pBuf)
        delete[] pBuf;
        
    if(pStallTrans != NULL){
        pStallTrans->pThread->fRunning = FALSE;
        delete pStallTrans;
    }

    FUNCTION_LEAVE_MSG();
    
    ExitThread(dwRet);
    return dwRet;
}



VOID               
SP_StartDataShortTrans(PUFL_CONTEXT pContext, PIPEPAIR_INFO PairInfo, 
UFN_PIPE OutPipe,  UFN_PIPE InPipe)
{
    if(pContext == NULL || OutPipe == NULL || InPipe == NULL)//actually this should not happen
        return;


    UCHAR uThread = 0;
    if(FindVacantThreadPos(pContext->spThreads, &uThread) == FALSE){//this should not happen either
        ERRORMSG(1,(_T("SP_StartDataShortTrans: there are already 8 threads running special tests, please run this test later")));
        return;
    }

    //now we are able to create test thread
    PSTALLDATATRANS pStallTrans = new STALLDATATRANS;
    if(pStallTrans == NULL){
        ERRORMSG(1,(_T("SP_StartDataShortTrans: OOM!")));
        return;
    }

    pStallTrans->pThread = &(pContext->spThreads[uThread]);
    pStallTrans->hDevice = pContext->hDevice;
    pStallTrans->pUfnFuncs = pContext->pUfnFuncs;
    pStallTrans->OutPipe = OutPipe;
    pStallTrans->InPipe = InPipe;
    memcpy(&pStallTrans->PairInfo, &PairInfo, sizeof(PIPEPAIR_INFO));

    DWORD dwThreadId = 0;
    pContext->spThreads[uThread].hThread = CreateThread(NULL, 0, ShortDataTransThread, (LPVOID)pStallTrans, 0, &dwThreadId);
    if(pContext->spThreads[uThread].hThread == NULL){
        ERRORMSG(1,(_T("SP_StartDataShortTrans: Create test thread failed!")));
        return;
    }
    else{
        pContext->spThreads[uThread].fRunning = TRUE;
    }

    return;
}




DWORD WINAPI 
ShortDataTransThread (LPVOID lpParameter) {

    PSTALLDATATRANS pStallTrans = (PSTALLDATATRANS)lpParameter;
    if(pStallTrans == NULL){//this should not happen
        ExitThread((DWORD)-1);
        return (DWORD)-1;
    }

    SETFNAME(_T("ShortlDataTransThread"));
    FUNCTION_ENTER_MSG();

    DWORD dwPacketSize = pStallTrans->PairInfo.wCurPacketSize;
    DWORD dwBufSize = dwPacketSize*12;
    DWORD dwSizeArr[] = {1, dwPacketSize-1, dwPacketSize, dwPacketSize+1, dwPacketSize*2-1,
                                      dwPacketSize*2, dwPacketSize*2+1, dwPacketSize*3-1};
    UCHAR  ucSizes = sizeof(dwSizeArr)/sizeof(DWORD);
    
    DWORD dwRet = (DWORD)-1;
    
    PBYTE pBuf = new BYTE[dwBufSize];
    if(pBuf == NULL){
        ERRORMSG(1, (_T("%s OOM!! \r\n"), pszFname));
        goto THREAD_EXIT;
    }
    memset(pBuf, 0, dwBufSize);
    
    PUFN_FUNCTIONS pUfnFuncs = pStallTrans->pUfnFuncs;
    UFN_HANDLE  hDevice = pStallTrans->hDevice;
    BOOL    fTrans;

    //now receive the OUT data
    memset(pBuf, 0, sizeof(pStallTrans->wBlockSize));
    PBYTE pCurr = pBuf;
    for(UCHAR i = 0; i < ucSizes; i ++){
        fTrans = IssueSynchronizedShortTransfer(pUfnFuncs, hDevice, pStallTrans->OutPipe, pCurr, (USHORT)(dwPacketSize*3), (USHORT)dwSizeArr[i], USB_OUT_TRANSFER);
        if(fTrans == FALSE){
            ERRORMSG(1, (_T("%s IssueSynchronizedsTransfer OUT call failed!!! \r\n"), pszFname));
            goto THREAD_EXIT;
        }
        pCurr += dwSizeArr[i];
    }

    //now send the IN data
    pCurr = pBuf;
    
    for(UCHAR i = 0; i < ucSizes; i ++){
        fTrans = IssueSynchronizedTransfer(pUfnFuncs, hDevice, pStallTrans->InPipe, pCurr, (USHORT)dwSizeArr[i], USB_IN_TRANSFER);
        if(fTrans == FALSE){
            ERRORMSG(1, (_T("%s IssueSynchronizedTransfer IN call failed!!! \r\n"), pszFname));
            goto THREAD_EXIT;
        }
        if((dwSizeArr[i] % dwPacketSize) == 0){
            fTrans = IssueSynchronizedTransfer(pUfnFuncs, hDevice, pStallTrans->InPipe, pCurr, 0, USB_IN_TRANSFER);
            if(fTrans == FALSE){
                ERRORMSG(1, (_T("%s IssueSynchronizedTransfer IN(ZERO LENGTH) call failed!!! \r\n"), pszFname));
                goto THREAD_EXIT;
            }
        }
        pCurr += dwSizeArr[i];
    }
    dwRet = 0;

THREAD_EXIT:

    if(pBuf)
        delete[] pBuf;
        
    if(pStallTrans != NULL){
        pStallTrans->pThread->fRunning = FALSE;
        delete pStallTrans;
    }

    FUNCTION_LEAVE_MSG();
    
    ExitThread(dwRet);
    return dwRet;
}


BOOL
IssueSynchronizedShortTransfer(
PUFN_FUNCTIONS pUfnFuncs, UFN_HANDLE hDevice, UFN_PIPE hPipe, 
PBYTE  pBuf, USHORT  cbSize, USHORT cbExpSize, DWORD dwFlags){

    if(pUfnFuncs == NULL || hDevice == NULL || hPipe == NULL || pBuf == NULL )
        return FALSE;

    SETFNAME(_T("IssueSynchronizedShortTransfer"));
    FUNCTION_ENTER_MSG();

    HANDLE hCompEvent = CreateEvent(0, FALSE, FALSE, NULL);
    UFN_TRANSFER    hTransfer = NULL;
    BOOL fRet = FALSE;
    
    DWORD dwErr = pUfnFuncs->lpIssueTransfer(hDevice, hPipe, 
                                                  &StallDTCallback, (LPVOID)hCompEvent, dwFlags,
                                                  cbSize, pBuf,  0, NULL, &hTransfer);

    if(dwErr != ERROR_SUCCESS || hTransfer == INVALID_HANDLE_VALUE || hTransfer == NULL ){//sth. wrong
        ERRORMSG(1, (_T("%s Issue transfer failed!, error = 0x%x\r\n"), pszFname, dwErr));
        goto IT_EXIT;
    }

    //Wait for transfer to complete
    DWORD dwWait = WaitForSingleObject(hCompEvent, TRANSFER_WAIT_COMPLETE_TIME);
    if(dwWait != WAIT_OBJECT_0){//sth. wrong
        ERRORMSG(1, (_T("%s OUT transfer not completed with 30 secs, test will quit!\r\n"), pszFname));
        pUfnFuncs->lpAbortTransfer(hDevice, hTransfer);
        goto IT_EXIT;
    }        

    //check transfer status and close transfer
    DWORD cbTransferred  = 0; 
    DWORD dwUSBError = 0;
    dwErr = pUfnFuncs->lpGetTransferStatus(hDevice, hTransfer, &cbTransferred, &dwUSBError);   
    if(dwErr != ERROR_SUCCESS){//failed, but need to close it anyway
        ERRORMSG(1, (_T("%s ERROR: out transfer hTrasnfer = %p got USB error = 0x%x\r\n"), 
                               pszFname, hTransfer, dwUSBError));
        pUfnFuncs->lpCloseTransfer(hDevice, hTransfer);
        goto IT_EXIT;
    }
    else{
        DEBUGMSG(ZONE_TRANSFER, (_T("%s %u bytes received!\r\n"), pszFname, cbTransferred));
        if(cbTransferred != cbExpSize){//we got as many bytes as we want?
            ERRORMSG(1, (_T("%s ERROR: pipe: 0x%x, Expected bytes received from host is %u, actual recieved %u, some data may lost !!!\r\n"), 
                                pszFname, hPipe,  cbExpSize, cbTransferred));
            pUfnFuncs->lpCloseTransfer(hDevice, hTransfer);
            goto IT_EXIT;
        }
        pUfnFuncs->lpCloseTransfer(hDevice, hTransfer);
        hTransfer = NULL;
    }

    fRet = TRUE;
    
IT_EXIT:

    CloseHandle(hCompEvent);
    FUNCTION_LEAVE_MSG();
    return fRet;

}

