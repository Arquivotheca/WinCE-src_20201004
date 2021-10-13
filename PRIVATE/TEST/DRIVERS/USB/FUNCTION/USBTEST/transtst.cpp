//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
				if (curDataSize>oneBlockLength) 
					pBlockLength[blockNumber]=oneBlockLength;
				else {
					pBlockLength[blockNumber]=curDataSize;
					blockNumber++;
					break;
				};
			DEBUGMSG(DBG_FUNC,(TEXT("IssueIsochTransfer(%s)(%s), BlockNumber(%lx), BlockSize(%lx), BufferAddr(%lx), BufferSize(%lx)"),
					aSyncMode?TEXT("Async"):TEXT("Sync"),
					isInPipe()?TEXT("Read"):TEXT("Write"),
					blockNumber,oneBlockLength,
					lpDataBuffer,dwDataSize));
			bTransfer.ReInitial(0);
			hTransfer=IssueIsochTransfer(&bTransfer, 0, blockNumber,
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
		if(dwBlockLength == 0)
		    return TRUE;
		DEBUGMSG(DBG_DETAIL,(TEXT("TransferTread::CheckPostTransfer check for Isoch BlockLength(%lx)"),dwBlockLength));
		if(dwBlockLength > 0x100000)
		    return TRUE;
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
				if (curDataSize>oneBlockLength) 
					pBlockLength[blockNumber]=oneBlockLength;
				else {
					pBlockLength[blockNumber]=curDataSize;
					blockNumber++;
					break;
				};
			DEBUGMSG(DBG_FUNC,(TEXT("IssueIsochTransfer(%s)(%s), BlockNumber(%lx), BlockSize(%lx), BufferAddr(%lx), BufferSize(%lx)"),
					aSyncMode?TEXT("Async"):TEXT("Sync"),
					isInPipe()?TEXT("Read"):TEXT("Write"),
					blockNumber,oneBlockLength,
					lpDataBuffer,dwDataSize));
			aTransfer.ReInitial(0);
			IssueIsochTransfer(&aTransfer,0,blockNumber,
				pBlockLength,lpDataBuffer,NULL);
			TransferCheck(aTransfer,
					blockNumber,pBlockLength);
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
//	Sleep(10);
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

    TCHAR  szCEDeviceName[64] = {0};
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
    DWORD dwPattern=Random();
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
            InPipe=	new CloseTransferThread(pDriverPtr,&(pInEP->Descriptor),(PVOID)pInDataBlock,TEST_DATA_SIZE*sizeof(DWORD));
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

    if(dwObjectNumber != 2){//abort test does not need to send command to function side
        //issue command to netchip2280
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utdl) == FALSE){
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

        g_pKato->Log(LOG_DETAIL, TEXT("start spcial transfer test on endpoint 0x%x(IN) and 0x%x(OUT) (%s)"), 
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
           // return FALSE;
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
           // return FALSE;
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

    TCHAR  szCEDeviceName[64] = {0};
    if(bOutput == TRUE){
        SetDeviceName(szCEDeviceName, (USHORT)iID, lpPipePoint, NULL);
    }
    else{
        SetDeviceName(szCEDeviceName, (USHORT)iID, NULL, lpPipePoint);
    }
    
    DEBUGMSG(DBG_FUNC,(TEXT("%s Start Trans Special Case 1 Test"), szCEDeviceName));

    PDWORD DataBlock = NULL;//  [TEST_DATA_SIZE];
    DataBlock = (PDWORD)LocalAlloc(LPTR, TEST_DATA_SIZE*sizeof(DWORD));
    if(DataBlock == NULL){
        DEBUGMSG(DBG_ERR,(TEXT("%s Pipe Special Case 1 Test: Out of Memory"), szCEDeviceName));
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
        LocalFree(DataBlock);
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
        LocalFree(DataBlock);
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
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utdl) == FALSE){
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
        // .
         g_pKato->Log(LOG_FAIL, TEXT("%s Failure:GetIsochStatus return FALSE, before transfer complete, NumOfBlock(%lx)"), szCEDeviceName, blockNumber);
         rtValue=FALSE;
    };
    DEBUGMSG(DBG_DETAIL,(TEXT("%s After GetIsochResults"), szCEDeviceName));
    delete [] pCurLengths;
    delete [] pCurErrors;

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

    TCHAR  szCEDeviceName[64] = {0};
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

    WORD dwDataSize=TEST_DATA_SIZE*sizeof(DWORD);
    WORD oneBlockLength=lpPipePoint->Descriptor.wMaxPacketSize;

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
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utdl) == FALSE){
            g_pKato->Log(LOG_FAIL, TEXT("%s Send vendor transfer failed!"), szCEDeviceName);
             LocalFree(DataBlock);
            return FALSE;
        }
    }

    DEBUGMSG(DBG_DETAIL,(TEXT("%s Case 2 Call IssueBulkTransfer(%s)(%s) write addr@%lx, length=%lx"), szCEDeviceName,
                                                TEXT("Async"), bOutput?TEXT("Out Transfer"):TEXT("In Transfer"),
                                                DataBlock,oneBlockLength));

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

    TCHAR  szCEDeviceName[64] = {0};
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

    WORD dwDataSize=TEST_DATA_SIZE*sizeof(DWORD);


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
        if(SendVendorTransfer(pDriverPtr, FALSE, &utdr, (LPVOID)&utdl) == FALSE){
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
        ULONG sleeptime = rand() % 500 + 500;
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
        
    TCHAR  szCEDeviceName[64] = {0};
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

    WORD dwDataSize=TEST_DATA_SIZE*sizeof(DWORD);

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


