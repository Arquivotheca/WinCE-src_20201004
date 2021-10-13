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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Implementation of the NetlogManager_t class.
//
// ----------------------------------------------------------------------------

#include <NetlogManager_t.hpp>

#include <cmnprint.h>
#include <netcmn.h>

#include "ntddndis.h" // For Ndis Ioctls
#include <inc/afdfunc.h> // For RasIOControl
#include <inc/netlogioctl.h> // For Netlog Ioctls

#include <assert.h>
#include <inc/sync.hxx>

using namespace ce;
using namespace ce::qa;

// ----------------------------------------------------------------------------
// 
// Local data:
//

// Current netlog driver status:
static NETLOG_GLOBAL_STATE NetlogDriverState;
static bool                NetlogDriverStateCurrent = false;

// Synchronization objects for controlling device access:
static ce::critical_section NetlogDeviceLocker;

// ----------------------------------------------------------------------------
// 
// Constructor.
//
NetlogManager_t::
NetlogManager_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
NetlogManager_t::
~NetlogManager_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Sends the specified command to the netlog device-driver.
//
static bool 
SendCommandToNetlog(
    DWORD  Command,
    void *pInBuffer,
    DWORD  InBufferSize,
    void *pOutBuffer, 
    DWORD  OutBufferSize,
    DWORD *pReturnSize)
{
    bool result = false;

    HANDLE driverHandle = RegisterDevice(L"NLG", 0, L"netlog.dll", 0);

    HANDLE fileHandle = CreateFile(L"NLG0:",
                                   GENERIC_READ,
                                   FILE_SHARE_READ,
                                   NULL,
                                   OPEN_ALWAYS,
                                   0,
                                   NULL);
    if (INVALID_HANDLE_VALUE != fileHandle)
    {
        if (DeviceIoControl(fileHandle,
                            Command,
                           pInBuffer,
                            InBufferSize,
                           pOutBuffer,
                            OutBufferSize,
                           pReturnSize,
                            NULL))
        {
            result = true;
        }
        
        CloseHandle(fileHandle);
        
        DeregisterDevice(driverHandle);
    }
   
    return result;
}

// ----------------------------------------------------------------------------
//
// Sends the specified command to the cxport (CTE) device-driver.
//
bool 
SendCommandToCxport (
    DWORD   Command,
    void  *pInBuffer,
    DWORD   InBufferSize,
    void  *pOutBuffer,
    DWORD   OutBufferSize,
    DWORD *pReturnSize)
{
    bool result = false;

    HANDLE driverHandle = RegisterDevice(L"CTE", 0, L"cxport.dll", 0);

    HANDLE fileHandle = CreateFile(L"CTE0:",
                                   GENERIC_READ,
                                   FILE_SHARE_READ,
                                   NULL,
                                   OPEN_ALWAYS,
                                   0,
                                   NULL);
    if (INVALID_HANDLE_VALUE != fileHandle)
    {
        if (DeviceIoControl(fileHandle,
                            Command,
                           pInBuffer,
                            InBufferSize,
                           pOutBuffer,
                            OutBufferSize,
                           pReturnSize,
                            NULL))
        {
            result = true;
        }
        
        CloseHandle(fileHandle);
        
        DeregisterDevice(driverHandle);
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Gets the current netlog driver status.
//
static NETLOG_GLOBAL_STATE *
GetNetlogState(void)
{
    if (!NetlogDriverStateCurrent)
    {
        ce::gate<ce::critical_section> locker(NetlogDeviceLocker);
        if (!NetlogDriverStateCurrent)
        {
            DWORD resultSize = 0;
            if (!SendCommandToNetlog(IOCTL_NETLOG_GET_STATE, NULL, 0,
                                     &NetlogDriverState,
                               sizeof(NetlogDriverState), &resultSize))
            {
                CmnPrint(PT_FAIL,
                         L"Failed getting netlog driver status: err=%s",
                         GetLastErrorText());
            }
            else
            {
                if (sizeof(NetlogDriverState) == resultSize)
                {
                    NetlogDriverStateCurrent = true;
                    return &NetlogDriverState;
                }
                CmnPrint(PT_FAIL, 
                         L"Received %u-byte netlog status msg; s/b %u bytes",
                         resultSize, sizeof(NetlogDriverState));
            }
        }
    }
    return &NetlogDriverState;
}

// ----------------------------------------------------------------------------
//
// Determines whether logging is currently enabled.
//
bool
NetlogManager_t::
IsLoggingEnabled(void)
{
    return GetNetlogState()->bStopped == FALSE;
}

// ----------------------------------------------------------------------------
//
// Gets or sets the output file base-name.
//
const WCHAR *
NetlogManager_t::
GetFileBaseName(void)
{
    return GetNetlogState()->wszCapFileBaseName;
}
bool
NetlogManager_t::
SetFileBaseName(const WCHAR *NewName)
{
    CmnPrint(PT_LOG, 
             L"Setting netlog capture-file base-name to \"%s\"", 
             NewName);
    
    assert(NULL  != NewName);
    assert(L'\0' != NewName[0]);
    if (NULL != NewName && L'\0' != NewName[0])
    {
        ce::gate<ce::critical_section> locker(NetlogDeviceLocker);
        NetlogDriverStateCurrent = false;
        
        if (SendCommandToNetlog(IOCTL_NETLOG_FILE_SET_PARAM,
                                const_cast<WCHAR *>(NewName), wcslen(NewName), 
                                NULL, 0, NULL))
        {
            return true;
        }
    }

    CmnPrint(PT_FAIL,
             L"Failed setting netlog file base-name to \"%s\": err=%s",
             NewName, GetLastErrorText());
    return false;
}

// ----------------------------------------------------------------------------
//
// Gets or sets the maximum size of each output file.
//
long
NetlogManager_t::
GetMaximumFileSize(void)
{
    return GetNetlogState()->dwHalfCaptureSize;
}
bool
NetlogManager_t::
SetMaximumFileSize(long NewSize)
{
    CmnPrint(PT_LOG, 
             L"Setting netlog capture-file size to %ld", 
             NewSize);
    
    assert(0L < NewSize);
    if (0L < NewSize)
    {
        DWORD dwSize = NewSize;
        
        ce::gate<ce::critical_section> locker(NetlogDeviceLocker);
        NetlogDriverStateCurrent = false;
        
        if (SendCommandToNetlog(IOCTL_NETLOG_HALF_CAPTURE_SIZE,
                                &dwSize, sizeof(dwSize), 
                                NULL, 0, NULL))
        {
            return true;
        }
    }

    CmnPrint(PT_FAIL,
             L"Failed setting netlog capture-file size to %ld: err=%s",
             NewSize, GetLastErrorText());
    return false;
}

// ----------------------------------------------------------------------------
//
// Gets or sets the maximum netlog packet size.
//
long
NetlogManager_t::
GetMaximumPacketSize(void)
{
    return GetNetlogState()->dwMaxPacketSize;
}
bool
NetlogManager_t::
SetMaximumPacketSize(long NewSize)
{
    CmnPrint(PT_LOG, 
             L"Setting netlog maximum packet size to %ld", 
             NewSize);
    
    assert(0L < NewSize);
    if (0L < NewSize)
    {
        DWORD dwSize = NewSize;
        
        ce::gate<ce::critical_section> locker(NetlogDeviceLocker);
        NetlogDriverStateCurrent = false;
        
        if (SendCommandToNetlog(IOCTL_NETLOG_CAPTURE_PACKET_SIZE,
                                &dwSize, sizeof(dwSize), 
                                NULL, 0, NULL))
        {
            return true;
        }
    }
    
    CmnPrint(PT_FAIL,
             L"Failed setting netlog maximum packet size to %ld: err=%s",
             NewSize, GetLastErrorText());
    return false;
}

// ----------------------------------------------------------------------------
//
// Gets or sets the current state of USB tracing.
//
bool
NetlogManager_t::
IsLoggingUSBPackets(void)
{
    return GetNetlogState()->bLogUSB == TRUE;
}
bool
NetlogManager_t::
SetLoggingUSBPackets(bool LogState)
{
    CmnPrint(PT_LOG, 
             L"Setting netlog USB capture state to %s", 
             (LogState? L"true" : L"false"));
    
    BOOL bState = LogState;
    
    ce::gate<ce::critical_section> locker(NetlogDeviceLocker);
    NetlogDriverStateCurrent = false;

    if (SendCommandToNetlog(IOCTL_NETLOG_LOG_USB,
                            &bState, sizeof(bState), 
                            NULL, 0, NULL))
    {
        return true;
    }
    
    CmnPrint(PT_FAIL,
             L"Failed setting netlog USB capture state to %s: err=%s",
             (LogState? L"true" : L"false"), GetLastErrorText());
    return false;
}

// ----------------------------------------------------------------------------
//
// Gets the current capture-file index.
//
int
NetlogManager_t::
GetCaptureFileIndex(void)
{
    NetlogDriverStateCurrent = false;  // force a status update
    return GetNetlogState()->dwCapFileIndex;
}

// ----------------------------------------------------------------------------
//
// Loads the netlog driver.
//
bool
NetlogManager_t::
Load(void)
{
    CmnPrint(PT_LOG, L"Loading netlog driver");
    
    ce::gate<ce::critical_section> locker(NetlogDeviceLocker);
    NetlogDriverStateCurrent = false;
    
    if (!SendCommandToCxport(IOCTL_NETLOG_LOAD, NULL, 0, NULL, 0, NULL))
    {
        CmnPrint(PT_FAIL,
                 L"Failed loading netlog driver module: err=%s",
                 GetLastErrorText());
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
//
// Starts logging.
//
bool
NetlogManager_t::
Start(void)
{
    CmnPrint(PT_LOG, L"Starting netlog logging");
    
    ce::gate<ce::critical_section> locker(NetlogDeviceLocker);
    NetlogDriverStateCurrent = false;
    
    if (!SendCommandToNetlog(IOCTL_NETLOG_START, NULL, 0, NULL, 0, NULL))
    {
        CmnPrint(PT_FAIL,
                 L"Failed starting netlog driver module: err=%s",
                 GetLastErrorText());
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
//
// Stops logging.
//
bool
NetlogManager_t::
Stop(void)
{
    CmnPrint(PT_LOG, L"Stopping netlog logging");
    
    ce::gate<ce::critical_section> locker(NetlogDeviceLocker);
    NetlogDriverStateCurrent = false;
    
    if (!SendCommandToNetlog(IOCTL_NETLOG_STOP, NULL, 0, NULL, 0, NULL))
    {
        CmnPrint(PT_FAIL,
                 L"Failed stopping netlog driver module: err=%s",
                 GetLastErrorText());
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
//
// Stops logging and unloads the netlog driver.
//
bool
NetlogManager_t::
Unload(void)
{
    CmnPrint(PT_LOG, L"Unloading netlog driver");
    
    ce::gate<ce::critical_section> locker(NetlogDeviceLocker);
    NetlogDriverStateCurrent = false;
    
    SendCommandToNetlog(IOCTL_NETLOG_STOP,   NULL, 0, NULL, 0, NULL);

    if (!SendCommandToCxport(IOCTL_NETLOG_UNLOAD, NULL, 0, NULL, 0, NULL))
    {
        CmnPrint(PT_FAIL,
                 L"Failed unloading netlog driver module: err=%s",
                 GetLastErrorText());
        return false;
    }

    return true;
}

#ifdef REMOVED_FOR_YAMAZAKI
// ----------------------------------------------------------------------------
//
// Gets the specified trace module status.
//
bool
NetlogManager_t::
GetTraceModuleInfo(
    int                                     ModuleId,
    struct CXLOG_GET_MODULE_INFO_RESPONSE *pModuleInfo,
    int                                     ModuleInfoSize)
{
    CmnPrint(PT_LOG, L"Getting state of module #%d", ModuleId);

    assert(0 <= ModuleId);
    assert(NULL != pModuleInfo);
    if (0 <= ModuleId && NULL != pModuleInfo)
    {
        DWORD dwModuleId = ModuleId;
        
        ce::gate<ce::critical_section> locker(NetlogDeviceLocker);
        
        if (SendCommandToCxport(IOCTL_CXLOG_GET_MODULE_INFO, 
                                &dwModuleId, sizeof(dwModuleId), 
                                pModuleInfo, ModuleInfoSize, NULL))
        {
            return true;
        }
    }

#ifdef EXTRA_DEBUG
    CmnPrint(PT_FAIL,
             L"Failed getting state of module %d: err=%s",
             ModuleId, GetLastErrorText());
#endif
    return false;
}
#endif /* REMOVED_FOR_YAMAZAKI */

#ifdef REMOVED_FOR_YAMAZAKI
// ----------------------------------------------------------------------------
//
// Sets the filter status for the specified trace module.
//
bool
NetlogManager_t::
SetTraceModuleFilter(
    int ModuleId,
    int FilterType,
    int ZoneMask)
{
    CmnPrint(PT_LOG, 
             L"Setting module #%d trace filter type %d to 0x%X",
             ModuleId, FilterType, ZoneMask);

    assert(0 <= ModuleId);
    assert(0 <= FilterType);
    assert(0 <= ZoneMask && ZoneMask <= 0xFFFF);
    if (0 <= ModuleId && 0 <= FilterType && 0 <= ZoneMask && ZoneMask <= 0xFFFF)
    {
        struct CXLOG_SET_FILTER_REQUEST request;
        request.dwModuleId = ModuleId;
        request.Type       = FilterType;
        request.ZoneMask   = ZoneMask;
        
        ce::gate<ce::critical_section> locker(NetlogDeviceLocker);
         
        if (SendCommandToCxport(IOCTL_CXLOG_SET_FILTER, 
                                &request, sizeof(request),
                                NULL, 0, NULL))
        {
            return true;
        }
    }
        
    CmnPrint(PT_FAIL,
             L"Failed sSetting module #%d trace filter type %d to 0x%X: err=%s",
             ModuleId, FilterType, ZoneMask, GetLastErrorText());
    return false;
}
#endif /* REMOVED_FOR_YAMAZAKI */

// ----------------------------------------------------------------------------
