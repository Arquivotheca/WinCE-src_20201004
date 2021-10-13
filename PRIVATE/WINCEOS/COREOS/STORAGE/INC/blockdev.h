//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __BLOCKDEV_H__
#define __BLOCKDEV_H__


#include <windows.h>
#include <storemgr.h>
#include <fsdmgrp.h>

typedef HANDLE (* PINITFN   )(LPCTSTR);
typedef BOOL   (* PDEINITFN )(HANDLE);
typedef HANDLE (* POPENFN   )(HANDLE, DWORD, DWORD);
typedef BOOL   (* PCLOSEFN  )(HANDLE);
typedef BOOL   (* PCONTROLFN)(HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
typedef void   (* PPOWERUPFN)(DWORD);
typedef void   (* PPOWERDNFN)(DWORD);

class CBlockDevice
{
private:
    HMODULE             m_hBlockDevice;
    HANDLE              m_hDevice;
    HANDLE              m_hOpenHandle;
    PINITFN             m_pInit;
    PDEINITFN           m_pDeinit;
    POPENFN             m_pOpen;
    PCLOSEFN            m_pClose;
    PCONTROLFN          m_pControl;
    PPOWERUPFN          m_pPowerup;
    PPOWERDNFN          m_pPowerdown;
    TCHAR               m_szDriverPath[MAX_PATH];
public:    
    CBlockDevice(TCHAR *szDriverPath) :
        m_hBlockDevice(NULL),
        m_pInit(NULL),
        m_pDeinit(NULL),
        m_pOpen(NULL),
        m_pClose(NULL),
        m_pControl(NULL),
        m_pPowerdown(NULL),
        m_hDevice(NULL)
    {
        wcsncpy( m_szDriverPath, szDriverPath, MAX_PATH-1);
        m_szDriverPath[MAX_PATH-1] = 0;
    }
    
    virtual ~CBlockDevice()
    {
        if (m_hBlockDevice)
            FreeLibrary( m_hBlockDevice);
    }        
    PFNVOID LoadFunction(TCHAR *szPrefix, TCHAR *szSuffix);
    HANDLE OpenBlockDevice();
    inline BOOL Close() 
    {
        if (m_pClose) {
            __try {
                return m_pClose(m_hOpenHandle);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                return ERROR_ACCESS_DENIED;
            }    
        }     
        return TRUE;
    }
    inline BOOL Control(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
    {
        __try {
            return m_pControl(m_hOpenHandle, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_ACCESS_DENIED;
        }    
    }
    
};

BOOL FS_DevCloseFileHandle(CBlockDevice *pDevice);
BOOL FS_DevDeviceIoControl(CBlockDevice *pDevice, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
BOOL FS_DevReadFile(CBlockDevice *pDevice, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead,    LPOVERLAPPED lpOverlapped);
BOOL FS_DevWriteFile(CBlockDevice *pDevice, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten,    LPOVERLAPPED lpOverlapped);
DWORD FS_DevSetFilePointer(CBlockDevice *pDevice, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
DWORD FS_DevGetFileSize(CBlockDevice *pDevice, LPDWORD lpFileSizeHigh);
BOOL FS_DevGetFileInformationByHandle(CBlockDevice *pDevice, LPBY_HANDLE_FILE_INFORMATION lpFileInfo);
BOOL FS_DevFlushFileBuffers(CBlockDevice *pDevice);
BOOL FS_DevGetFileTime(CBlockDevice *pDevice, LPFILETIME lpCreation, LPFILETIME lpLastAccess, LPFILETIME lpLastWrite);
BOOL FS_DevSetFileTime(CBlockDevice *pDevice, CONST FILETIME *lpCreation, CONST FILETIME *lpLastAccess, CONST FILETIME *lpLastWrite);
BOOL FS_DevSetEndOfFile(CBlockDevice *pDevice);


#endif //__BLOCKDEV_H__
