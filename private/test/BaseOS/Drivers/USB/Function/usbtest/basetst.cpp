//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//******************************************************************************
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************
/*++

Module Name:  

BaseTst.cpp

Abstract:  
    USB Driver Default Function Test .

Functions:

Notes: 

--*/

#define __THIS_FILE__   TEXT("BaseTst.cpp")

#include <windows.h>
#include "usbtest.h"
#include "BaseTst.h"


DWORD WINAPI UsbFuncTest::TransferNotify(LPVOID lpvNotifyParameter) 
{
    DEBUGMSG(DBG_FUNC,(TEXT("UsbFunctTest::TransferNotify(0x%lx)\n"),lpvNotifyParameter));
    return ((UsbFuncTest *)lpvNotifyParameter)->FunctionComplete();
};

BOOL UsbFuncTest::CheckResult(BOOL bSuccess,USB_HANDLE usbTransfer)
{
    DEBUGMSG(DBG_FUNC,(TEXT("CheckResult\n")));
    if (!usbTransfer) {
        if (bSuccess) 
            g_pKato->Log(LOG_FAIL, TEXT("Fail at Transfer,return zero handle"));
        SetErrorCode(ERROR_USB_TRANSFER);
        DEBUGMSG(DBG_DETAIL,(TEXT("CheckResult NULL handle return FALSE")));
        return FALSE;
    };

    UsbTransfer aTransfer(usbTransfer,usbDriver->lpUsbFuncs);
    if (fAsync && WaitForComplete(USB_TIME_OUT)==FALSE) { // time out
        if (bSuccess)
            g_pKato->Log(LOG_FAIL, TEXT("Fail at Transfer,Time Out"));
        SetErrorCode(ERROR_USB_TRANSFER);
        DEBUGMSG(DBG_FUNC,(TEXT("CheckResult Time out return TRUE")));
        return FALSE;
    }
    else
    if (!aTransfer.IsTransferComplete ()) { // somthing wrong
        if (bSuccess) {
            DWORD dwError;
            DWORD dwByteTransfer;
            DWORD rtError;
            rtError=(DWORD)aTransfer.GetTransferStatus(&dwByteTransfer,&dwError);
            g_pKato->Log(LOG_FAIL, TEXT("Fail at Transfer,Error(%ld),TransferedByte(%ld),Return(%ld)"),dwError,dwByteTransfer,rtError);
        };
        SetErrorCode(ERROR_USB_TRANSFER);
        DEBUGMSG(DBG_FUNC,(TEXT("CheckResult transfer not complete return FALSE\n")));
        return FALSE;
    }
    else {
        DWORD dwError;
        DWORD dwByteTransfer;
        DWORD rtError;
        rtError=(DWORD)aTransfer.GetTransferStatus(&dwByteTransfer,&dwError);
        DEBUGMSG(DBG_DETAIL,(TEXT("GetTrasferStatus Return (%ld,%ld)\n"),dwByteTransfer,dwError));
        if (dwError!=USB_NO_ERROR) {
            if (bSuccess )
                g_pKato->Log(LOG_FAIL, TEXT("Fail at Transfer,Error(%ld),TransferedByte(%ld),Return(%ld)"),dwError,dwByteTransfer,rtError);
            SetErrorCode(ERROR_USB_TRANSFER);
            DEBUGMSG(DBG_FUNC,(TEXT("CheckResult transfer complete with error(%ld) return FALSE\n"),dwError));
            return FALSE;
        };
    };
    DEBUGMSG(DBG_FUNC,(TEXT("CheckResult return TRUE\n")));
    return TRUE;
};

class GetDespTest: public UsbFuncTest {
public:
    GetDespTest(UsbClientDrv *pDriverPtr,BOOL Async,UCHAR bType,UCHAR bIndex,WORD wLanguage,
            WORD wLength,LPVOID lpvBuffer,BOOL bSuccess);
};

GetDespTest::GetDespTest(UsbClientDrv *pDriverPtr,BOOL Async,UCHAR bType,UCHAR bIndex,WORD wLanguage,
            WORD wLength,LPVOID lpvBuffer,BOOL bSuccess) :
    UsbFuncTest(pDriverPtr,Async)
{
    DEBUGMSG(DBG_DETAIL,(TEXT("GetDespTest")));
       if(lpvBuffer == NULL || wLength > 0x9000)
            return;
        
       // Zero out buffer, just in case
       memset( lpvBuffer, 0, wLength );

    USB_STRING_DESCRIPTOR aDescriptor;

    hTransfer=usbDriver->GetDescriptor(TransferNotify,(PVOID)this,(fAsync?USB_NO_WAIT:0),
            bType,bIndex,wLanguage,2,&aDescriptor);
    if (!hTransfer) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail GetDescriptor(Get Length) return NULL transfer handle"));
        SetErrorCode(ERROR_USB_TRANSFER);
    }
    else
    if (CheckResult(bSuccess,hTransfer)) {
        DEBUGMSG(DBG_DETAIL,(TEXT(" Descriptor Length (%ld),Type(%ld)"),
                (DWORD)aDescriptor.bLength,(DWORD)aDescriptor.bDescriptorType));
        if (aDescriptor.bDescriptorType!=bType) {
            g_pKato->Log(LOG_FAIL, TEXT("Fail GetDescriptor ,Wrong type request(%ld),Get(%ld)"),
                    (DWORD)bType,(DWORD)aDescriptor.bDescriptorType);
            SetErrorCode(ERROR_USB_TRANSFER);
        }
        else
        if (!aDescriptor.bLength) {
            memcpy(lpvBuffer,&aDescriptor,2);
        }
        else {
            wLength=min(wLength,aDescriptor.bLength);
            hTransfer=usbDriver->GetDescriptor(TransferNotify,(PVOID)this,(fAsync?USB_NO_WAIT:0),
                    bType,bIndex,wLanguage,wLength,lpvBuffer);
            if (!hTransfer) {
                g_pKato->Log(LOG_FAIL, TEXT("Fail GetDescriptor return NULL transfer handle"));
                SetErrorCode(ERROR_USB_TRANSFER);
            }
            else
                CheckResult(bSuccess,hTransfer);
        };
    };
    BOOL m_bHalted=FALSE;
    if (!usbDriver->IsDefaultPipeHalted(&m_bHalted)) {
        g_pKato->Log(LOG_FAIL,TEXT("IsDefaultPipeHalted return error"));
        SetErrorCode(ERROR_USB_TRANSFER);
    }
    else {
        if (m_bHalted) {
            if (!usbDriver->ResetDefaultPipe()) {
                g_pKato->Log(LOG_FAIL,TEXT("ResetDefaultPipe return Error!"));
                SetErrorCode(ERROR_USB_TRANSFER);
            };
        }
    };
};

class SetDespTest: public UsbFuncTest {
public:
    SetDespTest(UsbClientDrv *pDriverPtr,BOOL Async,UCHAR bType,UCHAR bIndex,WORD wLanguage,
            WORD wLength,LPVOID lpvBuffer,BOOL bSuccess);
};

class SetConfigTest: public UsbFuncTest {
public:
    SetConfigTest(UsbClientDrv *pDriverPtr,BOOL Async,UCHAR bType,UCHAR bIndex,WORD wLanguage,
            WORD wLength,LPVOID lpvBuffer,BOOL bSuccess);
};


SetDespTest::SetDespTest(UsbClientDrv *pDriverPtr,BOOL Async,UCHAR bType,UCHAR bIndex,WORD wLanguage,
            WORD wLength,LPVOID lpvBuffer,BOOL bSuccess) :
    UsbFuncTest(pDriverPtr,Async)
{
    DEBUGMSG(DBG_FUNC,(TEXT("SetDespTest with TransferNotify(0x%lx)\n"),this));
    hTransfer=
        usbDriver->SetDescriptor(TransferNotify,this,fAsync?USB_NO_WAIT:0,
            bType,bIndex,wLanguage,wLength,lpvBuffer);
    if (!hTransfer) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail GetDescriptor return NULL transfer handle"));
        SetErrorCode(ERROR_USB_TRANSFER);
    }
    else
        CheckResult(bSuccess,hTransfer);
};
    
SetConfigTest::SetConfigTest(UsbClientDrv *pDriverPtr,BOOL Async,UCHAR bType,UCHAR bIndex,WORD wLanguage,
            WORD wLength,LPVOID lpvBuffer,BOOL bSuccess) :
    UsbFuncTest(pDriverPtr,Async)
{
    DEBUGMSG(DBG_FUNC,(TEXT("SetDespTest with TransferNotify(0x%lx)\n"),this));
    hTransfer=
        usbDriver->SetConfig(TransferNotify,this,fAsync?USB_NO_WAIT:0,
            bType,bIndex,wLanguage,wLength,lpvBuffer);
    if (!hTransfer) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail GetDescriptor return NULL transfer handle"));
        SetErrorCode(ERROR_USB_TRANSFER);
    }
    else
        CheckResult(bSuccess,hTransfer);
};
BOOL GetCurrentDescriptor(UsbClientDrv *pDriverPtr,BOOL aSync,UCHAR bType,UCHAR bIndex,
                          WORD wLanguage,BOOL bSuccess)
{
    PUCHAR rBuffer = NULL; // 4 k;
    WORD bLength=0x1000;
    rBuffer = (PUCHAR)LocalAlloc(LPTR, 0x1000*sizeof(UCHAR));
    if(rBuffer == NULL){
        g_pKato->Log(LOG_FAIL, (TEXT("GetCurrentDescriptor: Out of Memory")));
        return FALSE;
    }

    DEBUGMSG(DBG_DETAIL,(TEXT("GetCurrentDescriptor (%s),bType(%lx),bIndex(%lx),wLanguage(%lx), should %s "),
        aSync?TEXT("Async"):TEXT("Sync"),
        (DWORD)bType,(DWORD)bIndex,(DWORD)wLanguage,
        bSuccess?TEXT("Success"):TEXT("Failure")));
    GetDespTest* pTest= new GetDespTest(pDriverPtr,aSync,bType,bIndex,
            wLanguage,bLength,rBuffer,bSuccess);
    if(pTest == NULL){
        g_pKato->Log(LOG_FAIL, (TEXT("GetCurrentDescriptor: Out of Memory")));
        LocalFree(rBuffer);
        return FALSE;
    }

    if ((pTest->GetErrorCode()==0) ^ bSuccess) { //Not match
        if (bSuccess)
            g_pKato->Log(LOG_FAIL, TEXT("Fail at GetDescriptor,DEVICE_DESCRIPTOR Error(%ld)"),pTest->GetErrorCode());
        else
            g_pKato->Log(LOG_FAIL, TEXT("Fail at GetDescriptor,Unexpected Success return"));
        delete pTest;
        LocalFree(rBuffer);
        return FALSE;
    } 
    else 
    if (pTest->GetErrorCode()==0) {
        PUSB_DEVICE_DESCRIPTOR pDeviceDesc=(PUSB_DEVICE_DESCRIPTOR)rBuffer;
        if (pDeviceDesc->bDescriptorType!=bType) {
            g_pKato->Log(LOG_FAIL, TEXT("Fail at GetDescriptor,DEVICE_DESCRIPTOR,Wrong Type" ));
            delete pTest;
            LocalFree(rBuffer);
            return FALSE;
        };
        if (bType==USB_STRING_DESCRIPTOR_TYPE) {
            TCHAR tString[0x100];
            if (pTest->usbDriver->TranslateStringDesc((LPCUSB_STRING_DESCRIPTOR)pDeviceDesc,tString,0x100))
                g_pKato->Log(LOG_COMMENT, TEXT("String Descr is \"%s\"" ),tString);
            else {
                g_pKato->Log(LOG_FAIL, TEXT("TranslateStringDescr" ));
                delete pTest;
                LocalFree(rBuffer);
                return FALSE;
            };
        }

    };
    g_pKato->Log(LOG_COMMENT, TEXT("GetCurrentDescriptor return success"));
    delete pTest;
    LocalFree(rBuffer);
    return TRUE;
}

BOOL SetConfigDescriptor(UsbClientDrv * pDriverPtr,UCHAR bConfigIndex )
{

    PUCHAR rBuffer = NULL; 
    UCHAR bLength =0;

     SetConfigTest* pTest = new SetConfigTest(pDriverPtr,TRUE,USB_CONFIGURATION_DESCRIPTOR_TYPE, bConfigIndex,0,
             bLength, rBuffer,TRUE);
    if(pTest == NULL)
    {
         g_pKato->Log(LOG_FAIL, (TEXT("SetConfigDescriptor: Out of Memory")));
    LocalFree(rBuffer);
    return FALSE;
    }
     if ((pTest->GetErrorCode()==0) ^ TRUE)
     { //Not match
#if TRUE
            g_pKato->Log(LOG_FAIL, TEXT("Fail at SetConfigDescriptor,USB_CONFIGURATION_DESCRIPTOR_TYPE Error(%ld)"),pTest->GetErrorCode());
#else
            g_pKato->Log(LOG_FAIL, TEXT("Fail at SetConfigDescriptor,Unexpected Success return"));
#endif
        delete pTest;
        LocalFree(rBuffer);
        return FALSE;
    } 

    return TRUE ;

}

BOOL GetDespscriptorTest(UsbClientDrv * pDriverPtr,BOOL aSync)
{
    LPCUSB_DEVICE pDevice=pDriverPtr->GetDeviceInfo();

    if (!GetCurrentDescriptor(pDriverPtr,aSync,USB_STRING_DESCRIPTOR_TYPE,0,0,TRUE))
        return FALSE;
    
    UCHAR uNumofConfigs = pDevice->Descriptor.bNumConfigurations;

    DEBUGMSG(DBG_DETAIL,(TEXT("Number of Configuration %d "),pDevice->Descriptor.bNumConfigurations));
    for (UCHAR uIndex=0;uIndex<pDevice->Descriptor.bNumConfigurations;uIndex++) {
        if (!GetCurrentDescriptor(pDriverPtr,aSync,USB_CONFIGURATION_DESCRIPTOR_TYPE,uIndex,0,TRUE))
            return FALSE;
    };
    if (!GetCurrentDescriptor(pDriverPtr,aSync,USB_DEVICE_DESCRIPTOR_TYPE,0,0,TRUE))
        return FALSE;

    if (!GetCurrentDescriptor(pDriverPtr,aSync,USB_DEVICE_DESCRIPTOR_TYPE,1,0,FALSE))
        return FALSE;
    if (!GetCurrentDescriptor(pDriverPtr,aSync,USB_CONFIGURATION_DESCRIPTOR_TYPE,uNumofConfigs+1,0,FALSE))
        return FALSE;
    if (!GetCurrentDescriptor(pDriverPtr,aSync,USB_STRING_DESCRIPTOR_TYPE,1,0,FALSE))
        return FALSE;

    if (!GetCurrentDescriptor(pDriverPtr,aSync,USB_DEVICE_DESCRIPTOR_TYPE,1,1,FALSE))
        return FALSE;
    if (!GetCurrentDescriptor(pDriverPtr,aSync,USB_CONFIGURATION_DESCRIPTOR_TYPE,uNumofConfigs+2,1,FALSE))
        return FALSE;
    if (!GetCurrentDescriptor(pDriverPtr,aSync,USB_STRING_DESCRIPTOR_TYPE,1,0x0409,TRUE))
        return FALSE;
    if (!GetCurrentDescriptor(pDriverPtr,aSync,USB_STRING_DESCRIPTOR_TYPE,2,0x0409, TRUE))
        return FALSE;
    if (!GetCurrentDescriptor(pDriverPtr,aSync,USB_STRING_DESCRIPTOR_TYPE,3,0x0409,TRUE))
        return FALSE;
    return TRUE;
};






class GetInterfaceTest: public UsbFuncTest {
public:
    GetInterfaceTest(UsbClientDrv *pDriverPtr,BOOL Async,UCHAR bInterfaceNumber,PUCHAR lprAlternateSetting,BOOL bSuccess);
};

GetInterfaceTest::GetInterfaceTest(UsbClientDrv *pDriverPtr,BOOL Async,UCHAR bInterfaceNumber,PUCHAR lprAlternateSetting,BOOL bSuccess) :
    UsbFuncTest(pDriverPtr,Async)
{
    DEBUGMSG(DBG_FUNC,(TEXT("+GetInterfaceTest::GetInterfaceTest(%ld)\n"),Async));

    DEBUGMSG(DBG_DETAIL,(TEXT("GetInterfaceTest with TransferNotify(0x%lx)\n"),this));
    hTransfer=
        usbDriver->GetInterface(TransferNotify,this,fAsync?USB_NO_WAIT:0,
            bInterfaceNumber,lprAlternateSetting);
    if (!hTransfer) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail GetDescriptor return NULL transfer handle"));
        SetErrorCode(ERROR_USB_TRANSFER);
    }
    else
        CheckResult(bSuccess,hTransfer);
    DEBUGMSG(DBG_FUNC,(TEXT("-GetInterfaceTest::GetInterfaceTest\n")));
};

class SetInterfaceTest: public UsbFuncTest {
public:
    SetInterfaceTest(UsbClientDrv *pDriverPtr,BOOL Async,UCHAR bInterfaceNumber,UCHAR bAlternateSetting,BOOL bSuccess);
};

SetInterfaceTest::SetInterfaceTest(UsbClientDrv *pDriverPtr,BOOL Async,UCHAR bInterfaceNumber,UCHAR bAlternateSetting,BOOL bSuccess) :
    UsbFuncTest(pDriverPtr,Async)
{
    hTransfer=
        usbDriver->SetInterface(TransferNotify,this,fAsync?USB_NO_WAIT:0,
            bInterfaceNumber,bAlternateSetting);
    if (!hTransfer) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail SetDescriptor return NULL transfer handle"));
        SetErrorCode(ERROR_USB_TRANSFER);
    }
    else
        CheckResult(bSuccess,hTransfer);
};

class FeatureSetting: public UsbFuncTest {
public:
    FeatureSetting(UsbClientDrv *pDriverPtr,BOOL bSet,BOOL Async,
        DWORD dwFlag,WORD wFeature,UCHAR bIndex,BOOL bSuccess);
};

FeatureSetting::FeatureSetting(UsbClientDrv *pDriverPtr,BOOL bSet,BOOL Async,
                               DWORD dwFlag,WORD wFeature,UCHAR bIndex,BOOL bSuccess) :
    UsbFuncTest(pDriverPtr,Async)
{
    if (fAsync)
        dwFlag |=USB_NO_WAIT;
    else
        dwFlag &=~USB_NO_WAIT;

    hTransfer=
        (bSet?
        usbDriver->SetFeature(TransferNotify,this,dwFlag,wFeature,bIndex)
            :
        usbDriver->ClearFeature(TransferNotify,this,dwFlag,wFeature,bIndex)
            );

    if (!hTransfer) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail SetDescriptor return NULL transfer handle"));
        SetErrorCode(ERROR_USB_TRANSFER);
    }
    else
        CheckResult(bSuccess,hTransfer);
};
class GetStatus: public UsbFuncTest {
public:
    GetStatus(UsbClientDrv *pDriverPtr,BOOL Async,UCHAR bIndex,DWORD dwFlag,
        LPWORD lpwStatus,BOOL bSuccess);
};

GetStatus::GetStatus(UsbClientDrv *pDriverPtr,BOOL Async,UCHAR bIndex,DWORD dwFlag,
                     LPWORD lpwStatus,BOOL bSuccess):
    UsbFuncTest(pDriverPtr,Async)
{
    if (fAsync)
        dwFlag |=USB_NO_WAIT;
    else
        dwFlag &=~USB_NO_WAIT;

    hTransfer=usbDriver->GetStatus(TransferNotify,this,dwFlag,bIndex,lpwStatus);
    if (!hTransfer) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail SetDescriptor return NULL transfer handle"));
        SetErrorCode(ERROR_USB_TRANSFER);
    }
    else
        CheckResult(bSuccess,hTransfer);
};

BOOL GetInterfaceTest_A(UsbClientDrv *pDriverPtr,BOOL Async)
{
    UCHAR bInterfaceNumber=0;
    UCHAR uNextInterface=0;
    UINT count=0;
    DWORD errorCode=0;
    do {
        GetInterfaceTest * pGetInterfaceTest = new GetInterfaceTest(pDriverPtr,Async,bInterfaceNumber,&uNextInterface,TRUE);
        if(pGetInterfaceTest == NULL){
            g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
            return FALSE;
        }
        errorCode=pGetInterfaceTest->GetErrorCode();
        delete pGetInterfaceTest;
        if ((errorCode)!=0) {
            g_pKato->Log(LOG_FAIL, TEXT("Fail at GetInterface,errorCode(%ld)"),errorCode);
            return FALSE;
        };
        if (bInterfaceNumber==uNextInterface) // only interface it has
            break;
        bInterfaceNumber=uNextInterface;
        uNextInterface=0;
        count++;
    }while (count<10);

    bInterfaceNumber=0xfe;
    uNextInterface=0xfe;
    GetInterfaceTest * pGetInterfaceTest = new GetInterfaceTest(pDriverPtr,Async,bInterfaceNumber,&uNextInterface,FALSE);
    if(pGetInterfaceTest == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
        return FALSE;
    }
    errorCode=pGetInterfaceTest->GetErrorCode();
    delete pGetInterfaceTest;
    if (errorCode==0 && bInterfaceNumber == uNextInterface) {
        g_pKato->Log(LOG_FAIL, TEXT("Fail at GetDescriptor,Unexpected Success return"));
        return FALSE;
    };
    return TRUE;
};

BOOL BaseCase1Command(UsbClientDrv *pDriverPtr,BOOL Async)
{

    DEBUGMSG(DBG_DETAIL,(TEXT("BaseCase1Command Test")));

    TransStatus transStatus;
    transStatus.lpUsbFuncs=pDriverPtr->lpUsbFuncs;
    transStatus.IsFinished=FALSE;

    LPCUSB_FUNCS lpUsbFuncs =pDriverPtr->lpUsbFuncs;
    USB_HANDLE usbHandle= pDriverPtr->GetUsbHandle();

    USB_DEVICE_DESCRIPTOR deviceDescriptor;

    memset(&deviceDescriptor,0,sizeof(USB_DEVICE_DESCRIPTOR));
    DEBUGMSG(DBG_DETAIL,(TEXT("Call GetDescriptor using correct argument")));
    transStatus.hTransfer=(*lpUsbFuncs->lpGetDescriptor)
            (usbHandle,
            Async?TransferNotify:NULL,
            &transStatus,
            (Async?USB_NO_WAIT:0),
            USB_DEVICE_DESCRIPTOR_TYPE,0,0,
            sizeof(USB_DEVICE_DESCRIPTOR),&deviceDescriptor);

    if (Async && transStatus.cEvent.Lock(USB_TIME_OUT)==FALSE) {
        g_pKato->Log(LOG_FAIL,TEXT("BaseCase1Command time out 1"));
        return FALSE;
    };
    if (transStatus.hTransfer) {
        if ((*lpUsbFuncs->lpIsTransferComplete)(transStatus.hTransfer))
            DEBUGMSG(DBG_DETAIL,(TEXT("Transfer Complete")));
        else
            DEBUGMSG(DBG_DETAIL,(TEXT("Transfer Complete")));
        (*lpUsbFuncs->lpCloseTransfer)(transStatus.hTransfer);
    };


    BOOL bReturn=TRUE;

    DEBUGMSG(DBG_DETAIL,(TEXT("Call GetDescriptor using Wrong argument")));
    memset(&deviceDescriptor,0,sizeof(USB_DEVICE_DESCRIPTOR));
    transStatus.hTransfer=(*lpUsbFuncs->lpGetDescriptor)
            (usbHandle,
            Async?TransferNotify:NULL,
            &transStatus,
            (Async?USB_NO_WAIT:0),
            USB_DEVICE_DESCRIPTOR_TYPE,1,0,
            sizeof(USB_DEVICE_DESCRIPTOR),&deviceDescriptor);

    if (Async && transStatus.cEvent.Lock(USB_TIME_OUT)==FALSE) {
        g_pKato->Log(LOG_FAIL,TEXT("BaseCase1Command time out 2"));
        bReturn=FALSE;
    }
    if (transStatus.hTransfer) {
        if ((*lpUsbFuncs->lpIsTransferComplete)(transStatus.hTransfer))
            DEBUGMSG(DBG_DETAIL,(TEXT("Transfer Complete")));
        else
            DEBUGMSG(DBG_DETAIL,(TEXT("Transfer Complete")));
        (*lpUsbFuncs->lpCloseTransfer)(transStatus.hTransfer);
    };

    DEBUGMSG(DBG_DETAIL,(TEXT("Call GetDescriptor using Wrong argument again")));
    memset(&deviceDescriptor,0,sizeof(USB_DEVICE_DESCRIPTOR));
    transStatus.hTransfer=(*lpUsbFuncs->lpGetDescriptor)
            (usbHandle,
            Async?TransferNotify:NULL,
            &transStatus,
            (Async?USB_NO_WAIT:0),
            USB_DEVICE_DESCRIPTOR_TYPE,1,0,
            sizeof(USB_DEVICE_DESCRIPTOR),&deviceDescriptor);

    if (Async && transStatus.cEvent.Lock(USB_TIME_OUT)==FALSE) {
        g_pKato->Log(LOG_FAIL,TEXT("BaseCase1Command time out 3"));
        bReturn=FALSE;
    };
    if (transStatus.hTransfer) {
        if ((*lpUsbFuncs->lpIsTransferComplete)(transStatus.hTransfer))
            DEBUGMSG(DBG_DETAIL,(TEXT("Transfer Complete")));
        else
            DEBUGMSG(DBG_DETAIL,(TEXT("Transfer Complete")));
        (*lpUsbFuncs->lpCloseTransfer)(transStatus.hTransfer);
    };

    return bReturn;
}
#define USB_GETSTATUS_DEVICE_STALL 1
BOOL TestEndpointFeature(UsbClientDrv * pDriverPtr,BOOL aSync,DWORD dwIndex,LPCUSB_ENDPOINT pEndpoint)
{
    UNREFERENCED_PARAMETER(dwIndex);
    g_pKato->Log(LOG_COMMENT,TEXT("End Point SetFeature USB_ENDPOINT_STALL "));
    {
        FeatureSetting *pSetFeature= new FeatureSetting(pDriverPtr,TRUE,aSync,
            USB_SEND_TO_ENDPOINT,USB_FEATURE_ENDPOINT_STALL,(UCHAR)pEndpoint->Descriptor.bEndpointAddress,TRUE);
        if(pSetFeature == NULL){
            g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
            return FALSE;
        }
        if (pSetFeature->GetErrorCode()) {
            g_pKato->Log(LOG_FAIL,TEXT("SetFeature USB_ENDPOINT_STALL Fail ErrorCode=%ld"),pSetFeature->GetErrorCode());
            delete pSetFeature;
            return FALSE;
        }
        else {
            delete pSetFeature;
            WORD wStatus=0;
            GetStatus * paGetStatus= new GetStatus(pDriverPtr,aSync,pEndpoint->Descriptor.bEndpointAddress,USB_SEND_TO_ENDPOINT,&wStatus,TRUE);
                    if(paGetStatus == NULL){
                    g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
                    return FALSE;
                }
            if (paGetStatus->GetErrorCode()) {
                g_pKato->Log(LOG_FAIL,TEXT("GetStatus Fail ErrorCode=%ld"),paGetStatus->GetErrorCode());
                delete paGetStatus;
                return FALSE;
            }
            else {
                delete paGetStatus;
                if (!(wStatus & USB_GETSTATUS_DEVICE_STALL)) {
                    g_pKato->Log(LOG_FAIL,TEXT("GetStatus return Can not SUPPORT STALL"));
                };
            }
        };
    };
    g_pKato->Log(LOG_COMMENT,TEXT("End Point ClearFeature USB_ENDPOINT_STALL "));
    {
        FeatureSetting *pClearFeature= new FeatureSetting(pDriverPtr,FALSE,aSync,
            USB_SEND_TO_ENDPOINT,USB_FEATURE_ENDPOINT_STALL,pEndpoint->Descriptor.bEndpointAddress,TRUE);
        if(pClearFeature == NULL){
            g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
            return FALSE;
        }
        if (pClearFeature->GetErrorCode()) {
            g_pKato->Log(LOG_FAIL,TEXT("ClearFeature REMODE_WAKEUP Fail ErrorCode=%ld"),pClearFeature->GetErrorCode());
            return FALSE;
        }
        else {
            WORD wStatus=0;
            GetStatus *paGetStatus= new GetStatus(pDriverPtr,aSync,pEndpoint->Descriptor.bEndpointAddress,USB_SEND_TO_ENDPOINT,&wStatus,TRUE);
            if(paGetStatus == NULL){
                    g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
                    return FALSE;
                }
            if (paGetStatus->GetErrorCode()) {
                g_pKato->Log(LOG_FAIL,TEXT("GetStatus Fail ErrorCode=%ld"),paGetStatus->GetErrorCode());
            }
            else
                if (wStatus & USB_GETSTATUS_DEVICE_STALL) {
                    g_pKato->Log(LOG_FAIL,TEXT("GetStatus return Can not clear STALL"));
                };
            delete paGetStatus;
        };
    };
    return TRUE;

}
BOOL TestInterface(UsbClientDrv *pDriverPtr,BOOL aSync,DWORD dwIndex,LPCUSB_INTERFACE pUsbInterface)
{
    UCHAR bAlternateSetting = 0;
    GetInterfaceTest *pGInterface= new GetInterfaceTest(pDriverPtr,aSync,(UCHAR)dwIndex,&bAlternateSetting,TRUE);
    if(pGInterface == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
        return FALSE;
    }
    if (pGInterface->GetErrorCode()) {
        g_pKato->Log(LOG_FAIL,TEXT("Get Interface  ErrorCode=%ld"),pGInterface->GetErrorCode());
        delete pGInterface;
        return FALSE;
    };
    delete pGInterface;

    g_pKato->Log(LOG_COMMENT,TEXT(" We Get Interface %d Interface, the AlternateSetting (%ld)"),
        (DWORD)dwIndex,(DWORD)bAlternateSetting);
    SetInterfaceTest * pSInterface= new SetInterfaceTest(pDriverPtr,aSync,(UCHAR)dwIndex,bAlternateSetting,TRUE);
    if(pSInterface == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
        return FALSE;
    }

    if (pSInterface->GetErrorCode()) {
        g_pKato->Log(LOG_FAIL,TEXT("Get Interface  ErrorCode=%ld"),pSInterface->GetErrorCode());
        delete pSInterface;
        return FALSE;
    };
    delete pSInterface;
    g_pKato->Log(LOG_COMMENT,TEXT(" We Set Interface %d Interface, The interface have %ld endpoint"),
        (DWORD)dwIndex,(DWORD)pUsbInterface->Descriptor.bNumEndpoints);
    for (DWORD index=0;index<(DWORD)pUsbInterface->Descriptor.bNumEndpoints;index++) {
        if (((pUsbInterface->lpEndpoints+index)->Descriptor.bEndpointAddress & 
            (~USB_ENDPOINT_DIRECTION_MASK))!=0) {// do not test end point zero.
            if (!TestEndpointFeature(pDriverPtr,aSync,index,pUsbInterface->lpEndpoints+index)) {
                g_pKato->Log(LOG_FAIL,TEXT("TestEndpointFeature Fail for index %d: endpoint addr is %x"),
                    index,(pUsbInterface->lpEndpoints+index)->Descriptor.bEndpointAddress);
                return FALSE;
            }
        };
    };
    return TRUE;

}
BOOL GetConfigureDescriptor(UsbClientDrv *pDriverPtr,BOOL aSync,DWORD dwIndex,
                            LPCUSB_CONFIGURATION pUsbConfigure,
                            PBYTE uDescripBuffer,WORD wLength)
{
    GetDespTest * pConfigDescriptor= new GetDespTest(pDriverPtr,aSync,USB_CONFIGURATION_DESCRIPTOR_TYPE,(UCHAR)dwIndex,0,
        wLength,uDescripBuffer,TRUE);
    if(pConfigDescriptor == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
        return FALSE;
    }
    if (pConfigDescriptor->GetErrorCode()) {
        g_pKato->Log(LOG_FAIL,TEXT("Get ConfigDescriptor Fail ErrorCode=%ld"),pConfigDescriptor->GetErrorCode());
        delete pConfigDescriptor;
        return FALSE;
    };
    delete pConfigDescriptor;
    PUSB_CONFIGURATION_DESCRIPTOR pConfigDescr = (PUSB_CONFIGURATION_DESCRIPTOR)uDescripBuffer;
    if (pConfigDescr->bLength!=sizeof(USB_CONFIGURATION_DESCRIPTOR)) {
        g_pKato->Log(LOG_COMMENT,TEXT("GetConfigDescriptor Size is uncompatable user device(%ld), usbd expect(%ld)"),
            (DWORD)(pConfigDescr->bLength),sizeof(USB_CONFIGURATION_DESCRIPTOR));
    };
    g_pKato->Log(LOG_COMMENT,TEXT(" We found %d Interface Descriptor"),
        (DWORD)pConfigDescr->bNumInterfaces);

    if (pConfigDescr->bNumInterfaces!=pUsbConfigure->Descriptor.bNumInterfaces){
        g_pKato->Log(LOG_FAIL,TEXT("Interface Number (%ld) is not same as GetDeviceInfo(%ld) for configur %ld"),
            (DWORD)pConfigDescr->bNumInterfaces,(DWORD)pUsbConfigure->Descriptor.bNumInterfaces,dwIndex);
        return FALSE;
    }
    DWORD dwNumInterface=pConfigDescr->bNumInterfaces;
    for (DWORD index=0;index<dwNumInterface;index++) {
        if (!TestInterface(pDriverPtr,aSync,index,pUsbConfigure->lpInterfaces+index)) {
            g_pKato->Log(LOG_FAIL,TEXT("TestInterface Fail for index %d"),index);
            return FALSE;
        }
    }
    return TRUE;
}
BOOL DeviceRequest(UsbClientDrv *pDriverPtr,BOOL aSync)
{
    PUCHAR uDescripBuffer = NULL ; // 4 k;
    uDescripBuffer = (PUCHAR)LocalAlloc(LPTR, 0x1000*sizeof(UCHAR));
    if(uDescripBuffer == NULL){
        g_pKato->Log(LOG_FAIL,TEXT("Fail at DeviceRequest,out of memory"));
        return FALSE;
    }

    GetDespTest *pDeviceDescriptor= new GetDespTest(pDriverPtr,aSync,USB_DEVICE_DESCRIPTOR_TYPE,0,0,
        0x1000,uDescripBuffer,TRUE);
    if(pDeviceDescriptor == NULL){
        g_pKato->Log(LOG_FAIL, TEXT("Out of memory"));
        return FALSE;
    }

    if (pDeviceDescriptor->GetErrorCode()) {
        g_pKato->Log(LOG_FAIL,TEXT("Get DeviceDescriptor Fail ErrorCode=%ld"),pDeviceDescriptor->GetErrorCode());
        delete pDeviceDescriptor;
        LocalFree(uDescripBuffer);
        return FALSE;
    };
    delete pDeviceDescriptor;

    LPCUSB_DEVICE pUsbDevice=pDriverPtr->GetDeviceInfo();
       if(pUsbDevice == NULL){
        g_pKato->Log(LOG_FAIL,TEXT("Invalid USB device data structure!"));
        LocalFree(uDescripBuffer);
        return FALSE;
       }
    PUSB_DEVICE_DESCRIPTOR pDevice = (PUSB_DEVICE_DESCRIPTOR)uDescripBuffer;
       ASSERT(pDevice);
    if (pDevice->bLength!=sizeof(USB_DEVICE_DESCRIPTOR)) {
        g_pKato->Log(LOG_FAIL,TEXT("DeviceDescrip Size is uncompatable user device(%ld), usbd expect(%ld)"),
            (DWORD)(pDevice->bLength),sizeof(USB_DEVICE_DESCRIPTOR));
    };

    if (pDevice->bNumConfigurations!=pUsbDevice->Descriptor.bNumConfigurations) {
        g_pKato->Log(LOG_FAIL,TEXT("Configarun Number (%ld) is not same as GetDeviceInfo(%ld)"),
            (DWORD)pDevice->bNumConfigurations,(DWORD)pUsbDevice->Descriptor.bNumConfigurations);
        LocalFree(uDescripBuffer);
        return FALSE;
    }
    g_pKato->Log(LOG_COMMENT,TEXT(" We found %d Configuration Descriptor"),
        (DWORD)pDevice->bNumConfigurations);
    DWORD dwNumConfiguration=(DWORD)pDevice->bNumConfigurations;
    
    for (DWORD index=0;index<dwNumConfiguration;index++) {
        if (!GetConfigureDescriptor(pDriverPtr,aSync,index,
                pUsbDevice->lpConfigs+index,uDescripBuffer,0x1000)) {
            g_pKato->Log(LOG_FAIL,TEXT("GetConfigureDescriptor Fail for index %d"),index);
            pDriverPtr->ResetDefaultPipe();
            LocalFree(uDescripBuffer);
            return FALSE;
        };
    }
    LocalFree(uDescripBuffer);
    return TRUE;

}
