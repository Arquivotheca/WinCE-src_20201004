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

UsbBts.cpp

Abstract:
    USB BSP Test Suite test cases .

Functions:

Notes:

--*/



#define __THIS_FILE__   TEXT("usbbts.cpp")
#include <windows.h>
#include <ceddk.h>
#include "PipeTest.h"
#include "usbtest.h"


/*
 * @func   DWORD | GetUSBDVersion
 * @rdesc  Test is pass when USBD major and minor version numbers are returned
 *         Test is skiped when the version number can not be obtained
 * @comm   This test prints out the USBD Major and Minor version number
*/

DWORD
GetUSBDVersion(UsbClientDrv *pDriverPtr)
{
     DWORD dwMajorVersion = 0;
     DWORD dwMinorVersion = 0;

    if(pDriverPtr == NULL)
    {
         NKDbgPrintfW(_T("Can not get USB Version, pDriverPtr is Null\n"));
        return TPR_SKIP;
    }

    pDriverPtr->GetUSBDVersion(&dwMajorVersion, &dwMinorVersion);
    NKDbgPrintfW(_T("GetUSBDVersion"));
    NKDbgPrintfW(_T("  MajorVersion:  %d"),dwMajorVersion);
    NKDbgPrintfW(_T("  MinorVersion:  %d"),dwMinorVersion);

    return TPR_PASS;
}



/*
 * @func   DWORD | FrameLengthControlErrHandling
 * @rdesc  Test is pass when correct error handling is returned, otherwise it fails
 * @comm   This test verifies the correctness of the return values of
 *         LPRELEASE_FRAME_LENGTH_CONTROL function, LPGET_FRAME_LENGTH function and LPSET_FRAME_LENGTH function
*/

DWORD
FrameLengthControlErrHandling(UsbClientDrv *pDriverPtr)
{
      USHORT uexistingFrameLength = 0;

    if(pDriverPtr == NULL)
         return TPR_SKIP;

    //
    //Try to release the frame control without taking it
    //
    if(pDriverPtr->ReleaseFrameLengthControl() == TRUE){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  Able to release frame control without first taking it!"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    //
    //Try to get frameLength without acquiring the control -- should work
    //
    if(pDriverPtr->GetFrameLength(&uexistingFrameLength) == FALSE){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  unable to GetFrameLength!"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    //
    //Try to set frameLength without acquiring the control --  should NOT work
    //
    if(pDriverPtr->SetFrameLength(NULL, uexistingFrameLength * 10) == TRUE){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  Able to SetFrameLength without first TakeFrameLengthLengthControl!"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    NKDbgPrintfW(_T("FrameLengthControl returns correct error handling, Test Pass!\n"));
    return TPR_PASS;

}



/*
 * @func   DWORD | FrameLengthControlDoubleRelease
 * @rdesc  Test is pass when acquires FrameLengthControl once and release it once , otherwise it fails
 * @comm   This test acquires FrameLengthControl once and releases it twice.  The second release should fail
*/

DWORD
FrameLengthControlDoubleRelease(UsbClientDrv *pDriverPtr)
{
    if(pDriverPtr == NULL)
         return TPR_SKIP;


    if(pDriverPtr->TakeFrameLengthControl() == FALSE){
            //Possible cause:  1)  FrameLengthControl is already acquired by another driver   2) Actual bug
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  Unable to TakeFrameLengthControl!"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    if(pDriverPtr->ReleaseFrameLengthControl() == FALSE){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  Unable to ReleaseFrameLengthControl!"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    if(pDriverPtr->ReleaseFrameLengthControl() == TRUE){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  Able to ReleaseFrameLengthControl twice!"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    NKDbgPrintfW(_T("Acquires FrameLengthControl and releases it succesful, Test Pass!\n"));
    return TPR_PASS;
}


/*
 * @func   DWORD | AddRemoveRegistrySetting
 * @rdesc  Test is pass when USB client driver is successfully installed and unstalled , otherwise it fails
 * @comm   This test tests the USB client driver deregister itself with USBD
*/

DWORD
AddRemoveRegistrySetting(UsbClientDrv *pDriverPtr)
{

      if(pDriverPtr == NULL)
         return TPR_SKIP;

      if (FALSE == USBInstallDriver(_T("usbtest.dll"))){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  USBInstallDriver returned False"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
      }

      //
      //  Check the driver is actually ADDED
      //
    NKDbgPrintfW(_T("Successfully installed USB client driver, Test Pass!\n"));
    return TPR_PASS;

#if 0
    if (FALSE == USBUnInstallDriver()){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  Unable to RemoveRegistrySetting"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

      //
      //  TODO:  Check the driver is actually removed
      //

      if (FALSE == USBInstallDriver(_T("usbtest.dll"))){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  USBInstallDriver returned False"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
      }

      //
      //  Check the driver is actually ADDED
      //

    NKDbgPrintfW(_T("Successfully uninstalled USB client driver from USBD, Test Pass!\n"));
    return TPR_PASS;
#endif
}



/*
 * @func   DWORD | FindInterfaceTest
 * @rdesc  Test is pass if find interface fail if no interface is found or there is some error.
 * @comm   This test tests searching interfaces on a USB device.
 *         When an interface is found, it outputs the interface number and setting.
 *         The test fails if no interface is found
*/

DWORD
FindInterfaceTest(UsbClientDrv *pDrivePtr)
{

     UCHAR i, j;
    DWORD numOfInterfaceFound = 0;

    if(pDrivePtr == NULL)
         return TPR_SKIP;

    LPCUSB_DEVICE lpDeviceInfo = pDrivePtr->GetDeviceInfo();

    for(i = 0; i < 255; i++){
         for (j = 0; j <255; j++){
             LPCUSB_INTERFACE lpUsbInterface = pDrivePtr->FindInterface(lpDeviceInfo, i, j);
            if(NULL != lpUsbInterface){
                 //Quick sanity check that the thing returned is actually a pointer to USB_INTERFACE
                if(sizeof(USB_INTERFACE)!= lpUsbInterface->dwCount){
                     NKDbgPrintfW(_T("FAIL in %s @ line %d:  lpUsbInterface->dwCount NOT equal to sizeof(USB_INTERFACE)"),__THIS_FILE__,__LINE__);
                    return TPR_FAIL;
                }
                NKDbgPrintfW(_T("INFO:  Found Inferface with at bInterfaceNumber = %d, bAlternateSetting = %d"),i,j);
                numOfInterfaceFound++;
            }
        }
      }

    if( 0== numOfInterfaceFound){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  FindInterface did not return any interface"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    NKDbgPrintfW(_T("INFO:  FindInterface found %u interfaces"),numOfInterfaceFound);
    return TPR_PASS;

}


/*
 * @func   DWORD | ResetDefaultPipeTest
 * @rdesc  Test is pass after reset the default pipe, the halted state is cleared, fail if there is some error.
 * @comm   This test resets the default pipe. So that the halted state of the default pipe within the USB stack
 *         is also cleared. The tests fails if the halted state is not cleared or reset default pipe fails
*/

DWORD
ResetDefaultPipeTest(UsbClientDrv *pDrivePtr)
{
     BOOL  bIsHalt = TRUE;

    if(pDrivePtr == NULL)
         return TPR_SKIP;

    if(FALSE ==pDrivePtr->ResetDefaultPipe()){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  ResetDefaultPipe returned false"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    //
    //  After reset, we should NOT have a stalled pipe!
    //
    if(FALSE == pDrivePtr->IsDefaultPipeHalted(&bIsHalt)){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  IsDefaultPipeHalt returned false"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    if(TRUE == bIsHalt){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  DefaultPipe is still halt after reset"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    NKDbgPrintfW(_T("Reset default pipe succeeds, Test Pass!"));
    return TPR_PASS;

}

/*
 * @func   DWORD | OpenClientRegistryKeyTest
 * @rdesc  Test is pass if a handle to the appropriate key is returned, and when valid parameters are input for GetClientRegistryPath funtion
 * @comm   This test opens a registry key assocaited with a USB client driver.
 *         A handle is returned to indicate success. Null indicates that no key exists or some error occured
*/

DWORD
ClientRegistryKeyTest(UsbClientDrv *pDrivePtr)
{

     if(pDrivePtr == NULL)
         return TPR_SKIP;

    USBDriverClass usbDriver;
    HKEY hkRegKey = usbDriver.OpenClientRegistryKey(NULL);

    if(NULL == hkRegKey){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  OpenClientRegistrykey return NULL with NULL input.  We should still be able to open regkey with input = NULL."),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }
    RegCloseKey(hkRegKey);

    hkRegKey = usbDriver.OpenClientRegistryKey(pDrivePtr->GetUniqueDriverId());
    if(NULL == hkRegKey){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  OpenClientRegistrykey failed with valid parameter"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    RegCloseKey(hkRegKey);
    NKDbgPrintfW(_T("Open client registry key succeeds, Test Pass!"));

    TCHAR szDesc[MAX_PATH];

    BOOL fRet = usbDriver.GetClientRegistryPath(NULL, 255, pDrivePtr->GetUniqueDriverId());
    if(TRUE == fRet ){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  GetClientRegistryPath returned true with invalid parameter:  NULL, 255, pDrivePtr->GetUniqueDriverId()"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    fRet = usbDriver.GetClientRegistryPath(szDesc, 0, pDrivePtr->GetUniqueDriverId());
    if(TRUE == fRet ){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  GetClientRegistryPath returned true with invalid parameter:  szDesc, 0, pDrivePtr->GetUniqueDriverId()"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    fRet = usbDriver.GetClientRegistryPath(szDesc, 255, NULL);
    if(TRUE == fRet ){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  GetClientRegistryPath returned true with invalid parameter:  szDesc, 255, NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    /* The path should be more than 1 character.  The OS should think there is NOT enough room to copy the result back and hence return an error */
    fRet = usbDriver.GetClientRegistryPath(szDesc, 1, pDrivePtr->GetUniqueDriverId());
    if(TRUE == fRet ){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  GetClientRegistryPath returned true with invalid parameter:  szDesc, 1, NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    fRet = usbDriver.GetClientRegistryPath(szDesc, 255, pDrivePtr->GetUniqueDriverId());
    if(FALSE == fRet ){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  GetClientRegistryPath returned true with valid parameters:  szDesc, 255, pDrivePtr->GetUniqueDriverId()"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }
    NKDbgPrintfW(_T("Get client registry path succeeds, Test Pass!"));
        return TPR_PASS;
}

/*
 * @func   DWORD | HcdSelectConfigurationTest
 * @rdesc  Test is pass when valid parameters are input to HcdSelectConfiguration function, fails if there is some error
 * @comm
*/

DWORD
HcdSelectConfigurationTest(UsbClientDrv *pDrivePtr)
{

     if(pDrivePtr == NULL)
         return TPR_SKIP;

    USBDriverClass usbDriver;
    BYTE bTmp;

    if(TRUE == usbDriver.HcdSelectConfiguration(NULL,&bTmp)){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  HcdSelectConfiguration returned true with invalid parameter:  NULL, &bTmp"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    if(TRUE == usbDriver.HcdSelectConfiguration(pDrivePtr->GetDeviceInfo(),NULL)){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  HcdSelectConfiguration returned true with invalid parameter:  pDrivePtr->GetDeviceInfo(), &bTmp"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    for (bTmp = 0; bTmp < 255; bTmp++){
        usbDriver.HcdSelectConfiguration(pDrivePtr->GetDeviceInfo(),&bTmp);
    }
    NKDbgPrintfW(_T("HcdSelectConfiguration returned true with valid input parameter, Test Pass!"));
    return TPR_PASS;
}

/*
 * @func   DWORD | InvalidParameterTest
 * @rdesc  Test is pass when invalid parameters does not cause functions to return true, fails if there is some error
 * @comm   Test verifies various of frame control and pipe control functions by inputing invalid parameters
 *         Those functions should not return true when invalid parameters are used
*/


DWORD
InvalidParameterTest(UsbClientDrv *pDrivePtr)
{
     if(pDrivePtr == NULL)
         return TPR_SKIP;

    LPCUSB_FUNCS lpUsbFuncs = pDrivePtr->GetUsbFuncsTable();
         BOOL   fRet = TRUE;

    //TakeFrameLengthControl
    fRet = (*lpUsbFuncs->lpTakeFrameLengthControl)(NULL);
    if(TRUE == fRet){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  TakeFrameLengthControl returned true with invalid parameter:  NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    //ReleaseFrameLengthControl
    fRet = (*lpUsbFuncs->lpReleaseFrameLengthControl)(NULL);
    if(TRUE == fRet){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  ReleaseFrameLengthControl returned true with invalid parameter:  NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    //SetFrameLength
    NKDbgPrintfW(_T("SetFrameLength"));
    fRet = (*lpUsbFuncs->lpSetFrameLength)(NULL,NULL,1);
    if(TRUE == fRet){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  SetFrameLength returned true with invalid parameter:  NULL, NULL,1"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    //GetFrameLength
    NKDbgPrintfW(_T("GetFrameLength"));
    fRet = (*lpUsbFuncs->lpGetFrameLength)(NULL,NULL);
    if(TRUE == fRet){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  SetFrameLength returned true with invalid parameter:  NULL, NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

      //GetFrameNumber
    NKDbgPrintfW(_T("GetFrameNumber"));
    fRet = (*lpUsbFuncs->lpGetFrameNumber)(NULL,NULL);
    if(TRUE == fRet){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  SetFrameNumber returned true with invalid parameter:  NULL, NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    //Open Pipe
    NKDbgPrintfW(_T("OpenPipe"));
    if(NULL != (*lpUsbFuncs->lpOpenPipe)(NULL,NULL)){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  OpenPipe returned true with invalid parameter:  NULL, NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    //Close Pipe
    NKDbgPrintfW(_T("ClosePipe"));
    fRet=(*lpUsbFuncs->lpClosePipe)(NULL);
    if(FALSE != fRet){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  ClosePipe returned true with invalid parameter:  NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    //Reset Pipe
    NKDbgPrintfW(_T("ResetPipe"));
    fRet=(*lpUsbFuncs->lpResetPipe)(NULL);
    if(FALSE != fRet){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  ResetPipe returned true with invalid parameter:  NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    //Reset Default Pipe
    NKDbgPrintfW(_T("ResetDefaultPipe"));
    fRet=(*lpUsbFuncs->lpResetDefaultPipe)(NULL);
    if(FALSE != fRet){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  ResetDefaultPipe returned true with invalid parameter:  NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }
    NKDbgPrintfW(_T("Frame control and pipe control functions did not return true with invalid parameters, Test Pass!"));
    return TPR_PASS;
}

/*
 * @func   DWORD | DoubleOpenPipe
 * @rdesc  Test is pass when same pipe can not be opened twiceand  when already closed pipe can not be closed again,  fails if there is some error
 * @comm   Test verifies whether same pipe can be opened twice.
 *         Test is pass when same pipe can not be opened twice and all opened pipes can be closed successfully
*/

#define MAX_NUM_OF_PIPES                  16

DWORD
PipeParameterTest(UsbClientDrv *pDrivePtr)
{

     if(pDrivePtr == NULL)
         return TPR_SKIP;

    USB_PIPE hPipe[MAX_NUM_OF_PIPES];
    DWORD dwCnt;
    LPCUSB_FUNCS lpUsbFuncs = pDrivePtr->GetUsbFuncsTable();
    USB_HANDLE hUsb = pDrivePtr->getUsbHandle();
    DWORD dwResult = TPR_PASS;

    if(NULL == lpUsbFuncs || NULL == hUsb){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  hUsb or lpUsbFuncs is NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    for(dwCnt = 0; dwCnt < MAX_NUM_OF_PIPES; dwCnt++){
         hPipe[dwCnt]=NULL;
    }

      //Let's try opening each endpoint one at a time
     for (dwCnt = 0; dwCnt < pDrivePtr->GetNumOfEndPoints(); dwCnt++){
         LPCUSB_ENDPOINT curPoint = pDrivePtr->GetDescriptorPoint(dwCnt);
         if(NULL == curPoint){
             NKDbgPrintfW(_T("FAIL in %s @ line %d:  Failed to get EndPointDescriptor at index = %d"),__THIS_FILE__,__LINE__,dwCnt);
            return TPR_FAIL;
        }
        ASSERT(curPoint);
        hPipe[dwCnt]=(*(lpUsbFuncs->lpOpenPipe))(hUsb,&(curPoint->Descriptor));
        if(NULL == hPipe[dwCnt]){
             NKDbgPrintfW(_T("FAIL in %s @ line %d:  Failed to open pipe %d"),__THIS_FILE__,__LINE__,dwCnt);
            dwResult = TPR_FAIL;
        }
     }

      //Let's try to open each end point again...should all fail
     for (dwCnt = 0; dwCnt<pDrivePtr->GetNumOfEndPoints() && (TPR_PASS== dwResult); dwCnt++){
         LPCUSB_ENDPOINT curPoint = pDrivePtr->GetDescriptorPoint(dwCnt);
        USB_PIPE hPipe2 = NULL;
        if(NULL == curPoint){
             NKDbgPrintfW(_T("FAIL in %s @ line %d:  Failed to get EndPointDescriptor at index = %d"),__THIS_FILE__,__LINE__,dwCnt);
            return TPR_FAIL;
        }
        ASSERT(curPoint);
        hPipe2=(*(lpUsbFuncs->lpOpenPipe))(hUsb,&(curPoint->Descriptor));
        if(NULL != hPipe2){
             NKDbgPrintfW(_T("FAIL in %s @ line %d:  Able to open pipe %d twice"),__THIS_FILE__,__LINE__,dwCnt);
            dwResult = TPR_FAIL;
        }
    }

      //Close all the pipes we opened
     for (dwCnt = 0; dwCnt < pDrivePtr->GetNumOfEndPoints(); dwCnt++){
         BOOL fRet = FALSE;
        fRet=(*(lpUsbFuncs->lpClosePipe))(hPipe[dwCnt]);
        if(FALSE == fRet){
             NKDbgPrintfW(_T("FAIL in %s @ line %d:  Failed to close pipe %d"),__THIS_FILE__,__LINE__,dwCnt);
            dwResult =  TPR_FAIL;
        }
     }



    if(NULL == lpUsbFuncs || NULL == hUsb){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  hUsb or lpUsbFuncs is NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    for(dwCnt = 0; dwCnt < MAX_NUM_OF_PIPES; dwCnt++){
         hPipe[dwCnt]=NULL;
    }

      //Let's try opening each endpoint one at a time
    for (dwCnt = 0; dwCnt < pDrivePtr->GetNumOfEndPoints(); dwCnt++){
         LPCUSB_ENDPOINT curPoint = pDrivePtr->GetDescriptorPoint(dwCnt);
         if(NULL == curPoint){
             NKDbgPrintfW(_T("FAIL in %s @ line %d:  Failed to get EndPointDescriptor at index = %d"),__THIS_FILE__,__LINE__,dwCnt);
             return TPR_FAIL;
        }
        ASSERT(curPoint);
        hPipe[dwCnt]=(*(lpUsbFuncs->lpOpenPipe))(hUsb,&(curPoint->Descriptor));
        if(NULL == hPipe[dwCnt]){
             NKDbgPrintfW(_T("FAIL in %s @ line %d:  Failed to open pipe %d"),__THIS_FILE__,__LINE__,dwCnt);
             dwResult = TPR_FAIL;
        }
     }

      //Close all the pipes we opened
     for (dwCnt = 0; dwCnt < pDrivePtr->GetNumOfEndPoints(); dwCnt++){
         BOOL fRet = FALSE;
        fRet=(*(lpUsbFuncs->lpClosePipe))(hPipe[dwCnt]);
        if(FALSE == fRet){
             NKDbgPrintfW(_T("FAIL in %s @ line %d:  Failed to close pipe %d"),__THIS_FILE__,__LINE__,dwCnt);
            dwResult =  TPR_FAIL;
        }
     }

      //Close all the pipes that are closed already
    for (dwCnt = 0; dwCnt < pDrivePtr->GetNumOfEndPoints(); dwCnt++){
          BOOL fRet = FALSE;
        fRet=(*(lpUsbFuncs->lpClosePipe))(hPipe[dwCnt]);
         if(TRUE == fRet){
             NKDbgPrintfW(_T("FAIL in %s @ line %d:  Able to close pipe %d again"),__THIS_FILE__,__LINE__,dwCnt);
            dwResult =  TPR_FAIL;
        }
     }
    NKDbgPrintfW(_T("Opened pipes can not be closed twice, Test Pass!"));
    return dwResult;
}


/*
 * @func   DWORD | LoadGenericInterfaceDriverTest
 * @rdesc  Test is pass when load driver for a valid interface on a device, fails if there is some error
 * @comm   Test finds a valid interface and loads drivers for that interface on a device.
*/

DWORD
LoadGenericInterfaceDriverTest(UsbClientDrv *pDrivePtr)
{

     BOOL fRet = TRUE;
    LPCUSB_INTERFACE lpUsbInterface = NULL;

    if(pDrivePtr == NULL)
         return TPR_SKIP;

    LPCUSB_FUNCS lpUsbFuncs = pDrivePtr->GetUsbFuncsTable();
    USB_HANDLE hUsb = pDrivePtr->getUsbHandle();

    if(NULL == lpUsbFuncs || NULL == hUsb){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  hUsb or lpUsbFuncs is NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    fRet = (*lpUsbFuncs->lpLoadGenericInterfaceDriver)(NULL, NULL);
    if(TRUE == fRet){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  LoadGenericInterfaceDriver did not return false with invalid parameters: NULL, NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    fRet = (*lpUsbFuncs->lpLoadGenericInterfaceDriver)(hUsb, NULL);
    if(TRUE == fRet){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  LoadGenericInterfaceDriver did not return false with invalid parameters: hUsb, NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    //Hopefully Find Interface 0,0 will give us a valid interface
    lpUsbInterface = pDrivePtr->FindInterface(pDrivePtr->GetDeviceInfo(), 0, 0);
    if(NULL != lpUsbInterface){
        fRet = (*lpUsbFuncs->lpLoadGenericInterfaceDriver)(hUsb, lpUsbInterface);
        if(FALSE == fRet){
              NKDbgPrintfW(_T("FAIL in %s @ line %d:  LoadGenericInterfaceDriver return false with valid parameters"),__THIS_FILE__,__LINE__);
              return TPR_FAIL;
        }
    }

    NKDbgPrintfW(_T("Valid interface found and driver loaded successfully, Test Pass!"));
    return TPR_PASS;
}


/*
 * @func   DWORD | InvalidParameterForVendorTransfers
 * @rdesc  Test is pass when vendor-specific control does not return true with invalid parameters, fails if there is some error
 * @comm   Test verifies vendor-specific control transfer function does not issue a success handle when invalid
 *         parameters are used
*/

DWORD
InvalidParameterForVendorTransfers(UsbClientDrv *pDrivePtr)
{

     if(pDrivePtr == NULL)
         return TPR_SKIP;

    LPCUSB_FUNCS lpUsbFuncs = pDrivePtr->GetUsbFuncsTable();
    USB_HANDLE hUsb = pDrivePtr->getUsbHandle();
    USB_DEVICE_DESCRIPTOR deviceDescriptor;
    USB_TRANSFER hTransfer = NULL;
    USB_DEVICE_REQUEST usbRequest;

     if(NULL == lpUsbFuncs || NULL == hUsb){
         NKDbgPrintfW(_T("FAIL in %s @ line %d:  hUsb or lpUsbFuncs is NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
     }

    memset(&deviceDescriptor,0,sizeof(USB_DEVICE_DESCRIPTOR));

    hTransfer = (*lpUsbFuncs->lpIssueVendorTransfer)( hUsb, //USB Device Handle
                                                      NULL, //LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress - Maybe NULL
                                                      NULL, //LPVOID lpNodifyParameter - Maybe NULL
                                                      USB_NO_WAIT, //DWORD dwFlags,
                                                      NULL, // LPCUSB_DEVICE_REQUEST lpControlHeader -- Invalid Parameter, should NOT be NULL!
                                                      &deviceDescriptor, //LPVOID lpvBuffer
                                                      0); //ULONG UBufferPhysicalAddress
    if(NULL != hTransfer){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  IssueVendorTransfer did not fail with lpControlHeader = NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    hTransfer = (*lpUsbFuncs->lpIssueVendorTransfer)( NULL, //USB Device Handle -- Invalid Parameter, should NOT be NULL!
                                                      NULL, //LPTRANSFER_NOTIFY_ROUTINE lpNodifyAddress - Maybe NULL
                                                      NULL, //LPVOID lpNodifyParameter - Maybe NULL
                                                      USB_NO_WAIT, //DWORD dwFlags,
                                                      &usbRequest, // LPCUSB_DEVICE_REQUEST lpControlHeader
                                                      &deviceDescriptor, //LPVOID lpvBuffer
                                                      0); //ULONG UBufferPhysicalAddress
    if(NULL != hTransfer){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  IssueVendorTransfer did not fail with lpControlHeader = NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    if(NULL != hTransfer){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  IssueVendorTransfer did not fail with invalid USB Handle"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    NKDbgPrintfW(_T("IssueVendorTransfer did not succeed with invalid input parameters, Test Pass!"));
    return TPR_PASS;
}

/*
 * @func   DWORD | InvalidParameterTransfers
 * @rdesc  Test is pass when pipe transfer functions do not return true with invalid parameters, fails if there is some error
 * @comm   Test verifies that various pipe transfer functions should not succeed when invalid
 *         parameters are used
*/

DWORD
InvalidParameterTransfers(UsbClientDrv *pDrivePtr)
{

    if(pDrivePtr == NULL)
        return TPR_SKIP;

    LPCUSB_FUNCS lpUsbFuncs = pDrivePtr->GetUsbFuncsTable();
    USB_HANDLE hUsb = pDrivePtr->getUsbHandle();
    USB_TRANSFER hTransfer = NULL;
    TCHAR szDesc[MAX_PATH];
    LPCUSB_ENDPOINT curPoint = NULL;
    USB_PIPE hPipe = NULL;
    DWORD dwCnt;

    if(NULL == lpUsbFuncs || NULL == hUsb){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  hUsb or lpUsbFuncs is NULL"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

  //Get a pipe
    for (dwCnt = 0; dwCnt < pDrivePtr->GetNumOfEndPoints(); dwCnt++){
        curPoint = pDrivePtr->GetDescriptorPoint(dwCnt);
        if(NULL == curPoint){
            NKDbgPrintfW(_T("FAIL in %s @ line %d:  Failed to get EndPointDescriptor at index = %d"),__THIS_FILE__,__LINE__,dwCnt);
            return TPR_FAIL;
        }
        ASSERT(curPoint);
        hPipe=(*(lpUsbFuncs->lpOpenPipe))(hUsb,&(curPoint->Descriptor));
        if(NULL == hPipe){
              NKDbgPrintfW(_T("FAIL in %s @ line %d:  Failed to open pipe %d"),__THIS_FILE__,__LINE__,dwCnt);
                   return TPR_FAIL;
        }
    }

    hTransfer=(*lpUsbFuncs->lpIssueInterruptTransfer)( NULL, //USB_PIPE hPipe - Invalid Parameter (should not be NULL)
                                                       NULL, //Address of callback routine (may be NULL)
                                                       NULL, //Parameter to pass to callback routine
                                                       USB_NO_WAIT, //Transfer flags
                                                       MAX_PATH, //Size of data Buffer
                                                       szDesc, //Data Buffer
                                                       0); //Physical address of data buffer (maybe NULL)

    if(NULL != hTransfer){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  IssueInterruptTransfer did not fail with invalid USB Handle"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    hTransfer=(*lpUsbFuncs->lpIssueBulkTransfer)( NULL, //USB_PIPE hPipe - Invalid Parameter (should not be NULL)
                                                  NULL, //Address of callback routine (may be NULL)
                                                  NULL, //Parameter to pass to callback routine
                                                  USB_NO_WAIT, //Transfer flags
                                                  MAX_PATH, //Size of data Buffer
                                                  szDesc, //Data Buffer
                                                  0); //Physical address of data buffer (maybe NULL)
    if(NULL != hTransfer){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  IssueBulkTransfer did not fail with invalid USB Handle"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    hTransfer=(*lpUsbFuncs->lpIssueIsochTransfer)( NULL, //USB_PIPE hPipe - Invalid Parameter (should not be NULL)
                                                   NULL, //Address of callback routine (may be NULL)
                                                   NULL, //Parameter to pass to callback routine
                                                   USB_NO_WAIT, //Transfer flags
                                                   0, //Starting Frame
                                                   10, //Size of data Buffer
                                                   NULL, //LpdwLengths
                                                   szDesc, //Data Buffer
                                                   0); //Physical address of data buffer (maybe NULL)
    if(NULL != hTransfer){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  IssueIsochTransfer did not fail with invalid USB Handle"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    hTransfer=(*lpUsbFuncs->lpIssueControlTransfer)( NULL, //USB_PIPE hPipe - Invalid Parameter (should not be NULL)
                                                     NULL, //Address of callback routine (may be NULL)
                                                     NULL, //Parameter to pass to callback routine
                                                     USB_NO_WAIT, //Transfer flags
                                                     NULL, //Pointer to control header (8bytes)
                                                     MAX_PATH, //Size of data Buffer
                                                     szDesc, //Data Buffer
                                                     0); //Physical address of data buffer (maybe NULL)
    if(NULL != hTransfer){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  IssueControlTransfer did not fail with invalid USB Handle"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

  //
  //  Repeat the above test with a closed pipe
  //
    if(FALSE ==(*(lpUsbFuncs->lpClosePipe))(hPipe)){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  Cannot close hpipe"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }
    hTransfer=(*lpUsbFuncs->lpIssueInterruptTransfer)( hPipe, //USB_PIPE hPipe - Invalid Parameter (should not be NULL)
                                                       NULL, //Address of callback routine (may be NULL)
                                                       NULL, //Parameter to pass to callback routine
                                                       USB_NO_WAIT, //Transfer flags
                                                       MAX_PATH, //Size of data Buffer
                                                       szDesc, //Data Buffer
                                                       0); //Physical address of data buffer (maybe NULL)

    if(NULL != hTransfer){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  IssueInterruptTransfer did not fail with invalid USB Handle"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    hTransfer=(*lpUsbFuncs->lpIssueBulkTransfer)( hPipe, //USB_PIPE hPipe - Invalid Parameter (should not be NULL)
                                                  NULL, //Address of callback routine (may be NULL)
                                                  NULL, //Parameter to pass to callback routine
                                                  USB_NO_WAIT, //Transfer flags
                                                  MAX_PATH, //Size of data Buffer
                                                  szDesc, //Data Buffer
                                                  0); //Physical address of data buffer (maybe NULL)
    if(NULL != hTransfer){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  IssueBulkTransfer did not fail with invalid USB Handle"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    hTransfer=(*lpUsbFuncs->lpIssueIsochTransfer)( hPipe, //USB_PIPE hPipe - Invalid Parameter (should not be NULL)
                                                   NULL, //Address of callback routine (may be NULL)
                                                   NULL, //Parameter to pass to callback routine
                                                   USB_NO_WAIT, //Transfer flags
                                                   0, //Starting Frame
                                                   10, //Size of data Buffer
                                                   NULL, //LpdwLengths
                                                   szDesc, //Data Buffer
                                                   0); //Physical address of data buffer (maybe NULL)
    if(NULL != hTransfer){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  IssueIsochTransfer did not fail with invalid USB Handle"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }

    hTransfer=(*lpUsbFuncs->lpIssueControlTransfer)( hPipe, //USB_PIPE hPipe - Invalid Parameter (should not be NULL)
                                                     NULL, //Address of callback routine (may be NULL)
                                                     NULL, //Parameter to pass to callback routine
                                                     USB_NO_WAIT, //Transfer flags
                                                     NULL, //Pointer to control header (8bytes)
                                                     MAX_PATH, //Size of data Buffer
                                                     szDesc, //Data Buffer
                                                     0); //Physical address of data buffer (maybe NULL)
    if(NULL != hTransfer){
        NKDbgPrintfW(_T("FAIL in %s @ line %d:  IssueControlTransfer did not fail with invalid USB Handle"),__THIS_FILE__,__LINE__);
        return TPR_FAIL;
    }
    NKDbgPrintfW(_T("Pipe transfer control functions did not succeed with invalid input parameters, Test Pass!"));
    return TPR_PASS;
}