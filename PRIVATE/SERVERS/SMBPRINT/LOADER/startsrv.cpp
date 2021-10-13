//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//
// Program to start and stop SMB server
//

#include <windows.h>
#include <service.h>
#include <pegdpar.h>
#include <encrypt.h>

//#include "utils.h"

typedef int (*INITFUNCTION)(DWORD);


DWORD
SMB_Init(
    DWORD dwContext
    );
DWORD
SMB_Deinit(
    DWORD dwClientContext
    );

BOOL
SMB_IOControl(
    DWORD dwOpenContext,
    DWORD dwIoControlCode,
    PBYTE pInBuf,
    DWORD nInBufSize,
    PBYTE pOutBuf,
    DWORD nOutBufSize,
    PDWORD pBytesReturned
    );





int WINAPI WinMain(
    HINSTANCE hInstance, 
    HINSTANCE hPrevInstance, 
    LPWSTR lpCmdLine, 
    int nShowCmd)
{    

DEBUGREGISTER(NULL);
#ifdef SMB_AS_DRIVER
    HANDLE hSrvDev = INVALID_HANDLE_VALUE;
#endif 

    DllMain(INVALID_HANDLE_VALUE,DLL_PROCESS_ATTACH,0);
                   
       
    fputws (L"Type \"help\" for command list...\n", stdout);
    for(;;) {
         WCHAR sz[1000];
         fputws (L"Command> ", stdout);
         fgetws (sz, sizeof(sz) / sizeof(sz[0]), stdin);

         WCHAR *p = wcschr (sz, L'\n');
         if (p)
               *p = '\0';

         if (sz[0] == '\0')
                 continue;
     if (wcsicmp (sz, L"help") == 0) {
           fputws(L"cmds are load/stop/start/reload/exit\n", stdout);
     }
   
    if (wcsicmp (sz, L"load") == 0) {    
        #ifdef SMB_AS_DRIVER
            hSrvDev = CreateFile(L"SRV1:", 
                      GENERIC_READ | GENERIC_WRITE, 
                      0, 
                      NULL, 
                      OPEN_EXISTING,
                      0, NULL);
            if(INVALID_HANDLE_VALUE == hSrvDev) {
                fputws(L"error loading srv1:", stdout);
            } 
        #else
            SMB_Init(0);
        #endif
    }
    if (wcsicmp (sz, L"reload") == 0) {    
        #ifdef SMB_AS_DRIVER
            if(INVALID_HANDLE_VALUE == hSrvDev) {
                fputws(L"must load first", stdout);
            } else {
                DeviceIoControl(hSrvDev,IOCTL_SERVICE_REFRESH, NULL,0,0,0,0,NULL); 
            }
        #else
            SMB_IOControl(0, IOCTL_SERVICE_REFRESH, NULL,0,0,0,NULL);
        #endif
    }
    
     if (wcsicmp (sz, L"start") == 0) {    
        #ifdef SMB_AS_DRIVER
            if(INVALID_HANDLE_VALUE == hSrvDev) {
                fputws(L"must load first", stdout);
            } else {
                DeviceIoControl(hSrvDev,IOCTL_SERVICE_START, NULL,0,0,0,0,NULL); 
            }
        #else
            SMB_IOControl(0, IOCTL_SERVICE_START, NULL,0,0,0,NULL);
        #endif
    }
    
     if (wcsicmp (sz, L"stop") == 0) {    
        #ifdef SMB_AS_DRIVER
            if(INVALID_HANDLE_VALUE == hSrvDev) {
                fputws(L"must load first", stdout);
            } else {
                DeviceIoControl(hSrvDev,IOCTL_SERVICE_STOP, NULL,0,0,0,0,NULL); 
            }
        #else
            SMB_IOControl(0, IOCTL_SERVICE_STOP, NULL,0,0,0,NULL);
        #endif
    }
    if (wcsicmp (sz, L"exit") == 0) {
        #ifdef SMB_AS_DRIVER
            if(INVALID_HANDLE_VALUE == hSrvDev) {
                fputws(L"must load first", stdout);
            } else {
                DeviceIoControl(hSrvDev,IOCTL_SERVICE_STOP, NULL,0,0,0,0,NULL); 
            }
        #else
            SMB_IOControl(0, IOCTL_SERVICE_STOP, NULL,0,0,0,NULL);
        #endif
        break;    
    }   
    }  
    
    

#ifdef SMB_AS_DRIVER    
//    DeregisterDevice(hSrvDev);
#else
    SMB_Deinit(0);   
#endif    
    
    return 0;   
}   // main
