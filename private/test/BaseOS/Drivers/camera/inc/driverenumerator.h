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
#ifndef DRIVERENUMERATOR_H
#define DRIVERENUMERATOR_H

#include <windows.h>
#include <tchar.h>
#include <pnp.h>

typedef class CDriverEnumerator
{
    public:
        CDriverEnumerator();
        virtual ~CDriverEnumerator();

        HRESULT Init(const GUID *DriverGuid);
        HRESULT GetDriverList(TCHAR ***tszCamDeviceName, int *nEntryCount);
        int GetDeviceIndex(TCHAR *tszDeviceName);
        TCHAR *GetDeviceName(int nDeviceIndex);
        int GetMaxDeviceIndex();

    private:
        void Cleanup();

        TCHAR **m_ptszDevices;
        int m_nDeviceCount;
}DRIVERENUMERATOR, *PDRIVERENUMERATOR;

#endif
