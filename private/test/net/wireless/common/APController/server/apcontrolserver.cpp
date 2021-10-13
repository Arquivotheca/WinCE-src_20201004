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
// This is the APController server.
//
// It resides on the system running the Azimuth Director test-control suite or
// attached to the device to be controlled and provides remote access to the
// device-control facilities.
//
// Using this facility, the APController libraries are able to remotely control
// the configurations of the APs and attenuators being used in automated RF
// test suites.
//
// ----------------------------------------------------------------------------

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <winsock2.h>
#include <windows.h>

#include <auto_xxx.hxx>
#include <inc/bldver.h>
#include <list.hxx>
#include <netmain.h>
#include <backchannel.h>
#include <strsafe.h>

#include <MemBuffer_t.hpp>
#include <AccessPointState_t.hpp>
#include <APControlData_t.hpp>
#include <DeviceController_t.hpp>
#include <RFAttenuatorState_t.hpp>

#include "APCUtils.hpp"
#include "RegistryAPPool_t.hpp"

#ifdef _PREFAST_
#pragma warning(disable:25028)
#else
#pragma warning(disable:4068)
#endif

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Global and local variables:
//

// Netmain information:
//
WCHAR *gszMainProgName = L"APControl";
DWORD  gdwMainInitFlags = 0;

// Pool of configured Access Points and Attenuators:
//
static RegistryAPPool_t s_APPool;

// Special registry-AP and registry-attenuator device-types:
//
static const TCHAR s_RegAPDeviceType[] = TEXT("RegAP");
static const TCHAR s_RegATDeviceType[] = TEXT("RegAT");

// Current server/test status:
//
static long s_ServerStatus = 0;

// ----------------------------------------------------------------------------
//
// Local functions:
//

// Called during connection startup and shutdown:
//
static BOOL
ConnectionStartup(SOCKET Sock);
static BOOL
ConnectionCleanup(SOCKET Sock);

//
// GetConfiguration:
//      Gets the list of Access Points and Attenuators from the registry
//      configuration.
// Return data:
//      A list of APs and attenuators:
//           RegAP, {unique-AP-name}, RegAT, {unique-atten-name}
//        [, RegAP, {unique-AP-name}, RegAT, {unique-atten-name} ...]
//      followed by the UTC time on the server's system in this format:
//         , YYYYMMDDHHMMSS
//   or String containing message describing the error
//
static DWORD
GetConfiguration(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);

//
// InitializeDevice:
//      Initializes a new connection to control the specified
//      device.
// Command data (string):
//      tt,nn where
//          tt = type of device (e.g., "Azimuth", "Weinshel" or "RegAP/AT")
//          nn = device name (e.g., "C1-M2-2A", "COM1" or )
//
static DWORD
InitializeDevice(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);

//
// GetAccessPoint:
//      Gets the current configuration of an Access Point.
// Return data:
//      AccessPointState_t containing current configuration settings
//   or String containing message describing the error
//
static DWORD
GetAccessPointV1(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);

static DWORD
GetAccessPoint(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);

//
// SetAccessPoint:
//      Sets the current configuration of an Access Point.
// Command data:
//      AccessPointState_t containing new configuration settings
// Return data:
//      AccessPointState_t containing updated configuration settings
//   or String containing message describing the error
//
static DWORD
SetAccessPointV1(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);
static DWORD
SetAccessPoint(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);

//
// GetAttenuator:
//      Gets the current settings for an RF attenuator.
// Return data:
//      RFAttenuatorState_t containing current attenuator settings
//   or String containing message describing the error
//
static DWORD
GetAttenuatorV1(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);
static DWORD
GetAttenuator(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);

//
// SetAttenuator:
//      Sets the current settings for an RF attenuator.
// Command data:
//      RFAttenuatorState_t containing new attenuator settings
// Return data:
//      RFAttenuatorState_t containing updated attenuator settings
//   or String containing message describing the error
//
static DWORD
SetAttenuatorV1(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);
static DWORD
SetAttenuator(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);

//
// DoDeviceCommand:
//      Performs a generic command for a remote client
// Command data:
//      Object containing command specs
// Return data:
//      Object containing command results
//   or String containing message describing the error
//
static DWORD
DoDeviceCommand(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);

//
// GetStatus:
//      Gets the current controller status.
// Return data:
//      DWORD containing current status
//   or String containing message describing the error
//
static DWORD
GetStatus(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);

//
// SetStatus:
//      Sets the current controller status.
// Command data:
//      DWORD containing new controller status
// Return data:
//      DWORD containing updated controller status
//   or String containing message describing the error
//
static DWORD
SetStatus(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);

//
// DoLaunchCommand
// Command data:
//      String containing command line of process to launch.
// Return data:
//      String containing hexadecimal representation of Process ID.
//   or String containing message describing the error
//
static DWORD
DoLaunchCommand(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);

//
// DoTerminateCommand
// Command data:
//      String containing module name of process to terminate, optionally
//          appended with an ampersand and process id, e.g. "myprocess&01ff".
// Return data:
//      None
//   or String containing message describing the error
//
static DWORD
DoTerminateCommand(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes);

// ----------------------------------------------------------------------------
//
// A simple, high speed, hash-table linking sockets and device controllers:
//
class DeviceTable_t
{
private:

    // Hash-table:
    static const int HASH_TABLE_SIZE = 47;
    static DeviceTable_t *s_HashTable[];
    static DeviceTable_t *s_pFreeList;

    // Synchronization object:
    static ce::critical_section s_CSection;

    // Hash-table pointer and index:
    DeviceTable_t *m_pNext;
    SOCKET         m_Socket;

    // Device being controlled:
    DeviceController_t *m_pDevice;    

    // Delete device when finished?
    bool m_fTemporary;

public:

    // Initializes and deallocates the hash-table:
    static void
    InitDeviceTable(void);
    static void
    ClearDeviceTable(void);

    // Adds a node for a socket controlling the specified device:
    static DWORD
    Insert(
        SOCKET Sock,
        DeviceController_t *pDevice,
        bool                fTemporary = false);

    // Remove the specified socket:
    static void
    Remove(
        SOCKET Sock);

    // Looks up the specified socket in the table:
    DeviceTable_t(
        SOCKET Sock);

    // Did the lookup succeed?
    bool
    IsValid(void) const
    {
        return m_pDevice != NULL;
    }

    // Retrieve the device being controlled:
    DeviceController_t *operator->(void) const
    {
        return m_pDevice;
    }

    operator DeviceController_t * () const
    {
        return m_pDevice;
    }
    

private:

    // If necessary, delete the controlled device object:
    void
    Clear(void)
    {
        if (m_pDevice && m_fTemporary)
        {
            delete m_pDevice;
            m_pDevice = NULL;
        }
    }
};

// ----------------------------------------------------------------------------
//
// Maintain a list of processes launched by LaunchCommandCode.
//
namespace ce {
namespace qa {

class ProcessManager_t
{
private:
    
    struct ProcessData_t
    {
        ce::tstring m_ModuleName;
        DWORD       m_ProcessId;
    };
    static ce::list<ProcessData_t> sm_ProcessList;
    static ce::critical_section    sm_CritSec;

public:

    typedef ce::list<ProcessData_t>   List_t;
    typedef List_t::iterator          ListIter;

    // Static initialization and cleanup:
    static void
    Init(void);

    static void
    Cleanup(void);

    // Launch a process with the specified command line, storing
    // its Process ID in the supplied parameter:
    static DWORD
    Launch(
        const TCHAR *pCommandLine,
        DWORD       *pProcessId);

    // Terminate previously launched process(es).
    static DWORD
    Terminate(
        const TCHAR *pModuleName,
        DWORD        ProcessId);

private:

    // Terminate the process with the specified Id:
    static DWORD
    TerminateProcessId(
        DWORD ProcessId);

    // Add a modulename/procid entry to the list:
    static DWORD
    AddEntry(
        const TCHAR *pModuleName,
        DWORD        ProcessId);

    // Remove a modulename/procid entry from the list:
    static DWORD
    RemoveEntry(
        const TCHAR *pModuleName,
        DWORD        ProcessId);
};
}; // qa
}; // ce

// ----------------------------------------------------------------------------
//

ProcessManager_t::List_t        ProcessManager_t::sm_ProcessList;
ce::critical_section            ProcessManager_t::sm_CritSec;

// ----------------------------------------------------------------------------
//
// Static initialization and cleanup.
//
void
ProcessManager_t::
Init(void)
{
    /* nothing to do */
}

// ----------------------------------------------------------------------------

void
ProcessManager_t::
Cleanup(void)
{
    // Terminate all processes in the list, and clear the list.
    ListIter iter;
    for (iter = sm_ProcessList.begin() ; iter != sm_ProcessList.end() ; ++iter)
        TerminateProcessId(iter->m_ProcessId);

    sm_ProcessList.clear();
}

// ----------------------------------------------------------------------------
//
// Add a modulename/procid entry to the list:
//
DWORD
ProcessManager_t::
AddEntry(
    const TCHAR *pModuleName,
    DWORD        ProcessId)
{
    DWORD result = NO_ERROR;
    ce::gate<ce::critical_section> locker(sm_CritSec);

    ProcessData_t data;
    data.m_ProcessId = ProcessId;
    if (!data.m_ModuleName.assign(pModuleName))
        result = ERROR_OUTOFMEMORY;
    else
    if (!sm_ProcessList.push_back(data))
        result = ERROR_OUTOFMEMORY;
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Remove a modulename/procid entry from the list:
//
DWORD
ProcessManager_t::
RemoveEntry(
    const TCHAR *pModuleName,
    DWORD        ProcessId)
{
    DWORD returnId = 0;
    ce::gate<ce::critical_section> locker(sm_CritSec);

    ListIter iter;
    for (iter = sm_ProcessList.begin() ; iter != sm_ProcessList.end() ; ++iter)
    {
        // If a process ID was supplied, they must match.
        if (0 != ProcessId && ProcessId != iter->m_ProcessId)
            continue;
        // No process ID supplied, or they match. Check for module name match.
        if (0 == _tcsicmp(pModuleName, iter->m_ModuleName))
        {
            returnId = iter->m_ProcessId;
            sm_ProcessList.erase(iter);
            break;
        }
    }
    
    return returnId;
}

// ----------------------------------------------------------------------------
//
// Launch a process with the specified command line, storing its Process ID
// in the supplied parameter:
//
DWORD
ProcessManager_t::
Launch(
   const TCHAR *pCommandLine,
   DWORD       *pProcessId)
{
    DWORD result = NO_ERROR;
    if (NULL == pProcessId || NULL == pCommandLine)
        return ERROR_INVALID_PARAMETER;

    LogDebug(TEXT("[AC] Launching '%s'"), pCommandLine);

    PROCESS_INFORMATION ProcInfo;
    memset(&ProcInfo, 0, sizeof(ProcInfo));
    
    STARTUPINFO StartupInfo;
    memset(&StartupInfo, 0, sizeof(StartupInfo));
    StartupInfo.cb = sizeof(StartupInfo);

    // Parse the module name as the first whitespace-delimited token
    // in the command line.
    for (int iPos = 0 ;
         pCommandLine[iPos] && !_istspace(pCommandLine[iPos]);
       ++iPos)
    {;}

    ce::tstring moduleName(pCommandLine, iPos);
    LogDebug(TEXT("[AC] ModuleName '%s'"), &moduleName[0]);

    BOOL bCreated;
    bCreated = CreateProcess(moduleName,
         const_cast<TCHAR *>(&pCommandLine[iPos]),
                             NULL, NULL, FALSE,
                             CREATE_NEW_CONSOLE,
                             NULL, NULL,
                            &StartupInfo,
                            &ProcInfo);
    if (!bCreated)
    {
        result = GetLastError();
        LogError(TEXT("CreateProcess() failed: %s"), Win32ErrorText(result));
    }
    else
    {

        LogDebug(TEXT("[AC] Process created, ID: %08x, Process handle: %08x"), ProcInfo.dwProcessId,
               ProcInfo.hProcess);

        *pProcessId = (DWORD)ProcInfo.hProcess;

        result = AddEntry(moduleName, (DWORD)ProcInfo.hProcess);
        if (NO_ERROR != result)
        {
            *pProcessId = 0;
            LogError(TEXT("[AC] Failed to add process entry")
                     TEXT(" - process %08X terminated"), ProcInfo.dwProcessId);
            TerminateProcessId(*pProcessId );
        }
       
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Terminate previously launched process(es).
//
// If ProcessId is zero:
//     Terminate all processes with the supplied module name.
//     Return ERROR_NOT_FOUND if no process with supplied name was found.
// Else
//     Terminate the one process that matches both module name and ProcessId.
//     Return ERROR_NOT_FOUND if no process matching both was found.
//
DWORD
ProcessManager_t::
Terminate(
    const TCHAR *pModuleName,
    DWORD        ProcessId)
{
    bool bOneTerminated = false;
    for(;;)
    {
        DWORD processId = RemoveEntry(pModuleName, ProcessId);
        if (0 == processId)
            break;

        LogDebug(TEXT("[AC] Terminating '%s' (id=%08x) "),
                 pModuleName, processId);

        if (NO_ERROR == TerminateProcessId(processId))
            bOneTerminated = true;

        // If a process ID was specified, we only look for 1 entry.
        if (0 != ProcessId)
            break;
        
        // Otherwise we continue as long as we find a module name match.
    }
    
    // Only return an error if no processes were terminated.
    return bOneTerminated ? NO_ERROR : ERROR_NOT_FOUND;
}

// ----------------------------------------------------------------------------
//
// Terminate the process with the specified Id.
//
DWORD
ProcessManager_t::
TerminateProcessId(
    DWORD ProcessId)
{
    assert(0 != ProcessId);
    DWORD result = NO_ERROR;

    HANDLE hProcess = (HANDLE)ProcessId;

    if (!TerminateProcess(hProcess, 0))
    {
       result = GetLastError();
       LogError(TEXT("\nTerminateProcess(%08x) (id=%08x) failed with Error: %s"),
               hProcess, ProcessId, Win32ErrorText(result));
    }

    
    CloseHandle(hProcess);
    return result;
}

// ----------------------------------------------------------------------------
//
// Prints the server usage.
//
static void
PrintUsage()
{
    LogAlways(TEXT("++Remote Access Point device-control server++"));
    LogAlways(TEXT("APControl [-s ServerHost] [-p Port] [-t tclsh-name] [-c command]"));
    LogAlways(TEXT("options:"));
    LogAlways(TEXT("-?      show this information"));
    LogAlways(TEXT("-k      registry key (default: %s)"), APCUtils::DefaultRegistryKey);
    LogAlways(TEXT("-s      name/address of server (default: %s)"), APCUtils::DefaultServerHost);
    LogAlways(TEXT("-p      port to connect on (default: %s)"), APCUtils::DefaultServerPort);
    LogAlways(TEXT("-t      TclSh exec name (default: %s)"), APCUtils::DefaultShellExecName);
    NetMainDumpParameters();
}

// ----------------------------------------------------------------------------
//
// Sets up the command-handlers and accepts connections.
//
static int
DoServer(void)
{
    ce::auto_handle stopService;
    ce::auto_handle serviceThread;

    APControlData_t::SetCommandFunction(APControlData_t::GetConfigurationCommandCode,
                                                         GetConfiguration);
    APControlData_t::SetCommandFunction(APControlData_t::InitializeDeviceCommandCode,
                                                         InitializeDevice);

    APControlData_t::SetCommandFunction(APControlData_t::GetAccessPointCommandCodeV1,
                                                         GetAccessPointV1);
    APControlData_t::SetCommandFunction(APControlData_t::GetAccessPointCommandCode,
                                                         GetAccessPoint);

    APControlData_t::SetCommandFunction(APControlData_t::SetAccessPointCommandCodeV1,
                                                         SetAccessPointV1);
    APControlData_t::SetCommandFunction(APControlData_t::SetAccessPointCommandCode,
                                                         SetAccessPoint);

    APControlData_t::SetCommandFunction(APControlData_t::GetAttenuatorCommandCodeV1,
                                                         GetAttenuatorV1);
    APControlData_t::SetCommandFunction(APControlData_t::GetAttenuatorCommandCode,
                                                         GetAttenuator);

    APControlData_t::SetCommandFunction(APControlData_t::SetAttenuatorCommandCodeV1,
                                                         SetAttenuatorV1);
    APControlData_t::SetCommandFunction(APControlData_t::SetAttenuatorCommandCode,
                                                         SetAttenuator);

    APControlData_t::SetCommandFunction(APControlData_t::DoDeviceCommandCode,
                                                         DoDeviceCommand);

    APControlData_t::SetCommandFunction(APControlData_t::GetStatusCommandCode,
                                                         GetStatus);
    APControlData_t::SetCommandFunction(APControlData_t::SetStatusCommandCode,
                                                         SetStatus);

    APControlData_t::SetCommandFunction(APControlData_t::LaunchCommandCode,
                                                         DoLaunchCommand);
    APControlData_t::SetCommandFunction(APControlData_t::TerminateCommandCode,
                                                         DoTerminateCommand);

    char cServerPort[20];
    WiFUtils::ConvertString(cServerPort, APCUtils::pServerPort, COUNTOF(cServerPort));
    stopService = BackChannel_StartService(cServerPort,
                                           ConnectionStartup,
                                           ConnectionCleanup
#if (CE_MAJOR_VER >= 6)
                                          ,&serviceThread);
#else
                                                         );
#endif
    if (!stopService.valid())
    {
        LogError(TEXT("[%ls] BackChannel_StartService() failed"),
                 gszMainProgName);
        return -1;
    }
    else
    {
        LogAlways(TEXT("[%ls] waiting for connections on port %s"),
                  gszMainProgName, APCUtils::pServerPort);
        WaitForSingleObject(stopService,   INFINITE);
#if (CE_MAJOR_VER >= 6)
        WaitForSingleObject(serviceThread, INFINITE);
#else
        Sleep(5000);
#endif
        return 0;
    }
}


// ----------------------------------------------------------------------------
//
//
int
mymain(int argc, WCHAR *argv[])
{
    WSADATA wsaData;

    CmnPrint_Initialize(gszMainProgName);
    CmnPrint_RegisterPrintFunctionEx(PT_LOG,     NetLogDebug,   FALSE);
    CmnPrint_RegisterPrintFunctionEx(PT_WARN1,   NetLogWarning, FALSE);
    CmnPrint_RegisterPrintFunctionEx(PT_WARN2,   NetLogWarning, FALSE);
    CmnPrint_RegisterPrintFunctionEx(PT_FAIL,    NetLogError,   FALSE);
    CmnPrint_RegisterPrintFunctionEx(PT_VERBOSE, NetLogMessage, FALSE);

    // Initialize static resources.
    ProcessManager_t::Init();
    DeviceTable_t::InitDeviceTable();

    // Parse the run-time arguments.
    if (QAWasOption(TEXT("?")) || QAWasOption(TEXT("help")))
    {
        PrintUsage();
        return 0;
    }

    QAGetOption(TEXT("k"), &APCUtils::pRegistryKey);
    QAGetOption(TEXT("s"), &APCUtils::pServerHost);
    QAGetOption(TEXT("p"), &APCUtils::pServerPort);
    QAGetOption(TEXT("t"), &APCUtils::pShellExecName);
 

    // Initialize the AP and attenuator configuration.
    HRESULT hr = s_APPool.LoadAPControllers(APCUtils::pRegistryKey);
    if (FAILED(hr))
    {
        LogError(TEXT("Failed loading configration from \"%s\": %s"),
                 APCUtils::pRegistryKey, HRESULTErrorText(hr));
        PrintUsage();
        return -1;
    }

    // Initialize WinSock.
    int result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (0 != result)
    {
        LogError(TEXT("WSAStartup failed: %s"), Win32ErrorText(result));
        return -1;
    }

   

    // Set up the command-handlers and accept connections.
    result = DoServer();

    // Delete the device-controllers and shut down WinSock.
    s_APPool.Clear();
    WSACleanup();

    // Clean up static resources.
    DeviceTable_t::ClearDeviceTable();
    ProcessManager_t::Cleanup();

    return result;
}

// ----------------------------------------------------------------------------
//
// Connection startup.
//
static BOOL
ConnectionStartup(SOCKET Sock)
{
    LogDebug(TEXT("[AC] ConnectionStartup"));
    return TRUE;
}

// ----------------------------------------------------------------------------
//
// Connection shutdown.
//
static BOOL
ConnectionCleanup(SOCKET Sock)
{
    LogDebug(TEXT("[AC] ConnectionCleanup"));
    DeviceTable_t::Remove(Sock);
    return TRUE;
}

// ----------------------------------------------------------------------------
//
// Performs the normal "cleanup" for one of the device-command functions.
//
static DWORD
DoCommandCleanup(
    DWORD         result,
    const char   *pFunctionName,
    ce::tstring  *pErrorMessage,
    BYTE       **ppReturnData,
    DWORD        *pReturnDataBytes)
{
    if (NO_ERROR == result)
    {
        LogDebug(TEXT("[AC] %hs-- result: 0"), pFunctionName);
    }
    else
    {
        if (*ppReturnData)
        {
            free(*ppReturnData);
           *ppReturnData = NULL;
            *pReturnDataBytes = 0;
        }
        
        if (0 == pErrorMessage->length())
        {
            pErrorMessage->assign(Win32ErrorText(result));
            LogError(TEXT("[AC] %hs-- result: %.128s"), pFunctionName,
                    &(*pErrorMessage)[0]);
        }
        else
        {
            LogError(TEXT("[AC] %hs-- result: error=%u (0x%x): %.128s"), pFunctionName,
                     result, result, &(*pErrorMessage)[0]);
        }

        result = APControlData_t::EncodeMessage(result, *pErrorMessage,
                                                ppReturnData,
                                                 pReturnDataBytes);
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Gets the list of Access Points and Attenuators from the registry
// configuration.
//
static DWORD
GetConfiguration(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    HRESULT hr;

    TCHAR buff[128];
    int   buffChars = COUNTOF(buff);
    
    ce::tstring resultMessage;    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);

    LogDebug(TEXT("[AC] GetConfiguration++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Generate the list of APs and attenuators.
    const char *prefix = "";
    for (int apx = 0 ; s_APPool.size() > apx ; ++apx)
    {
        hr = StringCchPrintf(buff, buffChars,
                             TEXT("%hs%s,%s,%s,%s"), prefix,
                             s_RegAPDeviceType, s_APPool.GetAPKey(apx),
                             s_RegATDeviceType, s_APPool.GetATKey(apx));
        if (FAILED(hr) || !resultMessage.append(buff))
        {
            result = ERROR_OUTOFMEMORY;
            break;
        }
        prefix = ",";
    }

    // Append the current server time.
    if (NO_ERROR == result)
    {
        SYSTEMTIME sysTime;
        memset(&sysTime, 0, sizeof(sysTime));
        GetSystemTime(&sysTime);

        hr = StringCchPrintf(buff, buffChars,
                             TEXT("%hs%04u%02u%02u%02u%02u%02u"), prefix,
                             sysTime.wYear, 
                             sysTime.wMonth, 
                             sysTime.wDay, 
                             sysTime.wHour, 
                             sysTime.wMinute, 
                             sysTime.wSecond); 
        if (FAILED(hr) || !resultMessage.append(buff))
        {
            result = ERROR_OUTOFMEMORY;
        }
        else
        {
            result = APControlData_t::EncodeMessage(result, resultMessage,
                                                    ppReturnData,
                                                     pReturnDataBytes);
        }
    }
    
    errorLock.StopCapture();
    return DoCommandCleanup(result, "GetConfiguration",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Initializes a new connection to control the specified device.
//
static DWORD
InitializeDevice(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    ce::wstring wCommandArgs;
    ce::tstring tCommandArgs;

    ce::auto_ptr<DeviceController_t> pDevice;
    bool                             temporary = false;
    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);
    
    LogDebug(TEXT("[AC] InitializeDevice++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Get the device-type and name.
    result = APControlData_t::DecodeMessage(result,
                                           pCommandData,
                                            CommandDataBytes, &wCommandArgs);
    if (NO_ERROR != result)
        goto Cleanup;

    WiFUtils::ConvertString(&tCommandArgs, wCommandArgs);
    TCHAR *type = tCommandArgs.get_buffer();
    TCHAR *name = _tcschr(type, TEXT(','));
    if (NULL == name)
    {
        name = const_cast<TCHAR *>(TEXT(""));
    }
    else
    {
       *name = TEXT(' ');
        while (name-- != type && _istspace(*name)) ;
        while (_istspace(*(++name))) *name = TEXT('\0');
    }
    LogDebug(TEXT("[AC]   device type = \"%s\""), type);
    LogDebug(TEXT("[AC]   device name = \"%s\""), name);

    size_t typeLength;
    if (FAILED(StringCchLength(type, 30, &typeLength))
     || 0 == typeLength)
    {
        result = ERROR_INVALID_PARAMETER;
        LogError(TEXT("Missing device type"));
        goto Cleanup;
    }

    size_t nameLength;
    if (FAILED(StringCchLength(name, 64, &nameLength))
     || 0 == nameLength)
    {
        result = ERROR_INVALID_PARAMETER;
        LogError(TEXT("Missing device name"));
        goto Cleanup;
    }

    // If it's one of the special "registry" device-types, look it up
    // in the AP-pool.
    if (0 == _tcscmp(type, s_RegAPDeviceType))
    {
        pDevice = s_APPool.FindAPConfigDevice(name);
        if (NULL == pDevice)
        {
            LogError(TEXT("Found no Access Point named \"%s\" in registry"),
                     name);
            result = ERROR_SERVICE_NOT_FOUND;
            goto Cleanup;
        }
    }
    else
    if (0 == _tcscmp(type, s_RegATDeviceType))
    {
        pDevice = s_APPool.FindAttenuatorDevice(name);
        if (NULL == pDevice)
        {
            LogError(TEXT("Found no Attenuator named \"%s\" in registry"),
                     name);
            result = ERROR_SERVICE_NOT_FOUND;
            goto Cleanup;
        }
    }

    // Otherwise, generate a regular device-controller.
    else
    {
        // Remember to delete the object when the socket detaches and
        // ConnectionCleanup removes it from the hash-table.
        temporary = true;

        result = APCUtils::CreateController(type, name, &pDevice);
        if (NO_ERROR != result)
        {
            goto Cleanup;
        }
    }

    // Register the controller (if any) for processing this socket's commands.
    if (pDevice.valid())
    {
        result = DeviceTable_t::Insert(Sock, pDevice, temporary);
        if (NO_ERROR != result)
        {
            LogError(TEXT("Cannot register new DeviceController: %s"),
                     Win32ErrorText(result));
            goto Cleanup;
        }

        LogDebug(TEXT("[AC] %hs device-controller \"%s;%s\" for socket 0x%X"),
                 temporary? "Created" : "Attached",
                 pDevice->GetDeviceType(),
                 pDevice->GetDeviceName(),
                 Sock);

        // Don't delete the device-controller even if it's temporary.
        temporary = false;
    }

    result = NO_ERROR;

Cleanup:

    if (!temporary)
    {
        pDevice.release();
    }

    errorLock.StopCapture();
    return DoCommandCleanup(result, "InitializeDevice",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Gets the settings for an Access Point.
//
static DWORD
GetAccessPointV1(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);
    
    LogDebug(TEXT("[AC] GetAccessPointV1++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket.
    DeviceTable_t pDevice(Sock);
    if (!pDevice.IsValid())
    {
        LogError(TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        // Get the device's configuration settings.
        AccessPointState_t resultState;
        result = pDevice->GetAccessPoint(&resultState);
        if (NO_ERROR == result)
        {
            resultState.SetFieldFlags(AccessPointState_t::FieldMaskAll);

            // Send the response in binary form.
            AccessPointBinary_t binaryState;
            result = binaryState.FromState(resultState);
            if (NO_ERROR == result)
            {
                result = binaryState.EncodeMessage(result,
                                                   ppReturnData,
                                                    pReturnDataBytes);
            }
        }
    }

    errorLock.StopCapture();
    return DoCommandCleanup(result, "GetAccessPointV1",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}

static DWORD
GetAccessPoint(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);
    
    LogDebug(TEXT("[AC] GetAccessPoint++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket.
    DeviceTable_t pDevice(Sock);
    if (!pDevice.IsValid())
    {
        LogError(TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        // Get the device's configuration settings.
        AccessPointState_t resultState;
        result = pDevice->GetAccessPoint(&resultState);
        if (NO_ERROR == result)
        {
            resultState.SetFieldFlags(AccessPointState_t::FieldMaskAll);
            result = resultState.EncodeMessage(result,
                                               ppReturnData,
                                                pReturnDataBytes);
        }
    }

    errorLock.StopCapture();
    return DoCommandCleanup(result, "GetAccessPoint",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Sets an Access Point configuration in a background, delayed, thread.
//
struct AsyncAPConfig_t
{
    DeviceController_t *pDevice;
    AccessPointState_t  oldState;
    AccessPointState_t  newState;
};
     
static DWORD WINAPI
SetAccessPointProc(LPVOID pParameter)
{
    AsyncAPConfig_t *pArgs = (AsyncAPConfig_t *)pParameter;
    AccessPointState_t response;
    DWORD result;

    LogDebug(TEXT("[AC] Startes sub-thread to config the AP %s \"%s\""),
             pArgs->pDevice->GetDeviceType(),
             pArgs->pDevice->GetDeviceName());
    
    // If necessary, delay before changing the state.
    if (0 < pArgs->newState.GetAsyncStateChangeDelay())
    {
        LogDebug(TEXT("[AC] delaying %lds before starting AP Configuraton"),
                 pArgs->newState.GetAsyncStateChangeDelay());

        Sleep(pArgs->newState.GetAsyncStateChangeDelay()*1000);
    }
    
    // Configure the AP.
    LogDebug(TEXT("[AC] asynchronous AP configuation started"));
    result = pArgs->pDevice->SetAccessPoint(pArgs->newState, &response);
    LogDebug(TEXT("[AC] asynchronous AP configuation finished: %s"),
             Win32ErrorText(result));

    // If necessary, delay before reseting the state.
    if (0 < pArgs->newState.GetAsyncStateChangeReset())
    {
        LogDebug(TEXT("[AC] delaying %lds before reseting AP Configuraton"),
                 pArgs->newState.GetAsyncStateChangeDelay());

        Sleep(pArgs->newState.GetAsyncStateChangeReset()*1000);

        // Reset the AP.
        LogDebug(TEXT("[AC] asynchronous AP reset started"));
        result = pArgs->pDevice->SetAccessPoint(pArgs->oldState, &response);
        LogDebug(TEXT("[AC] asynchronous AP reset finished: %s"),
             Win32ErrorText(result));
    }

    // Clean up.
    delete pArgs;
    return result;
}

// ----------------------------------------------------------------------------
//
// Sets the current settings for an Access Point.
//
static DWORD
DoSetAccessPoint(
    DeviceController_t *pDevice,
    AccessPointState_t *pNewState,
    AccessPointState_t *pResultState)
{
    DWORD result = NO_ERROR;
    
    // If it's an Async request...
    if (0 < pNewState->GetAsyncStateChangeDelay())
    {
        result = pDevice->GetAccessPoint(pResultState);
        if(NO_ERROR == result)
        {
            AsyncAPConfig_t *pArgs = new AsyncAPConfig_t;
            if (NULL == pArgs)
            {
                result = ERROR_OUTOFMEMORY;
                LogError(TEXT("Can't allocate args for %s \"%s\": %s"),
                         pDevice->GetDeviceType(),
                         pDevice->GetDeviceName(), 
                         Win32ErrorText(result));
            }
            else
            {
                pArgs->pDevice  = pDevice;
                pArgs->oldState = *pResultState;
                pArgs->newState = *pNewState;

                HANDLE thred = CreateThread(NULL, 0, SetAccessPointProc, pArgs, 0, NULL);
                if (NULL != thred)
                {
                    result = NO_ERROR;
                    CloseHandle(thred);
                }
                else
                {
                    result = GetLastError();
                    delete pArgs;
                    LogError(TEXT("Can't create thread to configure AP %s \"%s\": %s"),
                             pDevice->GetDeviceType(),
                             pDevice->GetDeviceName(), 
                             Win32ErrorText(result));
                }
             }
         }
    }
    else
    {
        result = pDevice->SetAccessPoint(*pNewState,
                                          pResultState);
    }

    return result;
}

static DWORD
SetAccessPointV1(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);
    
    LogDebug(TEXT("[AC] SetAccessPoint++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket.
    DeviceTable_t pDevice(Sock);
    if (!pDevice.IsValid())
    {
        LogError(TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        // Decode the binary configuration update request.
        AccessPointBinary_t binaryState;
        result = binaryState.DecodeMessage(result,
                                           pCommandData,
                                            CommandDataBytes);

        if (NO_ERROR == result)
        {
            // Convert the binary message to a standard object.
            AccessPointState_t newState;
            result = binaryState.ToState(&newState);
        
            if (NO_ERROR == result)
            {
                // Update the access point configuration.
                AccessPointState_t resultState = newState;
                result = DoSetAccessPoint(pDevice,
                                         &newState,
                                         &resultState);
                
                if (NO_ERROR == result)
                {
                    // Send the updated configuration in binary form.
                    resultState.SetFieldFlags(AccessPointState_t::FieldMaskAll);
                    result = binaryState.FromState(resultState);
                    if (NO_ERROR == result)
                    {
                        result = binaryState.EncodeMessage(result,
                                                           ppReturnData,
                                                            pReturnDataBytes);
                    }
                }
            }
        }
    }

    errorLock.StopCapture();
    return DoCommandCleanup(result, "SetAccessPoint",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}

static DWORD
SetAccessPoint(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);
    
    LogDebug(TEXT("[AC] SetAccessPoint++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket.
    DeviceTable_t pDevice(Sock);
    if (!pDevice.IsValid())
    {
        LogError(TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        // Decode the configuration update request.
        AccessPointState_t newState;
        result = newState.DecodeMessage(result,
                                        pCommandData,
                                         CommandDataBytes);
            
        if (NO_ERROR == result)
        {
            // Update the access point configuration.
            AccessPointState_t resultState = newState;    
            result = DoSetAccessPoint(pDevice,
                                     &newState,
                                     &resultState);
            
            if (NO_ERROR == result)
            {
                // Translate the updated configuration to message form.
                resultState.SetFieldFlags(AccessPointState_t::FieldMaskAll);
                result = resultState.EncodeMessage(result,
                                                   ppReturnData,
                                                   pReturnDataBytes);
            }
        }
    }

    errorLock.StopCapture();
    return DoCommandCleanup(result, "SetAccessPoint",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Gets the settings for an RF attenuator.
//
static DWORD
GetAttenuatorV1(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);

    LogDebug(TEXT("[AC] GetAttenuatorV1++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket.
    DeviceTable_t pDevice(Sock);
    if (!pDevice.IsValid())
    {
        LogError(TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        // Get the device's attenuation settings.
        RFAttenuatorState_t resultState;
        result = pDevice->GetAttenuator(&resultState);
        if (NO_ERROR == result)
        {
            resultState.SetFieldFlags(RFAttenuatorState_t::FieldMaskAll);

            // Send the response in binary form.
            RFAttenuatorBinary_t binaryState;
            result = binaryState.FromState(resultState);
            if (NO_ERROR == result)
            {
                result = binaryState.EncodeMessage(result,
                                                   ppReturnData,
                                                    pReturnDataBytes);
            }
        }
    }
    
    errorLock.StopCapture();
    return DoCommandCleanup(result, "GetAttenuatorV1",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}

static DWORD
GetAttenuator(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);

    LogDebug(TEXT("[AC] GetAttenuator++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket.
    DeviceTable_t pDevice(Sock);
    if (!pDevice.IsValid())
    {
        LogError(TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        // Get the device's attenuation settings.
        RFAttenuatorState_t resultState;
        result = pDevice->GetAttenuator(&resultState);
        if (NO_ERROR == result)
        {
            resultState.SetFieldFlags(RFAttenuatorState_t::FieldMaskAll);
            result = resultState.EncodeMessage(result,
                                               ppReturnData,
                                                pReturnDataBytes);
        }
    }
    
    errorLock.StopCapture();
    return DoCommandCleanup(result, "GetAttenuator",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Sets the current settings for an RF attenuator.
//
static DWORD
SetAttenuatorV1(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);
    
    LogDebug(TEXT("[AC] SetAttenuatorV1++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket.
    DeviceTable_t pDevice(Sock);
    if (!pDevice.IsValid())
    {
        LogError(TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        // Decode the binary attenuation update request.
        RFAttenuatorBinary_t binaryState;
        result = binaryState.DecodeMessage(result,
                                           pCommandData,
                                            CommandDataBytes);

        if (NO_ERROR == result)
        {
            // Convert the binary message to a standard object.
            RFAttenuatorState_t newState;
            result = binaryState.ToState(&newState);
        
            if (NO_ERROR == result)
            {
                // Update the access point attenuation.
                RFAttenuatorState_t resultState = newState;
                result = pDevice->SetAttenuator(newState,
                                               &resultState);
                
                if (NO_ERROR == result)
                {
                    // Send the updated attenuation in binary form.
                    resultState.SetFieldFlags(RFAttenuatorState_t::FieldMaskAll);
                    result = binaryState.FromState(resultState);
                    if (NO_ERROR == result)
                    {
                        result = binaryState.EncodeMessage(result,
                                                           ppReturnData,
                                                            pReturnDataBytes);
                    }
                }
            }
        }
    }

    errorLock.StopCapture();
    return DoCommandCleanup(result, "SetAttenuatorV1",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}


static DWORD
SetAttenuator(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);
    
    LogDebug(TEXT("[AC] SetAttenuator++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket.
    DeviceTable_t pDevice(Sock);
    if (!pDevice.IsValid())
    {
        LogError(TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        // Get the new attenuation value.
        RFAttenuatorState_t newState;
        result = newState.DecodeMessage(result,
                                        pCommandData,
                                         CommandDataBytes);
        if (NO_ERROR != result)
        {
            LogError(TEXT("Bad attenuation values"));
        }
        else
        {
            // Set the device's attenuation.
            RFAttenuatorState_t resultState = newState;
            result = pDevice->SetAttenuator(newState,
                                           &resultState);
            if (NO_ERROR == result)
            {
                // Translate the updated attenuation to message form.                
                resultState.SetFieldFlags(RFAttenuatorState_t::FieldMaskAll);
                result = resultState.EncodeMessage(result,
                                                   ppReturnData,
                                                    pReturnDataBytes);
            }
        }
    }

    errorLock.StopCapture();
    return DoCommandCleanup(result, "SetAttenuator",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Performs a generic remote device command.
//
static DWORD
DoDeviceCommand(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);
    
    LogDebug(TEXT("[AC] DoDeviceCommand++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket and perform the command.
    DeviceTable_t pDevice(Sock);
    if (!pDevice.IsValid())
    {
        LogError(TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        MemBuffer_t dataBuffer,
                    messageBuffer;
        
        result = pDevice->DoDeviceCommand(Command,
                                         pCommandData,
                                          CommandDataBytes,
                                         &dataBuffer,
                                         &messageBuffer);
        
        dataBuffer.CopyOut(ppReturnData, pReturnDataBytes);
        messageBuffer.CopyOut(&capturedErrors);
    }
    
    errorLock.StopCapture();
    return DoCommandCleanup(result, "DoDeviceCommand",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Gets the current server status.
//
static DWORD
GetStatus(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);

    LogDebug(TEXT("[AC] GetStatus++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Return the current status.
   *ppReturnData = (BYTE *) malloc(sizeof(DWORD));
    if (NULL == *ppReturnData)
    {
        LogError(TEXT("Can't allocate memory"));
        result = ERROR_OUTOFMEMORY;
    }
    else
    {
        DWORD oldStatus = InterlockedCompareExchange(&s_ServerStatus, 0, 0);
        memcpy(*ppReturnData, &oldStatus, sizeof(DWORD));
       *pReturnDataBytes = sizeof(DWORD);
    }

    errorLock.StopCapture();
    return DoCommandCleanup(result, "GetStatus",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Sets the current server status.
//
static DWORD
SetStatus(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);

    LogDebug(TEXT("[AC] SetStatus++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Set the new server status.
    if (sizeof(DWORD) != CommandDataBytes)
    {
        LogError(TEXT("Bad status-code size"));
        result = ERROR_INVALID_PARAMETER;
    }
    else
    {
       *ppReturnData = (BYTE *) malloc(sizeof(DWORD));
        if (NULL == *ppReturnData)
        {
            LogError(TEXT("Can't allocate memory"));
            result = ERROR_OUTOFMEMORY;
        }
        else
        {
            DWORD newStatus;
            memcpy(&newStatus, pCommandData, sizeof(DWORD));
            InterlockedExchange(&s_ServerStatus, newStatus);
            newStatus = InterlockedCompareExchange(&s_ServerStatus, 0, 0);
            memcpy(*ppReturnData, &newStatus, sizeof(DWORD));
           *pReturnDataBytes = sizeof(DWORD);
        }
    }

    errorLock.StopCapture();
    return DoCommandCleanup(result, "SetStatus",
                           &capturedErrors, ppReturnData,
                                             pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// DoLaunchCommand
//
static DWORD
DoLaunchCommand(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    HRESULT hr;
    DWORD result = NO_ERROR;
    
    ce::tstring commandLine;
    ce::tstring resultMessage;
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);

    LogDebug(TEXT("[AC] Launch++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Get the command line.
    result = APControlData_t::DecodeMessage(result,
                                            pCommandData,
                                            CommandDataBytes,
                                            &commandLine);
    if (NO_ERROR == result)
    {
        DWORD processId;
        result = ProcessManager_t::Launch(commandLine.get_buffer(), &processId);
        if (NO_ERROR == result)
        {
            TCHAR buff[16];
            hr = StringCchPrintf(buff, COUNTOF(buff), TEXT("%08x"), processId);
            if (FAILED(hr) || !resultMessage.append(buff))
            {
                result = ERROR_OUTOFMEMORY;
            }
            else
            {
                result = APControlData_t::EncodeMessage(result, resultMessage,
                                                        ppReturnData,
                                                        pReturnDataBytes);
            }
        }
    }
    
    errorLock.StopCapture();
    return DoCommandCleanup(result, "DoLaunchCommand", &capturedErrors,
                            ppReturnData, pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// DoTerminateCommand
//
static DWORD
DoTerminateCommand(
    SOCKET  Sock,
    DWORD   Command,
    BYTE  *pCommandData,
    DWORD   CommandDataBytes,
    BYTE **ppReturnData,
    DWORD  *pReturnDataBytes)
{
    DWORD result = NO_ERROR;
    
    HANDLE hProcess = NULL;
    ce::tstring capturedErrors;
    ErrorLock errorLock(&capturedErrors);

    LogDebug(TEXT("[AC] Terminate++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    ce::tstring comandLine;
    result = APControlData_t::DecodeMessage(result,
                                            pCommandData,
                                            CommandDataBytes,
                                            &comandLine);
    if (NO_ERROR == result)
    {
        TCHAR *pModuleName;
        TCHAR *pProcessId;
        pModuleName = comandLine.get_buffer();
        pProcessId = _tcschr(pModuleName, TEXT('&'));
        
        DWORD processId = 0;
        if (NULL != pProcessId)
        {
          *pProcessId = TEXT('\0');
            processId =_tcstoul(++pProcessId, NULL, 16);
        }
        
        TCHAR *pModuleEnd;
        for (pModuleEnd = pModuleName ;
             pModuleEnd[0] && !_istspace(pModuleEnd[0]) ;
           ++pModuleEnd)
        {;}
       *pModuleEnd = TEXT('\0');

        LogDebug(TEXT("[AC] ModuleName: %s"),     pModuleName);
        LogDebug(TEXT("[AC] Process ID: 0x%08x"), processId);

        result = ProcessManager_t::Terminate(pModuleName, processId);
    }

    *pReturnDataBytes = 0;
    errorLock.StopCapture();

    // If there is no captured error message, and result is ERROR_NOT_FOUND,
    // we don't want to log an error, but we do want to return the result
    // to the client.
    DWORD cleanupResult = result;
    if ((ERROR_NOT_FOUND == result) && (0 == capturedErrors.length()))
    {
        LogDebug(TEXT("[AC] No process terminated."));
        // Make DoCommandCleanup think there is no error.
        cleanupResult = NO_ERROR;
    }
    DoCommandCleanup(cleanupResult, "DoTerminateCommand", &capturedErrors,
                     ppReturnData, pReturnDataBytes);
    
    // Return the real error back to the client.
    return result;
}

// ----------------------------------------------------------------------------
//
// A simple, high speed, hash-table linking sockets and device controllers.
//

DeviceTable_t *DeviceTable_t::s_HashTable[DeviceTable_t::HASH_TABLE_SIZE + 1];
DeviceTable_t *DeviceTable_t::s_pFreeList = NULL;

ce::critical_section DeviceTable_t::s_CSection;

//
// Initializes and deallocates the hash-table.
//
void
DeviceTable_t::
InitDeviceTable(void)
{
    ce::gate<ce::critical_section> locker(s_CSection);
    memset(s_HashTable, 0, sizeof(s_HashTable));
}

void
DeviceTable_t::
ClearDeviceTable(void)
{
    ce::gate<ce::critical_section> locker(s_CSection);
    
    s_HashTable[HASH_TABLE_SIZE] = s_pFreeList;
    s_pFreeList = NULL;

    DeviceTable_t *node, *next;
    for (int hash = 0 ; hash <= HASH_TABLE_SIZE ; ++hash)
    {
        node = s_HashTable[hash];
        while (node)
        {
            next = node->m_pNext;
            node->Clear();
            free(node);
            node = next;
        }
    }
}

//
// Adds a node for a socket controlling the specified device.
//
DWORD
DeviceTable_t::
Insert(
    SOCKET Sock,
    DeviceController_t *pDevice,
    bool                fTemporary)
{
    int hash = (int)((unsigned long)Sock % HASH_TABLE_SIZE);
    ce::gate<ce::critical_section> locker(s_CSection);

    DeviceTable_t *node = s_HashTable[hash];
    while (node)
    {
        if (node->m_Socket == Sock)
        {
            if (node->m_pDevice != pDevice)
            {
                node->Clear();
                node->m_pDevice = pDevice;
            }
            node->m_fTemporary = fTemporary;
            return NO_ERROR;
        }
        node = node->m_pNext;
    }

    node = s_pFreeList;
    if (node)
    {
        s_pFreeList = node->m_pNext;

    }
    else
    {
        node = static_cast<DeviceTable_t *>(malloc(sizeof(DeviceTable_t)));
        if (NULL == node)
            return ERROR_OUTOFMEMORY;
    }

    node->m_pNext = s_HashTable[hash];
    node->m_Socket = Sock;
    node->m_pDevice = pDevice;
    node->m_fTemporary = fTemporary;
    s_HashTable[hash] = node;
    return NO_ERROR;   
}

//
// Removes the specified socket.
//
void
DeviceTable_t::
Remove(SOCKET Sock)
{
    int hash = (int)((unsigned long)Sock % HASH_TABLE_SIZE);
    ce::gate<ce::critical_section> locker(s_CSection);

    DeviceTable_t **parent = &s_HashTable[hash];
    while (*parent)
    {
        DeviceTable_t *node = *parent;
        if (node->m_Socket == Sock)
        {
            node->Clear();
           *parent = node->m_pNext;
            node->m_pNext = s_pFreeList;
            s_pFreeList = node;
            break;
        }
        parent = &(node->m_pNext);
    }
}

//
// Looks up the specified socket in the table.
//
DeviceTable_t::
DeviceTable_t(
    SOCKET Sock)
    : m_Socket(Sock),
      m_pDevice(NULL)
{
    int hash = (int)((unsigned long)Sock % HASH_TABLE_SIZE);
    ce::gate<ce::critical_section> locker(s_CSection);

    DeviceTable_t *node = s_HashTable[hash];
    while (node)
    {
        if (node->m_Socket == Sock)
        {
           *this = *node;
            break;
        }
        node = node->m_pNext;
    }
}

// ----------------------------------------------------------------------------
