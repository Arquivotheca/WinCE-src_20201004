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

/*++
Module Name:

transtst.cpp

Abstract:
    USB Driver Transfer Test .

Functions:

Notes:

--*/

#define __THIS_FILE__   TEXT("TransTst.cpp")

#include <windows.h>
#include "usbtest.h"
#include "TransTst.h"
#define TIMEOUT_INTICKS 5000 // 5 second

USB_DEVICE_REQUEST TransferThread::InRequest=
{USB_REQUEST_DEVICE_TO_HOST|USB_REQUEST_VENDOR|USB_REQUEST_FOR_ENDPOINT,0xff,0,0,0 };
USB_DEVICE_REQUEST TransferThread::OutRequest=
{USB_REQUEST_HOST_TO_DEVICE|USB_REQUEST_VENDOR|USB_REQUEST_FOR_ENDPOINT,0xff,0,0,0 };

BOOL TransferThread::SubmitRequest(LPVOID lpBuffer,DWORD dwSize)
{
    hTransfer=0;
    cEvent.ResetEvent();
    DEBUGMSG(DBG_FUNC,(TEXT("SubmitRequest (%s)%s flag(%lx),addr@%lx, length=%lx"),
        aSyncMode?TEXT("Async"):TEXT("Sync"),
        isInPipe()?TEXT("Read"):TEXT("Write"),
        GetFlags(),
        lpBuffer,dwSize));
    aTransfer.ReInitial(0);

    switch (GetEndPointAttribute()) {
      case USB_ENDPOINT_TYPE_BULK:
        hTransfer=IssueBulkTransfer(&aTransfer,dwSize,lpBuffer,NULL);
        break;
      case USB_ENDPOINT_TYPE_INTERRUPT:
        hTransfer=IssueInterruptTransfer(&aTransfer,dwSize,lpBuffer,NULL);
        break;
      case USB_ENDPOINT_TYPE_CONTROL: {
        USB_DEVICE_REQUEST ControlHead=(isInPipe()?InRequest:OutRequest);
        ControlHead.wLength=(WORD)dwSize;
        hTransfer=IssueControlTransfer(&aTransfer,&ControlHead,dwSize,lpBuffer,NULL);
                                  };
        break;
      default:
        ASSERT(FALSE);
    };
    BOOL bReturn=TransferCheck(aTransfer,dwSize);
    if (aTransfer.GetTransferHandle())
        aTransfer.CloseTransfer();
    return bReturn;

};
TransferThread::~TransferThread()
{
    if (GetThreadHandle()!=NULL && !ThreadTerminated(5000)) {
        ASSERT(FALSE);
        ForceTerminated();
    };
}
BOOL TransferThread::EmptyBuffer(DWORD dwTicks)
{
    PDWORD pBlockLength = NULL;

    if (!aSyncMode || isOutPipe())
        return TRUE;
    if (GetEndPointAttribute() ==USB_ENDPOINT_TYPE_ISOCHRONOUS)
        return TRUE;

    do {
        hTransfer=0;
        cEvent.ResetEvent();
        DEBUGMSG(DBG_FUNC,(TEXT("EmptyBuffer (%s)%s flag(%lx)"),
            aSyncMode?TEXT("Async"):TEXT("Sync"),
            isInPipe()?TEXT("Read"):TEXT("Write"),
            GetFlags()));
        bTransfer.ReInitial(0);
        DWORD dwSize=min(dwDataSize,LIMITED_UHCI_BLOCK*(DWORD)GetMaxPacketSize());

        switch (GetEndPointAttribute()) {
          case USB_ENDPOINT_TYPE_BULK:
            hTransfer=IssueBulkTransfer(&bTransfer,dwSize,lpDataBuffer,NULL);
            break;
          case USB_ENDPOINT_TYPE_INTERRUPT:
            hTransfer=IssueInterruptTransfer(&bTransfer,dwSize,lpDataBuffer,NULL);
            break;
          case USB_ENDPOINT_TYPE_CONTROL: {
            USB_DEVICE_REQUEST ControlHead=(isInPipe()?InRequest:OutRequest);
            ControlHead.wLength=(WORD)dwDataSize;
            hTransfer=IssueControlTransfer(&bTransfer,&ControlHead,dwSize,lpDataBuffer,NULL);
                                  };
            break;
          case USB_ENDPOINT_TYPE_ISOCHRONOUS: {
            WORD oneBlockLength=GetMaxPacketSize();
            ASSERT(oneBlockLength);
            oneBlockLength=min(oneBlockLength,MAX_ISOCH_FRAME_LENGTH);
            DWORD numOfBlock = (dwDataSize+oneBlockLength-1)/oneBlockLength;
            numOfBlock=min(numOfBlock,LIMITED_UHCI_BLOCK);
            pBlockLength = new DWORD[numOfBlock];
            if(pBlockLength == NULL)
                return FALSE;

            DWORD curDataSize=dwDataSize;
            for (DWORD blockNumber=0;blockNumber<numOfBlock;blockNumber++)
                if (curDataSize>oneBlockLength) {
                    pBlockLength[blockNumber]=oneBlockLength;
                    curDataSize -= oneBlockLength;
                }
                else {
                    pBlockLength[blockNumber]=curDataSize;
                    curDataSize = 0;
                    break;
                }
            DEBUGMSG(DBG_FUNC,(TEXT("IssueIsochTransfer(%s)(%s), BlockNumber(%lx), BlockSize(%lx), BufferAddr(%lx), BufferSize(%lx)"),
                    aSyncMode?TEXT("Async"):TEXT("Sync"),
                    isInPipe()?TEXT("Read"):TEXT("Write"),
                    numOfBlock,oneBlockLength,
                    lpDataBuffer,dwDataSize));
            bTransfer.ReInitial(0);
            hTransfer=IssueIsochTransfer(&bTransfer, 0, numOfBlock,
                pBlockLength,lpDataBuffer,NULL);
                                              }
              break;
        default:
            ASSERT(FALSE);
        };
        if (hTransfer) {
            if (!(bTransfer.WaitForTransferComplete(dwTicks) && bTransfer.IsCompleteNoError())) {  // failure
                hTransfer=0;
                bTransfer.AbortTransfer();
            };
            bTransfer.CloseTransfer();
        }
        if(GetEndPointAttribute() == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            Sleep(100);
            delete [] pBlockLength;
        }
    } while (hTransfer);
    return TRUE;
}

BOOL TransferThread::TransferCheck(UsbTransfer& aTransfer,DWORD dwBlockLength,PDWORD pdwLengthBlock)
{
    if (aSyncMode) {
        while (!aTransfer.IsTransferComplete ())
            if (!CheckPreTransfer(aTransfer,dwBlockLength,pdwLengthBlock)) {
                errorCode=ERROR_USB_PIPE_TRANSFER;
                return FALSE;
            };
        if (!cEvent.Lock(TIMEOUT_INTICKS)) {
            g_pKato->Log(LOG_FAIL, TEXT("Fail at Async Wait for call back time out"));
            errorCode=ERROR_USB_PIPE_TRANSFER;
            return FALSE;
        };

    };
    // assume finished.
    BOOL bReturn=TRUE;
    if(!CheckPostTransfer(aTransfer,dwBlockLength,pdwLengthBlock)) {
        errorCode=ERROR_USB_PIPE_TRANSFER;
        bReturn=FALSE;
    };
    EmptyBuffer(1000);
    return bReturn;
}

DWORD TransferThread::ActionCompleteNotify(UsbTransfer *pTransfer)
{
    DWORD dwReturn=UsbPipeService::ActionCompleteNotify(pTransfer);
    cEvent.SetEvent();
    return dwReturn;
}

BOOL TransferThread::CheckPreTransfer(UsbTransfer& aTransfer,DWORD dwBlockLength,PDWORD pdwLengthBlock)
{
    DEBUGMSG(DBG_FUNC,(TEXT("TransferTread::CheckPreTransfer")));
       if(dwBlockLength > 0x100000) //make prefast happy
         return FALSE;
    if (pdwLengthBlock) { // issoch transfer.
        PDWORD pdwErrors = new DWORD [dwBlockLength];
        PDWORD addrBlockLength = new DWORD [dwBlockLength];
        if(addrBlockLength == NULL || pdwErrors == NULL){
            if(pdwErrors)
                delete[] pdwErrors;
            if(addrBlockLength)
                delete[] addrBlockLength;
            g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
            return FALSE;
        }
        memset(addrBlockLength,0,sizeof(DWORD)*dwBlockLength);
        BOOL bIsochStatus=aTransfer.GetIsochResults(dwBlockLength,addrBlockLength, pdwErrors);
        if (!aTransfer.IsTransferComplete ()) {
            if (bIsochStatus==TRUE) {
                g_pKato->Log(LOG_FAIL, TEXT("Fail at GetIsochResults, return TRUE"));
                errorCode=ERROR_USB_TRANSFER_STATUS;
                delete [] pdwErrors;
                delete [] addrBlockLength;
                return FALSE;
            }

            for (DWORD index=0;index<dwBlockLength;index++)
                if (pdwErrors[index]!=USB_NO_ERROR ||
                        *(addrBlockLength+index)<*(pdwLengthBlock+index)) {
                    delete [] pdwErrors;
                    delete [] addrBlockLength;
                    return TRUE; // have not finished
                };
            g_pKato->Log(LOG_FAIL, TEXT("Fail at IsTransferComplete, return value is wrong"));
            errorCode=ERROR_USB_TRANSFER_STATUS;
            delete [] pdwErrors;
            delete [] addrBlockLength;
            return FALSE;
        }
        else{ // already finished nothing you can do.
            delete [] pdwErrors;
            delete [] addrBlockLength;
            return TRUE;
        }

    }
    else {
        DWORD dwError,dwTransferredByte;
        if (!aTransfer.GetTransferStatus (&dwTransferredByte,&dwError)) {
            g_pKato->Log(LOG_FAIL, TEXT("Fail at GetTransferStatus, Error code is %ld"),dwError);
            errorCode=ERROR_USB_TRANSFER_STATUS;
            return FALSE;
        }
        else
        if (!aTransfer.IsTransferComplete ()) {
            if (dwTransferredByte<dwBlockLength)
                return TRUE;
            else {
                g_pKato->Log(LOG_FAIL, TEXT("Fail at GetTransferStatus, return value is wrong"));
                errorCode=ERROR_USB_TRANSFER_STATUS;
                return FALSE;
            };

        }
        else
            return TRUE;
    }
}

BOOL TransferThread::CheckPostTransfer(UsbTransfer& aTransfer,DWORD dwBlockLength,PDWORD pdwLengthBlock)
{

    if (dwBlockLength <= 0 || dwBlockLength > 0x10000)
        return TRUE;
    DEBUGMSG(DBG_FUNC,(TEXT("TransferTread::CheckPostTransfer(%s)(%s)"),
        aSyncMode?TEXT("Async"):TEXT("Sync"),
        isInPipe()?TEXT("Read"):TEXT("Write")));

    if (!aTransfer.IsTransferComplete ()) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail at IsTransferComplete, return value is wrong"));
        errorCode=ERROR_USB_TRANSFER_STATUS;
        return FALSE;
    };

    if (pdwLengthBlock) { // issoch transfer. It may failure.
        if (!aTransfer.IsCompleteNoError())
            return TRUE;
        DEBUGMSG(DBG_DETAIL,(TEXT("TransferTread::CheckPostTransfer check for Isoch BlockLength(%lx)"),dwBlockLength));
        PDWORD pdwErrors = new DWORD [dwBlockLength];
        PDWORD addrBlockLength = new DWORD [dwBlockLength];
        if(addrBlockLength == NULL || pdwErrors == NULL){
            if(pdwErrors)
                delete[] pdwErrors;
            if(addrBlockLength)
                delete[] addrBlockLength;
            g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
            return FALSE;
        }
        memset(addrBlockLength,0,sizeof(DWORD)*dwBlockLength);
        if (!aTransfer.GetIsochResults(dwBlockLength,addrBlockLength, pdwErrors)) {
            g_pKato->Log(LOG_FAIL, TEXT("Fail at GetIsochResults"));
            errorCode=ERROR_USB_TRANSFER_STATUS;
            delete [] pdwErrors;
            delete [] addrBlockLength;
            return FALSE;
        }
        for (DWORD index=0;index<dwBlockLength;index++)
            if (*(addrBlockLength+index)<*(pdwLengthBlock+index) && isOutPipe()
                    || pdwErrors[index]!=USB_NO_ERROR) {
                g_pKato->Log(LOG_FAIL, TEXT("Fail at GetIsochResults, Submit(%lx),Result(%lx) Error(%lx) at %lx"),
                        *(pdwLengthBlock+index),*(addrBlockLength+index),pdwErrors[index],index);
                errorCode=ERROR_USB_TRANSFER_STATUS;
                delete [] pdwErrors;
                delete [] addrBlockLength;
                return FALSE;
            };
        delete [] pdwErrors;
        delete [] addrBlockLength;
        return TRUE; // have finished

    }
    else {
        DEBUGMSG(DBG_FUNC,(TEXT("TransferTread::CheckPostTransfer check for Other")));
        DWORD dwError,dwTransferredByte;
        if (!aTransfer.GetTransferStatus (&dwTransferredByte,&dwError)) {
            g_pKato->Log(LOG_FAIL, TEXT("Fail at GetTransferStatus, Error code is %ld"),dwError);
            errorCode=ERROR_USB_TRANSFER_STATUS;
            return FALSE;
        }
        else
        if (dwTransferredByte==dwBlockLength)
            return TRUE;
        else {
            g_pKato->Log(LOG_FAIL, TEXT("Fail check GetTransferStatus Transfer(%lx),Complete(%lx)"),
                dwBlockLength,dwTransferredByte);
            errorCode=ERROR_USB_TRANSFER_STATUS;
            return FALSE;
        };
    }
}

#define USB_GETSTATUS_DEVICE_STALL 1

BOOL TransferThread::SetEndpoint(BOOL bStalled)
{
    BYTE bEndpointAddr=GetPipeDescriptorPtr ()->bEndpointAddress;
    USB_TRANSFER hTransfer;
    if (bStalled)
        hTransfer=pDriver->SetFeature(NULL,NULL,USB_SEND_TO_ENDPOINT,USB_FEATURE_ENDPOINT_STALL,bEndpointAddr);
    else
        hTransfer=pDriver->ClearFeature(NULL,NULL,USB_SEND_TO_ENDPOINT,USB_FEATURE_ENDPOINT_STALL,bEndpointAddr);
    if (hTransfer==NULL) {
        ResetPipe();
        return FALSE;
    };
    UsbTransfer featureTransfer(hTransfer,GetUsbFuncPtr()) ;
    if (featureTransfer.IsTransferComplete () && featureTransfer.IsCompleteNoError()) {
        WORD wStatus=0;
        hTransfer=pDriver->GetStatus(NULL,NULL,USB_SEND_TO_ENDPOINT,bEndpointAddr,&wStatus);
        if (hTransfer==NULL) {
            ResetPipe();
            return FALSE;
        };
        UsbTransfer statusTransfer(hTransfer,GetUsbFuncPtr());
        if (statusTransfer.IsTransferComplete() && statusTransfer.IsCompleteNoError()) {
            if (bStalled ==((wStatus & USB_GETSTATUS_DEVICE_STALL)!=0))
                return TRUE;
        }
        else
            ResetPipe();
    }
    else
        ResetPipe();
    ASSERT(FALSE);
    return FALSE;
}

#define MAX_BLOCKS 100
DWORD TransferThread::ThreadRun()
{
    errorCode=GetPipeLastError();
    DEBUGMSG(DBG_FUNC,(TEXT("Start Transfer Thread")));
    if (errorCode==0) {

        switch (GetEndPointAttribute()) {
          case USB_ENDPOINT_TYPE_ISOCHRONOUS: {
            hTransfer=0;
            cEvent.ResetEvent();
            SetFlags(GetFlags()|USB_START_ISOCH_ASAP);
            WORD oneBlockLength=GetMaxPacketSize();
            ASSERT(oneBlockLength);
            oneBlockLength=min(oneBlockLength,MAX_ISOCH_FRAME_LENGTH);
            DWORD numOfBlock = (dwDataSize+oneBlockLength-1)/oneBlockLength;
            numOfBlock=min(numOfBlock,LIMITED_UHCI_BLOCK);
            PDWORD pBlockLength=new DWORD[numOfBlock];
            if(pBlockLength == NULL){
                    g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
                errorCode=ERROR_GEN_FAILURE;
                break;
            }
            DWORD curDataSize=dwDataSize;
            for (DWORD blockNumber=0;blockNumber<numOfBlock;blockNumber++)
                if (curDataSize>oneBlockLength) {
                    pBlockLength[blockNumber]=oneBlockLength;
                    curDataSize -= oneBlockLength;
                }
                else {
                    pBlockLength[blockNumber]=curDataSize;
                    curDataSize = 0;
                    break;
                }
            DEBUGMSG(DBG_FUNC,(TEXT("IssueIsochTransfer(%s)(%s), BlockNumber(%lx), BlockSize(%lx), BufferAddr(%lx), BufferSize(%lx)"),
                    aSyncMode?TEXT("Async"):TEXT("Sync"),
                    isInPipe()?TEXT("Read"):TEXT("Write"),
                    numOfBlock,oneBlockLength,
                    lpDataBuffer,dwDataSize));
            aTransfer.ReInitial(0);
            IssueIsochTransfer(&aTransfer,0,numOfBlock,
                pBlockLength,lpDataBuffer,NULL);
            TransferCheck(aTransfer,
                    numOfBlock,pBlockLength);
            if (aTransfer.GetTransferHandle())
                aTransfer.CloseTransfer();
            delete [] pBlockLength;
                                              };
            break;
          case USB_ENDPOINT_TYPE_BULK:
          case USB_ENDPOINT_TYPE_INTERRUPT:
          case USB_ENDPOINT_TYPE_CONTROL: {
            DWORD curDataSize=dwDataSize;
            WORD oneBlockLength=GetMaxPacketSize();
            pDriver->GetFrameLength(&oneBlockLength);
            oneBlockLength=min(oneBlockLength,LIMITED_UHCI_BLOCK*GetMaxPacketSize());
            DWORD dLength=min(curDataSize,oneBlockLength);
            if (!SubmitRequest((LPBYTE)lpDataBuffer+dwDataSize-curDataSize,dLength)) {
                errorCode=ERROR_USB_PIPE_TRANSFER;
                break;
            }
            else
                curDataSize-=dLength;
            if (curDataSize>=dwDataSize) {
                g_pKato->Log(LOG_FAIL, TEXT("Fail at Transfer,InCompleted, Left(%ld)Byte"),curDataSize);
                errorCode=ERROR_USB_PIPE_TRANSFER;
            };
                                          };
            break;
          default:
            ASSERT(FALSE);
        };
    DEBUGMSG(DBG_FUNC,(TEXT("End Transfer Thread")));
    };
    return 0;
};


BOOL AbortTransferThread::TransferCheck(UsbTransfer& aTransfer,DWORD dwBlockLength,PDWORD pdwLengthBlock)
{
//    Sleep(10);
    DEBUGMSG(DBG_FUNC,(TEXT("TransferCheck")));
    if (!aTransfer.IsTransferComplete ()) {
        if (aSyncMode) {
            DEBUGMSG(DBG_DETAIL,(TEXT("Before Call AbortTransfer")));
            aTransfer.AbortTransfer(USB_NO_WAIT);
        }
        else {
            aTransfer.AbortTransfer();
            if (!aTransfer.IsTransferComplete()) {
                g_pKato->Log(LOG_FAIL, TEXT("AbortTransfer Can not stop transfer"));
                errorCode=ERROR_USB_PIPE_TRANSFER;
                EmptyBuffer(1000);
                return FALSE;
            };
        };

        if (!cEvent.Lock(TIMEOUT_INTICKS)) {
            g_pKato->Log(LOG_FAIL, TEXT("Fail at Async Wait for call back time out"));
            errorCode=ERROR_USB_PIPE_TRANSFER;
            EmptyBuffer(1000);
            return FALSE;
        };
        // assume finished.
        if(!CheckPostTransfer(aTransfer,dwBlockLength,pdwLengthBlock)) {
            g_pKato->Log(LOG_FAIL, TEXT("Fail at CheckTransfer after abort"));
            errorCode=ERROR_USB_PIPE_TRANSFER;
            EmptyBuffer(1000);
            return FALSE;
        };
    }
    else {
        DEBUGMSG(DBG_DETAIL,(TEXT("Transfer End already. Test will end!")));
    };
    aTransfer.AbortTransfer();// after finish should do nothing.
    EmptyBuffer(1000);
    return TRUE;
}

BOOL AbortTransferThread::CheckPostTransfer(UsbTransfer& aTransfer,DWORD dwBlockLength,PDWORD pdwLengthBlock)
{
    DEBUGMSG(DBG_FUNC,(TEXT("AbortTransferTread::CheckPosTransfer")));
    if (pdwLengthBlock) { // issoch transfer.
        DWORD dwError,dwTransferredByte;
        if (!aTransfer.GetTransferStatus (&dwTransferredByte,&dwError)) {
            g_pKato->Log(LOG_FAIL, TEXT("Fail at GetTransferStatus, Error code is %ld"),dwError);
            errorCode=ERROR_USB_TRANSFER_STATUS;
            return FALSE;
        }
        return TRUE;
    }
    else {
        DWORD dwError,dwTransferredByte;
        if (!aTransfer.GetTransferStatus (&dwTransferredByte,&dwError)) {
            g_pKato->Log(LOG_FAIL, TEXT("Fail at GetTransferStatus, Error code is %ld"),dwError);
            errorCode=ERROR_USB_TRANSFER_STATUS;
            return FALSE;
        }
        else
        if (dwTransferredByte<dwBlockLength) {
            g_pKato->Log(LOG_COMMENT, TEXT("Abort Transfer,Submit(%lx),Transferred(%lx)"),dwBlockLength,dwTransferredByte);
            return TRUE;
        }
        else {
            g_pKato->Log(LOG_COMMENT, TEXT("Warning at GetTransferStatus, transfer not aborted"));
            return TRUE;
        };

    }
}
BOOL CloseTransferThread::TransferCheck(UsbTransfer& aTransfer,DWORD dwBlockLength,PDWORD pdwLengthBlock)
{
    UNREFERENCED_PARAMETER(dwBlockLength);
    UNREFERENCED_PARAMETER(pdwLengthBlock);
    USB_HANDLE tempHandle=aTransfer.GetTransferHandle();

    //we try to close transfer even transfer may not be done yet
    if(!aTransfer.CloseTransfer()) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail at CloseTransfer"));
        errorCode=ERROR_USB_TRANSFER_STATUS;
        EmptyBuffer(1000);
        return FALSE;
    };
    cEvent.ResetEvent();
    aTransfer.SetTransferHandle(tempHandle);
    if(aTransfer.CloseTransfer()) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail at CloseTransfer, Multi-Close Success"));
        errorCode=ERROR_USB_TRANSFER_STATUS;
        EmptyBuffer(1000);
        return FALSE;
    };
    if (cEvent.Lock(TIMEOUT_INTICKS)) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail at CloseTransfer, Call back after close"));
        errorCode=ERROR_USB_TRANSFER_STATUS;
        EmptyBuffer(1000);
        return FALSE;
    };
    aTransfer.SetTransferHandle(NULL);
    EmptyBuffer(1000);
    return TRUE;
}

//this function will traverse all pipe pairs, and run transfer test for each pipe pair
BOOL
SinglePairTrans(UsbClientDrv *pDriverPtr, DWORD dwTransType, BOOL fAsync, int iID){

    if(pDriverPtr == NULL)
        return FALSE;

    DEBUGMSG(DBG_FUNC,(TEXT("+SinglePairTrans\n")));

    for(UCHAR uIndex = 0; uIndex < g_pTstDevLpbkInfo[iID]->uNumOfPipePairs; uIndex ++){

        //then retrieve the corresponding endpoint structures
        LPCUSB_ENDPOINT pInEP = NULL;
        LPCUSB_ENDPOINT pOutEP = NULL;
        for (UINT i = 0;i < pDriverPtr->GetNumOfEndPoints(); i++) {
            LPCUSB_ENDPOINT curPoint=pDriverPtr->GetDescriptorPoint(i);
            ASSERT(curPoint);
            if (curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucInEp) {
                pInEP = curPoint;
            }
            else if (curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucOutEp) {
                pOutEP = curPoint;
            }
        }
        if (!(pInEP && pOutEP)) {//one or both endpoints are missing
        g_pKato->Log(LOG_FAIL, TEXT("Can not find Pipe Pair"));
        return FALSE;
        }

    g_pKato->Log(LOG_DETAIL, TEXT("start transfer test on endpoint 0x%x(IN) and 0x%x(OUT) (%s)"),
                                            pInEP->Descriptor.bEndpointAddress, pOutEP->Descriptor.bEndpointAddress,
                                            g_szEPTypeStrings[pInEP->Descriptor.bmAttributes]);

        BOOL bRet = TransNormalForCommand(pDriverPtr, pInEP, pOutEP, fAsync, dwTransType, iID);
        if(bRet == FALSE){
            g_pKato->Log(LOG_DETAIL, TEXT("transfer test on endpoint 0x%x(IN) and 0x%x(OUT) (%s) failed!!!"),
                                                    pInEP->Descriptor.bEndpointAddress, pOutEP->Descriptor.bEndpointAddress,
                                                    g_szEPTypeStrings[pInEP->Descriptor.bmAttributes]);
            return FALSE;
        }
    }

    DEBUGMSG(DBG_FUNC,(TEXT("-SinglePairTrans\n")));
    return TRUE;

}


//Transfer test for each pipe pair
BOOL TransNormalForCommand(UsbClientDrv * pDriverPtr, LPCUSB_ENDPOINT pInEP, LPCUSB_ENDPOINT pOutEP, BOOL ASync,DWORD dwObjectNumber, int iID)
{
    if(pDriverPtr == NULL || pInEP == NULL || pOutEP == NULL)
        return FALSE;

    TCHAR  szCEDeviceName[SET_DEV_NAME_BUFF_SIZE] = {0};
    SetDeviceName(szCEDeviceName, (USHORT)iID, pOutEP, pInEP);

    BOOL bRet=TRUE;
    PDWORD pInDataBlock=new DWORD[TEST_DATA_SIZE];
    if(pInDataBlock == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("%s out of memory!!!"), szCEDeviceName);
        return FALSE;
    }
    PDWORD pOutDataBlock=new DWORD[TEST_DATA_SIZE];
    if(pOutDataBlock == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("%s out of memory!!!"), szCEDeviceName);
        delete[] pInDataBlock;
        return FALSE;
    }
    DWORD dwPattern= 0x3456abcd; //Random();
    for (DWORD index=0;index<TEST_DATA_SIZE;index++) {
        pOutDataBlock[index]=dwPattern++;
        pInDataBlock[index]=0;
    }

    TransferThread *InPipe=NULL;
    TransferThread *OutPipe=NULL;
    switch (dwObjectNumber) {
        case 1:default:// normal transfer test
            g_pKato->Log(LOG_COMMENT, TEXT("%s Normal transfer test"), szCEDeviceName);
            InPipe=new TransferThread(pDriverPtr,&(pInEP->Descriptor),(PVOID)pInDataBlock,TEST_DATA_SIZE*sizeof(DWORD),ASync);
            OutPipe=new TransferThread(pDriverPtr,&(pOutEP->Descriptor),(PVOID)pOutDataBlock,TEST_DATA_SIZE*sizeof(DWORD),ASync);
            break;
        case 2: //abort transfer test: abort transfers after issued and before comletion
            g_pKato->Log(LOG_COMMENT, TEXT("%s Abort transfer test"), szCEDeviceName);
            InPipe= new AbortTransferThread(pDriverPtr,&(pInEP->Descriptor),(PVOID)pInDataBlock,TEST_DATA_SIZE*sizeof(DWORD),ASync);
            OutPipe= new AbortTransferThread(pDriverPtr,&(pOutEP->Descriptor),(PVOID)pOutDataBlock,TEST_DATA_SIZE*sizeof(DWORD),ASync);
            break;
        case 3://close transfer before completion
            g_pKato->Log(LOG_COMMENT, TEXT("%s Close transfer test"), szCEDeviceName);
            InPipe=    new CloseTransferThread(pDriverPtr,&(pInEP->Descriptor),(PVOID)pInDataBlock,TEST_DATA_SIZE*sizeof(DWORD));
            OutPipe=new CloseTransferThread(pDriverPtr,&(pOutEP->Descriptor),(PVOID)pOutDataBlock,TEST_DATA_SIZE*sizeof(DWORD));
        break;
    }

    if(InPipe == NULL || OutPipe == NULL){
        g_pKato->Log(LOG_FAIL, TEXT(" %s Create In or Out pipe instance failed!!!"), szCEDeviceName);
        if(InPipe)
            delete InPipe;
        if(OutPipe)
            delete OutPipe;
        delete[] pInDataBlock;
        delete[] pOutDataBlock;
    }

    DWORD dwMaxPSize = OutPipe->GetMaxPacketSize();
    //make ready for
    USB_TDEVICE_REQUEST utdr = {0};
    utdr.bmRequestType = USB_REQUEST_CLASS;
    utdr.bRequest = TEST_REQUEST_DATALPBK;
    utdr.wLength = sizeof(USB_TDEVICE_DATALPBK);
    USB_TDEVICE_DATALPBK utdl = {0};
    utdl.uOutEP = pOutEP->Descriptor.bEndpointAddress;
    utdl.wBlockSize = TEST_DATA_SIZE*sizeof(DWORD);
    utdl.wNumofBlocks = 1;
    NKDbgPrintfW(_T("%s packetsize is %d, block size = %d, rounds = %d"), szCEDeviceName, dwMaxPSize, utdl.wBlockSize, utdl.wNumofBlocks);

    if(dwObjectNumber == 1){//abort/close test does not need to send command to function side
        //issue command to netchip2280
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utdl, NULL) == FALSE){
            g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
            delete OutPipe;
            delete InPipe;
            delete[] pInDataBlock;
            delete[] pOutDataBlock;
            return FALSE;
        }
    }

    OutPipe->SetEndpoint(TRUE);
    OutPipe->SetEndpoint(FALSE);
    InPipe->SetEndpoint(TRUE);
    InPipe->SetEndpoint(FALSE);

    if ((pOutEP->Descriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_ISOCHRONOUS) { // special for ISOCH
        OutPipe->ThreadStart();
        Sleep(900);
        InPipe->ThreadStart();
    }
    else {
        InPipe->ThreadStart();
        OutPipe->ThreadStart();
    };

    if (InPipe->WaitThreadComplete(50000)==FALSE ||OutPipe->WaitThreadComplete(50000)==FALSE) {
        g_pKato->Log(LOG_FAIL, TEXT("%s Test Time Out"), szCEDeviceName);
        bRet=FALSE;
    };
    if (InPipe->GetErrorCode()) {
        g_pKato->Log(LOG_FAIL, TEXT("%s InPipe Error Code (%d)"), szCEDeviceName, InPipe->GetErrorCode());
        bRet=FALSE;
    };
    if (OutPipe->GetErrorCode())  {
        g_pKato->Log(LOG_FAIL, TEXT("%s OutPipe Error Code (%d)"),szCEDeviceName,OutPipe->GetErrorCode());
        bRet=FALSE;
    }

    InPipe->ResetPipe();
    OutPipe->ResetPipe();

    delete InPipe;
    delete OutPipe;
    delete [] pInDataBlock;
    delete [] pOutDataBlock;

    return bRet;
};


//------------------------------
// some additional tests below
//------------------------------


//this function issues a large data transfer (4k) among bulk and isoch pipe pairs
BOOL
SinglePairSpecialTrans(UsbClientDrv *pDriverPtr, int iID, int iCaseID){

    if(pDriverPtr == NULL)
        return FALSE;

    DEBUGMSG(DBG_FUNC,(TEXT("+SinglePairSpecialTrans\n")));

    for(UCHAR uIndex = 0; uIndex < g_pTstDevLpbkInfo[iID]->uNumOfPipePairs; uIndex ++){
        if( g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucType == USB_ENDPOINT_TYPE_INTERRUPT ||
             g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucType == USB_ENDPOINT_TYPE_CONTROL)
             continue;

        //then retrieve the corresponding endpoint structures
        LPCUSB_ENDPOINT pInEP = NULL;
        LPCUSB_ENDPOINT pOutEP = NULL;
        for (UINT i = 0;i < pDriverPtr->GetNumOfEndPoints(); i++) {
            LPCUSB_ENDPOINT curPoint=pDriverPtr->GetDescriptorPoint(i);
            ASSERT(curPoint);
            if (curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucInEp) {
                pInEP = curPoint;
            }
            else if (curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucOutEp) {
                pOutEP = curPoint;
            }
        }
        if (!(pInEP && pOutEP)) {//one or both endpoints are missing
        g_pKato->Log(LOG_FAIL, TEXT("Can not find Pipe Pair"));
        return FALSE;
        }

        g_pKato->Log(LOG_DETAIL, TEXT("start special transfer test on endpoint 0x%x(IN) and 0x%x(OUT) (%s)"),
                                            pInEP->Descriptor.bEndpointAddress, pOutEP->Descriptor.bEndpointAddress,
                                            g_szEPTypeStrings[pInEP->Descriptor.bmAttributes]);
      //OUT transfer first
      BOOL bRet = TRUE;
      if((pInEP->Descriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) ==  USB_ENDPOINT_TYPE_BULK){
          if(iCaseID == 1)
                bRet = TransCase2TestCommand(pDriverPtr, pOutEP, TRUE, iID);
            else if(iCaseID == 2)
                bRet = TransCase3TestCommand(pDriverPtr, pOutEP, TRUE, iID);
            else if(iCaseID == 3)
                bRet = TransCase4TestCommand(pDriverPtr, pOutEP, TRUE, iID);

        }
        else if(iCaseID == 1){
            bRet = TransCase1TestCommand(pDriverPtr, pOutEP, TRUE, iID);
        }

        if(bRet == FALSE){
            g_pKato->Log(LOG_DETAIL, TEXT("transfer test on endpoint 0x%x(IN) and 0x%x(OUT) (%s) failed!!!"),
                                                    pInEP->Descriptor.bEndpointAddress, pOutEP->Descriptor.bEndpointAddress,
                                                    g_szEPTypeStrings[pInEP->Descriptor.bmAttributes]);
            return FALSE;
        }

        //IN transfer
      if((pInEP->Descriptor.bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_BULK){
          if(iCaseID == 1)
                bRet = TransCase2TestCommand(pDriverPtr, pInEP, FALSE, iID);
            else if(iCaseID == 2)
                bRet = TransCase3TestCommand(pDriverPtr, pInEP, FALSE, iID);
            else if(iCaseID == 3)
                bRet = TransCase4TestCommand(pDriverPtr, pInEP, FALSE, iID);

        }
        else if(iCaseID == 1){
            bRet = TransCase1TestCommand(pDriverPtr, pInEP, FALSE, iID);
        }

        if(bRet == FALSE){
            g_pKato->Log(LOG_DETAIL, TEXT("transfer test on endpoint 0x%x(IN) and 0x%x(OUT) (%s) failed!!!"),
                                                    pInEP->Descriptor.bEndpointAddress, pOutEP->Descriptor.bEndpointAddress,
                                                    g_szEPTypeStrings[pInEP->Descriptor.bmAttributes]);
            return FALSE;
        }

    }

    DEBUGMSG(DBG_FUNC,(TEXT("-SinglePairSpecialTrans\n")));
    return TRUE;

}

//isoch transfer test -- send/receive 4k data
BOOL TransCase1TestCommand(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpPipePoint, BOOL bOutput, int iID)
{
    if(pDriverPtr == NULL || lpPipePoint == NULL)
        return FALSE;

    TCHAR  szCEDeviceName[SET_DEV_NAME_BUFF_SIZE] = {0};
    if(bOutput == TRUE){
        SetDeviceName(szCEDeviceName, (USHORT)iID, lpPipePoint, NULL);
    }
    else{
        SetDeviceName(szCEDeviceName, (USHORT)iID, NULL, lpPipePoint);
    }

    DEBUGMSG(DBG_FUNC,(TEXT("%s Start Trans Special Case 1 Test"), szCEDeviceName));

    DEBUGMSG(DBG_DETAIL,(TEXT("%s Create and Open Pipe"), szCEDeviceName));
    LPCUSB_FUNCS lpUsbFuncs =pDriverPtr->lpUsbFuncs;
    USB_HANDLE usbHandle= pDriverPtr->GetUsbHandle();
    USB_PIPE hPipe=(*(lpUsbFuncs->lpOpenPipe))(usbHandle,&(lpPipePoint->Descriptor));
    if (hPipe==NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("%s Error at Open Pipe"), szCEDeviceName);
        return FALSE;
    }
    (*(lpUsbFuncs->lpResetPipe))(hPipe);

    WORD dwDataSize=TEST_DATA_SIZE*sizeof(DWORD);
    WORD oneBlockLength=lpPipePoint->Descriptor.wMaxPacketSize;

    TransStatus aTransStatus;
    aTransStatus.lpUsbFuncs=lpUsbFuncs;
    aTransStatus.hTransfer=0;
    aTransStatus.IsFinished=FALSE;
    aTransStatus.cEvent.ResetEvent();

    DWORD numBlock=(dwDataSize+oneBlockLength-1)/oneBlockLength;
    PDWORD BlockLength=new DWORD[numBlock];
    if(BlockLength == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("%s Out of Memory!"), szCEDeviceName);
        return FALSE;
    }
    DWORD curDataSize=dwDataSize;
    for (DWORD blockNumber=0;blockNumber<numBlock;blockNumber++){
        if (curDataSize>oneBlockLength)
            BlockLength[blockNumber]=oneBlockLength;
        else {
            BlockLength[blockNumber]=curDataSize;
            blockNumber++;
            break;
        }
    }

    DWORD dwDataBufferSize = 0; // Number of bytes.
    for(DWORD dwBlk=0; dwBlk<numBlock; dwBlk++)
    {
        dwDataBufferSize += BlockLength[dwBlk];
    }

    PDWORD DataBlock = NULL;
    DataBlock = (PDWORD)LocalAlloc(LPTR, dwDataBufferSize);
    if(DataBlock == NULL){
        DEBUGMSG(DBG_ERR,(TEXT("%s Pipe Special Case 1 Test: Out of Memory"), szCEDeviceName));
        delete[] BlockLength;
        return FALSE;
    }

    if(bOutput == TRUE){
        DWORD dwValue = 1;
        for(DWORD i = 0; i < (dwDataBufferSize>>2); i++){
            DataBlock[i] = dwValue;
            dwValue ++;
        }
    }
    else{
        memset(DataBlock, 0, dwDataBufferSize);
    }

    // Get Transfer Status.
    PDWORD pCurLengths= new DWORD[blockNumber];
    PDWORD pCurErrors = new DWORD[blockNumber];
    ASSERT(pCurLengths);
    ASSERT(pCurErrors);

    if(bOutput == TRUE){
        USB_TDEVICE_REQUEST utdr = {0};
        utdr.bmRequestType = USB_REQUEST_CLASS;
        utdr.bRequest = TEST_REQUEST_DATALPBK;
        utdr.wLength = sizeof(USB_TDEVICE_DATALPBK);
        USB_TDEVICE_DATALPBK utdl = {0};
        utdl.uOutEP = lpPipePoint->Descriptor.bEndpointAddress;
        utdl.wBlockSize = TEST_DATA_SIZE*sizeof(DWORD);
        utdl.wNumofBlocks = 1;
        NKDbgPrintfW(_T("%s packetsize is %d, block size = %d, rounds = %d"), szCEDeviceName, lpPipePoint->Descriptor.wMaxPacketSize, utdl.wBlockSize, utdl.wNumofBlocks);

        //issue command to netchip2280
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utdl, NULL) == FALSE){
            g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
            delete [] pCurLengths;
            delete [] pCurErrors;
             delete[] BlockLength;
             LocalFree(DataBlock);
            return FALSE;
        }

    }

    DEBUGMSG(DBG_DETAIL,(TEXT("%s Case 1 Call IssueIsochTransfer(%s)(%s) write addr@%lx, length=%lx,numOfBlock=%lx,firstBlockLength=%lx"),
                                        TEXT("Async"), bOutput?TEXT("Out Transfer"):TEXT("In Transfer"),
                                        szCEDeviceName, DataBlock,oneBlockLength, blockNumber,BlockLength[0]));

    aTransStatus.hTransfer=(*lpUsbFuncs->lpIssueIsochTransfer)(
                                                                    hPipe,
                                                                    TransferNotify,
                                                                    &aTransStatus,
                                                                    USB_START_ISOCH_ASAP|USB_SEND_TO_ENDPOINT|USB_NO_WAIT|(bOutput?USB_OUT_TRANSFER:USB_IN_TRANSFER),
                                                                    0,
                                                                    blockNumber,
                                                                    BlockLength,
                                                                    DataBlock,
                                                                    NULL);

    ASSERT(aTransStatus.hTransfer);

    BOOL rtValue=TRUE;
    if (aTransStatus.cEvent.Lock(3000)!=TRUE || aTransStatus.IsFinished==FALSE) {// Wait for call back . 3 second timeout
        g_pKato->Log(LOG_FAIL, TEXT("%s IsTransferComplet not return true in call back"),szCEDeviceName);
        rtValue=FALSE;
    }
    if (!(*lpUsbFuncs->lpIsTransferComplete)(aTransStatus.hTransfer)) {
        g_pKato->Log(LOG_FAIL, TEXT("%s IsTransferComplet not return true after call back"), szCEDeviceName);
        rtValue=FALSE;
    }

    if (!(*lpUsbFuncs->lpGetIsochResults)(aTransStatus.hTransfer,blockNumber,pCurLengths,pCurErrors))  { // fail
        // BUG unknow reason failure.
         g_pKato->Log(LOG_FAIL, TEXT("%s Failure:GetIsochStatus return FALSE, before transfer complete, NumOfBlock(%lx)"), szCEDeviceName, blockNumber);
         rtValue=FALSE;
    };
    DEBUGMSG(DBG_DETAIL,(TEXT("%s After GetIsochResults"), szCEDeviceName));
    delete [] pCurLengths;
    delete [] pCurErrors;

    if (!(*lpUsbFuncs->lpCloseTransfer)(aTransStatus.hTransfer)) { // failure
        g_pKato->Log(LOG_FAIL,TEXT("%s CloseTransfer Failure"), szCEDeviceName);
        rtValue = FALSE;
    }

    if ((*(lpUsbFuncs->lpClosePipe))(hPipe)==FALSE) {
        g_pKato->Log(LOG_FAIL, TEXT("%s Error at Close Pipe"), szCEDeviceName);
        DEBUGMSG(DBG_ERR,(TEXT("%s Error at Close Pipe"), szCEDeviceName));
    }

    if(bOutput == FALSE){//check data
        DWORD dwValue = 1;
        for(int i = 0; i < TEST_DATA_SIZE; i++){
            if(DataBlock[i] != dwValue){//this is iscoh transfer, so we just issue a warning here
                DEBUGMSG(DBG_WARN, (_T("%s At offset %d, expect value=0x%x, real value=0x%x"),
                                                        szCEDeviceName,i, dwValue, DataBlock[i]));
                break;
            }
            dwValue ++;
        }
    }

    delete[] BlockLength;
    LocalFree(DataBlock);
    return (rtValue);

}

//bulk transfer test
BOOL TransCase2TestCommand(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpPipePoint, BOOL bOutput, int iID)
{
    if(pDriverPtr == NULL || lpPipePoint == NULL)
        return FALSE;

    TCHAR  szCEDeviceName[SET_DEV_NAME_BUFF_SIZE] = {0};
    if(bOutput == TRUE){
        SetDeviceName(szCEDeviceName, (USHORT)iID, lpPipePoint, NULL);
    }
    else{
        SetDeviceName(szCEDeviceName, (USHORT)iID, NULL, lpPipePoint);
    }

    DEBUGMSG(DBG_FUNC,(TEXT("%s Start Transfer Special Case 2 Test"), szCEDeviceName));

    PDWORD DataBlock = NULL;//  [TEST_DATA_SIZE];
    DataBlock = (PDWORD)LocalAlloc(LPTR, TEST_DATA_SIZE*sizeof(DWORD));
    if(DataBlock == NULL){
        DEBUGMSG(DBG_ERR,(TEXT("%s Pipe Special Case 2Test: Out of Memory"), szCEDeviceName));
        return FALSE;
    }
    if(bOutput == TRUE){
        DWORD dwValue = 1;
        for(int i = 0; i < TEST_DATA_SIZE; i++){
            DataBlock[i] = dwValue;
            dwValue ++;
        }
    }
    else{
        memset(DataBlock, 0, sizeof(DWORD)*TEST_DATA_SIZE);
    }


    DEBUGMSG(DBG_DETAIL,(TEXT("%s Create and Open Pipe"), szCEDeviceName));
    LPCUSB_FUNCS lpUsbFuncs =pDriverPtr->lpUsbFuncs;
    USB_HANDLE usbHandle= pDriverPtr->GetUsbHandle();
    USB_PIPE hPipe=(*(lpUsbFuncs->lpOpenPipe))(usbHandle,&(lpPipePoint->Descriptor));
    if (hPipe==NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("%s Error at Open Pipe"), szCEDeviceName);
        DEBUGMSG(DBG_ERR,(TEXT("Error at Open Pipe"), szCEDeviceName));
        LocalFree(DataBlock);
        return FALSE;
    }
    (*(lpUsbFuncs->lpResetPipe))(hPipe);

    // WORD dwDataSize=TEST_DATA_SIZE*sizeof(DWORD);
    // WORD oneBlockLength=lpPipePoint->Descriptor.wMaxPacketSize;

    TransStatus aTransStatus;
    aTransStatus.lpUsbFuncs=lpUsbFuncs;
    aTransStatus.hTransfer=0;
    aTransStatus.IsFinished=FALSE;
    aTransStatus.cEvent.ResetEvent();

    if(bOutput == TRUE){
        USB_TDEVICE_REQUEST utdr = {0};
        utdr.bmRequestType = USB_REQUEST_CLASS;
        utdr.bRequest = TEST_REQUEST_DATALPBK;
        utdr.wLength = sizeof(USB_TDEVICE_DATALPBK);
        USB_TDEVICE_DATALPBK utdl = {0};
        utdl.uOutEP = lpPipePoint->Descriptor.bEndpointAddress;
        utdl.wBlockSize = TEST_DATA_SIZE*sizeof(DWORD);
        utdl.wNumofBlocks = 1;
        NKDbgPrintfW(_T("%s packetsize is %d, block size = %d, rounds = %d"), szCEDeviceName, lpPipePoint->Descriptor.wMaxPacketSize, utdl.wBlockSize, utdl.wNumofBlocks);

        //issue command to netchip2280
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utdl, NULL) == FALSE){
            g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
             LocalFree(DataBlock);
            return FALSE;
        }
    }

    DEBUGMSG(DBG_DETAIL,(TEXT("%s Case 2 Call IssueBulkTransfer(%s)(%s) write addr@%lx, length=%lx"), szCEDeviceName,
                                                TEXT("Async"), bOutput?TEXT("Out Transfer"):TEXT("In Transfer"),
                                                DataBlock,lpPipePoint->Descriptor.wMaxPacketSize));

    DWORD dwFlag = (bOutput == TRUE)?USB_OUT_TRANSFER:USB_IN_TRANSFER;
    dwFlag |= (USB_NO_WAIT | USB_SEND_TO_ENDPOINT);
    aTransStatus.hTransfer=(*lpUsbFuncs->lpIssueBulkTransfer)(
                                                                hPipe,
                                                                TransferNotify,
                                                                &aTransStatus,
                                                                dwFlag,
                                                                TEST_DATA_SIZE*sizeof(DWORD),
                                                                DataBlock,
                                                                NULL);

    ASSERT(aTransStatus.hTransfer);
    while(aTransStatus.IsFinished == FALSE)
        Sleep(10);

    BOOL fRet = TRUE;
    if (!(*lpUsbFuncs->lpCloseTransfer)(aTransStatus.hTransfer)) { // failure
        g_pKato->Log(LOG_FAIL,TEXT("%s CloseTransfer Failure"), szCEDeviceName);
        LocalFree(DataBlock);
        fRet = FALSE;
    }
    else {
        g_pKato->Log(LOG_COMMENT,TEXT("%s Transfer Closed successfully"), szCEDeviceName);
    }

    if(bOutput == FALSE){//check data
        DWORD dwValue = 1;
        for(int i = 0; i < TEST_DATA_SIZE; i++){
            if(DataBlock[i] != dwValue){
                DEBUGMSG(DBG_ERR, (_T("%s At offset %d, expect value=0x%x, real value=0x%x"),
                                                        szCEDeviceName,i, dwValue, DataBlock[i]));
                fRet = FALSE;
                break;
            }
            dwValue ++;
        }
    }

    LocalFree(DataBlock);

    if ((*(lpUsbFuncs->lpClosePipe))(hPipe)==FALSE) {
        g_pKato->Log(LOG_FAIL, TEXT("%s Error at Close Pipe"),szCEDeviceName);
        DEBUGMSG(DBG_ERR,(TEXT("%s Error at Close Pipe"), szCEDeviceName));
        fRet = FALSE;
    }

    return fRet;

}

//this test will deliberately add some delays between bulk transfers
BOOL TransCase3TestCommand(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpPipePoint, BOOL bOutput, int iID)
{
    if(pDriverPtr == NULL || lpPipePoint == NULL)
        return FALSE;

    TCHAR  szCEDeviceName[SET_DEV_NAME_BUFF_SIZE] = {0};
    if(bOutput == TRUE){
        SetDeviceName(szCEDeviceName, (USHORT)iID, lpPipePoint, NULL);
    }
    else{
        SetDeviceName(szCEDeviceName, (USHORT)iID, NULL, lpPipePoint);
    }
    DEBUGMSG(DBG_FUNC,(TEXT("%s Start Transfer Special Case 2 Test"), szCEDeviceName));

    PDWORD DataBlock = NULL;//  [TEST_DATA_SIZE];
    DataBlock = (PDWORD)LocalAlloc(LPTR, TEST_DATA_SIZE*sizeof(DWORD));
    if(DataBlock == NULL){
        DEBUGMSG(DBG_ERR,(TEXT("%s Pipe Special Case 2Test: Out of Memory"), szCEDeviceName));
        return FALSE;
    }

    if(bOutput == TRUE){
        DWORD dwValue = 1;
        for(int i = 0; i < TEST_DATA_SIZE; i++){
            DataBlock[i] = dwValue;
            dwValue ++;
        }
    }
    else{
        memset(DataBlock, 0, sizeof(DWORD)*TEST_DATA_SIZE);
    }

    DEBUGMSG(DBG_DETAIL,(TEXT("Create and Open Pipe")));
    LPCUSB_FUNCS lpUsbFuncs =pDriverPtr->lpUsbFuncs;
    USB_HANDLE usbHandle= pDriverPtr->GetUsbHandle();
    USB_PIPE hPipe=(*(lpUsbFuncs->lpOpenPipe))(usbHandle,&(lpPipePoint->Descriptor));
    if (hPipe==NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("Error at Open Pipe"));
        DEBUGMSG(DBG_ERR,(TEXT("Error at Open Pipe")));
        LocalFree(DataBlock);
        return FALSE;
    }

    // WORD dwDataSize=TEST_DATA_SIZE*sizeof(DWORD);


    if(bOutput == TRUE){
        USB_TDEVICE_REQUEST utdr = {0};
        utdr.bmRequestType = USB_REQUEST_CLASS;
        utdr.bRequest = TEST_REQUEST_DATALPBK;
        utdr.wLength = sizeof(USB_TDEVICE_DATALPBK);
        USB_TDEVICE_DATALPBK utdl = {0};
        utdl.uOutEP = lpPipePoint->Descriptor.bEndpointAddress;
        utdl.wBlockSize = TEST_DATA_SIZE*sizeof(DWORD);
        utdl.wNumofBlocks = 1;
        NKDbgPrintfW(_T("%s packetsize is %d, block size = %d, rounds = %d"), szCEDeviceName, lpPipePoint->Descriptor.wMaxPacketSize, utdl.wBlockSize, utdl.wNumofBlocks);

        //issue command to netchip2280
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utdl, NULL) == FALSE){
            g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
             LocalFree(DataBlock);
            return FALSE;
        }
    }

    DWORD oneBlockLength = TEST_DATA_SIZE;
    DEBUGMSG(DBG_DETAIL,(TEXT("%S Case 3 Call IssueBulkTransfer(%s)(%s) write addr@%lx, length=%lx, split into 4 transfers, with random delays"),
                                                szCEDeviceName,TEXT("Async"), bOutput?TEXT("Out Transfer"):TEXT("In Transfer"),
                                                DataBlock,oneBlockLength));

    BOOL fRet = TRUE;
    PBYTE pBuffer = (PBYTE)DataBlock;
    for(int jj = 0; jj < 4; jj++){
        (*(lpUsbFuncs->lpResetPipe))(hPipe);

        TransStatus aTransStatus = {0};
        aTransStatus.lpUsbFuncs=lpUsbFuncs;
        aTransStatus.hTransfer=0;
        aTransStatus.IsFinished=FALSE;
        aTransStatus.cEvent.ResetEvent();

        DWORD dwFlag = (bOutput == TRUE)?USB_OUT_TRANSFER:USB_IN_TRANSFER;
        dwFlag |= (USB_NO_WAIT | USB_SEND_TO_ENDPOINT);
        aTransStatus.hTransfer=(*lpUsbFuncs->lpIssueBulkTransfer)(
                                                                    hPipe,
                                                                    TransferNotify,
                                                                    &aTransStatus,
                                                                    dwFlag, //USB_OUT_TRANSFER|USB_SEND_TO_ENDPOINT|USB_NO_WAIT,
                                                                    oneBlockLength,
                                                                    pBuffer,
                                                                    NULL);

        ASSERT(aTransStatus.hTransfer);

        while(aTransStatus.IsFinished == FALSE)
            Sleep(10);

        if (!(*lpUsbFuncs->lpCloseTransfer)(aTransStatus.hTransfer)) { // failure
            g_pKato->Log(LOG_FAIL,TEXT("%s CloseTransfer Failure"), szCEDeviceName);
            fRet = FALSE;
            break;
        }
        //deliberately delay for a while
        UINT uiRand = 0;
        rand_s(&uiRand);
        ULONG sleeptime = uiRand % 500 + 500;
        Sleep(sleeptime);


        pBuffer += oneBlockLength;

    }

    if(bOutput == FALSE){//check data
        DWORD dwValue = 1;
        for(int i = 0; i < TEST_DATA_SIZE; i++){
            if(DataBlock[i] != dwValue){
                DEBUGMSG(DBG_ERR, (_T("%s At offset %d, expect value=0x%x, real value=0x%x"),
                                                        szCEDeviceName,i, dwValue, DataBlock[i]));
                fRet = FALSE;
                break;
            }
            dwValue ++;
        }
    }

    LocalFree(DataBlock);

   if ((*(lpUsbFuncs->lpClosePipe))(hPipe)==FALSE) {
        g_pKato->Log(LOG_FAIL, TEXT(" %s Error at Close Pipe"), szCEDeviceName);
        DEBUGMSG(DBG_ERR,(TEXT("%s Error at Close Pipe"), szCEDeviceName));
        fRet = FALSE;
    }

    return fRet;

}

//this test will close tranfers in a reverse order of when they issued
BOOL TransCase4TestCommand(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpPipePoint, BOOL bOutput, int iID)
{
    if(pDriverPtr == NULL || lpPipePoint == NULL)
        return FALSE;

    if(bOutput == FALSE)
        return TRUE;

    TCHAR  szCEDeviceName[SET_DEV_NAME_BUFF_SIZE] = {0};
    if(bOutput == TRUE){
        SetDeviceName(szCEDeviceName, (USHORT)iID, lpPipePoint, NULL);
    }
    else{
        SetDeviceName(szCEDeviceName, (USHORT)iID, NULL, lpPipePoint);
    }
    DEBUGMSG(DBG_FUNC,(TEXT("%s Start Transfer Special Case 4 Test"), szCEDeviceName));

    PDWORD DataBlock = NULL;//  [TEST_DATA_SIZE];
    DataBlock = (PDWORD)LocalAlloc(LPTR, TEST_DATA_SIZE*sizeof(DWORD));
    if(DataBlock == NULL){
        DEBUGMSG(DBG_ERR,(TEXT("%s Pipe Special Case 4 Test: Out of Memory"), szCEDeviceName));
        return FALSE;
    }


    DEBUGMSG(DBG_DETAIL,(TEXT("%s Create and Open Pipe"), szCEDeviceName));
    LPCUSB_FUNCS lpUsbFuncs =pDriverPtr->lpUsbFuncs;
    USB_HANDLE usbHandle= pDriverPtr->GetUsbHandle();
    USB_PIPE hPipe=(*(lpUsbFuncs->lpOpenPipe))(usbHandle,&(lpPipePoint->Descriptor));
    if (hPipe==NULL) {
        g_pKato->Log(LOG_FAIL, TEXT("%s Error at Open Pipe"), szCEDeviceName);
        DEBUGMSG(DBG_ERR,(TEXT("%s Error at Open Pipe"), szCEDeviceName));
        LocalFree(DataBlock);
        return FALSE;
    }

    // WORD dwDataSize=TEST_DATA_SIZE*sizeof(DWORD);

    DWORD oneBlockLength = TEST_DATA_SIZE;
    DEBUGMSG(DBG_DETAIL,(TEXT("%s Case 4 Call IssueBulkTransfer(%s)(%s) write addr@%lx, length=%lx, split into 4 transfers, with random delays"),
                                                szCEDeviceName, TEXT("Async"), bOutput?TEXT("Out Transfer"):TEXT("In Transfer"),
                                                DataBlock,oneBlockLength));

    BOOL bRet = TRUE;
    PBYTE pBuffer = (PBYTE)DataBlock;

    TransStatus aTransStatus[4];
    for(int jj = 0; jj < 4; jj++){
        (*(lpUsbFuncs->lpResetPipe))(hPipe);

        aTransStatus[jj].lpUsbFuncs=lpUsbFuncs;
        aTransStatus[jj].hTransfer=0;
        aTransStatus[jj].IsFinished=FALSE;
        aTransStatus[jj].cEvent.ResetEvent();

        DWORD dwFlag = (bOutput == TRUE)?USB_OUT_TRANSFER:USB_IN_TRANSFER;
        dwFlag |= (USB_NO_WAIT | USB_SEND_TO_ENDPOINT);
        aTransStatus[jj].hTransfer=(*lpUsbFuncs->lpIssueBulkTransfer)(
                                                                    hPipe,
                                                                    TransferNotify,
                                                                    &aTransStatus[jj],
                                                                    dwFlag, //USB_OUT_TRANSFER|USB_SEND_TO_ENDPOINT|USB_NO_WAIT,
                                                                    oneBlockLength,
                                                                    pBuffer,
                                                                    NULL);

        ASSERT(aTransStatus[jj].hTransfer);

        pBuffer += oneBlockLength;

    }

    for(int jj = 3; jj >=0; jj--){
        if (!(*lpUsbFuncs->lpCloseTransfer)(aTransStatus[jj].hTransfer)) { // failure
            g_pKato->Log(LOG_FAIL,TEXT("%s CloseTransfer Failure"), szCEDeviceName);
            bRet = FALSE;
        }
    }


    LocalFree(DataBlock);

   if ((*(lpUsbFuncs->lpClosePipe))(hPipe)==FALSE) {
        g_pKato->Log(LOG_FAIL, TEXT("%s Error at Close Pipe"), szCEDeviceName);
        DEBUGMSG(DBG_ERR,(TEXT("%s Error at Close Pipe"), szCEDeviceName));
        bRet = FALSE;
    }

    return bRet;

}


//this function issues transfers with deliberate device side stalls happening.
BOOL
TransWithDeviceSideStalls(UsbClientDrv *pDriverPtr, int iID, int iCaseID){

    if(pDriverPtr == NULL)
        return FALSE;

    DEBUGMSG(DBG_FUNC,(TEXT("+TransWithDeviceSideStalls\n")));

    UCHAR ucType = 0;
    TCHAR szTransType[8]= {0};
    switch(iCaseID){
        case 1:
            ucType = USB_ENDPOINT_TYPE_BULK;
            g_pKato->Log(LOG_DETAIL, _T("BULK transfer with device side stalls!"));
            wcsncpy_s(szTransType, _countof(szTransType), _T("BULK"), 4);
            break;
        case 2:
            ucType = USB_ENDPOINT_TYPE_INTERRUPT;
            g_pKato->Log(LOG_DETAIL, _T("INTR transfer with device side stalls!"));
            wcsncpy_s(szTransType, _countof(szTransType), _T("INTR"), 4);
            break;
        case 3:
            ucType = USB_ENDPOINT_TYPE_ISOCHRONOUS;
            g_pKato->Log(LOG_DETAIL, _T("ISOCH transfer with device side stalls!"));
            wcsncpy_s(szTransType, _countof(szTransType), _T("ISOCH"), 5);
            break;
        default:
            g_pKato->Log(LOG_FAIL, _T("Unknown transfer type!"));
            return FALSE;
    }

    LPCUSB_FUNCS lpUsbFuncs =pDriverPtr->lpUsbFuncs;
    USB_HANDLE usbHandle= pDriverPtr->GetUsbHandle();

    for(UCHAR uIndex = 0; uIndex < g_pTstDevLpbkInfo[iID]->uNumOfPipePairs; uIndex ++){
        if( g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucType != ucType)
             continue;

        //then retrieve the corresponding endpoint structures
        LPCUSB_ENDPOINT pInEP = NULL;
        LPCUSB_ENDPOINT pOutEP = NULL;
        for (UINT i = 0;i < pDriverPtr->GetNumOfEndPoints(); i++) {
            LPCUSB_ENDPOINT curPoint=pDriverPtr->GetDescriptorPoint(i);
            ASSERT(curPoint);
            if (curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucInEp) {
                pInEP = curPoint;
            }
            else if (curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucOutEp) {
                pOutEP = curPoint;
            }
        }
        if (!(pInEP && pOutEP)) {//one or both endpoints are missing
        g_pKato->Log(LOG_FAIL, TEXT("Can not find Pipe Pair"));
        return FALSE;
        }

        //open these two pipes
        USB_PIPE hOutPipe=(*(lpUsbFuncs->lpOpenPipe))(usbHandle,&(pOutEP->Descriptor));
        if(hOutPipe == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("Can not OutPipe with address 0x%x"), pOutEP->Descriptor.bEndpointAddress);
        return FALSE;
        }
        USB_PIPE hInPipe=(*(lpUsbFuncs->lpOpenPipe))(usbHandle,&(pInEP->Descriptor));
        if(hInPipe == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("Can not InPipe with address 0x%x"), pInEP->Descriptor.bEndpointAddress);
            (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
            return FALSE;
        }

        //reset these two pipes
        (*(lpUsbFuncs->lpResetPipe))(hOutPipe);
        (*(lpUsbFuncs->lpResetPipe))(hInPipe);

        g_pKato->Log(LOG_DETAIL, TEXT("start transfer with device side stall test on endpoint 0x%x(IN) and 0x%x(OUT) (%s)"),
                                            pInEP->Descriptor.bEndpointAddress, pOutEP->Descriptor.bEndpointAddress,
                                            g_szEPTypeStrings[pInEP->Descriptor.bmAttributes]);

        USB_TDEVICE_REQUEST utdr = {0};
        utdr.bmRequestType = USB_REQUEST_CLASS;
        utdr.bRequest = TEST_SPREQ_DATAPIPESTALL;
        utdr.wLength = sizeof(USB_TDEVICE_STALLDATAPIPE);
        USB_TDEVICE_STALLDATAPIPE utst = {0};
        utst.uOutEP = pOutEP->Descriptor.bEndpointAddress;
        utst.wBlockSize = pOutEP->Descriptor.wMaxPacketSize * 3;
        utst.uStallRepeat = 3;
        NKDbgPrintfW(_T("Packetsize is %d, block size = %d, stall repeats = %d"),utst.wBlockSize/3, utst.wBlockSize, utst.uStallRepeat);

        //issue command to USB function device
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utst, NULL) == FALSE){
            g_pKato->Log(LOG_FAIL, TEXT("Send vendor transfer failed!"));
            (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
            (*(lpUsbFuncs->lpClosePipe))(hInPipe);
            return FALSE;
        }

        Sleep(2000);  //make sure device side is ready

        //prepare the buffer
        PBYTE pBuf = new BYTE[utst.wBlockSize];
        if(pBuf == NULL){
            g_pKato->Log(LOG_FAIL, TEXT("OOM!"));
            (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
            (*(lpUsbFuncs->lpClosePipe))(hInPipe);
            return FALSE;
        }
        UCHAR uVal = 1;
        for(USHORT i = 0; i  < utst.wBlockSize; i++){
            pBuf[i] = uVal ++;
        }

        BOOL fRet = FALSE;
      //OUT transfers, they should all fail
        g_pKato->Log(LOG_DETAIL, TEXT("OUT transfer on stalled pipe, should fail!"));
        fRet = IssueSyncedTransfer(pDriverPtr, pOutEP, hOutPipe, utst.wBlockSize, pBuf, TRUE);
        if(fRet == TRUE){
            g_pKato->Log(LOG_FAIL, TEXT("OUT transfer is expected to be failed!"));
            delete[] pBuf;
            (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
            (*(lpUsbFuncs->lpClosePipe))(hInPipe);
            return FALSE;
        }
        //clear stall
        pDriverPtr->ClearFeature(NULL,NULL,USB_SEND_TO_ENDPOINT,USB_FEATURE_ENDPOINT_STALL, utst.uOutEP);
       (*(lpUsbFuncs->lpResetPipe))(hOutPipe);

        //this time it should go through
        Sleep(500);
        g_pKato->Log(LOG_DETAIL, TEXT("OUT transfer on cleared pipe, should successs!"));
        fRet = IssueSyncedTransfer(pDriverPtr, pOutEP, hOutPipe, utst.wBlockSize, pBuf, TRUE);
        if(fRet == FALSE){
            g_pKato->Log(LOG_FAIL, TEXT("OUT transfer failed!"));
            delete[] pBuf;
            (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
            (*(lpUsbFuncs->lpClosePipe))(hInPipe);
            return FALSE;
        }

      memset(pBuf, 0, sizeof(BYTE)*utst.wBlockSize);
        Sleep(3000);
      //IN transfers, they should all fail
        g_pKato->Log(LOG_DETAIL, TEXT("IN transfer on stalled pipe, should fail!"));
        fRet = IssueSyncedTransfer(pDriverPtr, pInEP, hInPipe, utst.wBlockSize, pBuf, FALSE);
        if(fRet == TRUE){
            g_pKato->Log(LOG_FAIL, TEXT("IN transfer is expected to be failed!"));
            delete[] pBuf;
            (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
            (*(lpUsbFuncs->lpClosePipe))(hInPipe);
            return FALSE;
        }
        //clear stall
        pDriverPtr->ClearFeature(NULL,NULL,USB_SEND_TO_ENDPOINT,USB_FEATURE_ENDPOINT_STALL, pInEP->Descriptor.bEndpointAddress);
        (*(lpUsbFuncs->lpResetPipe))(hInPipe);

        //this time it should go through
        Sleep(500);
        g_pKato->Log(LOG_DETAIL, TEXT("IN transfer on cleared pipe, should success !"));
        fRet = IssueSyncedTransfer(pDriverPtr, pInEP, hInPipe, utst.wBlockSize, pBuf, FALSE);
        if(fRet == FALSE){
            g_pKato->Log(LOG_FAIL, TEXT("IN transfer failed!"));
            delete[] pBuf;
            (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
            (*(lpUsbFuncs->lpClosePipe))(hInPipe);
            return FALSE;
        }

        //comparing data
        uVal = 1;
        for(USHORT i = 0; i < utst.wBlockSize; i ++){
            if(pBuf[i] != uVal){
                g_pKato->Log(LOG_FAIL, TEXT("Received data corrupted! at offset %d, expected value 0x%x, actuall value 0x%x"),
                                        i, uVal, pBuf[i]);
                delete[] pBuf;
                (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
                (*(lpUsbFuncs->lpClosePipe))(hInPipe);
                return FALSE;
            }
            uVal ++;
        }

        (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
        (*(lpUsbFuncs->lpClosePipe))(hInPipe);
        delete[] pBuf;
    }

    DEBUGMSG(DBG_FUNC,(TEXT("-TransWithDeviceSideStalls\n")));
    return TRUE;

}


//this function issues transfers with deliberate device side stalls happening.
BOOL
ShortTransferExercise(UsbClientDrv *pDriverPtr, int iID, int iCaseID){

    if(pDriverPtr == NULL)
        return FALSE;

    DEBUGMSG(DBG_FUNC,(TEXT("+ShortTransferExercise\n")));

    UCHAR ucType = 0;
    TCHAR szTransType[8]= {0};
    switch(iCaseID){
        case 1:
            ucType = USB_ENDPOINT_TYPE_BULK;
            g_pKato->Log(LOG_DETAIL, _T("BULK short transfers!"));
            wcsncpy_s(szTransType, _countof(szTransType), _T("BULK"), 4);
            break;
        case 2:
            ucType = USB_ENDPOINT_TYPE_INTERRUPT;
            g_pKato->Log(LOG_DETAIL, _T("INTR short transfer!"));
            wcsncpy_s(szTransType, _countof(szTransType), _T("INTR"), 4);
            break;
        default:
            g_pKato->Log(LOG_FAIL, _T("Unknown transfer type!"));
            return FALSE;
    }

    LPCUSB_FUNCS lpUsbFuncs =pDriverPtr->lpUsbFuncs;
    USB_HANDLE usbHandle= pDriverPtr->GetUsbHandle();

    const UCHAR ucRounds = 8;

    for(UCHAR uIndex = 0; uIndex < g_pTstDevLpbkInfo[iID]->uNumOfPipePairs; uIndex ++){
        if( g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucType != ucType)
             continue;

        //then retrieve the corresponding endpoint structures
        LPCUSB_ENDPOINT pInEP = NULL;
        LPCUSB_ENDPOINT pOutEP = NULL;
        for (UINT i = 0;i < pDriverPtr->GetNumOfEndPoints(); i++) {
            LPCUSB_ENDPOINT curPoint=pDriverPtr->GetDescriptorPoint(i);
            ASSERT(curPoint);
            if (curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucInEp) {
                pInEP = curPoint;
            }
            else if (curPoint->Descriptor.bEndpointAddress == g_pTstDevLpbkInfo[iID]->pLpbkPairs[uIndex].ucOutEp) {
                pOutEP = curPoint;
            }
        }
        if (!(pInEP && pOutEP)) {//one or both endpoints are missing
        g_pKato->Log(LOG_FAIL, TEXT("Can not find Pipe Pair"));
        return FALSE;
        }

        //open these two pipes
        USB_PIPE hOutPipe=(*(lpUsbFuncs->lpOpenPipe))(usbHandle,&(pOutEP->Descriptor));
        if(hOutPipe == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("Can not OutPipe with address 0x%x"), pOutEP->Descriptor.bEndpointAddress);
        return FALSE;
        }
        USB_PIPE hInPipe=(*(lpUsbFuncs->lpOpenPipe))(usbHandle,&(pInEP->Descriptor));
        if(hInPipe == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("Can not InPipe with address 0x%x"), pInEP->Descriptor.bEndpointAddress);
            (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
            return FALSE;
        }

        //reset these two pipes
        (*(lpUsbFuncs->lpResetPipe))(hOutPipe);
        (*(lpUsbFuncs->lpResetPipe))(hInPipe);

        g_pKato->Log(LOG_DETAIL, TEXT("start transfer with device side stall test on endpoint 0x%x(IN) and 0x%x(OUT) (%s)"),
                                            pInEP->Descriptor.bEndpointAddress, pOutEP->Descriptor.bEndpointAddress,
                                            g_szEPTypeStrings[pInEP->Descriptor.bmAttributes]);

        DWORD dwPacketSize = pOutEP->Descriptor.wMaxPacketSize;
        DWORD  dwSizeArr[] = {1, dwPacketSize-1, dwPacketSize, dwPacketSize+1, dwPacketSize*2-1,
                                            dwPacketSize*2, dwPacketSize*2+1, dwPacketSize*3-1};
        DWORD dwBufSize = dwPacketSize*12;

        //prepare the buffer
        PBYTE pOutBuf = new BYTE[dwBufSize+dwPacketSize*3];
        if(pOutBuf == NULL){
            g_pKato->Log(LOG_FAIL, TEXT("OOM!"));
            (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
            (*(lpUsbFuncs->lpClosePipe))(hInPipe);
            return FALSE;
        }
        UCHAR uVal = 1;
        for(DWORD  i = 0; i  < dwBufSize; i++){
            pOutBuf[i] = uVal ++;
        }
        PBYTE pInBuf = new BYTE[dwBufSize + dwPacketSize*3 ];
        if(pInBuf == NULL){
            g_pKato->Log(LOG_FAIL, TEXT("OOM!"));
            (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
            (*(lpUsbFuncs->lpClosePipe))(hInPipe);
            delete[] pOutBuf;
            return FALSE;
        }
        memset(pInBuf, 0, dwBufSize);

        USB_TDEVICE_REQUEST utdr = {0};
        utdr.bmRequestType = USB_REQUEST_CLASS;
        utdr.bRequest = TEST_SPREQ_DATAPIPESHORT;
        utdr.wLength = sizeof(USB_TDEVICE_DATALPBK);
        USB_TDEVICE_DATALPBK utdl = {0};
        utdl.uOutEP = pOutEP->Descriptor.bEndpointAddress;

        //issue command to USB function device
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utdl, NULL) == FALSE){
            g_pKato->Log(LOG_FAIL, TEXT("Send vendor transfer failed!"));
            (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
            (*(lpUsbFuncs->lpClosePipe))(hInPipe);
            delete[] pInBuf;
            delete[] pOutBuf;
            return FALSE;
        }

        BOOL fRet = FALSE;
      //OUT transfers
        g_pKato->Log(LOG_DETAIL, TEXT("send OUT transfers"));
        PBYTE pCurPoint = pOutBuf;
        for(UCHAR ucIndex = 0; ucIndex < ucRounds; ucIndex ++){
            fRet = IssueSyncedTransfer(pDriverPtr, pOutEP, hOutPipe, (USHORT)dwSizeArr[ucIndex], pCurPoint, TRUE);
            if(fRet == FALSE){
                g_pKato->Log(LOG_FAIL, TEXT("OUT transfer failed!"));
                (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
                (*(lpUsbFuncs->lpClosePipe))(hInPipe);
                delete[] pInBuf;
                delete[] pOutBuf;
                return FALSE;
            }
            if((dwSizeArr[ucIndex] % dwPacketSize) == 0){
                fRet = IssueSyncedTransfer(pDriverPtr, pOutEP, hOutPipe, 0, pCurPoint, TRUE);
                if(fRet == FALSE){
                    g_pKato->Log(LOG_FAIL, TEXT("OUT transfer(ZERO LENGTH) failed!"));
                    (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
                    (*(lpUsbFuncs->lpClosePipe))(hInPipe);
                    delete[] pInBuf;
                    delete[] pOutBuf;
                    return FALSE;
                }
            }
            g_pKato->Log(LOG_DETAIL, TEXT("%d bytes sent out!"), dwSizeArr[ucIndex]);
            pCurPoint += dwSizeArr[ucIndex];
        }

        Sleep(500);

      //IN transfers, all use short transfer
        g_pKato->Log(LOG_DETAIL, TEXT("send IN transfers"));
        pCurPoint = pInBuf;
        for(UCHAR ucIndex = 0; ucIndex < ucRounds; ucIndex ++){
            g_pKato->Log(LOG_DETAIL, TEXT("send IN transfers, expect to receive %d bytes"), dwSizeArr[ucIndex]);
            fRet = IssueSyncedShortTransfer(pDriverPtr, pInEP, hInPipe, (USHORT)dwPacketSize*3, (USHORT)dwSizeArr[ucIndex], pCurPoint, FALSE);
            if(fRet == FALSE){
                g_pKato->Log(LOG_FAIL, TEXT("IN transfer failed!"));
                (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
                (*(lpUsbFuncs->lpClosePipe))(hInPipe);
                delete[] pInBuf;
                delete[] pOutBuf;
                return FALSE;
            }
            g_pKato->Log(LOG_DETAIL, TEXT("%d bytes recieved!"), dwSizeArr[ucIndex]);
            pCurPoint += dwSizeArr[ucIndex];
        }

        //Check IN data
        uVal = 1;
        for(USHORT i = 0; i < dwBufSize; i ++){
            if(pInBuf[i] != uVal){
                g_pKato->Log(LOG_FAIL, TEXT("Received data corrupted! at offset %d, expected value 0x%x, actuall value 0x%x"),
                                        i, uVal, pInBuf[i]);
                (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
                (*(lpUsbFuncs->lpClosePipe))(hInPipe);
                delete[] pInBuf;
                delete[] pOutBuf;
                return FALSE;
            }
            uVal ++;
        }

        (*(lpUsbFuncs->lpClosePipe))(hOutPipe);
        (*(lpUsbFuncs->lpClosePipe))(hInPipe);
        delete[] pInBuf;
        delete[] pOutBuf;
    }

    DEBUGMSG(DBG_FUNC,(TEXT("-ShortTransferExercise\n")));
    return TRUE;

}



//isoch transfer test -- send/receive 4k data
BOOL
IssueSyncedTransfer(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpPipePoint, USB_PIPE hPipe,  USHORT usSize, PBYTE pBuf, BOOL fOUT)
{
    if(pDriverPtr == NULL || lpPipePoint == NULL || hPipe == NULL || pBuf == NULL)
        return FALSE;

    DEBUGMSG(DBG_FUNC,(TEXT("+IssueSyncedTransfer")));

    LPCUSB_FUNCS lpUsbFuncs =pDriverPtr->lpUsbFuncs;
    // USB_HANDLE usbHandle= pDriverPtr->GetUsbHandle();
    TransStatus aTransStatus;
    aTransStatus.lpUsbFuncs=lpUsbFuncs;
    aTransStatus.hTransfer=0;
    aTransStatus.IsFinished=FALSE;
    aTransStatus.cEvent.ResetEvent();
    BOOL fRet=TRUE;


    if(lpPipePoint->Descriptor.bmAttributes == USB_ENDPOINT_TYPE_ISOCHRONOUS){//isoch transfer
        WORD oneBlockLength=lpPipePoint->Descriptor.wMaxPacketSize;

        //setup length array
        DWORD numBlock=(usSize + oneBlockLength-1)/oneBlockLength;
        PDWORD BlockLength=new DWORD[numBlock];
        if(BlockLength == NULL){
            g_pKato->Log(LOG_FAIL, TEXT("%Out of Memory!"));
            return FALSE;
        }

        DWORD curDataSize=(DWORD)usSize;
        for (DWORD blockNumber=0;blockNumber<numBlock;blockNumber++){
            if (curDataSize>oneBlockLength)
                BlockLength[blockNumber]=oneBlockLength;
            else {
                BlockLength[blockNumber]=curDataSize;
                blockNumber++;
                break;
            }
        }

        // Get Transfer Status.
        PDWORD pCurLengths= new DWORD[blockNumber];
        PDWORD pCurErrors = new DWORD[blockNumber];
        ASSERT(pCurLengths);
        ASSERT(pCurErrors);

        DEBUGMSG(DBG_DETAIL,(TEXT("Call IssueIsochTransfer(%s)(%s) write addr@%lx, length=%lx,numOfBlock=%lx,firstBlockLength=%lx"),
                                            TEXT("Async"), fOUT?TEXT("Out Transfer"):TEXT("In Transfer"),
                                            pBuf,oneBlockLength, blockNumber,BlockLength[0]));

        aTransStatus.hTransfer=(*lpUsbFuncs->lpIssueIsochTransfer)(
                                                                        hPipe,
                                                                        TransferNotify,
                                                                        &aTransStatus,
                                                                        USB_START_ISOCH_ASAP|USB_SEND_TO_ENDPOINT|USB_NO_WAIT|(fOUT?USB_OUT_TRANSFER:USB_IN_TRANSFER),
                                                                        0,
                                                                        blockNumber,
                                                                        BlockLength,
                                                                        pBuf,
                                                                        NULL);

        ASSERT(aTransStatus.hTransfer);

        if (aTransStatus.cEvent.Lock(10000)!=TRUE || aTransStatus.IsFinished==FALSE) {// Wait for call back . 3 second timeout
            g_pKato->Log(LOG_FAIL, TEXT("Transfer not completed in 10 sec!!!"));
            fRet=FALSE;
        }
        if (!(*lpUsbFuncs->lpIsTransferComplete)(aTransStatus.hTransfer)) {
            g_pKato->Log(LOG_FAIL, TEXT("IsTransferComplete not return true after call back"));
            fRet=FALSE;
        }

        if (!(*lpUsbFuncs->lpGetIsochResults)(aTransStatus.hTransfer,blockNumber,pCurLengths,pCurErrors))  { // fail
            // BUG unknow reason failure.
             g_pKato->Log(LOG_FAIL, TEXT("Failure:GetIsochStatus return FALSE, before transfer complete, NumOfBlock(%lx)"), blockNumber);
             fRet = FALSE;
        }

        if (!(*lpUsbFuncs->lpCloseTransfer)(aTransStatus.hTransfer)) { // failure
            g_pKato->Log(LOG_FAIL,TEXT("CloseTransfer Failure"));
            fRet = FALSE;
        }

        delete [] pCurLengths;
        delete [] pCurErrors;
        delete[] BlockLength;

        return fRet;
    }

    //now dealing with BULK and INTR transfers


    DWORD dwFlag = (fOUT== TRUE)?USB_OUT_TRANSFER:USB_IN_TRANSFER;
    dwFlag |= (USB_NO_WAIT | USB_SEND_TO_ENDPOINT);

    if(lpPipePoint->Descriptor.bmAttributes == USB_ENDPOINT_TYPE_BULK){
        DEBUGMSG(DBG_DETAIL,(TEXT("Call IssueBulkTransfer(%s)(%s) write addr@%lx, length=%lx"),
                                    fOUT?TEXT("Out Transfer"):TEXT("In Transfer"), pBuf, usSize));
        aTransStatus.hTransfer=(*lpUsbFuncs->lpIssueBulkTransfer)(
                                                                    hPipe,
                                                                    TransferNotify,
                                                                    &aTransStatus,
                                                                    dwFlag,
                                                                    usSize,
                                                                    pBuf,
                                                                    NULL);

        ASSERT(aTransStatus.hTransfer);
    }
    else if(lpPipePoint->Descriptor.bmAttributes == USB_ENDPOINT_TYPE_INTERRUPT){
        DEBUGMSG(DBG_DETAIL,(TEXT("Call IssueInterruptTransfer(%s)(%s) write addr@%lx, length=%lx"),
                                    fOUT?TEXT("Out Transfer"):TEXT("In Transfer"), pBuf, usSize));
        aTransStatus.hTransfer=(*lpUsbFuncs->lpIssueInterruptTransfer)(
                                                                    hPipe,
                                                                    TransferNotify,
                                                                    &aTransStatus,
                                                                    dwFlag,
                                                                    usSize,
                                                                    pBuf,
                                                                    NULL);

        ASSERT(aTransStatus.hTransfer);
    }
    else{//this should not happen
        g_pKato->Log(LOG_FAIL, TEXT("Unknown type of transfer!"));
        return FALSE;
    }

    if (aTransStatus.cEvent.Lock(10000)!=TRUE || aTransStatus.IsFinished==FALSE) {// Wait for call back . 3 second timeout
        g_pKato->Log(LOG_FAIL, TEXT("Transfer not completed in 10 sec!!!"));
        fRet=FALSE;
    }
    if (!(*lpUsbFuncs->lpIsTransferComplete)(aTransStatus.hTransfer)) {
        g_pKato->Log(LOG_FAIL, TEXT("IsTransferComplete not return true after call back"));
        fRet=FALSE;
    }

    //get status
    DWORD dwBytesTransferred = 0;
    DWORD dwError = 0;
    if (!(*lpUsbFuncs->lpGetTransferStatus)(aTransStatus.hTransfer, &dwBytesTransferred, &dwError))  { // fail
         g_pKato->Log(LOG_FAIL, TEXT("Failed:GetTransferStatus return FALSE, bytes returned = %d, dwError= 0x%x"),
                                                        dwBytesTransferred, dwError);
         fRet = FALSE;
    }
    else if(dwError != USB_NO_ERROR || dwBytesTransferred != usSize) {
         g_pKato->Log(LOG_FAIL, TEXT("Failed:GetTransferStatus return FALSE, bytes returned = %d, dwError= 0x%x"),
                                                        dwBytesTransferred, dwError);
         fRet = FALSE;
    }
    else{
         g_pKato->Log(LOG_DETAIL, TEXT("Succeded:GetTransferStatus, bytes returned = %d, dwError= 0x%x"),
                                                        dwBytesTransferred, dwError);
    }

    if (!(*lpUsbFuncs->lpCloseTransfer)(aTransStatus.hTransfer)) { // failure
        g_pKato->Log(LOG_FAIL,TEXT("CloseTransfer Failure"));
        fRet = FALSE;
    }

    return fRet;


}


//specific for short transfer on BULK and INTR pipes
BOOL
IssueSyncedShortTransfer(UsbClientDrv *pDriverPtr, LPCUSB_ENDPOINT lpPipePoint, USB_PIPE hPipe,
                      USHORT usIssueSize, USHORT usExpRetSize, PBYTE pBuf, BOOL fOUT)
{
    if(pDriverPtr == NULL || lpPipePoint == NULL || hPipe == NULL || usIssueSize == 0 || pBuf == NULL)
        return FALSE;

    DEBUGMSG(DBG_FUNC,(TEXT("+IssueSyncedShortTransfer")));

    LPCUSB_FUNCS lpUsbFuncs =pDriverPtr->lpUsbFuncs;
    // USB_HANDLE usbHandle= pDriverPtr->GetUsbHandle();
    TransStatus aTransStatus;
    aTransStatus.lpUsbFuncs=lpUsbFuncs;
    aTransStatus.hTransfer=0;
    aTransStatus.IsFinished=FALSE;
    aTransStatus.cEvent.ResetEvent();
    BOOL fRet=TRUE;


    DWORD dwFlag = (fOUT== TRUE)?USB_OUT_TRANSFER:USB_IN_TRANSFER;
    dwFlag |= (USB_NO_WAIT | USB_SEND_TO_ENDPOINT | USB_SHORT_TRANSFER_OK);

    if(lpPipePoint->Descriptor.bmAttributes == USB_ENDPOINT_TYPE_BULK){
        DEBUGMSG(DBG_DETAIL,(TEXT("Call IssueBulkTransfer(%s)(%s) write addr@%lx, length=%lx"),
                                    fOUT?TEXT("Out Transfer"):TEXT("In Transfer"), pBuf, usIssueSize));
        aTransStatus.hTransfer=(*lpUsbFuncs->lpIssueBulkTransfer)(
                                                                    hPipe,
                                                                    TransferNotify,
                                                                    &aTransStatus,
                                                                    dwFlag,
                                                                    usIssueSize,
                                                                    pBuf,
                                                                    NULL);

        ASSERT(aTransStatus.hTransfer);
    }
    else if(lpPipePoint->Descriptor.bmAttributes == USB_ENDPOINT_TYPE_INTERRUPT){
        DEBUGMSG(DBG_DETAIL,(TEXT("Call IssueInterruptTransfer(%s)(%s) write addr@%lx, length=%lx"),
                                    fOUT?TEXT("Out Transfer"):TEXT("In Transfer"), pBuf, usIssueSize));
        aTransStatus.hTransfer=(*lpUsbFuncs->lpIssueInterruptTransfer)(
                                                                    hPipe,
                                                                    TransferNotify,
                                                                    &aTransStatus,
                                                                    dwFlag,
                                                                    usIssueSize,
                                                                    pBuf,
                                                                    NULL);

        ASSERT(aTransStatus.hTransfer);
    }
    else{//this should not happen
        g_pKato->Log(LOG_FAIL, TEXT("Unknown type of transfer!"));
        return FALSE;
    }

    if (aTransStatus.cEvent.Lock(10000)!=TRUE ) {// Wait for call back . 3 second timeout
        g_pKato->Log(LOG_FAIL, TEXT("Transfer not completed in 10 sec!!!"));
        fRet=FALSE;
    }
    if(aTransStatus.IsFinished==FALSE){
        g_pKato->Log(LOG_FAIL, TEXT("Transfer is not marked as finished!!!"));
        fRet=FALSE;
    }

    if (!(*lpUsbFuncs->lpIsTransferComplete)(aTransStatus.hTransfer)) {
        g_pKato->Log(LOG_FAIL, TEXT("IsTransferComplete not return true after call back"));
        fRet=FALSE;
    }

    //get status
    DWORD dwBytesTransferred = 0;
    DWORD dwError = 0;
    if (!(*lpUsbFuncs->lpGetTransferStatus)(aTransStatus.hTransfer, &dwBytesTransferred, &dwError))  { // fail
         g_pKato->Log(LOG_FAIL, TEXT("Failed:GetTransferStatus return FALSE, bytes returned = %d, dwError= 0x%x"),
                                                        dwBytesTransferred, dwError);
         fRet = FALSE;
    }
    else if(dwError != USB_NO_ERROR || dwBytesTransferred != (DWORD)usExpRetSize) {
         g_pKato->Log(LOG_FAIL, TEXT("Failed:GetTransferStatus return FALSE, bytes returned = %d, expected bytes return = %d,  dwError= 0x%x"),
                                                        dwBytesTransferred, usExpRetSize, dwError);
         fRet = FALSE;
    }
    else{
         g_pKato->Log(LOG_DETAIL, TEXT("Succeded:GetTransferStatus, bytes returned = %d, dwError= 0x%x"),
                                                        dwBytesTransferred, dwError);
    }

    if (!(*lpUsbFuncs->lpCloseTransfer)(aTransStatus.hTransfer)) { // failure
        g_pKato->Log(LOG_FAIL,TEXT("CloseTransfer Failure"));
        fRet = FALSE;
    }

    return fRet;


}

