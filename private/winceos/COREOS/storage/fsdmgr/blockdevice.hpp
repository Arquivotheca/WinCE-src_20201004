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
#ifndef __BLOCKDEVICE_HPP__
#define __BLOCKDEVICE_HPP__

LRESULT InitializeBlockDeviceAPI ();

typedef HANDLE (* PINITFN   )(LPVOID);
typedef BOOL   (* PDEINITFN )(HANDLE);
typedef HANDLE (* POPENFN   )(HANDLE, DWORD, DWORD);
typedef BOOL   (* PCLOSEFN  )(HANDLE);
typedef BOOL   (* PCONTROLFN)(HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
typedef void   (* PPOWERUPFN)(void);
typedef void   (* PPOWERDNFN)(void);

class BlockDevice_t
{
protected:
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
    LPWSTR              m_pszIClass;
public:    
    
    BlockDevice_t(const TCHAR *szDriverPath) :
        m_hBlockDevice(NULL),
        m_pInit(NULL),
        m_pDeinit(NULL),
        m_pOpen(NULL),
        m_pClose(NULL),
        m_pControl(NULL),
        m_pPowerdown(NULL),
        m_hDevice(NULL),
        m_pszIClass(NULL)
    {
        StringCchCopyW (m_szDriverPath, MAX_PATH, szDriverPath);
    }
        
    virtual ~BlockDevice_t()
    {
        if (m_hBlockDevice) {
            if (m_pDeinit && m_hDevice) {
                __try {
                    m_pDeinit(m_hDevice);
                } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
                    DEBUGCHK (0);
                }    
            }     
            FreeLibrary( m_hBlockDevice);
        }

        if (m_pszIClass) {
            delete[] m_pszIClass;
        }
    }        
    virtual PFNVOID LoadFunction(const TCHAR *szPrefix, const TCHAR *szSuffix);
    virtual HANDLE OpenBlockDevice();
    virtual void   GetDeviceIClass(HKEY hKey);
    virtual void   AdvertiseInterface(LPCWSTR szProfile, BOOL fAdd);

    inline BOOL Close() 
    {
        if (m_pClose) {
            __try {
                return m_pClose(m_hOpenHandle);
            } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
                return ERROR_ACCESS_DENIED;
            }    
        }     
        return TRUE;
    }
    inline BOOL Control(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED /* lpOverlapped */)
    {
        __try {
            return m_pControl(m_hOpenHandle, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned);
        } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
            return ERROR_ACCESS_DENIED;
        }    
    }
    inline void PowerOn()
    {
        if (m_pPowerup) {
            __try {
                m_pPowerup();
            } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
                DEBUGCHK (0);
            }    
        }    
    }
    inline void PowerOff()
    {
        if (m_pPowerdown) {
            __try {
                m_pPowerdown();
            } __except(ReportFault(GetExceptionInformation(), 0), EXCEPTION_EXECUTE_HANDLER) {
                DEBUGCHK (0);
            }    
        }    
    }
    inline PCTSTR GetDeviceName() { return m_szDriverPath; }
};

BOOL FS_DevCloseFileHandle(BlockDevice_t *pDevice);
BOOL FS_DevDeviceIoControl(BlockDevice_t *pDevice, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
BOOL FS_DevReadFile(BlockDevice_t *pDevice, LPVOID buffer, DWORD nBytesToRead, LPDWORD lpNumBytesRead, LPOVERLAPPED lpOverlapped);
BOOL FS_DevWriteFile(BlockDevice_t *pDevice, LPCVOID buffer, DWORD nBytesToWrite, LPDWORD lpNumBytesWritten, LPOVERLAPPED lpOverlapped);
DWORD FS_DevSetFilePointer(BlockDevice_t *pDevice, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
DWORD FS_DevGetFileSize(BlockDevice_t *pDevice, LPDWORD lpFileSizeHigh);
BOOL FS_DevGetFileInformationByHandle(BlockDevice_t *pDevice, LPBY_HANDLE_FILE_INFORMATION lpFileInfo);
BOOL FS_DevFlushFileBuffers(BlockDevice_t *pDevice);
BOOL FS_DevGetFileTime(BlockDevice_t *pDevice, LPFILETIME lpCreation, LPFILETIME lpLastAccess, LPFILETIME lpLastWrite);
BOOL FS_DevSetFileTime(BlockDevice_t *pDevice, CONST FILETIME *lpCreation, CONST FILETIME *lpLastAccess, CONST FILETIME *lpLastWrite);
BOOL FS_DevSetEndOfFile(BlockDevice_t *pDevice);

#endif //__BLOCKDEVICE_HPP__
