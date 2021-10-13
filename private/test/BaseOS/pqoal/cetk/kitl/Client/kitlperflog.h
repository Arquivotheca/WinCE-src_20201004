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
#pragma once
#include "BTSPerfLog.h"

#pragma warning(push)
#pragma warning(disable: 4512 )
extern LPCWSTR g_optionalMetrics[];
class CKitlPerfLog : public BTSPerfLog
{
public:
    CKitlPerfLog(DWORD cThreads, LPCWSTR sessionPath, HMODULE hMod);
    bool PerfInitialize();
    bool PerfFinalize(DWORD dwSent, DWORD dwReceived);
    bool PerfWriteLog(HANDLE h, PVOID pv);

private:
    LPCWSTR m_btsLogfile;
    static const WCHAR m_Scenario_Namespace[];
    static const WCHAR m_Scenario_Name[];
    static const GUID m_guidDiskTest;
};
#pragma warning(pop)
