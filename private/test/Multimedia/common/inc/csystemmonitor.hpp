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
/////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////

#pragma once
#include <vector>
static const int INITIAL_BUF_SIZE = 500;

struct SysmonInfo
{
    DWORD MemoryLoad;    // between 0 and 100 (percentage)
    DWORD PhysMemoryUse; // in bytes
    DWORD VirtMemoryUse; // in bytes
    int CpuUse;        // between 0 and 100 (percentage)
};

class SystemMonitor
{
public:
    static SystemMonitor& GetInstance();

    void StartSystemMonitoring(UINT SysMonInterval/* in seconds */);
    void StopSystemMonitoring();

    inline const SysmonInfo& operator[](UINT i) const { return m_sysmonInfo[i]; }
    inline UINT  GetSysmonInfoRecordCount() { return m_sysmonInfo.size(); }

    inline void  SetSysmonTimeInterval(DWORD dwTimeInterval) { m_interval = dwTimeInterval; }
    inline DWORD GetSysmonTimeInterval() { return m_interval; }

    HRESULT LogSystemMonitoringData(WCHAR *pwzFileNamePrefix);
    void AnalyzeSystemMonitoringData();

private:
    // Construction/destruction
    SystemMonitor();
    SystemMonitor(SystemMonitor const&) {};
    SystemMonitor& operator=(SystemMonitor const&) {};
    static int GetCPUUtilization(UINT sleeptime, UINT samples);
    // Private member functions
    static DWORD WINAPI CpuMonThreadProc(LPVOID);

    void            InternalStopSystemMonitoring(bool bThrows);
    void            StopCpuMonitoring(bool bThrows) const;
    inline void     InsertSysmonRecord(SysmonInfo& info) { m_sysmonInfo.push_back(info); }

    HRESULT CreateLogFile(HANDLE  &auto_phFile, WCHAR *pwzFileNamePrefix);
    HRESULT WriteToFile(HANDLE auto_phFile, WCHAR *wzBuffer);

    // Private member data
    typedef std::vector<SysmonInfo> SysmonInfoList;
    SysmonInfoList  m_sysmonInfo;
    volatile bool   m_bMonitoring;
    volatile DWORD  m_interval;
    HANDLE          m_hThread;
};

struct ScopedMonitor
{
    ScopedMonitor(UINT interval /*in seconds */) :
        sm(SystemMonitor::GetInstance())
    {
        sm.StartSystemMonitoring(interval);
    }
    ~ScopedMonitor()
    {
        sm.StopSystemMonitoring();
    }
private:
    SystemMonitor& sm;
};
