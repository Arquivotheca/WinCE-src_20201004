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
//     ufnloopback.cpp
// 
// Abstract:  This file includes functions that related the function driver initialization
//                reset, close, and other events' handling.
//            
//     
// Notes: 
//



#include <windows.h>
#include <usbfn.h>
#include <usbfntypes.h>
#include "config.h"
#include "pipepair.h"
#include "ufnloopback.h"

//-------------------------GLOABLE VARS-------------------------

static LPCWSTR g_rgpszStrings0409[] = {
    L"Generic Manufacturer", L"Loopback Test Device"
};

static UFN_STRING_SET g_rgStringSets[] = {
    0x0409, g_rgpszStrings0409, dim(g_rgpszStrings0409)
};

static CDeviceConfig* g_pDevConfig;

UFN_GENERATE_DPCURSETTINGS(_T("UFNLOOPBACK"), _T(""), _T(""), _T(""), _T(""), 0x3)

DWORD 
WINAPI
ResetThread(LPVOID lpParameter) 
{
    PUFL_CONTEXT pContext = (PUFL_CONTEXT)lpParameter;
    if(pContext == NULL){
        ExitThread(0);
        return 0;
    }

    SETFNAME(_T("ResetThread:"));
    FUNCTION_ENTER_MSG();

    //wait for possible host side test program to exit
    Sleep(5000);

    //-------------------------------------------------
    //      --    stop, deregister and cleanup    --
    //-------------------------------------------------
    
    pContext->pUfnFuncs->lpStop(pContext->hDevice);
    pContext->pUfnFuncs->lpDeregisterDevice(pContext->hDevice);

    DeleteCriticalSection(&pContext->cs);
    DeleteCriticalSection(&pContext->Ep0Callbackcs);

    //clean up EP0 requst handling thread
    if(pContext->hRequestThread){
        pContext->fInLoop = FALSE;
        WaitForSingleObject(pContext->hRequestThread, INFINITE);
        CloseHandle(pContext->hRequestThread);
        pContext->hRequestThread = NULL;
    }
    if(pContext->hGotContentEvent){
        CloseHandle(pContext->hGotContentEvent);
        pContext->hGotContentEvent = NULL;
    }
    if(pContext->hRequestCompleteEvent){
        CloseHandle(pContext->hRequestCompleteEvent);
        pContext->hRequestCompleteEvent = NULL;
    }

    //delete all pipe pair instances
    if(pContext->pPairMan){
        delete pContext->pPairMan;
        pContext->pPairMan = NULL;
    }

    Sleep(2000);

    //-------------------------------------------------
    //      --    re-initializatin and re-startup    --
    //-------------------------------------------------

    BOOL fReInit = FALSE;
    InitializeCriticalSection(&pContext->cs);
    InitializeCriticalSection(&pContext->Ep0Callbackcs);
    pContext->pHighSpeedDeviceDesc = g_pDevConfig->GetCurHSDeviceDesc();
    pContext->pHighSpeedConfig = g_pDevConfig->GetCurHSConfig();
    pContext->pFullSpeedDeviceDesc = g_pDevConfig->GetCurFSDeviceDesc();
    pContext->pFullSpeedConfig = g_pDevConfig->GetCurFSConfig();
    
    DEBUGMSG(ZONE_INIT, (_T("%s Register device \r\n"), pszFname));
    DWORD dwRet = (pContext->pUfnFuncs)->lpRegisterDevice(pContext->hDevice,
                                                             pContext->pHighSpeedDeviceDesc, pContext->pHighSpeedConfig, 
                                                            pContext->pFullSpeedDeviceDesc, pContext->pFullSpeedConfig, 
                                                            g_rgStringSets, dim(g_rgStringSets));
    if (dwRet != ERROR_SUCCESS) {
        ERRORMSG(1, (_T("%s Descriptor registration using default config failed\r\n"), pszFname));        
        goto EXIT;
    }
	
    //start handle endpoint 0 request thread
    pContext->hGotContentEvent = CreateEvent(0, FALSE, FALSE, NULL);
    pContext->hRequestCompleteEvent = CreateEvent(0, FALSE, FALSE, NULL);
    if(pContext->hGotContentEvent == NULL || pContext->hRequestCompleteEvent == NULL){
        ERRORMSG(1, (_T("%s failed to create event(s)\r\n"), pszFname));
        goto EXIT;
    }
    pContext->fInLoop = TRUE;
    pContext->hRequestThread = CreateThread(NULL, 0,  HandleRequestThread, pContext, 0, NULL);
    if (pContext->hRequestThread == NULL) {
        ERRORMSG(1, (_T("%s Endpoint 0 request handling thread creation failed\r\n"), pszFname));
	  goto EXIT;
    }
    
    // Start the device controller
    dwRet = pContext->pUfnFuncs->lpStart(pContext->hDevice, UFNLPBK_DeviceNotify, pContext, 
        &pContext->hDefaultPipe);
    if (dwRet != ERROR_SUCCESS) {
        ERRORMSG(1, (_T("%s Device controller failed to start\r\n"), pszFname));
        goto EXIT;
    }

    fReInit = TRUE;

EXIT:

    if(fReInit == FALSE){
        ERRORMSG(1, (_T("%s Reset loopback driver failed!!! Suggest to reboot CE device!!!\r\n"), pszFname));
    }
    
    FUNCTION_LEAVE_MSG();
    ExitThread(0);
    return 0;
}


DWORD
WINAPI
RequestCallBack(LPVOID  lpParameter){
    PEP0CALLBACK_CONTEXT pCallBack = (PEP0CALLBACK_CONTEXT)lpParameter;
    if(pCallBack == NULL || pCallBack->pContext == NULL || pCallBack->pInfo == NULL)
        return 0;

    PUFL_CONTEXT pContext = pCallBack->pContext;
    PTCOMMAND_INFO pInfo = pCallBack->pInfo;
    delete pCallBack;
    pCallBack = NULL;
    DEBUGMSG(ZONE_TRANSFER, (_T("EP0 got request back, pInfo->request = %d, size = %d"), pInfo->uRequestType, pInfo->cbBufSize));
    EnterCriticalSection(&pContext->Ep0Callbackcs);
    //remember the pInfo pointer here
    pContext->tCommand = pInfo;
    DEBUGMSG(ZONE_COMMENT, (_T("RequestCallback: pInfo = 0x%x; hTransfer = 0x%x"), pInfo, pInfo->hTransfer));
    SetEvent(pContext->hRequestCompleteEvent);
    WaitForSingleObject(pContext->hGotContentEvent, 200);
    ResetEvent(pContext->hGotContentEvent);
    LeaveCriticalSection(&pContext->Ep0Callbackcs);
    
    return 0;
}


VOID
SendDataLpbkCommand(PUFL_CONTEXT pContext, PTCOMMAND_INFO pInfo, BOOL bShortStress) 
{
    PUSB_TDEVICE_DATALPBK putdl;
    DWORD cbTransferred = 0;
    DWORD usbErrors = 0;

    if(pContext == NULL ||pInfo == NULL)
        return;
        
    SETFNAME(_T("SendDataLpckCommand:"));
    FUNCTION_ENTER_MSG();

    putdl = (PUSB_TDEVICE_DATALPBK)pInfo->pBuffer;
    
    if(bShortStress == FALSE){
        //send command
        if(pContext->pPairMan){
            pContext->pPairMan->SendDataLoopbackRequest(putdl->uOutEP, putdl->wBlockSize, putdl->wNumofBlocks, putdl->usAlignment, putdl->dwStartVal);
        }
    }
    else{
        if(pContext->pPairMan){
            pContext->pPairMan->SendShortStressRequest(putdl->uOutEP, putdl->wNumofBlocks);
        }
    }

    FUNCTION_LEAVE_MSG();
}


VOID
ReConfig(PTCOMMAND_INFO pInfo) 
{
    PUSB_TDEVICE_RECONFIG putrc;
    UFN_TRANSFER	hTransfer = NULL;
    DWORD cbTransferred = 0;
    DWORD usbErrors = 0;

    if(pInfo == NULL)
        return;
        
    SETFNAME(_T("ReConfig:"));
    FUNCTION_ENTER_MSG();

    putrc = (PUSB_TDEVICE_RECONFIG)pInfo->pBuffer;

    UCHAR uConfig = 1;
    if(putrc->ucConfig != 0)
        uConfig = putrc->ucConfig;
    UCHAR uInterface = 1;
    if(putrc->ucInterface != 0)
        uInterface = putrc->ucInterface;

    if(g_pDevConfig != NULL){
        g_pDevConfig->Cleanup();

        //setup device configuation
        PDEVICE_SETTING pDS = InitializeSettings();
        if(pDS == NULL){
            ERRORMSG(1,(_T("%s Can not get device-specific data!!!\r\n"), pszFname));
            return;
        }
        g_pDevConfig->SetDevSetting(pDS);

        if(g_pDevConfig->InitDescriptors(uConfig, uInterface) == FALSE){
            ERRORMSG(1,(_T("%s Can not create loopback device settings data structures!!!\r\n"), pszFname));
            delete g_pDevConfig;
            g_pDevConfig = NULL;
            return;
        }

        //modify packet sizes if needed
        g_pDevConfig->ModifyConfig(*putrc);

    }

    FUNCTION_LEAVE_MSG();
	
}

VOID
VerifyEP0OUTData(PTCOMMAND_INFO pInfo){

    if(pInfo == NULL || pInfo->pBuffer == NULL)//should not happen
        return;

    BYTE bCurVal = pInfo->bStartVal;

    for (USHORT i = 0; i < pInfo->cbBufSize; i ++){
        if(bCurVal != pInfo->pBuffer[i]){
            ERRORMSG(1, (_T("EP0 TEST OUT DATA ERROR!!! at offset %d, expected value = %d, real value = %d\r\n"), 
                                i, bCurVal, pInfo->pBuffer[i]));
            return;
        }
        bCurVal ++;
    }
}

DWORD 
WINAPI
HandleRequestThread(LPVOID lpParameter) 
{
    PUFL_CONTEXT pContext = (PUFL_CONTEXT)lpParameter;
    if(pContext == NULL){//this should not happen
        ExitThread(0);
        return 0;
    }
    
    SETFNAME(_T("HandleRequestThread:"));
    FUNCTION_ENTER_MSG();

    while(pContext->fInLoop == TRUE){
        DWORD dwWait = WaitForSingleObject(pContext->hRequestCompleteEvent, 100);
        if(pContext->fInLoop == FALSE){//quit thread now
            break;
        }

        if(dwWait == WAIT_OBJECT_0){
            ResetEvent(pContext->hRequestCompleteEvent);
            PTCOMMAND_INFO pInfo = pContext->tCommand;
            DEBUGMSG(ZONE_COMMENT, (_T("HandleRequest thread: pInfo = 0x%x; hTransfer = 0x%x"), pInfo, pInfo->hTransfer));
            DEBUGCHK(pInfo->hTransfer != (UFN_TRANSFER)0xCCCCCCCC);
            SetEvent(pContext->hGotContentEvent);

            //we only process valid ep0 transfer
            if(pInfo == NULL || pInfo->hTransfer == NULL){
                if(pInfo != NULL){
                    delete pInfo;
                    pInfo = NULL;
                }
                continue;
            }
            
           //get transfer to complete
            DWORD cbTransferred = 0;
            DWORD usbErrors = 0;
            UFN_TRANSFER hTransfer = pInfo->hTransfer;
            DEBUGCHK(hTransfer != (UFN_TRANSFER)0xCCCCCCCC);
            DWORD dwErr = pContext->pUfnFuncs->lpGetTransferStatus(pContext->hDevice, hTransfer, &cbTransferred, &usbErrors);

            if(dwErr != ERROR_SUCCESS || cbTransferred != pInfo->cbBufSize){//something wrong

                ERRORMSG(1, (_T("Transfer failed!, dwErr = %d, transferred size  = %d, expected size = %d\r\n"), 
                                                    dwErr, cbTransferred, pInfo->cbBufSize));
                pContext->pUfnFuncs->lpAbortTransfer(pContext->hDevice, pInfo->hTransfer);
                pContext->pUfnFuncs->lpStallPipe(pContext->hDevice, pContext->hDefaultPipe);
                pContext->pUfnFuncs->lpSendControlStatusHandshake(pContext->hDevice);
                delete pInfo;
                pInfo = NULL;
                continue;
            }
            else{
                
                switch(pInfo->uRequestType){
                    case TEST_REQUEST_DATALPBK: //bulk out
                        DEBUGMSG(ZONE_TRANSFER, (_T("%s EP0 Request callback: TEST_REQUEST_DATALPBK\r\n"), pszFname));
                        SendDataLpbkCommand(pContext, pInfo, FALSE);
                        break;
                    case TEST_REQUEST_SHORTSTRESS:
                        DEBUGMSG(ZONE_TRANSFER, (_T("%s EP0 Request callback: TEST_REQUEST_SHORTSTRESS\r\n"), pszFname));
                        SendDataLpbkCommand(pContext, pInfo, TRUE);
                        break;
                    case TEST_REQUEST_RESET: //reset
                        DEBUGMSG(ZONE_TRANSFER, (_T("%s EP0 Request callback: TEST_REQUEST_RESET\r\n"), pszFname));
                        ReConfig(pInfo);
                        pContext->hResetThread = CreateThread(NULL, 0, ResetThread, (LPVOID)pContext, 0, NULL);
                        break;
                    case TEST_REQUEST_EP0TESTOUT:
                        DEBUGMSG(ZONE_TRANSFER, (_T("%s EP0 Request callback: TEST_REQUEST_EP0TESTOUT\r\n"), pszFname));
                        VerifyEP0OUTData(pInfo);
                        break;
                    case TEST_REQUEST_LPBKINFO:
                        DEBUGMSG(ZONE_TRANSFER, (_T("%s EP0 Request callback: TEST_REQUEST_LPBKINFO\r\n"), pszFname));
                        break;
                    case TEST_REQUEST_PAIRNUM:
                        DEBUGMSG(ZONE_TRANSFER, (_T("%s EP0 Request callback: TEST_REQUEST_PAIRNUM\r\n"), pszFname));
                        break;
                    case TEST_REQUEST_EP0TESTIN:
                        DEBUGMSG(ZONE_TRANSFER, (_T("%s EP0 Request callback: TEST_REQUEST_EP0TESTIN\r\n"), pszFname));
                        break;
                    default://ignore other commands
                    DEBUGMSG(ZONE_WARNING, (_T("%s unrecognized transfer command!\r\n"), pszFname));
                    break;
                }

                pContext->pUfnFuncs->lpCloseTransfer(pContext->hDevice, pInfo->hTransfer);
                pContext->pUfnFuncs->lpSendControlStatusHandshake(pContext->hDevice);
                delete pInfo;
                pInfo = NULL;
            }
	  }
    }
    
    FUNCTION_LEAVE_MSG();
    ExitThread(0);
    return 0;
}


// Open the pipes associated with specified  interface.
BOOL
UFNLPBK_OpenInterface(PUFL_CONTEXT pContext
    )
{
    if(pContext == NULL || pContext->pPairMan == NULL)
        return FALSE;
        
    SETFNAME(_T("UFNLPBK_OpenInterface"));
    FUNCTION_ENTER_MSG();

    BOOL bRet = FALSE;
    if(pContext->pPairMan){
        bRet = pContext->pPairMan->OpenAllPipes();
    }

    FUNCTION_LEAVE_MSG();

    return bRet;
}

//handle clear feature, doing nothing right now
static
VOID
UFNLPBK_HandleClearFeature(
    USB_TDEVICE_REQUEST udr
    )
{
    SETFNAME(_T("UFNLPBK_HandleClearFeature"));
    FUNCTION_ENTER_MSG();

    //not used here

    FUNCTION_LEAVE_MSG();
}

BOOL
SendEp0Transfer(DWORD dwTransFlags, PUFL_CONTEXT pContext, PTCOMMAND_INFO pInfo){

    if(pContext == NULL || pContext->pUfnFuncs == NULL || pInfo == NULL) //should not happen
        return FALSE;
        
    SETFNAME(_T("SendEp0Transfer"));
    FUNCTION_ENTER_MSG();

    PEP0CALLBACK_CONTEXT pCallBack = new EP0CALLBACK_CONTEXT;
    if(pCallBack == NULL){
        return FALSE;
    }
    pCallBack->pContext = pContext;
    pCallBack->pInfo = pInfo;
    
    DWORD dwErr = pContext->pUfnFuncs->lpIssueTransfer(pContext->hDevice, pContext->hDefaultPipe, 
                                                      RequestCallBack, pCallBack, dwTransFlags,
                                                      pInfo->cbBufSize, pInfo->pBuffer,  0, NULL, &(pInfo->hTransfer));
    if(dwErr != ERROR_SUCCESS || pInfo->hTransfer == NULL){
        ERRORMSG(1, (_T("Issue Ep0 transfer failed!!!\r\n")));
        delete pInfo;
        return FALSE;        
    }
    else{
         DEBUGMSG(ZONE_TRANSFER, (_T("Sent EP0 transfer! data size is : %d"), pInfo->cbBufSize));
    }

    FUNCTION_LEAVE_MSG();
    return TRUE;
}

// Process a USB loopback request.  Call Request-specific handler.
static
CONTROL_RESPONSE
UFNLPBK_HandleLoopbackRequest(
    PUFL_CONTEXT pContext,
    USB_TDEVICE_REQUEST udr
    )
{
    if(pContext == NULL || pContext->pUfnFuncs == NULL)//this should not happen
        return CR_STALL_DEFAULT_PIPE;
        
    SETFNAME(_T("UFNLPBK_HandleLoopbackRequest"));
    FUNCTION_ENTER_MSG();
    HANDLE hResetThread = NULL;

    UCHAR uNumofPairs = g_pDevConfig->GetNumofPairs();
    UCHAR uNumofSinglePipes = g_pDevConfig->GetNumofEPs() - uNumofPairs*2;
    CONTROL_RESPONSE response = CR_STALL_DEFAULT_PIPE;
    int i = 0;
    PUSB_TDEVICE_LPBKINFO putli = NULL;
    PPIPEPAIR_INFO pPairsInfo = NULL;    
    BYTE bVal = 0;
    
    DEBUGMSG(ZONE_USB_EVENTS, (_T("%s LOOPBACK EVENTS: bmRequestType = %d, bRequest = %d, wValue = %d, wIndex = %d\r\n"),
                    pszFname, udr.bmRequestType, udr.bRequest, udr.wValue, udr.wIndex));
           
    if (udr.bmRequestType & USB_REQUEST_CLASS){
         PTCOMMAND_INFO pTComm = NULL;
        switch(udr.bRequest){
    		case TEST_REQUEST_DATALPBK: 
    		case TEST_REQUEST_SHORTSTRESS:
    		    pTComm = new TCOMMAND_INFO;
    		    if(pTComm == NULL){
                    ERRORMSG(1, (_T("Out of Memory\r\n")));
                    pContext->pUfnFuncs->lpStallPipe(pContext->hDevice, pContext->hDefaultPipe);
                    pContext->pUfnFuncs->lpSendControlStatusHandshake(pContext->hDevice);
                    break;
    		    }
    		    memset(pTComm, 0, sizeof(TCOMMAND_INFO));
                 pTComm->uRequestType = udr.bRequest;
                 pTComm->cbBufSize = sizeof(USB_TDEVICE_DATALPBK);
                 if(SendEp0Transfer(USB_OUT_TRANSFER, pContext, pTComm) == TRUE){
                    response = CR_SUCCESS_SEND_CONTROL_HANDSHAKE;
                 }
    		    break;
    		case TEST_REQUEST_RESET: //reset
    		    pTComm = new TCOMMAND_INFO;
    		    if(pTComm == NULL){
                    ERRORMSG(1, (_T("Out of Memory\r\n")));
                    pContext->pUfnFuncs->lpStallPipe(pContext->hDevice, pContext->hDefaultPipe);
                    pContext->pUfnFuncs->lpSendControlStatusHandshake(pContext->hDevice);
                    break;
    		    }
    		    memset(pTComm, 0, sizeof(TCOMMAND_INFO));
                 pTComm->uRequestType = udr.bRequest;
                 pTComm->cbBufSize = sizeof(USB_TDEVICE_RECONFIG);
                 if(SendEp0Transfer(USB_OUT_TRANSFER, pContext, pTComm) == TRUE){
                    response = CR_SUCCESS_SEND_CONTROL_HANDSHAKE;
                 }
    		    break;
    		case TEST_REQUEST_PAIRNUM:
    		    pTComm = new TCOMMAND_INFO;
    		    if(pTComm == NULL){
                    ERRORMSG(1, (_T("Out of Memory\r\n")));
                    pContext->pUfnFuncs->lpStallPipe(pContext->hDevice, pContext->hDefaultPipe);
                    pContext->pUfnFuncs->lpSendControlStatusHandshake(pContext->hDevice);
                    break;
    		    }
    		    memset(pTComm, 0, sizeof(TCOMMAND_INFO));
                 pTComm->uRequestType = udr.bRequest;
                 pTComm->cbBufSize = sizeof(UCHAR);
                 *((PUCHAR)pTComm->pBuffer) = uNumofPairs + uNumofSinglePipes;
                 if(SendEp0Transfer(USB_IN_TRANSFER, pContext, pTComm) == TRUE){
                    response = CR_SUCCESS_SEND_CONTROL_HANDSHAKE;
                 }
    		    break;
                 
    		case TEST_REQUEST_LPBKINFO:
    		    pTComm = new TCOMMAND_INFO;
    		    if(pTComm == NULL){
                    ERRORMSG(1, (_T("Out of Memory\r\n")));
                    pContext->pUfnFuncs->lpStallPipe(pContext->hDevice, pContext->hDefaultPipe);
                    pContext->pUfnFuncs->lpSendControlStatusHandshake(pContext->hDevice);
                    break;
    		    }
    		    memset(pTComm, 0, sizeof(TCOMMAND_INFO));
                 pTComm->uRequestType = udr.bRequest;
                 pTComm->cbBufSize = sizeof(USB_TDEVICE_LPBKINFO)+(uNumofPairs + uNumofSinglePipes -1)*sizeof(USB_TDEVICE_LPBKPAIR);;

                putli = (PUSB_TDEVICE_LPBKINFO)pTComm->pBuffer;
                putli->uNumofPairs = uNumofPairs;
                putli->uNumofSinglePipes = uNumofSinglePipes;
                pPairsInfo = pContext->pPairMan->GetPairsInfo();
                if(pPairsInfo == NULL){
                    ERRORMSG(1, (_T("No valid pair info !!!\r\n")));
                    delete pTComm;
                    pContext->pUfnFuncs->lpStallPipe(pContext->hDevice, pContext->hDefaultPipe);
                    pContext->pUfnFuncs->lpSendControlStatusHandshake(pContext->hDevice);
                    break;
                }

                for(i = 0; i < uNumofPairs; i++){
                    putli->pLpbkPairs[i].ucOutEp = pPairsInfo[i].bOutEPAddress;
                    putli->pLpbkPairs[i].ucInEp = pPairsInfo[i].bInEPAddress;
                    putli->pLpbkPairs[i].ucType = pPairsInfo[i].bmAttribute;
                }

                if(uNumofSinglePipes != 0){
                    PSINGLEPIPE_INFO pSinglesInfo = pContext->pPairMan->GetSinglePipesInfo();
                    for(int j = 0; j < uNumofSinglePipes; j++){
                        if((pSinglesInfo[j].bEPAddress & 0x80) == 0){
                            putli->pLpbkPairs[i].ucOutEp = pSinglesInfo[j].bEPAddress;
                            putli->pLpbkPairs[i].ucInEp = 0xFF;
                        }
                        else{
                            putli->pLpbkPairs[i].ucInEp = pSinglesInfo[j].bEPAddress;
                            putli->pLpbkPairs[i].ucOutEp = 0xFF;
                        }
                        putli->pLpbkPairs[i].ucType = pSinglesInfo[j].bmAttribute;
                        i++;
                    }
                }
                 if(SendEp0Transfer(USB_IN_TRANSFER, pContext, pTComm) == TRUE){
                    response = CR_SUCCESS_SEND_CONTROL_HANDSHAKE;
                 }
    		    break;

    		case TEST_REQUEST_EP0TESTIN:
    		    pTComm = new TCOMMAND_INFO;
    		    if(pTComm == NULL){
                    ERRORMSG(1, (_T("Out of Memory\r\n")));
                    pContext->pUfnFuncs->lpStallPipe(pContext->hDevice, pContext->hDefaultPipe);
                    pContext->pUfnFuncs->lpSendControlStatusHandshake(pContext->hDevice);
                    break;
    		    }
    		    memset(pTComm, 0, sizeof(TCOMMAND_INFO));
                 pTComm->uRequestType = udr.bRequest;
                 pTComm->bStartVal = (BYTE)udr.wValue;
                 pTComm->cbBufSize = udr.wLength;

                 bVal = pTComm->bStartVal;
                 for(USHORT usi = 0; usi < udr.wLength; usi++){
                     pTComm->pBuffer[usi] = bVal ++;
                 }

                 if(SendEp0Transfer(USB_IN_TRANSFER, pContext, pTComm) == TRUE){
                    response = CR_SUCCESS_SEND_CONTROL_HANDSHAKE;
                 }
    		    break;

    		case TEST_REQUEST_EP0TESTOUT:
    		    pTComm = new TCOMMAND_INFO;
    		    if(pTComm == NULL){
                    ERRORMSG(1, (_T("Out of Memory\r\n")));
                    pContext->pUfnFuncs->lpStallPipe(pContext->hDevice, pContext->hDefaultPipe);
                    pContext->pUfnFuncs->lpSendControlStatusHandshake(pContext->hDevice);
                    break;
    		    }
    		    memset(pTComm, 0, sizeof(TCOMMAND_INFO));
                 pTComm->uRequestType = udr.bRequest;
                 pTComm->bStartVal = (BYTE)udr.wValue;
                 pTComm->cbBufSize = udr.wLength;
                 if(SendEp0Transfer(USB_OUT_TRANSFER, pContext, pTComm) == TRUE){
                    response = CR_SUCCESS_SEND_CONTROL_HANDSHAKE;
                 }
    		    break;

    		default://ignore other commands
                 DEBUGMSG(ZONE_WARNING, (_T("%s unrecognized transfer command!\r\n"), pszFname));
                 //don't care about these unhandled commands
                 pContext->pUfnFuncs->lpSendControlStatusHandshake(pContext->hDevice);
   		    break;
        }

    }
    else {
        ERRORMSG(1, (_T("%s Unrecognized class bRequest -> 0x%x\r\n"), 
            pszFname, udr.bmRequestType));
    }

    FUNCTION_LEAVE_MSG();

    return response;
}


// Process a USB Standard Request.  Call Request-specific handler.
static
VOID
UFNLPBK_HandleRequest(
    DWORD dwMsg,
    PUFL_CONTEXT pContext,
    USB_TDEVICE_REQUEST udr
    )
{

    if(pContext == NULL || pContext->pUfnFuncs == NULL) //this should not happen
        return;
        
    SETFNAME(_T("UFNLPBK_HandleRequest"));
    FUNCTION_ENTER_MSG();

    CONTROL_RESPONSE response;

    if (dwMsg == UFN_MSG_PREPROCESSED_SETUP_PACKET) {
        response = CR_SUCCESS; // Don't respond since it was already handled.
        
        if ( udr.bmRequestType ==
            (USB_REQUEST_HOST_TO_DEVICE | USB_REQUEST_STANDARD | USB_REQUEST_FOR_ENDPOINT) ) {
            switch (udr.bRequest) {
                case USB_REQUEST_CLEAR_FEATURE:
                    UFNLPBK_HandleClearFeature(udr);
                    break;
            }
        }

        else if (udr.bmRequestType ==
            (USB_REQUEST_HOST_TO_DEVICE | USB_REQUEST_STANDARD | USB_REQUEST_FOR_DEVICE) ) {
            if (udr.bRequest == USB_REQUEST_SET_CONFIGURATION) {                
                // 
                //doing nothing here;
            }
        }
    }
    else {
        DEBUGCHK(dwMsg == UFN_MSG_SETUP_PACKET);
        response = CR_STALL_DEFAULT_PIPE;

        if (udr.bmRequestType & USB_REQUEST_CLASS) {
            DEBUGMSG(ZONE_COMMENT, (_T("%s Class request\r\n"), pszFname));
            response = UFNLPBK_HandleLoopbackRequest(pContext, udr);
            FUNCTION_LEAVE_MSG();
            return;
        }
    }

    if (response == CR_STALL_DEFAULT_PIPE) {
        pContext->pUfnFuncs->lpStallPipe(pContext->hDevice, pContext->hDefaultPipe);
        pContext->pUfnFuncs->lpSendControlStatusHandshake(pContext->hDevice);
    }
    else if (response == CR_SUCCESS_SEND_CONTROL_HANDSHAKE) {
        pContext->pUfnFuncs->lpSendControlStatusHandshake(pContext->hDevice);
    }

    FUNCTION_LEAVE_MSG();
}


// Process a device event.
static
BOOL
WINAPI
UFNLPBK_DeviceNotify(
    PVOID   pvNotifyParameter,
    DWORD   dwMsg,
    DWORD   dwParam
    ) 
{

    SETFNAME(_T("UFNLPBK_DeviceNotify"));
    FUNCTION_ENTER_MSG();

    PUFL_CONTEXT pContext = (PUFL_CONTEXT)pvNotifyParameter;
    if(pContext == NULL) //  this should not happen
        return FALSE;

    EnterCriticalSection(&pContext->cs);
    CONTROL_RESPONSE response = CR_SUCCESS;
    UFN_BUS_SPEED  Speed  = BS_UNKNOWN_SPEED;

    DEBUGMSG(ZONE_INIT | ZONE_USB_EVENTS, (_T("event is: 0x%x, param is: 0x%x \r\n"), dwMsg, dwParam));
	
    switch(dwMsg) {
        case UFN_MSG_BUS_EVENTS: {
            // Ensure device is in running state
            if(pContext->hDefaultPipe == NULL)
                break;

            if (dwParam == UFN_DETACH) {//delete all pipe pairs
                DEBUGMSG(ZONE_INIT | ZONE_USB_EVENTS, (_T("UFN_DETACH EVENT")));
                if(pContext->pPairMan){
                    delete pContext->pPairMan;
                    pContext->pPairMan = NULL;
                }
            }

            if (dwParam & UFN_ATTACH) {
                DEBUGMSG(ZONE_INIT | ZONE_USB_EVENTS, (_T("UFN_ATTACH EVENT")));
                if(pContext->pPairMan == NULL){
                    pContext->pPairMan = (CPipePairManager *) new CPipePairManager(g_pDevConfig, pContext->hDevice, pContext->pUfnFuncs);
                    if(pContext->pPairMan == NULL){
                        ERRORMSG(1, (_T("%s Out of memory\r\n"), pszFname));
                    }
                }

            }
        
            if (dwParam & UFN_RESET) {
                DEBUGMSG(ZONE_INIT | ZONE_USB_EVENTS, (_T("UFN_RESET EVENT")));
                // Reset device
                if(pContext->pPairMan){
                    pContext->pPairMan->CloseAllPipes();
                }
            	}
            break;
        }

        case UFN_MSG_SETUP_PACKET:
        case UFN_MSG_PREPROCESSED_SETUP_PACKET: {
            DEBUGMSG(ZONE_INIT | ZONE_USB_EVENTS, (_T("UFN_MSG_SETUP_PACKET EVENT")));
            PUSB_TDEVICE_REQUEST pudr = (PUSB_TDEVICE_REQUEST) dwParam;
            if(dwMsg != 0 && pudr != NULL)
                UFNLPBK_HandleRequest(dwMsg, pContext, *pudr);
            break;
        }

        case UFN_MSG_BUS_SPEED:
            DEBUGMSG(ZONE_INIT | ZONE_USB_EVENTS, (_T("UFN_MSG_BUS_SPEED EVENT")));
            Speed = (UFN_BUS_SPEED)dwParam;

            if(pContext->pPairMan != NULL){
	         if(Speed != BS_UNKNOWN_SPEED){
            	      pContext->pPairMan->SetCurSpeed(Speed);
	         }

                //Initialize pipe instances
                if((pContext->pPairMan)->Init() == FALSE){
                    ERRORMSG(1, (_T("%s Can not initialize paired pipes\r\n"), pszFname));
                    delete pContext->pPairMan;
                    pContext->pPairMan = NULL;
                }
            }
            break;
            
        case UFN_MSG_CONFIGURED: 
            DEBUGMSG(ZONE_INIT | ZONE_USB_EVENTS, (_T("UFN_MSG_CONFIGURED EVENT")));
            UFNLPBK_OpenInterface(pContext);
            break;
            
    }
    
    FUNCTION_LEAVE_MSG();

    LeaveCriticalSection(&pContext->cs);

    return TRUE;
}

// **************************************************************************
//                                            P U B L I C  F U N C T I O N S
// **************************************************************************

// +-------------------------------------------------------------------------
//  DllEntry
// --------------------------------------------------------------------------
//  Purpose     DLL entry-point.
//  Parameters  Standard parameters.
//  Returns     Success of processing.
// -------------------------------------------------------------------------+
extern "C"
BOOL
WINAPI
DllEntry(
    HINSTANCE           hinstDll,
    DWORD               dwReason,
    LPVOID              lpReserved
    )
{
    SETFNAME(_T("UsbFnTest.DllEntry:"));

    if ( dwReason == DLL_PROCESS_ATTACH ) {
        DEBUGREGISTER(hinstDll);
        DEBUGMSG(ZONE_INIT, (TEXT("%s Attach. \r\n"), pszFname));

        //setup device configuation
        PDEVICE_SETTING pDS = InitializeSettings();
        if(pDS == NULL){
            ERRORMSG(1,(_T("%s Can not get device-specific data!!!\r\n"), pszFname));
            return FALSE;
        }
        g_pDevConfig = (CDeviceConfig *) new CDeviceConfig;
        if(g_pDevConfig == NULL){
            ERRORMSG(1,(_T("%s Out of memory\r\n"), pszFname));
            return FALSE;
        }
        g_pDevConfig->SetDevSetting(pDS);

        if(g_pDevConfig->InitDescriptors(1, 1) == FALSE){
            ERRORMSG(1,(_T("%s Can not create loopback device settings data structures!!!\r\n"), pszFname));
            delete g_pDevConfig;
            g_pDevConfig = NULL;
            return FALSE;
        }

        DisableThreadLibraryCalls( (HMODULE) hinstDll );
    }

    if ( dwReason == DLL_PROCESS_DETACH ) {
        DEBUGMSG(ZONE_INIT, (TEXT("%s Detach. \r\n"), pszFname));
        if(g_pDevConfig != NULL){
            delete g_pDevConfig;
            g_pDevConfig = NULL;
        }
    }

    return TRUE;
    
} // DllEntry


//
// Initialiaztion
//
extern "C"
DWORD
UFL_Init(
    LPCTSTR pszActiveKey
    )
{
    SETFNAME(_T("UsbFnTest.UFNLPBK_Init:"));
    FUNCTION_ENTER_MSG();

    if(pszActiveKey == NULL || g_pDevConfig == NULL){
        return 0;
    }

    PUFL_CONTEXT pContext = (PUFL_CONTEXT) new UFL_CONTEXT;
    if(pContext == NULL){
        ERRORMSG(1,(_T("%s Can not initialize context!!!\r\n"), pszFname));
        return 0;
    }
    memset(pContext, 0, sizeof(UFL_CONTEXT));

    BOOL fInit = FALSE;
    
    wcscpy(pContext->szActiveKey, pszActiveKey);
    pContext->pUfnFuncs = (PUFN_FUNCTIONS)new UFN_FUNCTIONS;
    if(pContext->pUfnFuncs == NULL){
        ERRORMSG(1,(_T("%s Can not initialize UFN function table!!!\r\n"), pszFname));
        goto EXIT;
    }

    DWORD dwRet = UfnInitializeInterface(pszActiveKey, &pContext->hDevice, 
                                                            pContext->pUfnFuncs, &pContext->pvInterface);
    
    if (dwRet != ERROR_SUCCESS) {
        goto EXIT;
    }

    InitializeCriticalSection(&pContext->cs);
    InitializeCriticalSection(&pContext->Ep0Callbackcs);
    pContext->pHighSpeedDeviceDesc = g_pDevConfig->GetCurHSDeviceDesc();
    pContext->pHighSpeedConfig = g_pDevConfig->GetCurHSConfig();
    pContext->pFullSpeedDeviceDesc = g_pDevConfig->GetCurFSDeviceDesc();
    pContext->pFullSpeedConfig = g_pDevConfig->GetCurFSConfig();
    
    if(pContext->pHighSpeedConfig == NULL || pContext->pHighSpeedDeviceDesc == NULL || 
            pContext->pFullSpeedConfig == NULL ||pContext->pFullSpeedDeviceDesc == NULL){//this should not happen
        ERRORMSG(1,(_T("%s got invalid descriptor or config!!!\r\n"), pszFname));
        goto EXIT;
    }

  
    DEBUGMSG(ZONE_INIT, (_T("%s Register device \r\n"), pszFname));
    dwRet = (pContext->pUfnFuncs)->lpRegisterDevice(pContext->hDevice,
                                                             pContext->pHighSpeedDeviceDesc, pContext->pHighSpeedConfig, 
                                                            pContext->pFullSpeedDeviceDesc, pContext->pFullSpeedConfig, 
                                                            g_rgStringSets, dim(g_rgStringSets));
    if (dwRet != ERROR_SUCCESS) {
        ERRORMSG(1, (_T("%s Descriptor registration using default config failed\r\n"), pszFname));        
        goto EXIT;
    }
	
    //start handle endpoint 0 request thread
    pContext->hGotContentEvent = CreateEvent(0, FALSE, FALSE, NULL);
    pContext->hRequestCompleteEvent = CreateEvent(0, FALSE, FALSE, NULL);
    if(pContext->hGotContentEvent == NULL || pContext->hRequestCompleteEvent == NULL){
        ERRORMSG(1, (_T("%s failed to create event(s)\r\n"), pszFname));
        goto EXIT;
    }
    pContext->fInLoop = TRUE;
    pContext->hRequestThread = CreateThread(NULL, 0, HandleRequestThread, pContext, 0, NULL);
    if (pContext->hRequestThread == NULL) {
        ERRORMSG(1, (_T("%s Endpoint 0 request handling thread creation failed\r\n"), pszFname));
	  goto EXIT;
    }
    
    // Start the device controller
    dwRet = pContext->pUfnFuncs->lpStart(pContext->hDevice, UFNLPBK_DeviceNotify, pContext, 
        &pContext->hDefaultPipe);
    if (dwRet != ERROR_SUCCESS) {
        ERRORMSG(1, (_T("%s Device controller failed to start\r\n"), pszFname));
        goto EXIT;
    }

    fInit = TRUE;
    
EXIT:

    if(fInit == FALSE){//initialization failed
        //free pipe pairs
        if(pContext->pPairMan){
            delete pContext->pPairMan;
            pContext->pPairMan = NULL;
        }
        
        if(pContext->pUfnFuncs != NULL){
            pContext->pUfnFuncs->lpStop(pContext->hDevice); //we may need to stop device
            DeleteCriticalSection(&pContext->cs);
            DeleteCriticalSection(&pContext->Ep0Callbackcs);
            delete pContext->pUfnFuncs;
            pContext->pUfnFuncs = NULL;
            UfnDeinitializeInterface(pContext->pvInterface);
        }
        

        if(pContext->hRequestThread){
            pContext->fInLoop = FALSE;
            WaitForSingleObject(pContext->hRequestThread, INFINITE);
            CloseHandle(pContext->hRequestThread);
            pContext->hRequestThread = NULL;
        }
        if(pContext->hGotContentEvent){
            CloseHandle(pContext->hGotContentEvent);
            pContext->hGotContentEvent = NULL;
        }
        if(pContext->hRequestCompleteEvent){
            CloseHandle(pContext->hRequestCompleteEvent);
            pContext->hRequestCompleteEvent = NULL;
        }

        delete pContext;
        pContext = NULL;
    }

    FUNCTION_LEAVE_MSG();
    return (DWORD)pContext;
}

extern "C"
BOOL
UFL_Deinit(
    DWORD               dwCtx)
{    

    PUFL_CONTEXT pContext = (PUFL_CONTEXT)dwCtx;
    if(pContext == NULL || pContext->pUfnFuncs == NULL || pContext->hDevice == NULL 
        || pContext->pvInterface == NULL) {//nothing to do here
        return TRUE;
    }

    SETFNAME(_T("UsbFnTest.UFNLPBK_Deinit:"));
    FUNCTION_ENTER_MSG();
        
    pContext->pUfnFuncs->lpStop(pContext->hDevice);
    pContext->pUfnFuncs->lpDeregisterDevice(pContext->hDevice);

    UfnDeinitializeInterface(pContext->pvInterface);
    delete pContext->pUfnFuncs;
    pContext->pUfnFuncs = NULL;

    DeleteCriticalSection(&pContext->cs);
    DeleteCriticalSection(&pContext->Ep0Callbackcs);

    //clean up possible reset thread
    if(pContext->hResetThread != NULL){
        CloseHandle(pContext->hResetThread);
        pContext->hResetThread = NULL;
    }

    if(pContext->hRequestThread){
        pContext->fInLoop = FALSE;
        WaitForSingleObject(pContext->hRequestThread, INFINITE);
        CloseHandle(pContext->hRequestThread);
        pContext->hRequestThread = NULL;
    }
    if(pContext->hGotContentEvent){
        CloseHandle(pContext->hGotContentEvent);
        pContext->hGotContentEvent = NULL;
    }
    if(pContext->hRequestCompleteEvent){
        CloseHandle(pContext->hRequestCompleteEvent);
        pContext->hRequestCompleteEvent = NULL;
    }

    if(pContext->pPairMan){
        delete pContext->pPairMan;
        pContext->pPairMan = NULL;
    }

    delete pContext;
    pContext = NULL;

    FUNCTION_LEAVE_MSG();
    return TRUE;
}

extern "C"
DWORD
UFL_Open(
    DWORD               dwCtx, 
    DWORD               dwAccMode, 
    DWORD               dwShrMode
    )
{   
    return 1;
}

extern "C"
DWORD
UFL_Close()
{    
    return ERROR_SUCCESS;
}

extern "C"
DWORD
UFL_Read(
    DWORD               dwA,
    LPVOID              lpA,
    DWORD               dwB
    )
{   
   return -1;
}

extern "C"
DWORD
UFL_Write(
    DWORD               dwA,
    LPVOID              lpA,
    DWORD               dwB
    )
{
   
   return -1;
}

extern "C"
DWORD
UFL_Seek(
    DWORD               dwA,
    LPVOID              lpA,
    DWORD               dwB
    )
{   
   return -1;
}

extern "C"
BOOL
UFL_IOControl(
    LPVOID              pHidKbd,
    DWORD               dwCode,
    PBYTE               pBufIn,
    DWORD               dwLenIn,
    PBYTE               pBufOut,
    DWORD               dwLenOut,
    PDWORD              pdwActualOut
    )
{
    return TRUE;
}


