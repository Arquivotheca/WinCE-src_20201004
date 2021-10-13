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
//
#pragma once 

#include <wtypes.h>
#include <ceddk.h>
#include <cebus.h>
#include <sensor.h>
#include "snsDebug.h"
#include "snsDefines.h"


struct SnsRegistryEntry
{
    TCHAR szMddDll[MAX_PATH];
    TCHAR szPddDll[MAX_PATH];
    TCHAR szPrefix[MAX_PATH];
    TCHAR szIClass[1024];
    DWORD dwOrder;
};


struct AccRegistryEntry : SnsRegistryEntry
{
    TCHAR szI2CDriver[MAX_PATH];
    DWORD dwI2CAddr;
    DWORD dwI2CBus;
    DWORD dwI2CSpeed;
    DWORD dwI2COptions;
};
    

void DumpAllAvailableSensorInformation();
BOOL isSensorPresent(LPCTSTR pszPrefix);

void ListAllSensors();

void GetSnsRegistryInfo( SnsRegistryEntry & snsInfo, LPCTSTR pszKeyEntry );


void GetSnsRegistryInfo( SnsRegistryEntry & snsInfo );
void GetAccRegistryInfo( AccRegistryEntry & accInfo );

void DumpSnsRegistryInfo( SnsRegistryEntry & snsInfo );
void DumpAccRegistryInfo( AccRegistryEntry & accInfo );

void DumpDevMgrDevInfo( PDEVMGR_DEVICE_INFORMATION di, LPCTSTR pszPrefix, DWORD dwCount );


