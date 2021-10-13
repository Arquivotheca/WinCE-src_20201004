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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:

    udevice.cpp

Abstract:

    User Mode Driver Reflector implementation.

Revision History:

--*/


#pragma once
#include <CRefCon.h>
class UserDriver;
typedef struct __DRIVERFILECONTENT *PDRIVERFILECONTENT;
typedef struct __DRIVERFILECONTENT {
    PDRIVERFILECONTENT  pNextFile;
    DWORD               dwFileContent;
} *PDRIVERFILECONTENT, DRIVERFILECONTENT;
class UserDriver;
class UserDriver : public CRefObject, public CLockObject {
public:
    UserDriver(FNDRIVERLOAD_PARAM& fnDriverLoad_Param,UserDriver * pNext);
    virtual ~UserDriver();
    virtual BOOL    Init() { return LoadDriver(); };
    virtual BOOL    DriverInit(FNINIT_PARAM& fnInitParam);
    virtual BOOL    DriverDeinit();
    virtual BOOL    DriverPreDeinit();
    virtual BOOL    DriverPowerDown();
    virtual BOOL    DriverPowerUp();
    virtual DWORD   CreateDriverFile(FNOPEN_PARAM& fnOpenParam);
    virtual BOOL    DeleteDriverFile(DWORD dwFileContent);
    virtual BOOL    IsContainFileContent(DWORD dwFileContent);
    virtual DWORD   GetDriverFileContent(DWORD dwFileContent);
    virtual BOOL    PreClose(DWORD dwFileContent);
    virtual BOOL    Read(FNREAD_PARAM& fnReadParam);
    virtual BOOL    Write(FNWRITE_PARAM& fnWriteParam);
    virtual BOOL    Seek(FNSEEK_PARAM& fnSeekParam);
    virtual BOOL    Ioctl(FNIOCTL_PARAM& fnIoctlParam);
    HANDLE          GetUDriverHandle() { return m_hUDriver; };
    HANDLE          SetUDriverHandle(HANDLE hDriver) { return (m_hUDriver = hDriver); };
    UserDriver *    GetNextObject() { return m_pNext; };
    UserDriver *    SetNextObject(UserDriver * pNext) { return (m_pNext= pNext); };
    void            DetermineReflectorHandle(const TCHAR *ActiveRegistry);

    virtual void FreeDriverLibrary() {
        if (m_hLib)
            FreeLibrary(m_hLib);
    }
protected:
    FNDRIVERLOAD_PARAM m_fnDriverLoadParam;
    PDRIVERFILECONTENT m_pUserDriverFileList ;

    HINSTANCE   m_hLib;             // driver dll handle
    // driver entry points
    pInitFn     m_fnInit;             // required
    pPreDeinitFn m_fnPreDeinit;   // optional
    pDeinitFn   m_fnDeinit;         // required
    pOpenFn     m_fnOpen;             // if present, need close and at least one of read/write/seek/control
    pPreCloseFn m_fnPreClose;     // optional even if close is present
    pCloseFn    m_fnClose;           // required if open is present
    pReadFn     m_fnRead;
    pWriteFn    m_fnWrite;
    pSeekFn     m_fnSeek;
    pControlFn  m_fnControl;
    pPowerupFn  m_fnPowerup;       // optional, can be NULL
    pPowerdnFn  m_fnPowerdn;       // optional, can be NULL
    
    DWORD m_dwInitData;
    HANDLE m_hReflector;
    HANDLE m_hUDriver;
// Internal Function
    BOOL LoadDriver();
    PFNVOID GetDMProcAddr(LPCWSTR type, LPCWSTR lpszName, HINSTANCE hInst) ;
    UserDriver * m_pNext ;

};

class UserDriverContainer: public CLinkedContainer <UserDriver> { 
public:
     BOOL InsertNewDriverObject(UserDriver *pNewDriver);
     BOOL DeleteDriverObject(UserDriver * pDriver);
     UserDriver * FindDriverByFileContent(DWORD dwFileContent) ;
     UserDriver * GetFirstDriver();
};

extern UserDriverContainer * g_pUserDriverContainer;

// Class Factory
UserDriver * CreateUserModeDriver(FNDRIVERLOAD_PARAM& fnDriverLoadParam);

// Help function from main.
extern BOOL WaitForPrimaryThreadExit(DWORD dwTicks);
extern BOOL RegisterAFSAPI (LPCTSTR VolString);
extern void  UnRegisterAFSAPI();

