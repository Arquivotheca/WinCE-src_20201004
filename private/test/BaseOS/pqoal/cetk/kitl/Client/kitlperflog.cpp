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
#include "KITLPerfLog.h"

LPCWSTR g_optionalMetrics[] = {
    {L"Send Packets"},
    {L"Receive Packets"},
    {L"Absolute Zero"},
    {L"Speed of Light"},
    {NULL}
};

#define KITL_SPEED_OF_LIGHT 299792458

enum {send, receive, min, max} metric;

const WCHAR CKitlPerfLog::m_Scenario_Namespace[] = L"KITL Stress Test";
const WCHAR CKitlPerfLog::m_Scenario_Name[] = L"Send-Receive";

// {312EFFF9-A636-476a-99AB-E0A9B1C5921D}
const GUID CKitlPerfLog::m_guidDiskTest = { 0x312efff9, 0xa636, 0x476a, { 0x99, 0xab, 0xe0, 0xa9, 0xb1, 0xc5, 0x92, 0x1d } };


CKitlPerfLog::CKitlPerfLog(DWORD cThreads, LPCWSTR sessionPath, HMODULE hMod) : BTSPerfLog(cThreads, sessionPath,m_Scenario_Namespace,m_Scenario_Name, m_guidDiskTest, hMod), m_btsLogfile(NULL) 
{
    m_btsLogfile = CopyBTSString(L"Kitl_Test.xml");
}

bool CKitlPerfLog::PerfInitialize()
{
    return BTSPerfLog::PerfInitialize(g_optionalMetrics, m_btsLogfile);
}


bool CKitlPerfLog::PerfWriteLog(HANDLE h, PVOID pv)
{
    return BTSPerfLog::PerfWriteLog(h, pv);
}



bool CKitlPerfLog::PerfFinalize(DWORD dwSent, DWORD dwReceived)
{
    bool fRet1, fRet2, fRet3, fRet4;
    fRet1 = PerfWriteLog(usrDefinedHnd(g_optionalMetrics[send]), reinterpret_cast<PVOID>(dwSent));
    fRet2 = PerfWriteLog(usrDefinedHnd(g_optionalMetrics[receive]), reinterpret_cast<PVOID>(dwReceived));
    fRet3 = PerfWriteLog(usrDefinedHnd(g_optionalMetrics[min]), reinterpret_cast<PVOID>(0));
    fRet4 = PerfWriteLog(usrDefinedHnd(g_optionalMetrics[max]), reinterpret_cast<PVOID>(KITL_SPEED_OF_LIGHT));

    if (fRet1 && fRet2 && fRet3 & fRet4)
    {
        return BTSPerfLog::PerfFinalize();
    }
    return false;
}
