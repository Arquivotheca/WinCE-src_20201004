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
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

/*++
Module Name:  

PipeTest.cpp

Abstract:  
    USB Driver Transfer Test .

Functions:

Notes: 

--*/

#define __THIS_FILE__   TEXT("PerfTst.cpp")

#include <windows.h>
#include "usbperf.h"
#include "sysmon.h"
#include "Perftst.h"

#define TIMING_ROUNDS 50
#define MAX_RESULT_SIZE 200


CRITICAL_SECTION  g_CntCS;
static USHORT         g_usCallbackCnt, g_usGenericCallbackCnt, g_usGenericTotalCnt;
static USHORT         g_usRounds;
BOOL                     g_bIsoch;
HANDLE      g_hCompEvent, g_hGenericEvent;
USHORT g_usResultBuf[MAX_RESULT_SIZE];

#define  PERF_WAIT_TIME     20*1000  
//#else

PVOID AllocPMemory(PDWORD pdwPhAddr,PDWORD pdwPageLength)
{


	PVOID InDataBlock;
	DWORD dwPages = *pdwPageLength/PAGE_SIZE;

	if (*pdwPageLength == 0) {
		g_pKato->Log(LOG_FAIL, TEXT("AllocPMemory tried to allocate NO MEMORY\n"),  *pdwPageLength);
		return NULL;
	}
	if ((dwPages*PAGE_SIZE) < (*pdwPageLength))
		dwPages += 1;

	InDataBlock = AllocPhysMem(	dwPages*PAGE_SIZE,
								PAGE_READWRITE|PAGE_NOCACHE,
                                0,    // alignment
                                0,    // Reserved
                                pdwPhAddr);
	if(InDataBlock==NULL) {
		DEBUGMSG(DBG_ERR,(TEXT("AllocPMemory failed to allocate a memory page\n")));
		*pdwPageLength = 0;
	}
	else {
		*pdwPageLength = dwPages*PAGE_SIZE;
		DEBUGMSG(DBG_DETAIL,(TEXT(" AllocPMemory allocated %u bytes of Memory\n"), *pdwPageLength));
		g_pKato->Log(LOG_DETAIL, TEXT("AllocPMemory allocated %u bytes of Memory\n"), *pdwPageLength);
	}

	return InDataBlock;
}



BOOL 
NormalPerfTests(UsbClientDrv *pDriverPtr, UCHAR uEPType, int iID, int iPhy, BOOL fOUT, int iTiming, int iBlocking){

    if(pDriverPtr == NULL || (iTiming < 1 && iTiming > 3) || (iBlocking < 5 && iBlocking > 6) || (iPhy > 3))
        return FALSE;

    static const DWORD dwBlockSizes[] = {64, 128, 512, 1024, 1536, 2048, 4096, 1024*8, 1024*16, 1024*32};
    UCHAR uNumofSizes = sizeof(dwBlockSizes)/sizeof(DWORD);
    TP_FIXEDPKSIZE  PerfResults = {0};
    PerfResults.uNumofSizes = uNumofSizes;
    
    g_bIsoch = FALSE;

    UCHAR uEPIndex = 0;
    //first find out the loopback pair's IN/OUT enpoint addresses
    while(g_pTstDevLpbkInfo[iID]->pLpbkPairs[uEPIndex].ucType != uEPType){
        uEPIndex ++;
        if(uEPIndex >= g_pTstDevLpbkInfo[iID]->uNumOfPipePairs){
            g_pKato->Log(LOG_FAIL, TEXT("Can not find data pipe pair of the type %s "),  g_szEPTypeStrings[uEPType]);
             return FALSE;
        }
    }

    //then retrieve the corresponding endpoint structures
    LPCUSB_ENDPOINT pOUTEP = NULL;
    LPCUSB_ENDPOINT pINEP = NULL;
   
    for (UINT i = 0;i < pDriverPtr->GetNumOfEndPoints(); i++) {
        LPCUSB_ENDPOINT curPoint=pDriverPtr->GetDescriptorPoint(i);
        ASSERT(curPoint);
        if (curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uEPIndex].ucOutEp) {
            pOUTEP = curPoint;
        }
        if (curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uEPIndex].ucInEp) {
            pINEP = curPoint;
        }
        
    }
    if (pOUTEP == NULL || pINEP == NULL) {//one or both endpoints are missing
	g_pKato->Log(LOG_FAIL, TEXT("Can not find Pipe Pair"));
	return FALSE;
    }
    NKDbgPrintfW(_T("EA address =0x%x, packetsize= %d"), pOUTEP->Descriptor.bEndpointAddress, pOUTEP->Descriptor.wMaxPacketSize);
    PerfResults.usPacketSize = pOUTEP->Descriptor.wMaxPacketSize;

    USBPerf_StartSysMonitor(2, 1000);
    switch(uEPType){
        case USB_ENDPOINT_TYPE_BULK:
        case USB_ENDPOINT_TYPE_INTERRUPT:
        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
            for(UCHAR uIndex = 0; uIndex <uNumofSizes; uIndex++){
		  if(dwBlockSizes[uIndex] < pOUTEP->Descriptor.wMaxPacketSize)
		  	continue;
                PerfResults.UnitTPs[uIndex].dwBlockSize = dwBlockSizes[uIndex];
                USHORT usRounds = 64;
                if(pOUTEP->Descriptor.wMaxPacketSize <= 64){
                    if((dwBlockSizes[uIndex] / pOUTEP->Descriptor.wMaxPacketSize) >= 4)
                        usRounds = 32;
                    if((dwBlockSizes[uIndex] / pOUTEP->Descriptor.wMaxPacketSize) >= 16)
                        usRounds = 4;
                    if((dwBlockSizes[uIndex] / pOUTEP->Descriptor.wMaxPacketSize) >= 32)
                        usRounds = 2;
                }
                else if(pOUTEP->Descriptor.wMaxPacketSize >= 1024){
                    if((dwBlockSizes[uIndex] / pOUTEP->Descriptor.wMaxPacketSize) >= 1)
                        usRounds = 16;
                    if((dwBlockSizes[uIndex] / pOUTEP->Descriptor.wMaxPacketSize) >= 4)
                        usRounds = 8;
                    if((dwBlockSizes[uIndex] / pOUTEP->Descriptor.wMaxPacketSize) >= 8)
                        usRounds = 4;
                    if((dwBlockSizes[uIndex] / pOUTEP->Descriptor.wMaxPacketSize) >= 32)
                        usRounds = 2;
                        
                }
                else if(pOUTEP->Descriptor.wMaxPacketSize >= 256){
                    if((dwBlockSizes[uIndex] / pOUTEP->Descriptor.wMaxPacketSize) >= 2)
                        usRounds = 32;
                    if((dwBlockSizes[uIndex] / pOUTEP->Descriptor.wMaxPacketSize) >= 4)
                        usRounds = 16;
                    if((dwBlockSizes[uIndex] / pOUTEP->Descriptor.wMaxPacketSize) >= 8)
                        usRounds = 4;
                    if((dwBlockSizes[uIndex] / pOUTEP->Descriptor.wMaxPacketSize) >= 32)
                        usRounds = 2;
                        
                }
                if(uEPType == USB_ENDPOINT_TYPE_ISOCHRONOUS){
                    Isoch_Perf(pDriverPtr, pOUTEP, pINEP, dwBlockSizes[uIndex], usRounds, (iPhy == 1), &(PerfResults.UnitTPs[uIndex]), fOUT);
                }
                else {
                    BulkIntr_Perf(pDriverPtr, pOUTEP, pINEP, uEPType, dwBlockSizes[uIndex], usRounds, iPhy, &(PerfResults.UnitTPs[uIndex]), fOUT, iTiming, iBlocking);
                }
                Sleep(8000);
            }
            
            break;
   /*     case USB_ENDPOINT_TYPE_INTERRUPT:
          //  BulkIntr_Perf(pDriverPtr, pEP, uEPType, pEP->Descriptor.wMaxPacketSize, 128, bPhy);
            Sleep(8000);
           // BulkIntr_Perf(pDriverPtr, pEP, uEPType, pEP->Descriptor.wMaxPacketSize*32, 16, bPhy);
            break;
        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
            Isoch_Perf(pDriverPtr, pEP, pEP->Descriptor.wMaxPacketSize, 129, bPhy);
            Sleep(8000);
            Isoch_Perf(pDriverPtr, pEP, pEP->Descriptor.wMaxPacketSize*32, 11, bPhy);
            break;*/
        default:
            USBPerf_StopSysMonitor();
            return FALSE;
         
    }

    USBPerf_StopSysMonitor();

    PrintOutPerfResults(PerfResults);

    return TRUE;
	
}



BOOL BulkIntr_Perf(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpOUTPipePoint, LPCUSB_ENDPOINT lpINPipePoint, 
	UCHAR uType, DWORD dwBlockSize, USHORT usRounds, int iPhy, PONE_THROUGHPUT pTP, 
	BOOL fOUT, int iTiming, int iBlocking)
{
    DWORD dwOrigSize = dwBlockSize;
    if(pDriverPtr == NULL || lpOUTPipePoint == NULL || lpINPipePoint == NULL|| usRounds == 0 
	|| usRounds > 512 || dwBlockSize < 0 || dwBlockSize > 1024*1024) //prefast cautions
        return FALSE;

    LARGE_INTEGER   PerfReq;
    if(QueryPerformanceFrequency(&PerfReq) == FALSE || PerfReq.QuadPart == 0 || PerfReq.QuadPart == 1000){
        PerfReq.QuadPart = 1000; //does not support  high solution 
    }

    TCHAR szType[14] = {0};
    if(uType == USB_ENDPOINT_TYPE_BULK){
        wcscpy(szType, TEXT("Bulk"));
    }
    else{
        wcscpy(szType, TEXT("Interrupt"));
    }

    NKDbgPrintfW(_T("Performance Frequency is: %d"), PerfReq.QuadPart);

    LPCUSB_FUNCS lpUsbFuncs =pDriverPtr->lpUsbFuncs;
    USB_HANDLE usbHandle= pDriverPtr->GetUsbHandle();
    LPCUSB_ENDPOINT lpPipePoint = fOUT?lpOUTPipePoint:lpINPipePoint;
    USB_PIPE hPipe=(*(lpUsbFuncs->lpOpenPipe))(usbHandle, &(lpPipePoint->Descriptor));
    USB_PIPE hResultPipe=fOUT?(*(lpUsbFuncs->lpOpenPipe))(usbHandle, &(lpINPipePoint->Descriptor)):hPipe;
    if (hPipe==NULL || hResultPipe == NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("Error at Open Pipe"));
        return FALSE;
    }

    USB_TDEVICE_REQUEST utdr = {0};
    utdr.bmRequestType = USB_REQUEST_CLASS;
    if(uType == USB_ENDPOINT_TYPE_BULK) {
	if(iTiming == HOST_TIMING) {
		if(iBlocking == HOST_BLOCK) {
			if(iPhy == PHYS_MEM_FUNC || iPhy == PHYS_MEM_ALL) {
				NKDbgPrintfW(_T("host blocking, host timing, physical memory on the device.\n"));
				utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_HBLOCKING_HTIMING_PDEV:TEST_REQUEST_PERFIN_HBLOCKING_HTIMING_PDEV;
			}
			else {
				NKDbgPrintfW(_T("host blocking, host timing.\n"));
				utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_HBLOCKING_HTIMING:TEST_REQUEST_PERFIN_HBLOCKING_HTIMING;
			}
		}
		else if(iBlocking == FUNCTION_BLOCK) {
			if(iPhy == PHYS_MEM_FUNC || iPhy == PHYS_MEM_ALL) {
				NKDbgPrintfW(_T("function blocking, host timing, physical memory on the device.\n"));
				utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_DBLOCKING_HTIMING_PDEV:TEST_REQUEST_PERFIN_DBLOCKING_HTIMING_PDEV;
			}
			else {
				NKDbgPrintfW(_T("function blocking, host timing.\n"));
				utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_DBLOCKING_HTIMING:TEST_REQUEST_PERFIN_DBLOCKING_HTIMING;
			}
		}
		else {
			NKDbgPrintfW(_T("host timing.\n"));
			utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT:TEST_REQUEST_PERFIN;
		}
	}
	else if(iTiming == DEV_TIMING) {
		if(iBlocking == HOST_BLOCK) {
			if(iPhy == PHYS_MEM_FUNC || iPhy == PHYS_MEM_ALL) {
				NKDbgPrintfW(_T("host blocking, function timing, physical memory.\n"));
				utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_HBLOCKING_DTIMING_PDEV:TEST_REQUEST_PERFIN_HBLOCKING_DTIMING_PDEV;
			}
			else {
				NKDbgPrintfW(_T("host blocking, function timing\n"));
				utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_HBLOCKING_DTIMING:TEST_REQUEST_PERFIN_HBLOCKING_DTIMING;
			}
		}
		else if(iBlocking == FUNCTION_BLOCK) {
			if(iPhy == PHYS_MEM_FUNC || iPhy == PHYS_MEM_ALL) {
				NKDbgPrintfW(_T("function blocking, function timing, physical memory.\n"));
				utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_DBLOCKING_DTIMING_PDEV:TEST_REQUEST_PERFIN_DBLOCKING_DTIMING_PDEV;
			}
			else {
				NKDbgPrintfW(_T("function blocking, function timing.\n"));
				utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_DBLOCKING_DTIMING:TEST_REQUEST_PERFIN_DBLOCKING_DTIMING;
			}
		}
		else {
			NKDbgPrintfW(_T("Invalid test.\n"));
			return FALSE;
		}
	}
	else {
		if(iBlocking == HOST_BLOCK) {
			if(iPhy == PHYS_MEM_FUNC || iPhy == PHYS_MEM_ALL) {
				NKDbgPrintfW(_T("host blocking, sync timing, physical memory.\n"));
				utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_HBLOCKING_STIMING_PDEV:TEST_REQUEST_PERFIN_HBLOCKING_STIMING_PDEV;
			}
			else {
				NKDbgPrintfW(_T("host blocking, sync timing.\n"));
				utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_HBLOCKING_STIMING:TEST_REQUEST_PERFIN_HBLOCKING_STIMING;
			}
		}
		else if(iBlocking == FUNCTION_BLOCK) {
			if(iPhy == PHYS_MEM_FUNC || iPhy == PHYS_MEM_ALL) {
				NKDbgPrintfW(_T("device blocking, sync timing, physical memory.\n"));
				utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_DBLOCKING_STIMING_PDEV:TEST_REQUEST_PERFIN_DBLOCKING_STIMING_PDEV;
			}
			else {
				NKDbgPrintfW(_T("device blocking, sync timing.\n"));
				utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT_DBLOCKING_STIMING:TEST_REQUEST_PERFIN_DBLOCKING_STIMING;
			}
		}
		else {
			NKDbgPrintfW(_T("Invalid test.\n"));
			return FALSE;
		}
	}
    }
    else {
	utdr.bRequest = (fOUT)?TEST_REQUEST_PERFOUT:TEST_REQUEST_PERFIN;
    }
    utdr.wLength = sizeof(USB_TDEVICE_PERFLPBK);
    USB_TDEVICE_PERFLPBK utperfdl = {0};
    utperfdl.uOutEP = lpOUTPipePoint->Descriptor.bEndpointAddress;
 	if(iBlocking == FUNCTION_BLOCK) {
	    utperfdl.dwBlockSize = dwBlockSize * usRounds;
	    utperfdl.usRepeat = 1;
 	}
	else {
	    utperfdl.dwBlockSize = dwBlockSize;
	    utperfdl.usRepeat = usRounds;
	}
    utperfdl.uDirection = (fOUT)?0:1;

    if(iBlocking == HOST_BLOCK) {
		dwBlockSize *= usRounds;
		usRounds = 1;
	}

	
    PBYTE pBuffer = NULL;
    DWORD pPhyBufferAddr = NULL;
    if(iPhy == PHYS_MEM_HOST || iPhy == PHYS_MEM_ALL){
        DWORD dwRealLen = dwBlockSize;
        pBuffer = (PBYTE)AllocPMemory(&pPhyBufferAddr, &dwRealLen);
    }
    else{
	 PREFAST_SUPPRESS(419, "Potential buffer overrun: The size parameter 'dwBlockSize' is being used without validation.");
        pBuffer = (PBYTE) new BYTE [dwBlockSize];
    }
    
    if(pBuffer == NULL)
        return FALSE;

        
    PPerfTransStatus pTrSt = (PPerfTransStatus) new PerfTransStatus[usRounds];
    if(pTrSt == NULL){
        delete[] pBuffer;
        return FALSE;
    }

    g_usRounds = usRounds;

    TIME_REC  TimeRec = {0};
    TimeRec.perfFreq.QuadPart = PerfReq.QuadPart;
    
    for(int i = 0; i < usRounds; i++){
        pTrSt[i].hTransfer=0;
        pTrSt[i].fDone=FALSE;
        pTrSt[i].pTimeRec = &TimeRec;
        pTrSt[i].flCPUPercentage = 0.0;
        pTrSt[i].flMemPercentage = 0.0;
    }

    NKDbgPrintfW(_T("packetsize is %d, block size = %d, rounds = %d"), lpPipePoint->Descriptor.wMaxPacketSize, utperfdl.dwBlockSize, utperfdl.usRepeat);


    USB_TRANSFER hVTransfer = NULL;
    BOOL bRet = TRUE;
    
#if 0
    //-----------------------------------------------------------------------------------
    //  COUNT API CALLING TIME 
    //-----------------------------------------------------------------------------------

    //issue command to netchip2280
    USB_TRANSFER hVTransfer = pDriverPtr->IssueVendorTransfer(NULL, NULL, USB_OUT_TRANSFER, (PUSB_DEVICE_REQUEST)&utdr, (PVOID)&utdl, 0);
    if(hVTransfer != NULL){ 
        (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);
    }

    Sleep(300);
    
    if(uType == USB_ENDPOINT_TYPE_BULK){
        TimeCounting(&TimeRec, TRUE);
        for(int i = 0; i < usRounds; i++){
            pTrSt[i].hTransfer=(*lpUsbFuncs->lpIssueBulkTransfer)(
                                                                    hPipe,
                                                                    DummyNotify,
                                                                    &pTrSt[i],
                                                                    USB_OUT_TRANSFER|USB_SEND_TO_ENDPOINT|USB_NO_WAIT,
                                                                    usBlockSize,
                                                                    pBuffer,
                                                                    (ULONG)pPhyBufferAddr);
        }
        TimeCounting(&TimeRec, FALSE);
    }
    else{
        TimeCounting(&TimeRec, TRUE);
        for(int i = 0; i < usRounds; i++){
            pTrSt[i].hTransfer=(*lpUsbFuncs->lpIssueInterruptTransfer)(
                                                                    hPipe,
                                                                    DummyNotify,
                                                                    &pTrSt[i],
                                                                    USB_OUT_TRANSFER|USB_SEND_TO_ENDPOINT|USB_NO_WAIT,
                                                                    usBlockSize,
                                                                    pBuffer,
                                                                    (ULONG)pPhyBufferAddr);
        }
        TimeCounting(&TimeRec, FALSE);
    }


    Sleep(10000);
    
    bRet = TRUE;
    for(int i = 0; i < usRounds; i++){
        if(pTrSt[i].hTransfer == NULL || pTrSt[i].IsFinished == FALSE){
            g_pKato->Log(LOG_FAIL,TEXT("Transfer No. %d failed!"), i);
            bRet = FALSE;
            continue;
        }
        if (!(*lpUsbFuncs->lpCloseTransfer)(pTrSt[i].hTransfer)) {
            g_pKato->Log(LOG_FAIL,TEXT("CloseTransfer Failure on No. %d"), i);
            bRet = FALSE;
        }
    }

    if(bRet == FALSE){
        g_pKato->Log(LOG_FAIL,TEXT("Performance test on %s transfer API calling time failed!"), szType);
        goto EXIT;
    }
    else{
        double AvgTime = CalcAvgTime(TimeRec, usRounds);
        if(uType == USB_ENDPOINT_TYPE_BULK){
            g_pKato->Log(LOG_DETAIL, TEXT("Average calling time for IssueBulkTransfer()  with packetsize %d, block size %d is %.3f mil sec"), 
                                                     lpPipePoint->Descriptor.wMaxPacketSize, usBlockSize, AvgTime);
        }
        else {
            g_pKato->Log(LOG_DETAIL, TEXT("Average calling time for IssueInterruptTransfer()  with packetsize %d, block size %d is %.3f mil sec"), 
                                                     lpPipePoint->Descriptor.wMaxPacketSize, usBlockSize, AvgTime);
        }
    }

    Sleep(5000);
#endif

    //-----------------------------------------------------------------------------------
    //  COUNT throughput 
    //-----------------------------------------------------------------------------------

    
    //issue command to netchip2280
    NKDbgPrintfW(_T("Going to issue vendor transfer.\n"));
    hVTransfer = pDriverPtr->IssueVendorTransfer(NULL, NULL, USB_OUT_TRANSFER, (PUSB_DEVICE_REQUEST)&utdr, (PVOID)&utperfdl, 0);
    NKDbgPrintfW(_T("Going to issue vendor transfer.\n"));
    if(hVTransfer != NULL){ 
        (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);
    }
    else{
        g_pKato->Log(LOG_FAIL, TEXT("Issue Vendor transfer failed!"));
        goto EXIT;
    }
    NKDbgPrintfW(_T("Going to issue vendor transfer.\n"));    
    g_hGenericEvent = CreateEvent(0, FALSE, FALSE, NULL);
	if(iTiming == HOST_TIMING) {
	    Sleep(300);
	    TimeCounting(&TimeRec, TRUE);
	}
	else if(iTiming == SYNC_TIMING) {
		g_usGenericCallbackCnt = 0;
		g_usGenericTotalCnt = TIMING_ROUNDS;
		ResetEvent(g_hGenericEvent);
		for(int i = 0; i < TIMING_ROUNDS; i++) {
			USB_TRANSFER timingTransfer = (*lpUsbFuncs->lpIssueBulkTransfer)(
                                                                    hPipe,
                                                                    GenericNotify,
                                                                    (LPVOID)&TimeRec,
                                                                    ((fOUT)?USB_OUT_TRANSFER:USB_IN_TRANSFER)|USB_SEND_TO_ENDPOINT|USB_NO_WAIT,
                                                                    MAX_RESULT_SIZE,
                                                                    g_usResultBuf,
                                                                    (ULONG)pPhyBufferAddr);
			if(timingTransfer == NULL || WaitForSingleObject(g_hGenericEvent,PERF_WAIT_TIME) != WAIT_OBJECT_0){//sth. wrong
		       	 g_pKato->Log(LOG_FAIL, _T("Timing transfer not completed with 60 secs, test will quit!\r\n"));
        			goto EXIT;
			}
		}
	}
	else {
	       TimeCounting(&TimeRec, TRUE);
	}
    
    g_usCallbackCnt = 0;
    g_usRounds = usRounds;
    g_hCompEvent = CreateEvent(0, FALSE, FALSE, NULL);


    if(uType == USB_ENDPOINT_TYPE_BULK){
        for(int i = 0; i < usRounds; i++){
            DWORD dwFlags = (fOUT)?USB_OUT_TRANSFER:USB_IN_TRANSFER;
            pTrSt[i].hTransfer=(*lpUsbFuncs->lpIssueBulkTransfer)(
                                                                    hPipe,
                                                                    ThroughputNotify,
                                                                    &pTrSt[i],
                                                                    dwFlags |USB_SEND_TO_ENDPOINT|USB_NO_WAIT,
                                                                    dwBlockSize,
                                                                    pBuffer,
                                                                    (ULONG)pPhyBufferAddr);
        }
    }
    else{
        TimeCounting(&TimeRec, TRUE);
        for(int i = 0; i < usRounds; i++){
            DWORD dwFlags = (fOUT)?USB_OUT_TRANSFER:USB_IN_TRANSFER;
            pTrSt[i].hTransfer=(*lpUsbFuncs->lpIssueInterruptTransfer)(
                                                                    hPipe,
                                                                    ThroughputNotify,
                                                                    &pTrSt[i],
                                                                    dwFlags |USB_SEND_TO_ENDPOINT|USB_NO_WAIT,
                                                                    dwBlockSize,
                                                                    pBuffer,
                                                                    (ULONG)pPhyBufferAddr);
        }
    }
    
    //Wait for all transfers to complete
    DWORD dwWait = WaitForSingleObject(g_hCompEvent, PERF_WAIT_TIME);
    if(dwWait != WAIT_OBJECT_0){//sth. wrong
        g_pKato->Log(LOG_FAIL, _T("Transfer not completed with 60 secs, test will quit!\r\n"));
        goto EXIT;
    }        

 
    int iSysPerfs = 0;
    float flTotalCPUper = 0.0;
    float flTotalMemper = 0.0;
    
    for(int i = 0; i < usRounds; i++){
        if(pTrSt[i].hTransfer == NULL || pTrSt[i].fDone == FALSE){
            g_pKato->Log(LOG_FAIL,TEXT("Transfer No. %d failed!"), i);
            (*lpUsbFuncs->lpAbortTransfer)(pTrSt[i].hTransfer, USB_NO_WAIT);
            bRet = FALSE;
            continue;
        }
        if (!(*lpUsbFuncs->lpCloseTransfer)(pTrSt[i].hTransfer)) {
            g_pKato->Log(LOG_FAIL,TEXT("CloseTransfer Failure on No. %d"), i);
            bRet = FALSE;
        }

        if(pTrSt[i].flCPUPercentage > 0.0){
            flTotalCPUper += pTrSt[i].flCPUPercentage;
            flTotalMemper += pTrSt[i].flMemPercentage;
            iSysPerfs ++;
            pTrSt[i].flCPUPercentage = 0.0;
            pTrSt[i].flMemPercentage = 0.0;
        }
    }

 if(iTiming == DEV_TIMING || iTiming == SYNC_TIMING) {
	 	memset(g_usResultBuf,0,sizeof(unsigned short) * MAX_RESULT_SIZE);
		g_usGenericCallbackCnt = 0;
		g_usGenericTotalCnt = 1;
		USB_TRANSFER hInfTransfer = 0;
		ResetEvent(g_hGenericEvent);
		hInfTransfer=(*lpUsbFuncs->lpIssueBulkTransfer)(
                                                                    hResultPipe,
                                                                    GenericNotify,
                                                                    (PVOID)0xcccccccc,
                                                                    USB_IN_TRANSFER |USB_SEND_TO_ENDPOINT|USB_NO_WAIT,
                                                                    MAX_RESULT_SIZE*sizeof(unsigned short),
                                                                    g_usResultBuf,
                                                                    (ULONG)NULL);
		if(hInfTransfer == NULL) {
			NKDbgPrintfW(_T("Failed to issue result transfer.\n"));
            		(*lpUsbFuncs->lpAbortTransfer)(pTrSt[i].hTransfer, USB_NO_WAIT);
		}
		if(WaitForSingleObject(g_hGenericEvent,PERF_WAIT_TIME) != WAIT_OBJECT_0) {
			g_pKato->Log(LOG_FAIL, _T("Information transfer not completed with 60 secs, test will quit!\r\n"));
		       goto EXIT;
		}
		(*lpUsbFuncs->lpCloseTransfer)(hInfTransfer);
		NKDbgPrintfW(_T("%s\n"),g_usResultBuf);
	 }
    if(bRet == FALSE){
        g_pKato->Log(LOG_FAIL,TEXT("Performance test on %s transfer throughput failed!"), szType);
        goto EXIT;
    }
    else{
        double AvgThroughput = CalcAvgThroughput(TimeRec, usRounds, dwBlockSize);
	 float CpuUsage = flTotalCPUper / (float)iSysPerfs;
	 float MemUsage = flTotalMemper / (float)iSysPerfs;
	 
	
		
        if(uType == USB_ENDPOINT_TYPE_BULK){
            g_pKato->Log(LOG_DETAIL, TEXT("Average throughput for Bulk Transfer with packetsize %d, block size %d is %.3f Mbps"), 
                                                     lpPipePoint->Descriptor.wMaxPacketSize, dwBlockSize, AvgThroughput);
        }
        else{
            g_pKato->Log(LOG_DETAIL, TEXT("Average throughput for Interrupt Transfer with packetsize %d, block size %d is %.3f Mbps"), 
                                                     lpPipePoint->Descriptor.wMaxPacketSize, dwBlockSize, AvgThroughput);
        }

        g_pKato->Log(LOG_DETAIL, TEXT("Average CPU usage is %2.3f %%; Mem usage is %2.3f %%"), 
                                                 CpuUsage, MemUsage);
        if(pTP != NULL){
            pTP->dbThroughput = AvgThroughput;
            if(iSysPerfs == 0){
                g_pKato->Log(LOG_DETAIL, TEXT("WARNING: No CPU/Memory usage number availabe"));                
            }
            else {
                pTP->flCPUUsage = CpuUsage;
                pTP->flMemUsage = MemUsage;
            }
        }
    }
    
EXIT:
    if ((*(lpUsbFuncs->lpClosePipe))(hPipe)==FALSE) {
        g_pKato->Log(LOG_FAIL, TEXT("Error at Close Pipe"));
        DEBUGMSG(DBG_ERR,(TEXT("Error at Close Pipe")));
        bRet = FALSE;
    }
    if(fOUT && (*(lpUsbFuncs->lpClosePipe))(hResultPipe)==FALSE) {
        g_pKato->Log(LOG_FAIL, TEXT("Error at Close Pipe"));
        DEBUGMSG(DBG_ERR,(TEXT("Error at Close Pipe")));
        bRet = FALSE;
    }

    if(pPhyBufferAddr)
        FreePhysMem(pBuffer);
    else if(pBuffer)
        delete[] pBuffer;
        
    if(pTrSt){
        delete[] pTrSt;
    }

    g_usRounds = 0;
    g_usCallbackCnt = 0;

    if(g_hCompEvent != NULL){
        CloseHandle(g_hCompEvent);
        g_hCompEvent = NULL;
    }
    if(g_hGenericEvent != NULL){
        CloseHandle(g_hGenericEvent);
        g_hGenericEvent = NULL;
    }
    
    return bRet;

}


BOOL Isoch_Perf(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpOUTPipePoint,  LPCUSB_ENDPOINT lpINPipePoint, 
DWORD dwBlockSize, USHORT usRounds, BOOL bPhy, PONE_THROUGHPUT pTP, BOOL fOUT)
{
    if(pDriverPtr == NULL || lpOUTPipePoint == NULL || lpINPipePoint == NULL|| usRounds == 0 
        || usRounds > 512 || dwBlockSize < 0 || dwBlockSize > 1024*1024) //prefast cautions
        return FALSE;

    LARGE_INTEGER   PerfReq;
    if(QueryPerformanceFrequency(&PerfReq) == FALSE || PerfReq.QuadPart == 0 || PerfReq.QuadPart == 1000){
        PerfReq.QuadPart = 1000; //does not support  high solution 
    }

    NKDbgPrintfW(_T("Performance Frequency is: %d"), PerfReq.QuadPart);

    LPCUSB_FUNCS lpUsbFuncs =pDriverPtr->lpUsbFuncs;
    USB_HANDLE usbHandle= pDriverPtr->GetUsbHandle();
    
    LPCUSB_ENDPOINT lpPipePoint = fOUT?lpOUTPipePoint:lpINPipePoint;
    USB_PIPE hPipe=(*(lpUsbFuncs->lpOpenPipe))(usbHandle,&(lpPipePoint->Descriptor));
    if (hPipe==NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("Error at Open Pipe"));
        return FALSE;
    }

    (*(lpUsbFuncs->lpResetPipe))(hPipe);

    PBYTE pBuffer = NULL;
    DWORD pPhyBufferAddr = NULL;
    if(bPhy == TRUE){
        DWORD dwRealLen = dwBlockSize;
        pBuffer = (PBYTE)AllocPMemory(&pPhyBufferAddr, &dwRealLen);
    }
    else{
	 PREFAST_SUPPRESS(419, "Potential buffer overrun: The size parameter 'dwBlockSize' is being used without validation.");
        pBuffer = (PBYTE) new BYTE [dwBlockSize];
    }
    if(pBuffer == NULL)
        return FALSE;
        
    PPerfTransStatus pTrSt = (PPerfTransStatus) new PerfTransStatus[usRounds];
    if(pTrSt == NULL){
        delete[] pBuffer;
        return FALSE;
    }

    DWORD dwPacketSize = lpPipePoint->Descriptor.wMaxPacketSize;  
    DWORD dwNumofPackets = dwBlockSize/dwPacketSize;
    PDWORD pBlockLengths = new DWORD[dwNumofPackets];
    if(pBlockLengths == NULL){
        delete[] pBuffer;
        delete[] pTrSt;
        return FALSE;
    }

    g_bIsoch = TRUE;
    
    for(DWORD dwi = 0; dwi < dwNumofPackets; dwi ++)
        pBlockLengths[dwi] = dwPacketSize;

    g_usRounds = usRounds;

    TIME_REC  TimeRec = {0};
    TimeRec.perfFreq.QuadPart = PerfReq.QuadPart;
    
    for(int i = 0; i < usRounds; i++){
        pTrSt[i].hTransfer=0;
        pTrSt[i].fDone=FALSE;
        pTrSt[i].pTimeRec = &TimeRec;
        pTrSt[i].flCPUPercentage = 0.0;
        pTrSt[i].flMemPercentage = 0.0;
    }

    USB_TDEVICE_REQUEST utdr = {0};
    utdr.bmRequestType = USB_REQUEST_CLASS;
    utdr.bRequest = TEST_REQUEST_PERFOUT;
    utdr.wLength = sizeof(USB_TDEVICE_PERFLPBK);
    USB_TDEVICE_PERFLPBK utperfdl = {0};
    utperfdl.uOutEP = lpOUTPipePoint->Descriptor.bEndpointAddress;
    utperfdl.dwBlockSize = dwBlockSize;
    utperfdl.usRepeat = usRounds;
    utperfdl.uDirection = 0;
    NKDbgPrintfW(_T("packetsize is %d, block size = %d, rounds = %d"), lpPipePoint->Descriptor.wMaxPacketSize, utperfdl.dwBlockSize, utperfdl.usRepeat);


    USB_TRANSFER hVTransfer = NULL;
    BOOL bRet = TRUE;
    PDWORD pCurLengths= new DWORD[dwNumofPackets];
    PDWORD pCurErrors = new DWORD[dwNumofPackets];
    if(pCurErrors == NULL || pCurLengths == NULL){
        bRet = FALSE;
        goto EXIT;
    }
  
#if 0
    //-----------------------------------------------------------------------------------
    //  COUNT API CALLING TIME 
    //-----------------------------------------------------------------------------------

    //issue command to netchip2280
    USB_TRANSFER hVTransfer = pDriverPtr->IssueVendorTransfer(NULL, NULL, USB_OUT_TRANSFER, (PUSB_DEVICE_REQUEST)&utdr, (PVOID)&utdl, 0);
    if(hVTransfer != NULL){ 
        (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);
    }
    Sleep(300);

    DWORD dwCurFrame = 0;
    if(pDriverPtr->GetFrameNumber(&dwCurFrame) == FALSE){
        g_pKato->Log(LOG_FAIL,TEXT("GetCurrentFrameNumber failed!"));
        goto EXIT;
    }
    else{
        NKDbgPrintfW(_T("Current frame number is: %d"), dwCurFrame);
    }

    pTrSt[0].hTransfer=(*lpUsbFuncs->lpIssueIsochTransfer)(
                                                            hPipe,
                                                            DummyNotify,
                                                            &pTrSt[0],
                                                            USB_SEND_TO_ENDPOINT|USB_NO_WAIT|USB_OUT_TRANSFER,
                                                            dwCurFrame+500,
                                                            dwNumofPackets,
                                                            pBlockLengths,
                                                            pBuffer,
                                                            (ULONG)pPhyBufferAddr);

    NKDbgPrintfW(_T("PHYADDR +0x%p"), pPhyBufferAddr);
    TimeCounting(&TimeRec, TRUE);
    for(int i = 1; i < usRounds; i++){
        pTrSt[i].hTransfer=(*lpUsbFuncs->lpIssueIsochTransfer)(
                                                                hPipe,
                                                                DummyNotify,
                                                                &pTrSt[i],
                                                                USB_START_ISOCH_ASAP|USB_SEND_TO_ENDPOINT|USB_NO_WAIT|USB_OUT_TRANSFER,
                                                                0,
                                                                dwNumofPackets,
                                                                pBlockLengths,
                                                                pBuffer,
                                                                (ULONG)pPhyBufferAddr);
    }
    TimeCounting(&TimeRec, FALSE);

    Sleep(10000);
    
    BOOL bRet = TRUE;
    // Get Transfer Status.
    
    for(int i = 0; i < usRounds; i++){
        if(pTrSt[i].hTransfer == NULL || pTrSt[i].IsFinished == FALSE){
            g_pKato->Log(LOG_FAIL,TEXT("Transfer No. %d failed!"), i);
            bRet = FALSE;
            continue;
        }

        if (!(*lpUsbFuncs->lpIsTransferComplete)(pTrSt[i].hTransfer)) {
            g_pKato->Log(LOG_FAIL, TEXT("Transfer No. %d:  IsTransferComplet not return true after call back"), i);
            bRet = FALSE;
            continue;
        }

        if (!(*lpUsbFuncs->lpGetIsochResults)(pTrSt[i].hTransfer, dwNumofPackets, pCurLengths,pCurErrors))  { // fail
            g_pKato->Log(LOG_FAIL, TEXT(" Transfer No. %d: Failure:GetIsochStatus return FALSE, before transfer complete, NumOfBlock(%lx)"), i, dwNumofPackets);
            bRet = FALSE;
        };

        if (!(*lpUsbFuncs->lpCloseTransfer)(pTrSt[i].hTransfer)) {
            g_pKato->Log(LOG_FAIL,TEXT("CloseTransfer Failure on No. %d"), i);
            bRet = FALSE;
        }
    }

    if(bRet == FALSE){
        g_pKato->Log(LOG_FAIL,TEXT("Performance test on ISOCH transfer API calling time failed!"));
        goto EXIT;
    }
    else{
        double AvgTime = CalcAvgTime(TimeRec, usRounds-1);
        g_pKato->Log(LOG_DETAIL, TEXT("Average calling time for IssueIsochTransfer()  with packetsize %d, block size %d is %.3f mil sec"), 
                                                 lpPipePoint->Descriptor.wMaxPacketSize, usBlockSize, AvgTime);
    }

    Sleep(5000);
#endif


    //-----------------------------------------------------------------------------------
    //  COUNT throughput 
    //-----------------------------------------------------------------------------------

    //issue command to netchip2280
    hVTransfer = pDriverPtr->IssueVendorTransfer(NULL, NULL, USB_OUT_TRANSFER, (PUSB_DEVICE_REQUEST)&utdr, (PVOID)&utperfdl, 0);
    if(hVTransfer != NULL){ 
        (pDriverPtr->lpUsbFuncs)->lpCloseTransfer(hVTransfer);
    }
    Sleep(300);

    DWORD dwCurFrame = 0;
    if(pDriverPtr->GetFrameNumber(&dwCurFrame) == FALSE){
        g_pKato->Log(LOG_FAIL,TEXT("GetCurrentFrameNumber failed!"));
        goto EXIT;
    }
    else{
        NKDbgPrintfW(_T("Current frame number is: %d"), dwCurFrame);
    }

    g_usCallbackCnt = 0;
    g_usRounds = usRounds;
    g_hCompEvent = CreateEvent(0, FALSE, FALSE, NULL);
    DWORD dwFlags = fOUT?USB_OUT_TRANSFER:USB_IN_TRANSFER;
    
    pTrSt[0].hTransfer=(*lpUsbFuncs->lpIssueIsochTransfer)(
                                                            hPipe,
                                                            ThroughputNotify,
                                                            &pTrSt[0],
                                                            USB_SEND_TO_ENDPOINT|USB_NO_WAIT|dwFlags,
                                                            dwCurFrame+500,
                                                            dwNumofPackets,
                                                            pBlockLengths,
                                                            pBuffer,
                                                            (ULONG)pPhyBufferAddr);
    
    for(int i = 1; i < usRounds; i++){
        pTrSt[i].hTransfer=(*lpUsbFuncs->lpIssueIsochTransfer)(
                                                                hPipe,
                                                                ThroughputNotify,
                                                                &pTrSt[i],
                                                                USB_START_ISOCH_ASAP|USB_SEND_TO_ENDPOINT|USB_NO_WAIT|dwFlags,
                                                                0,
                                                                dwNumofPackets,
                                                                pBlockLengths,
                                                                pBuffer,
                                                                (ULONG)pPhyBufferAddr);
    }


    //Wait for all transfers to complete
    DWORD dwWait = WaitForSingleObject(g_hCompEvent, PERF_WAIT_TIME);
    if(dwWait != WAIT_OBJECT_0){//sth. wrong
        g_pKato->Log(LOG_FAIL, _T("Transfer not completed with 60 secs, test will quit!\r\n"));
        goto EXIT;
    }        

    int iSysPerfs = 0;
    float flTotalCPUper = 0.0;
    float flTotalMemper = 0.0;

    for(int i = 0; i < usRounds; i++){
        if(pTrSt[i].hTransfer == NULL || pTrSt[i].fDone == FALSE){
            g_pKato->Log(LOG_FAIL,TEXT("Transfer No. %d failed!"), i);
            bRet = FALSE;
            continue;
        }

        if (!(*lpUsbFuncs->lpIsTransferComplete)(pTrSt[i].hTransfer)) {
            g_pKato->Log(LOG_FAIL, TEXT("Transfer No. %d:  IsTransferComplet not return true after call back"), i);
            bRet = FALSE;
        }

        if (!(*lpUsbFuncs->lpGetIsochResults)(pTrSt[i].hTransfer, dwNumofPackets, pCurLengths,pCurErrors))  { // fail
            g_pKato->Log(LOG_FAIL, TEXT(" Transfer No. %d: Failure:GetIsochStatus return FALSE, before transfer complete, NumOfBlock(%lx)"), i, dwNumofPackets);
            bRet = FALSE;
        };

        if (!(*lpUsbFuncs->lpCloseTransfer)(pTrSt[i].hTransfer)) {
            g_pKato->Log(LOG_FAIL,TEXT("CloseTransfer Failure on No. %d"), i);
            bRet = FALSE;
        }

        if(pTrSt[i].flCPUPercentage > 0.0){
            flTotalCPUper += pTrSt[i].flCPUPercentage;
            flTotalMemper += pTrSt[i].flMemPercentage;
            iSysPerfs ++;
            pTrSt[i].flCPUPercentage = 0.0;
            pTrSt[i].flMemPercentage = 0.0;
        }
    }

    if(bRet == FALSE){
        g_pKato->Log(LOG_FAIL,TEXT("Performance test on ISOCH transfer throughput failed!"));
        goto EXIT;
    }
    else{
        double AvgThroughput = CalcAvgThroughput(TimeRec, usRounds-1, dwBlockSize);
        g_pKato->Log(LOG_DETAIL, TEXT("Average throughput for Isoch Transfer with packetsize %d, block size %d is %.3f Mbps"), 
                                                 lpPipePoint->Descriptor.wMaxPacketSize, dwBlockSize, AvgThroughput);
        g_pKato->Log(LOG_DETAIL, TEXT("Average CPU usage is %2.3f %%; Mem usage is %2.3f %%"), 
                                                 flTotalCPUper / (float)iSysPerfs, flTotalMemper / (float)iSysPerfs);
        if(pTP != NULL){
            pTP->dbThroughput = AvgThroughput;
            if(iSysPerfs == 0){
                g_pKato->Log(LOG_DETAIL, TEXT("WARNING: No CPU/Memory usage number availabe"));                
            }
            else {
                pTP->flCPUUsage = flTotalCPUper / (float)iSysPerfs;
                pTP->flMemUsage = flTotalMemper / (float)iSysPerfs;
            }
        }
   }
    
EXIT:

    g_bIsoch = FALSE;
    if ((*(lpUsbFuncs->lpClosePipe))(hPipe)==FALSE) {
        g_pKato->Log(LOG_FAIL, TEXT("Error at Close Pipe"));
        DEBUGMSG(DBG_ERR,(TEXT("Error at Close Pipe")));
        bRet = FALSE;
    }

    if(pPhyBufferAddr)
        FreePhysMem(pBuffer);
    else if(pBuffer)
        delete[] pBuffer;

    if(pTrSt){
        delete[] pTrSt;
    }
    if(pCurErrors)
        delete[] pCurErrors;
    if(pCurLengths)
        delete[]  pCurLengths;
    if(pBlockLengths)
        delete[]  pBlockLengths;

    if(g_hCompEvent != NULL){
        CloseHandle(g_hCompEvent);
        g_hCompEvent = NULL;
    }
       
    g_usRounds = 0;
    g_usCallbackCnt = 0;
        
    return TRUE; //bRet;

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
    result = ((double)(dwBlockSize*8)/result) /1000;
    return result;
}

// transfer callback.

DWORD WINAPI DummyNotify(LPVOID lpvNotifyParameter)
{
    if(lpvNotifyParameter == NULL )
        return -1;

	PPerfTransStatus pTransStatus=(PPerfTransStatus)lpvNotifyParameter;
	if (!pTransStatus)
		return -1;
      pTransStatus->fDone = TRUE;
	return 1;
}


DWORD WINAPI GenericNotify(LPVOID lpvNotifyParameter)
{
    NKDbgPrintfW(_T("Got callback: %d->%d, total = %d\n"), g_usGenericCallbackCnt, g_usGenericCallbackCnt+1, g_usGenericTotalCnt);
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

DWORD WINAPI ThroughputNotify(LPVOID lpvNotifyParameter)
{
    if(lpvNotifyParameter == NULL )
        return -1;

    PPerfTransStatus pTransStatus=(PPerfTransStatus)lpvNotifyParameter;
    if (!pTransStatus)
        return -1;
        
   pTransStatus->fDone = TRUE;

    g_usCallbackCnt ++;
    if(g_usCallbackCnt == 1 && g_bIsoch == TRUE){//be aware that there's slight possiblity that g_usCallBackCnt got incremented by other callback
        TimeCounting(pTransStatus->pTimeRec, TRUE);
    }
    
    if(g_usCallbackCnt == g_usRounds){
        TimeCounting(pTransStatus->pTimeRec, FALSE);
        SetEvent(g_hCompEvent);
    }
//    else if((g_usCallbackCnt % 3) == 0){
        pTransStatus->flCPUPercentage = USBPerf_MarkCPU();
        pTransStatus->flMemPercentage = USBPerf_MarkMem();
//    }
    
    return 1;
}

VOID
PrintOutPerfResults(TP_FIXEDPKSIZE PerfResults){
    if(PerfResults.uNumofSizes == 0)
        return;

    NKDbgPrintfW(_T("\r\n"));
    NKDbgPrintfW(_T("Perf Results:\r\n"));
    NKDbgPrintfW(_T("\r\n"));

    TCHAR   szOneLine[1024] = {0};

    //print the first line
    wcscpy(szOneLine, _T("HEAD\t"));
    for(int i = 0; i < PerfResults.uNumofSizes; i++){
        TCHAR   szBlockSizes[8] = {0};
        wsprintf(szBlockSizes, _T("%6d"), PerfResults.UnitTPs[i].dwBlockSize);
        szBlockSizes[6] = _T('\t');
        szBlockSizes[7] = (TCHAR)0;
        wcscat(szOneLine, szBlockSizes);
        if(wcslen(szOneLine) >= 1016)
            break; //this should not happen, just for caution.
    }
    szOneLine[wcslen(szOneLine)-1] = _T('\n'); //change last '\t' ot '\n'
    NKDbgPrintfW(_T("%s"), szOneLine);

    //print the throughput
    memset(szOneLine, 0, sizeof(szOneLine));
    TCHAR   szPacketSize[6] = {0};
    wsprintf(szPacketSize, _T("%4d"), PerfResults.usPacketSize);
    szPacketSize[4] = _T('\t');
    szPacketSize[5] = (TCHAR)0;
    wcscpy(szOneLine, szPacketSize);
    for(int i = 0; i < PerfResults.uNumofSizes; i++){
        TCHAR   szTP[9] = {0};
        wsprintf(szTP, _T("%3.3f\t"), PerfResults.UnitTPs[i].dbThroughput);
//        szTP[7] = _T('\t');
//        szTP[8] = (TCHAR)0;
        wcscat(szOneLine, szTP);
        if(wcslen(szOneLine) >= 1015)
            break; //this should not happen, just for caution.
    }
    szOneLine[wcslen(szOneLine)-1] = _T('\n'); //change last '\t' ot '\n'
    NKDbgPrintfW(_T("%s"), szOneLine);

    //print the CPU Usage
    memset(szOneLine, 0, sizeof(szOneLine));
    wcscpy(szOneLine, _T("CPU Usage\t"));
    for(int i = 0; i < PerfResults.uNumofSizes; i++){
        TCHAR   szCPU[9] = {0};
        wsprintf(szCPU, _T("%3.3f\t"), PerfResults.UnitTPs[i].flCPUUsage);
//        szCPU[7] = _T('\t');
//        szCPU[8] = (TCHAR)0;
        wcscat(szOneLine, szCPU);
        if(wcslen(szOneLine) >= 1015)
            break; //this should not happen, just for caution.
    }
    szOneLine[wcslen(szOneLine)-1] = _T('\n'); //change last '\t' ot '\n'
    NKDbgPrintfW(_T("%s"), szOneLine);

    //print the MEM Usage
    memset(szOneLine, 0, sizeof(szOneLine));
    wcscpy(szOneLine, _T("MEM Usage\t"));
    for(int i = 0; i < PerfResults.uNumofSizes; i++){
        TCHAR   szMEM[9] = {0};
        wsprintf(szMEM, _T("%3.3f\t"), PerfResults.UnitTPs[i].flMemUsage);
//        szMEM[7] = _T('\t');
//        szMEM[8] = (TCHAR)0;
        wcscat(szOneLine, szMEM);
        if(wcslen(szOneLine) >= 1015)
            break; //this should not happen, just for caution.
    }
    szOneLine[wcslen(szOneLine)-1] = _T('\n'); //change last '\t' ot '\n'
    NKDbgPrintfW(_T("%s"), szOneLine);


    NKDbgPrintfW(_T("\r\n"));
    NKDbgPrintfW(_T("\r\n"));
    
    
}

