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
#ifndef CAMDRIVERLIBRARY_H
#define CAMDRIVERLIBRARY_H

#include <windows.h>
#include <tchar.h>
#include <pnp.h>
#include "DriverEnumerator.h"
#include <camera.h>
#include <devload.h>

static GUID CLSID_CameraDriver = DEVCLASS_CAMERA_GUID;

#define TESTDRIVER_PATH TEXT("Drivers\\Capture\\TestCam")
#define TESTDRIVER_PREFIX TEXT("CAM")
#define TESTDRIVER_DLL TEXT("TestCam.dll")
#define TESTDRIVER_ORDER 1
#define TESTDRIVER_INDEX 9
#define TESTDRIVER_ICLASS TEXT("{CB998A05-122C-4166-846A-933E4D7E3C86}")

#define TESTDRIVER_PIN_PATH         TEXT("Software\\Microsoft\\Directx\\Directshow\\Capture\\PIN9")
#define TESTDRIVER_PIN_PREFIX TEXT("PIN")
#define TESTDRIVER_PIN_ORDER 1
#define TESTDRIVER_PIN_INDEX 9
#define TESTDRIVER_PIN_ICLASS TEXT("{C9D092D6-827A-45E2-8144-DE1982BFC3A8}")

#define TESTDRIVER_USERMODEPROCGROUP 3

// NULLCAM doesn't work well with other drivers, it's hard coded to index 1.
#define MONOLITHICDRIVER_PATH TEXT("Drivers\\Capture\\NullCam")
#define MONOLITHICDRIVER_PREFIX TEXT("CAM")
#define MONOLITHICDRIVER_DLL TEXT("NullCam.dll")
#define MONOLITHICDRIVER_ORDER 1
#define MONOLITHICDRIVER_INDEX 8
#define MONOLITHICDRIVER_ICLASS TEXT("{CB998A05-122C-4166-846A-933E4D7E3C86}")

#define MONOLITHICDRIVER_PIN_PATH         TEXT("Software\\Microsoft\\Directx\\Directshow\\Capture\\")
#define MONOLITHICDRIVER_PIN_PREFIX TEXT("PIN")
#define MONOLITHICDRIVER_PIN_ORDER 1
#define MONOLITHICDRIVER_PIN_INDEX 8
#define MONOLITHICDRIVER_PIN_ICLASS TEXT("{C9D092D6-827A-45E2-8144-DE1982BFC3A8}")

typedef class CCamDriverLibrary : public CDriverEnumerator
{
    public:
        CCamDriverLibrary();
        ~CCamDriverLibrary();

        HRESULT SetupTestCameraDriver(TCHAR *tszXML, TCHAR * tszProfile, BOOL bUserMode = FALSE);
        HRESULT SetupMonolithicCameraDriver();
        HRESULT Init();
        int GetTestCameraDriverIndex();
        int GetMonolithicCameraDriverIndex();
        HRESULT Cleanup();

    private:
        int m_nTestDriverIndex;
        HANDLE m_hTestDriverHandle;

        int m_nMonolithicDriverIndex;
        HANDLE m_hMonolithicDriverHandle;


        HRESULT AddTestCameraRegKey(TCHAR *tszPath, TCHAR *tszKey, TCHAR *tszData);
        HRESULT AddTestCameraRegKey(TCHAR *tszPath, TCHAR *tszKey, DWORD tszData);

}CAMDRIVERLIBRARY, *PCAMDRIVERLIBRARY;

#endif
