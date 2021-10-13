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

    refl_imp.h

Abstract:

    User Mode Driver Reflector implementation.

Revision History:

--*/

#pragma once
#include <CRefCon.h>
#include <ceddk.h>
#include <cebus.h>
#include <Reflector.h>
#include <ddkreg.h>

class CFileFolder ;
class CReflector ;
class UserDriverProcessor;
class CPhysMemoryWindow;
class CPhysMemoryWindow {
public:
    CPhysMemoryWindow( PHYSICAL_ADDRESS PhysicalAddress, ULONG NumberOfBytes,ULONG AddressSpace,INTERFACE_TYPE  InterfaceType,ULONG BusNumber, HANDLE hParentBus,CPhysMemoryWindow * pNext = NULL );
    BOOL Init() { return (m_PhSystemAddresss.QuadPart!=0);};
    BOOL IsValidStaticAddr(DWORD dwPhysAddr,DWORD dwSize,ULONG AddressSpace);
    BOOL IsSystemAddrValid(PHYSICAL_ADDRESS phPhysAddr,DWORD dwSize);
    BOOL IsSystemAlignedAddrValid(PHYSICAL_ADDRESS phPhysAddr,DWORD dwSize);
    CPhysMemoryWindow * GetNextObject() { return m_pNextFileFolder ;};
    void SetNextObject (CPhysMemoryWindow *pNext) { m_pNextFileFolder = pNext; };
    PVOID   RefMmMapIoSpace (PHYSICAL_ADDRESS PhysicalAddress,ULONG NumberOfBytes, BOOLEAN CacheEnable);
    PVOID   RefCreateStaticMapping(DWORD dwPhisAddrShift8, DWORD dwAlignSize);
    HANDLE  RefLoadIntChainHandler(LPCTSTR lpIsrDll, LPCTSTR lpIsrHandler, BYTE bIrq);
protected:
    CPhysMemoryWindow * m_pNextFileFolder;
    PHYSICAL_ADDRESS m_PhBusAddress, m_PhSystemAddresss;
    ULONG       m_AddressSpace;
    DWORD       m_dwSize;
//    PVOID       m_pMappedUserPtr;
    // Installable ISR static mapped
    PVOID       m_pStaticMappedUserPtr;
    DWORD       m_dwStaticMappedLength;
//    DWORD       m_dwRefCount;
};
class CFileFolder {
public:
//Entry
    CFileFolder(CReflector * pReflector, DWORD AccessCode, DWORD ShareMode, CFileFolder *  pNextFileFolder ) ;
    virtual ~CFileFolder();
    virtual BOOL Init() {  return (m_pReflector!=NULL && m_dwUserData!=0); };
    BOOL PreClose();
    DWORD Read(LPVOID buffer, DWORD nBytesToRead,HANDLE hIoRef) ;
    DWORD Write(LPCVOID buffer, DWORD nBytesToWrite,HANDLE hIoRef) ;
    DWORD SeekFn(long lDistanceToMove, DWORD dwMoveMethod) ;
    BOOL Control(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned,HANDLE hIoRef);
    BOOL CancelIo(HANDLE hIoHandle);
    CReflector * GetReflector() { return m_pReflector; };
    CFileFolder * GetNextObject() { return m_pNextFileFolder ;};
    void SetNextObject (CFileFolder *pNext) { m_pNextFileFolder = pNext; };
protected:    
    CReflector * m_pReflector;
    DWORD m_dwUserData;
    CFileFolder * m_pNextFileFolder;
    LPVOID  AllocCopyBuffer(LPVOID lpOrigBuffer, DWORD dwLength, BOOL fInput,LPVOID& KernelAddress);
    BOOL    FreeCopyBuffer(LPVOID lpMappedBuffer, __in_bcount(dwLength) LPVOID lpOrigBuffer,DWORD dwLength, BOOL fInput,LPVOID KernelAddress);
};

class CReflector  :public CRefObject,  public CLockObject{
public:
//Entry:
    CReflector(UserDriverProcessor * pUDP,LPCTSTR lpszDeviceKey, LPCTSTR lpPreFix, LPCTSTR lpDrvName,DWORD dwFlags, CReflector * pNext = NULL );
    virtual ~CReflector();
    virtual BOOL Init() {
        return (m_pUDP!=NULL && m_dwData!=0);
    }
    virtual BOOL InitEx(DWORD dwInfo, LPVOID lpvParam);
    BOOL DeInit();
    BOOL PreDeinit() {
        return SendIoControl(IOCTL_USERDRIVER_PREDEINIT,&m_dwData, sizeof(m_dwData),NULL, NULL, NULL);
    }
    virtual CFileFolder* Open(DWORD AccessCode, DWORD ShareMode);
    virtual BOOL Close (CFileFolder* pFileFolder);
    void Powerup() {
        //SendIoControl(IOCTL_USERDRIVER_POWERUP,&m_dwData, sizeof(m_dwData),NULL, NULL, NULL);
    }
    void Powerdn() {
        //SendIoControl(IOCTL_USERDRIVER_POWERDOWN,&m_dwData, sizeof(m_dwData),NULL, NULL, NULL);
    }
    BOOL SendIoControl(DWORD dwIoControlCode,LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned) ;
    UserDriverProcessor * GetUDP() { return m_pUDP; };
    DWORD GetDriverData() { return m_dwData; };
    BOOL IsBufferShouldMapped(LPCVOID ptr, BOOL fWrite);
    BOOL ReflService( DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) ;
protected:
    BOOL FnDriverLoad(FNDRIVERLOAD_PARAM& DriverLoadParam, FNDRIVERLOAD_RETURN& driversReturn) {
        return SendIoControl(IOCTL_USERDRIVER_LOAD,&DriverLoadParam, sizeof(DriverLoadParam),&driversReturn, sizeof(FNDRIVERLOAD_RETURN) ,NULL);
    }
    BOOL FnDriverExit(DWORD dwData) {
        return SendIoControl(IOCTL_USERDRIVER_EXIT, &dwData,sizeof(dwData), NULL, 0, NULL);
    }
    BOOL FnInit(FNINIT_PARAM& InitParam ) {
        return SendIoControl(IOCTL_USERDRIVER_INIT,&InitParam,sizeof(InitParam), NULL ,0 ,NULL);
    }
    BOOL FnDeInit(DWORD dwData) {
        return SendIoControl(IOCTL_USERDRIVER_DEINIT, &dwData,sizeof(dwData),NULL,0, NULL);
    }
    DWORD   m_dwData;
    UserDriverProcessor *m_pUDP;
    BOOL    InitAccessWindow( DDKWINDOWINFO& dwi, HANDLE hParentBus );
    PVOID   RefMmMapIoSpace (PHYSICAL_ADDRESS PhysicalAddress,ULONG NumberOfBytes, BOOLEAN CacheEnable);
    BOOL    RefVirtualCopy(LPVOID lpvDest,LPVOID lpvSrc,DWORD cbSize,DWORD fdwProtect) ;

    HANDLE  RefLoadIntChainHandler(LPCTSTR lpIsrDll, LPCTSTR lpIsrHandler, BYTE bIrq);
    BOOL    RefFreeIntChainHandler(HANDLE hRetHandle) ;
    PVOID   RefCreateStaticMapping(DWORD dwPhysBase,DWORD dwAlignSize);
    BOOL    RefIntChainHandlerIoControl(HANDLE hRetHandle, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, const LPDWORD lpBytesReturned ) ;
private:
    BOOL    IsValidStaticAddr(DWORD dwPhysAddr,DWORD dwSize, ULONG AddressSpace);
    CPhysMemoryWindow *m_pPhysicalMemoryWindowList ;
// Linker:
public:
    CReflector * GetNextObject() { return m_pNextReflector ;};
    void SetNextObject (CReflector *pNext) { m_pNextReflector = pNext; };
private:
    CReflector * m_pNextReflector;
    BOOL        m_fUserInit;
// FileFolder;
public:
    CFileFolder * FindFileFolderBy( CFileFolder * pFileFolder);
    CFileFolder * m_pFileFolderList;
private:
    DDKISRINFO m_DdkIsrInfo;
    HANDLE m_hInterruptHandle;
    HANDLE m_hUDriver;
    // Installable ISR.
    HANDLE      m_hIsrHandle;
};

class ReflectorContainer: public CLinkedContainer <CReflector>
{
};

