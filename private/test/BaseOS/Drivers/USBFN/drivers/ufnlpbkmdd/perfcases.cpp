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
#include "perfcases.h"

//globals

static USHORT   g_usRounds;
static USHORT   g_usCurrRets, g_usGenericCallbackCnt, g_usGenericTotalCnt;
HANDLE  g_hCompEvent, g_hGenericEvent;

#define PERF_WAIT_TIME 60*1000
#define TIMING_ROUNDS 50
#define MAX_RESULT_SIZE 200

USHORT g_usResultBuf[MAX_RESULT_SIZE];

static const 
LPCTSTR epAttr[4] = { _T("CONTROL"), _T("ISOC"), _T("BULK"), _T("INTR") };


PVOID AllocPMemory(PDWORD pdwPhAddr,PDWORD pdwPageLength)
{


    PVOID InDataBlock;
    DWORD dwPages = *pdwPageLength/PAGE_SIZE;

    if (*pdwPageLength == 0) {
        NKDbgPrintfW(TEXT("AllocPMemory tried to allocate NO MEMORY\n"));
        return NULL;
    }
    if ((*pdwPageLength % PAGE_SIZE) > 0)
        dwPages += 1;

    InDataBlock = AllocPhysMem( dwPages*PAGE_SIZE,
                                PAGE_READWRITE|PAGE_NOCACHE,
                                0,    // alignment
                                0,    // Reserved
                                pdwPhAddr);
    if(InDataBlock==NULL) {
        NKDbgPrintfW(_T("AllocPMemory failed to allocate a page.\n"));
        *pdwPageLength = 0;
    }
    else {
        *pdwPageLength = dwPages*PAGE_SIZE;
        NKDbgPrintfW(TEXT("AllocPMemory allocated %u bytes of Memory\n"),*pdwPageLength);
    }

    return InDataBlock;
}

VOID               
SP_StartPerfDataTrans(PUFL_CONTEXT pContext,
PIPEPAIR_INFO PairInfo, 
UFN_PIPE OutPipe,  UFN_PIPE InPipe, BOOL fOUT,
USHORT usRepeat, DWORD dwBlockSize, BOOL fDBlocking,
int iTiming, BOOL fPhysMem)
{
    if(pContext == NULL || OutPipe == NULL || InPipe == NULL || usRepeat == 0 || dwBlockSize == 0)//actually this should not happen
        return;
    SETFNAME(_T("SP_StartPerfDataTrans"));
    FUNCTION_ENTER_MSG();

    UCHAR uThread = 0;
    if(FindVacantThreadPos(pContext->spThreads, &uThread) == FALSE){//this should not happen either
        ERRORMSG(1,(_T("%s there are already 8 threads running special tests, please run this test later"), pszFname));
        return;
    }

    //now we are able to create test thread
    PPERFDATATRANS pPerfTrans = new PERFDATATRANS;
    if(pPerfTrans == NULL){
        ERRORMSG(1,(_T("%s: OOM!"), pszFname));
        return;
    }

    pPerfTrans->pThread = &(pContext->spThreads[uThread]);
    pPerfTrans->hDevice = pContext->hDevice;
    pPerfTrans->pUfnFuncs = pContext->pUfnFuncs;
    pPerfTrans->fOUT = fOUT;
    pPerfTrans->Pipe = (fOUT == TRUE)?OutPipe:InPipe;
    pPerfTrans->ResultPipe = InPipe;
    pPerfTrans->usRepeat = usRepeat;
    pPerfTrans->dwBlockSize = dwBlockSize;
    pPerfTrans->fDBlocking =fDBlocking;
    pPerfTrans->iTiming = iTiming;
    pPerfTrans->fPhysMem = fPhysMem;
    memcpy(&pPerfTrans->PairInfo, &PairInfo, sizeof(PIPEPAIR_INFO));

    DWORD dwThreadId = 0;
    pContext->spThreads[uThread].hThread = CreateThread(NULL, 0, PerfDataTransThread, (LPVOID)pPerfTrans, 0, &dwThreadId);
    if(pContext->spThreads[uThread].hThread == NULL){
        ERRORMSG(1,(_T("%s: Create test thread failed!"), pszFname));
        return;
    }
    else{
        pContext->spThreads[uThread].fRunning = TRUE;
    }

    FUNCTION_LEAVE_MSG();
    return;
}

DWORD WINAPI
PerfDTCallback(LPVOID dParam){
    PPERFCALLBACKPARAM pPerfPara = (PPERFCALLBACKPARAM)dParam;

    if(pPerfPara != NULL){
        pPerfPara->uDone = 1;
        g_usCurrRets ++;
        if(g_usCurrRets == g_usRounds){//all transfers are done
            TimeCounting((PTIME_REC)pPerfPara->pTimeRec,FALSE);
            SetEvent(g_hCompEvent);
        }
    }
   
    return 0;
}

#define PERF_WAIT_COMPLETE_TIME     60*1000  //60 secounds wait time

BOOL
IssueContinuousTransfer(
PUFN_FUNCTIONS pUfnFuncs, UFN_HANDLE hDevice, BOOL fOUT, UFN_PIPE hPipe, 
PBYTE  pBuf, USHORT usRepeat, DWORD dwBlockSize, UFN_PIPE hResultPipe, int iTiming, ULONG pPhyBufAddr){

    if(pUfnFuncs == NULL || hDevice == NULL || hPipe == NULL || hResultPipe == NULL || pBuf == NULL || dwBlockSize == 0 || usRepeat == 0 || usRepeat > 1024)
        return FALSE;

    SETFNAME(_T("IssueContinuousTransfer"));
    FUNCTION_ENTER_MSG();

    BOOL fRet = FALSE;
    g_usCurrRets = 0;
    g_usRounds = usRepeat;
    TIME_REC TimeRec = {0};
    
    g_hCompEvent = CreateEvent(0, FALSE, FALSE, NULL);
    g_hGenericEvent = CreateEvent(0,FALSE,FALSE,NULL);
    PPERFCALLBACKPARAM  pCallBackPara = new PERFCALLBACKPARAM[usRepeat];
    if(pCallBackPara == NULL){
        ERRORMSG(1, (_T("%s OOM!!!\r\n"), pszFname));
        goto IT_EXIT;
    }
    memset(pCallBackPara, 0, sizeof(PERFCALLBACKPARAM)*usRepeat);

    DEBUGMSG(ZONE_TRANSFER, (_T("%s start executing transfer request, size = %d, rounds = %d, at endpoint %d(%S)\r\n"), 
                                        pszFname, dwBlockSize, usRepeat, (fOUT == TRUE)?_T("IN"):_T("OUT")));

    if(iTiming == DEV_TIMING) {
        Sleep(300);
        DEBUGMSG(ZONE_COMMENT, (_T("%%%%%%%% Dev timing.\n")));
        TimeCounting(&TimeRec, TRUE);
        }
    else if(iTiming == SYNC_TIMING) {
        DEBUGMSG(ZONE_COMMENT,(_T("%%%%%%%% Sync timing.\n")));
        g_usGenericCallbackCnt = 0;
        g_usGenericTotalCnt = TIMING_ROUNDS;
        ResetEvent(g_hGenericEvent);
        for(int i = 0; i < TIMING_ROUNDS; i++) {
            UFN_TRANSFER timingTransfer = NULL;
            DWORD dwErr = pUfnFuncs->lpIssueTransfer(hDevice,
                                                     hPipe,
                                                     &GenericNotify,
                                                     (LPVOID)&TimeRec,
                                                     ((fOUT)?USB_OUT_TRANSFER:USB_IN_TRANSFER),
                                                     MAX_RESULT_SIZE,
                                                     g_usResultBuf,
                                                     (ULONG)NULL,
                                                     0,
                                                     &timingTransfer);
            if(dwErr != ERROR_SUCCESS || timingTransfer == INVALID_HANDLE_VALUE || timingTransfer == NULL) {
                ERRORMSG(1,(_T("Failed to issue timing transfer [%d]\r\n"), i));
                    goto IT_EXIT;
            }
            if(WaitForSingleObject(g_hGenericEvent,PERF_WAIT_TIME) != WAIT_OBJECT_0){//sth. wrong
                ERRORMSG(1,(_T("Timing transfer not completed with 60 secs, test will quit!\r\n")));
                    goto IT_EXIT;
            }
            pUfnFuncs->lpCloseTransfer(hDevice,timingTransfer);
        }
    }
    
    for(int i = 0; i < usRepeat; i++){
     pCallBackPara[i].pTimeRec = &TimeRec;
        DWORD dwErr = pUfnFuncs->lpIssueTransfer(hDevice, hPipe, 
                                                  &PerfDTCallback, (LPVOID)(&pCallBackPara[i]), fOUT?USB_OUT_TRANSFER:USB_IN_TRANSFER,
                                                  dwBlockSize, pBuf, pPhyBufAddr, 0, &(pCallBackPara[i].hTransfer));

        if(dwErr != ERROR_SUCCESS || pCallBackPara[i].hTransfer == INVALID_HANDLE_VALUE || pCallBackPara[i].hTransfer == NULL ){//sth. wrong
            ERRORMSG(1, (_T("%s Issuing transfer #%i failed, error = 0x%x\r\n"), pszFname, i, dwErr));
            goto IT_EXIT;
        }
    }
    
    //Wait for all transfers to complete
    DWORD dwWait = WaitForSingleObject(g_hCompEvent, PERF_WAIT_COMPLETE_TIME);
    if(dwWait != WAIT_OBJECT_0){//sth. wrong
        ERRORMSG(1, (_T("%s Transfer(s) not complete within 60 secs, test will quit!\r\n"), pszFname));
        goto IT_EXIT;
    }        

    double AvgThroughput = CalcAvgThroughput(TimeRec, usRepeat, dwBlockSize);
     
    if(iTiming == DEV_TIMING || iTiming == SYNC_TIMING) {
        memset(g_usResultBuf,0,sizeof(unsigned short) * MAX_RESULT_SIZE);
        _stprintf_s(g_usResultBuf,MAX_RESULT_SIZE,_T("%0.3f"),AvgThroughput);
        g_usGenericCallbackCnt = 0;
        g_usGenericTotalCnt = 1;
        UFN_TRANSFER infTransfer = {0};
        ResetEvent(g_hGenericEvent);
        DWORD dwErr = pUfnFuncs->lpIssueTransfer(hDevice,
                                                 hResultPipe,
                                                 &GenericNotify,
                                                 (PVOID)0xcccccccc,
                                                 USB_IN_TRANSFER,
                                                 MAX_RESULT_SIZE*sizeof(unsigned short),
                                                 g_usResultBuf,
                                                 NULL,
                                                 0,
                                                 &infTransfer);
        if(dwErr != ERROR_SUCCESS || infTransfer == INVALID_HANDLE_VALUE) {
            ERRORMSG(1,(_T("Failed to issue information transfer.\r\n")));
               goto IT_EXIT;
        }
        if(WaitForSingleObject(g_hGenericEvent,PERF_WAIT_TIME) != WAIT_OBJECT_0) {
            ERRORMSG(1,(_T("Information transfer not completed with 60 secs, test will quit!\r\n")));
            goto IT_EXIT;
            }
        pUfnFuncs->lpCloseTransfer(hDevice,infTransfer);
        ERRORMSG(1,(_T("average transfer speed: %s\n"),g_usResultBuf));
     }
     
    Sleep(100);
    fRet = TRUE;

IT_EXIT:

    if(pCallBackPara != NULL){
        for(int i = 0; i < usRepeat; i++){
            if(pCallBackPara[i].uDone != 0){
                pUfnFuncs->lpCloseTransfer(hDevice, pCallBackPara[i].hTransfer);
            }
            else {
                pUfnFuncs->lpAbortTransfer(hDevice, pCallBackPara[i].hTransfer);
                ERRORMSG(1, (_T("%s Completion of transfer #%i failed!!!\r\n"), pszFname, i));
            }
        }
        delete[] pCallBackPara;
        pCallBackPara = NULL;
    }

    if(g_hCompEvent != NULL){
        CloseHandle(g_hCompEvent);
        g_hCompEvent = NULL;
    }
    if(g_hGenericEvent != NULL){
        CloseHandle(g_hGenericEvent);
        g_hGenericEvent = NULL;
    }
    FUNCTION_LEAVE_MSG();
    return fRet;

}

DWORD WINAPI 
PerfDataTransThread (LPVOID lpParameter) {

    PPERFDATATRANS pPerfTrans = (PPERFDATATRANS)lpParameter;
    if(pPerfTrans == NULL){//this should not happen
        ExitThread((DWORD)-1);
        return (DWORD)-1;
    }

    SETFNAME(_T("PerfDataTransThread"));
    FUNCTION_ENTER_MSG();

    DWORD dwRet = (DWORD)-1;
    DWORD phyBufAddr = NULL;
    
    PBYTE pBuf = NULL;

    if(pPerfTrans->fPhysMem) {
        DEBUGMSG(ZONE_COMMENT, (_T("Physical memory.\n")));
        DWORD dwRealLen = pPerfTrans->dwBlockSize;
        pBuf = (PBYTE)AllocPMemory(&phyBufAddr,&dwRealLen);
    }
    else {
        pBuf = new BYTE[pPerfTrans->dwBlockSize];
    }
    if(pBuf == NULL){
        ERRORMSG(1, (_T("%s OOM!! \r\n"), pszFname));
        goto THREAD_EXIT;
    }

    PUFN_FUNCTIONS pUfnFuncs = pPerfTrans->pUfnFuncs;
    UFN_HANDLE  hDevice = pPerfTrans->hDevice;
    BOOL    fTrans;

    fTrans = IssueContinuousTransfer(pUfnFuncs, hDevice, pPerfTrans->fOUT, 
                                                pPerfTrans->Pipe, pBuf, pPerfTrans->usRepeat, pPerfTrans->dwBlockSize, 
                                                pPerfTrans->ResultPipe, pPerfTrans->iTiming, phyBufAddr);
    if(fTrans == FALSE){
        ERRORMSG(1,(_T("%s IssueContinuousTransfer call failed!\r\n\t\t")
                    _T("TransferRequest:  %s_EPs=%02x:%02x, frm=%x; %s %d*%d bytes\r\n"),pszFname,
                    epAttr[pPerfTrans->PairInfo.bmAttribute],pPerfTrans->PairInfo.bInEPAddress,pPerfTrans->PairInfo.bOutEPAddress,
                    pPerfTrans->PairInfo.wCurPacketSize,pPerfTrans->fOUT?_T("RCV"):_T("SND"),pPerfTrans->dwBlockSize,pPerfTrans->usRepeat));
        goto THREAD_EXIT;
    }

THREAD_EXIT:

    if(phyBufAddr)
    FreePhysMem(pBuf);
    else if(pBuf)
        delete[] pBuf;
        
    if(pPerfTrans != NULL){
        pPerfTrans->pThread->fRunning = FALSE;
        delete pPerfTrans;
    }

    FUNCTION_LEAVE_MSG();
    
    ExitThread(dwRet);
    return dwRet;
}

VOID
TimeCounting(PTIME_REC pTimeRec, BOOL bStart){
    if(pTimeRec == NULL )
        return;

    LARGE_INTEGER   curTick;

    if(pTimeRec->perfFreq.QuadPart <= 1000){
        curTick.LowPart = GetTickCount();
    }
    else{
        QueryPerformanceCounter(&curTick);
    }

    if(bStart == TRUE){
        pTimeRec->startTime.QuadPart = curTick.QuadPart;
    }
    else{
        pTimeRec->endTime.QuadPart = curTick.QuadPart;
    }

}

double
CalcAvgTime(TIME_REC TimeRec, USHORT usRounds){

    if(TimeRec.perfFreq.QuadPart > 1000){
        LARGE_INTEGER Duration;
        Duration.QuadPart = TimeRec.endTime.QuadPart - TimeRec.startTime.QuadPart;
        double result = ((double)(Duration.QuadPart)*1000.0)/((double)(TimeRec.perfFreq.QuadPart)); 
        
        return result / (double)usRounds;
    }    
    else{
        return (double)(TimeRec.endTime.LowPart - TimeRec.startTime.LowPart) / (double)usRounds;
    }
}

double
CalcAvgThroughput(TIME_REC TimeRec, USHORT usRounds, DWORD dwBlockSize){
    double result = CalcAvgTime(TimeRec, usRounds);
    result = ((double)(dwBlockSize*8)/result/1000.0);
    return result;
}

DWORD WINAPI GenericNotify(LPVOID lpvNotifyParameter)
{    
    DEBUGMSG(ZONE_TRANSFER, (_T("Got callback: %d->%d, total = %d\n"), g_usGenericCallbackCnt, g_usGenericCallbackCnt+1, g_usGenericTotalCnt));
    g_usGenericCallbackCnt++;
    PTIME_REC pTimeRec = (PTIME_REC)lpvNotifyParameter; 
    if(g_usGenericCallbackCnt == g_usGenericTotalCnt){
        if(g_usGenericTotalCnt > 1 && (int)pTimeRec != 0xcccccccc)
        TimeCounting(pTimeRec, TRUE);
        SetEvent(g_hGenericEvent);
    }
    else {
     PulseEvent(g_hGenericEvent);
    }
    
    return 1;
}
