#ifndef __USBBT_H_
#define __USBBT_H_
#include "UsbServ.h" 
#include "Usbtest.h"
#include <usbdi.h>

DWORD GetUSBDVersion(UsbClientDrv *pDriverPtr);
DWORD FrameLengthControlErrHandling(UsbClientDrv *pDriverPtr);
DWORD FrameLengthControlDoubleRelease(UsbClientDrv *pDriverPtr);
DWORD AddRemoveRegistrySetting(UsbClientDrv *pDriverPtr);

DWORD FindInterfaceTest(UsbClientDrv *pDrivePtr);
DWORD ResetDefaultPipeTest(UsbClientDrv *pDrivePtr);
DWORD ClientRegistryKeyTest(UsbClientDrv *pDrivePtr);

DWORD HcdSelectConfigurationTest(UsbClientDrv *pDrivePtr);
DWORD InvalidParameterTest(UsbClientDrv *pDrivePtr);
DWORD PipeParameterTest(UsbClientDrv *pDrivePtr);
DWORD LoadGenericInterfaceDriverTest(UsbClientDrv *pDrivePtr);

DWORD InvalidParameterForVendorTransfers(UsbClientDrv *pDrivePtr);
DWORD InvalidParameterTransfers(UsbClientDrv *pDrivePtr);

#endif
