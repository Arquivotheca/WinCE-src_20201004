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
#include <ceddk.h>
#include <pcibus.h>

//Options for the specifying the device
#define DEVICE_BY_LEGACYNAME    1
#define DEVICE_BY_DEVICENAME    2
#define DEVICE_BY_BUSNAME   3
#define DEVICE_BY_GUID      4
#define DEVICE_BY_PCIBUS    5
#define DEVICE_BY_IRQ   6
#define DEVICE_BY_SYSINTR   7
#define DEVICE_BY_UNLOADINDEX		8

typedef struct _SEARCH_DEVICE_BY{
	DWORD dwSearchMethod;
	TCHAR  szParas[256];
}SEARCH_DEVICE_BY, *PSEARCH_DEVICE_BY;

typedef struct _DEVICE_LIST{
	DWORD dwNumOfDevices;
	DEVMGR_DEVICE_INFORMATION DevInfo[1];
}DEVICE_LIST, *PDEVICE_LIST;

typedef struct _PCI_DEVICE_CONFIG{
	int iBusNum;
	int iDeviceNum;
	int iFunctionNum;
	PCI_COMMON_CONFIG	PciConfig;
}PCI_DEVICE_CONFIG, *PPCI_DEVICE_CONFIG;

typedef struct _PCI_DEVICE_ARRAY{
	DWORD dwNumofPCIDevices;
	PCI_DEVICE_CONFIG  PciDevices[1];
}PCI_DEVICE_ARRAY, *PPCI_DEVICE_ARRAY;

typedef struct _PCI_ENUM_OPTIONS{
	BOOL fReturnInfo;
	BOOL fShowInfo;
}PCI_ENUM_OPTIONS, *PPCI_ENUM_OPTIONS;

typedef struct _LOAD_ONE_DEVICE{
	DWORD dwIndex;
	TCHAR szDevName[128];
}LOAD_ONE_DEVICE, *PLOAD_ONE_DEVICE;

typedef struct _APPVER_INFO{
	TCHAR szShimDllName[64];
	TCHAR szDevDllName[64];
}APPVER_INFO, *PAPPVER_INFO;


//  DeviceIoControl( IOCTL_TDU_ENABLE_WAKEUPRESOURCE, 
//                  &dwWakupRes, 
//                  sizeof(DWORD), 
//                  NULL, 
//                  0, 
//                  NULL, 
//                  NULL);
#define IOCTL_TUD_ENABLE_WAKEUPRESOURCE             1

//  DeviceIoControl( IOCTL_TDU_DISABLE_WAKEUPRESOURCE, 
//                  &dwWakupRes, 
//                  sizeof(DWORD), 
//                  NULL, 
//                  0, 
//                  NULL, 
//                  NULL);
#define IOCTL_TUD_DISABLE_WAKEUPRESOURCE             2

//  DeviceIoControl( IOCTL_TDU_UNLOAD_PCIDRIVERS, 
//                  NULL, 
//                  0, 
//                  NULL, 
//                  0, 
//                  NULL, 
//                  NULL);
#define IOCTL_TUD_UNLOAD_PCIDRIVERS             101

//  DeviceIoControl( IOCTL_TDU_UNLOAD_PCIDRIVERS, 
//                  NULL, 
//                  0, 
//                  NULL, 
//                  0, 
//                  NULL, 
//                  NULL);
#define IOCTL_TUD_UNLOAD_PCIDRIVERS_WITHRESET             102

#define IOCTL_TUD_QUERY_NUM_OF_DEVICES	 	1001
#define IOCTL_TUD_RETRIEVE_DEVICE_INFO		1002
#define IOCTL_TUD_UNLOAD_DEVICE				1003
#define IOCTL_TUD_RELOAD_DEVICE				1004
#define IOCTL_TUD_RESET_SYSTEM					1005
#define IOCTL_TUD_ENUM_PCI						1006
#define IOCTL_TUD_ENUM_ALL						1007
#define IOCTL_TUD_LOAD_DEVICE					1008
#define IOCTL_TUD_LAUNCH_APPVER				1009



HANDLE LoadTestUtilDriver(VOID);
BOOL UnloadTestUtilDriver(HANDLE hGen);
HANDLE OpenTestUtilDriver();

