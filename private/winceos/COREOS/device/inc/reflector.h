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
/*++


Module Name:

    reflector.h

Abstract:

    User Mode Driver Reflector

Revision History:

--*/
#pragma once
#include <devmgrp.h>

#if defined (__cplusplus)
extern "C" {
#endif
    BOOL IsServicesRegKey(LPCWSTR lpszRegKeyName);

#if defined (__cplusplus)
}
#endif

#define _USERDRIVERACCESS_CTL_CODE(_Function)  \
            CTL_CODE(FILE_DEVICE_USERDRIVER, _Function, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_USERDRIVER_INIT       _USERDRIVERACCESS_CTL_CODE(1)
typedef struct {
    DWORD   dwCallerProcessId; 
    DWORD   dwDriverContent;
    DWORD   dwInfo;
    LPVOID  lpvParam;
    TCHAR   ActiveRegistry[MAX_PATH];
} FNINIT_PARAM, *PFNINIT_PARAM;

#define IOCTL_USERDRIVER_DEINIT     _USERDRIVERACCESS_CTL_CODE(2)
//dwContent Only

#define IOCTL_USERDRIVER_PREDEINIT  _USERDRIVERACCESS_CTL_CODE(3)
//dwContent Only

#define IOCTL_USERDRIVER_POWERDOWN  _USERDRIVERACCESS_CTL_CODE(4)
//dwContent Only

#define IOCTL_USERDRIVER_POWERUP    _USERDRIVERACCESS_CTL_CODE(5)
//dwContent Only

#define IOCTL_USERDRIVER_OPEN       _USERDRIVERACCESS_CTL_CODE(6)
typedef struct {
    DWORD   dwCallerProcessId; 
    DWORD   dwDriverContent;
    DWORD   AccessCode;
    DWORD   ShareMode;
} FNOPEN_PARAM, *PFNOPEN_PARAM;

#define IOCTL_USERDRIVER_CLOSE      _USERDRIVERACCESS_CTL_CODE(7)
//    DWORD   dwContent;

#define IOCTL_USERDRIVER_PRECLOSE   _USERDRIVERACCESS_CTL_CODE(8)
//    DWORD   dwContent;

#define IOCTL_USERDRIVER_READ       _USERDRIVERACCESS_CTL_CODE(9)
#define IOCTL_USERDRIVER_WRITE      _USERDRIVERACCESS_CTL_CODE(0xa)
typedef struct {
    DWORD   dwCallerProcessId; 
    DWORD   dwContent;
    LPVOID  buffer;
    DWORD   nBytes ;
    DWORD   dwReturnBytes;
    HANDLE  hIoRef;
} FNREAD_PARAM, FNWRITE_PARAM, *PFNREAD_PARAM, *PFNWRITE_PARAM ;


#define IOCTL_USERDRIVER_SEEK       _USERDRIVERACCESS_CTL_CODE(0xb)
typedef struct {
    DWORD   dwCallerProcessId; 
    DWORD   dwContent;
    LONG    lDistanceToMove;
    DWORD   dwMoveMethod;
    DWORD   dwReturnBytes;
} FNSEEK_PARAM, *PFNSEEK_PARAM;


#define IOCTL_USERDRIVER_IOCTL      _USERDRIVERACCESS_CTL_CODE(0xc)
typedef struct {
    DWORD   dwCallerProcessId; 
    DWORD   dwContent;
    DWORD   dwIoControlCode;
    LPVOID  lpInBuf;
    DWORD   nInBufSize;
    LPVOID  lpOutBuf;
    DWORD   nOutBufSize;
    BOOL    fUseBytesReturned;
    DWORD   BytesReturned;
    HANDLE  hIoRef;
} FNIOCTL_PARAM, *PFNIOCTL_PARAM ;

#define IOCTL_SERVICES_IOCTL        _USERDRIVERACCESS_CTL_CODE(0xd)
// Uses FNIOCTL_PARAM as its argument

#define IOCTL_USERDRIVER_CANCELIO   _USERDRIVERACCESS_CTL_CODE(0xe)
typedef struct {
    DWORD   dwCallerProcessId; 
    DWORD   dwContent;
    HANDLE  hIoHandle;
} FNCANCELIO_PARAM,*PFNCANCELIO_PARAM;


#define IOCTL_USERDRIVER_LOAD       _USERDRIVERACCESS_CTL_CODE(0x100)
typedef struct {
    DWORD dwAccessKey;
    DWORD dwFlags;
    TCHAR Prefix[PREFIX_CHARS+1];
    TCHAR DriverName[DEVDLL_LEN];    
    TCHAR DeviceRegPath[DEVKEY_LEN];
} FNDRIVERLOAD_PARAM, *PFNDRIVERLOAD_PARAM ;
typedef struct {
    DWORD   dwDriverContext;
    HANDLE  hDriversAccessHandle;
} FNDRIVERLOAD_RETURN, *PFNDRIVERLOAD_RETURN ;
#define IOCTL_USERDRIVER_EXIT       _USERDRIVERACCESS_CTL_CODE(0x101)
// dwContent only



#define IOCTL_USERPROCESSOR_EXIT       _USERDRIVERACCESS_CTL_CODE(0x200)
// No Argument.
#define IOCTL_USERPROCESSOR_ALIVE       _USERDRIVERACCESS_CTL_CODE(0x201)
// No Argument



#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
DWORD Reflector_Create(LPCTSTR lpszDeviceKey, LPCTSTR lpPreFix, LPCTSTR lpDrvName,DWORD dwFlags);
BOOL  Reflector_Delete(DWORD dwDataEx);
DWORD Reflector_InitEx( DWORD dwData, DWORD dwInfo, LPVOID lpvParam );
BOOL Reflector_PreDeinit(DWORD dwData );
BOOL Reflector_Deinit(DWORD dwData);
DWORD Reflector_Open(DWORD dwData, DWORD AccessCode, DWORD ShareMode);
BOOL Reflector_PreClose(DWORD dwOpenData);
BOOL Reflector_Close(DWORD dwOpenData);
DWORD Reflector_Read(DWORD dwOpenData,LPVOID buffer, DWORD nBytesToRead,HANDLE hIoRef) ;
DWORD Reflector_Write(DWORD dwOpenData,LPCVOID buffer, DWORD nBytesToWrite,HANDLE hIoRef) ;
DWORD Reflector_SeekFn(DWORD dwOpenData,long lDistanceToMove, DWORD dwMoveMethod) ;
BOOL Reflector_Control(DWORD dwOpenData,DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned,HANDLE hIoRef);
void Reflector_Powerup(DWORD dwData) ;
void Reflector_Powerdn(DWORD dwData) ;
BOOL Reflector_CancelIo(DWORD dwOpenData,HANDLE hIoHandle);
PROCESS_INFORMATION  Reflector_GetOwnerProcId(DWORD dwDataEx);
LPCTSTR  Reflector_GetAccountId(DWORD dwDataEx);

BOOL InitUserProcMgr();
BOOL DeiniUserProcMgr();
BOOL DM_REL_UDriverProcIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);

#ifdef __cplusplus
}
#endif // __cplusplus

