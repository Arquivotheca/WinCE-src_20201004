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

#include <inc/auto_xxx.hxx>
#include <inc/bldver.h>
#include <netmain.h>
#include <backchannel.h>
#include <strsafe.h>

#include <MemBuffer_t.hpp>
#include <AccessPointState_t.hpp>
#include <APControlData_t.hpp>
#include <DeviceController_t.hpp>

#include "RegistryAPPool_t.hpp"
#include "RFAttenuatorState_t.hpp"
#include "Utils.hpp"

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// Global and local variables:
//

// Netmain information:
WCHAR *gszMainProgName = L"APControl";
DWORD  gdwMainInitFlags = 0;

// Pool of configured Access Points and Attenuators:
static RegistryAPPool_t s_APPool;

// Special registry-AP and registry-attenuator device-types:
static const TCHAR s_RegAPDeviceType[] = TEXT("RegAP");
static const TCHAR s_RegATDeviceType[] = TEXT("RegAT");

// Current server/test status:
static long s_ServerStatus = 0;

// ----------------------------------------------------------------------------
//
// Local functions:
//

// Called during connection startup and shutdown:
//
static BOOL
ConnectionStartup(IN SOCKET Sock);
static BOOL
ConnectionCleanup(IN SOCKET Sock);

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
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes);

//
// InitializeDevice:
//      Initializes a new connection to control the specified
//      device.
// Command data (string):
//      tt,nn where
//          tt = type of device (e.g., "Azimuth", "Weinshel")
//          nn = device name (e.g., "C1-M2-2A", "COM1")
//
static DWORD
InitializeDevice(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes);

//
// GetAccessPoint:
//      Gets the current configuration of an Access Point.
// Return data:
//      AccessPointState_t containing current configuration settings
//   or String containing message describing the error
//
static DWORD
GetAccessPoint(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes);

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
SetAccessPoint(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes);

//
// GetAttenuator:
//      Gets the current settings for an RF attenuator.
// Return data:
//      RFAttenuatorState_t containing current attenuator settings
//   or String containing message describing the error
//
static DWORD
GetAttenuator(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes);

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
SetAttenuator(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes);

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
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes);

//
// GetStatus:
//      Gets the current controller status.
// Return data:
//      DWORD containing current status
//   or String containing message describing the error
//
static DWORD
GetStatus(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes);

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
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes);

//
// A simple, high speed, hash-table linking sockets and device controllers:
//
static void
AllocateDeviceTable(void);
static void
ReleaseDeviceTable(void);
static DeviceController_t *
LookupDevice(
    IN SOCKET Sock);
static DWORD
InsertDevice(
    IN SOCKET              Sock, 
    IN DeviceController_t *pDevice, 
    IN bool                fTemporary);
static bool
RemoveDevice(
    IN SOCKET Sock);

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
    LogAlways(TEXT("   -z   send debug output to standard output"));
    LogAlways(TEXT("   -mt  use multi-threaded logging methods"));
    LogAlways(TEXT("   -v   verbose debug output"));
    LogAlways(TEXT("   -k   registry key (default: %s)"), Utils::DefaultRegistryKey);
    LogAlways(TEXT("   -s   name/address of server (default: %s)"), Utils::DefaultServerHost);
    LogAlways(TEXT("   -p   port to connect on (default: %s)"), Utils::DefaultServerPort);
    LogAlways(TEXT("   -t   TclSh exec name (default: %s)"), Utils::DefaultShellExecName);
    LogAlways(TEXT("   -?   show this information"));
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

    APControlData_t::SetCommandFunction(
                     APControlData_t::GetConfigurationCommandCode,
                                      GetConfiguration);
    APControlData_t::SetCommandFunction(
                     APControlData_t::InitializeDeviceCommandCode,
                                      InitializeDevice);
    APControlData_t::SetCommandFunction(
                     APControlData_t::GetAccessPointCommandCode,
                                      GetAccessPoint);
    APControlData_t::SetCommandFunction(
                     APControlData_t::SetAccessPointCommandCode,
                                      SetAccessPoint);
    APControlData_t::SetCommandFunction(
                     APControlData_t::GetAttenuatorCommandCode,
                                      GetAttenuator);
    APControlData_t::SetCommandFunction(
                     APControlData_t::SetAttenuatorCommandCode,
                                      SetAttenuator);
    APControlData_t::SetCommandFunction(
                     APControlData_t::DoDeviceCommandCode,
                                      DoDeviceCommand);
    APControlData_t::SetCommandFunction(
                     APControlData_t::GetStatusCommandCode,
                                      GetStatus);
    APControlData_t::SetCommandFunction(
                     APControlData_t::SetStatusCommandCode,
                                      SetStatus);

    char cServerPort[20];
    WiFUtils::ConvertString(cServerPort, Utils::pServerPort,
                    COUNTOF(cServerPort));
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
                  gszMainProgName, Utils::pServerPort);
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

    CmnPrint_Initialize();
    CmnPrint_RegisterPrintFunctionEx(PT_LOG,     NetLogDebug,   FALSE);
    CmnPrint_RegisterPrintFunctionEx(PT_WARN1,   NetLogWarning, FALSE);
    CmnPrint_RegisterPrintFunctionEx(PT_WARN2,   NetLogWarning, FALSE);
    CmnPrint_RegisterPrintFunctionEx(PT_FAIL,    NetLogError,   FALSE);
    CmnPrint_RegisterPrintFunctionEx(PT_VERBOSE, NetLogMessage, FALSE);

    // Initialize static resources.
    AllocateDeviceTable();
    Utils::StartupInitialize();

    // Parse the run-time arguments.
    if (QAWasOption(TEXT("?")) || QAWasOption(TEXT("help")))
    {
        PrintUsage();
        return 0;
    }

    QAGetOption(TEXT("k"), &Utils::pRegistryKey);
    QAGetOption(TEXT("s"), &Utils::pServerHost);
    QAGetOption(TEXT("p"), &Utils::pServerPort);
    QAGetOption(TEXT("t"), &Utils::pShellExecName);

    // Initialize the AP and attenuator configuration.
    HRESULT hr = s_APPool.LoadAPControllers(Utils::pRegistryKey);
    if (FAILED(hr))
    {
        LogError(TEXT("Failed loading configration from \"%s\": %s"),
                 Utils::pRegistryKey, HRESULTErrorText(hr));
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
    Utils::StartupInitialize();
    ReleaseDeviceTable();
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Connection startup.
//
static BOOL
ConnectionStartup(IN SOCKET Sock)
{
    LogDebug(TEXT("[AC] ConnectionStartup"));
    return TRUE;
}

// ----------------------------------------------------------------------------
//
// Connection shutdown.
//
static BOOL
ConnectionCleanup(IN SOCKET Sock)
{
    LogDebug(TEXT("[AC] ConnectionCleanup"));
    RemoveDevice(Sock);
    return TRUE;
}

// ----------------------------------------------------------------------------
//
// Performs the normal "cleanup" for one of the device-command functions.
//
static DWORD
DoCommandCleanup(
    IN  DWORD         result,
    IN  const char   *pFunctionName,
    OUT ce::tstring  *pErrorMessage,
    OUT BYTE       **ppReturnData,
    OUT DWORD        *pReturnDataBytes)
{
    if (ERROR_SUCCESS == result)
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
            LogDebug(TEXT("[AC] %hs-- result: %.128s"), pFunctionName,
                    &(*pErrorMessage)[0]);
        }
        else
        {
            LogDebug(TEXT("[AC] %hs-- result: 0x%x(%u): %.128s"), pFunctionName,
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
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes)
{
    DWORD result = ERROR_SUCCESS;
    HRESULT hr;

    TCHAR buff[128];
    int   buffChars = COUNTOF(buff);
    
    ce::tstring resultMessage;
    ce::tstring errorMessage;

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
    if (ERROR_SUCCESS == result)
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
    
    return DoCommandCleanup(result, "GetConfiguration",
                           &errorMessage, ppReturnData,
                                          pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Initializes a new connection to control the specified device.
//
static DWORD
InitializeDevice(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes)
{
    DWORD result = ERROR_SUCCESS;
    ce::tstring errorMessage;
    ce::wstring wCommandArgs;
    ce::tstring tCommandArgs;

    DeviceController_t *pDevice = NULL;
    bool                temporary = false;

    LogDebug(TEXT("[AC] InitializeDevice++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Get the device-type and name.
    result = APControlData_t::DecodeMessage(result,
                                           pCommandData,
                                            CommandDataBytes, &wCommandArgs);
    if (ERROR_SUCCESS != result)
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
        WiFUtils::FmtMessage(&errorMessage, TEXT("Missing device type"));
        goto Cleanup;
    }

    size_t nameLength;
    if (FAILED(StringCchLength(name, 64, &nameLength))
     || 0 == nameLength)
    {
        result = ERROR_INVALID_PARAMETER;
        WiFUtils::FmtMessage(&errorMessage, TEXT("Missing device name"));
        goto Cleanup;
    }

    // If it's one of the special "registry" device-types, look it up
    // in the AP-pool.
    if (0 == _tcscmp(type, s_RegAPDeviceType))
    {
        pDevice = s_APPool.FindAPConfigDevice(name);
        if (NULL == pDevice)
        {
            WiFUtils::FmtMessage(&errorMessage,
                   TEXT("Found no Access Point named \"%s\" in registry"),
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
            WiFUtils::FmtMessage(&errorMessage,
                   TEXT("Found no Attenuator named \"%s\" in registry"),
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

        result = Utils::CreateController(type, name, &pDevice, &errorMessage);
        if (ERROR_SUCCESS != result)
        {
            goto Cleanup;
        }
    }

    // Register the controller (if any) for processing this socket's commands.
    if (pDevice)
    {
        result = InsertDevice(Sock, pDevice, temporary);
        if (ERROR_SUCCESS != result)
        {
            WiFUtils::FmtMessage(&errorMessage,
                                 TEXT("Cannot register new DeviceController"));
            goto Cleanup;
        }

        LogDebug(TEXT("[AC] %hs device-controller \"%s,%s\" for socket 0x%X"),
                 temporary? "Created" : "Attached",
                 pDevice->GetDeviceType(), pDevice->GetDeviceName(), Sock);
    }

    result = ERROR_SUCCESS;

Cleanup:

    if (pDevice && temporary && ERROR_SUCCESS != result)
    {
        delete pDevice;
    }

    return DoCommandCleanup(result, "InitializeDevice",
                           &errorMessage, ppReturnData,
                                          pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Gets the settings for an Access Point.
//
static DWORD
GetAccessPoint(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes)
{
    DWORD result = ERROR_SUCCESS;
    ce::tstring errorMessage;

    LogDebug(TEXT("[AC] GetAccessPoint++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket.
    DeviceController_t *pDevice = LookupDevice(Sock);
    if (NULL == pDevice)
    {
        WiFUtils::FmtMessage(&errorMessage, TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        // Get the device's configuration settings.
        AccessPointState_t resultState;
        result = pDevice->GetAccessPoint(&resultState,
                                         &errorMessage);
        if (ERROR_SUCCESS == result)
        {
            result = resultState.EncodeMessage(result,
                                               ppReturnData,
                                                pReturnDataBytes);
        }
    }

    return DoCommandCleanup(result, "GetAccessPoint",
                           &errorMessage, ppReturnData,
                                          pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Sets the current settings for an Access Point.
//
static DWORD
SetAccessPoint(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes)
{
    DWORD result = ERROR_SUCCESS;
    ce::tstring errorMessage;

    LogDebug(TEXT("[AC] SetAccessPoint++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket.
    DeviceController_t *pDevice = LookupDevice(Sock);
    if (NULL == pDevice)
    {
        WiFUtils::FmtMessage(&errorMessage, TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        // Get the new attenuation value.
        AccessPointState_t newState;
        result = newState.DecodeMessage(result,
                                        pCommandData,
                                         CommandDataBytes);
        if (ERROR_SUCCESS != result)
        {
            WiFUtils::FmtMessage(&errorMessage, TEXT("Bad attenuation values"));
        }
        else
        {
            // Set the device's attenuation.
            AccessPointState_t resultState;
            result = pDevice->SetAccessPoint(newState,
                                            &resultState,
                                            &errorMessage);
            if (ERROR_SUCCESS == result)
            {
                result = resultState.EncodeMessage(result,
                                                   ppReturnData,
                                                    pReturnDataBytes);
            }
        }
    }
    
    return DoCommandCleanup(result, "SetAccessPoint",
                           &errorMessage, ppReturnData,
                                          pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Gets the settings for an RF attenuator.
//
static DWORD
GetAttenuator(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes)
{
    DWORD result = ERROR_SUCCESS;
    ce::tstring errorMessage;

    LogDebug(TEXT("[AC] GetAttenuator++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket.
    DeviceController_t *pDevice = LookupDevice(Sock);
    if (NULL == pDevice)
    {
        WiFUtils::FmtMessage(&errorMessage, TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        // Get the device's attenuation settings.
        RFAttenuatorState_t resultState;
        result = pDevice->GetAttenuator(&resultState,
                                        &errorMessage);
        if (ERROR_SUCCESS == result)
        {
            result = resultState.EncodeMessage(result,
                                               ppReturnData,
                                                pReturnDataBytes);
        }
    }
    
    return DoCommandCleanup(result, "GetAttenuator",
                           &errorMessage, ppReturnData,
                                          pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Sets the current settings for an RF attenuator.
//
static DWORD
SetAttenuator(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes)
{
    DWORD result = ERROR_SUCCESS;
    ce::tstring errorMessage;

    LogDebug(TEXT("[AC] SetAttenuator++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket.
    DeviceController_t *pDevice = LookupDevice(Sock);
    if (NULL == pDevice)
    {
        WiFUtils::FmtMessage(&errorMessage, TEXT("Device not initialized"));
        result = WSANOTINITIALISED;
    }
    else
    {
        // Get the new attenuation value.
        RFAttenuatorState_t newState;
        result = newState.DecodeMessage(result,
                                        pCommandData,
                                         CommandDataBytes);
        if (ERROR_SUCCESS != result)
        {
            WiFUtils::FmtMessage(&errorMessage, TEXT("Bad attenuation values"));
        }
        else
        {
            // Set the device's attenuation.
            RFAttenuatorState_t resultState;
            result = pDevice->SetAttenuator(newState,
                                           &resultState,
                                           &errorMessage);
            if (ERROR_SUCCESS == result)
            {
                result = resultState.EncodeMessage(result,
                                                   ppReturnData,
                                                    pReturnDataBytes);
            }
        }
    }
    
    return DoCommandCleanup(result, "SetAttenuator",
                           &errorMessage, ppReturnData,
                                          pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Performs a generic remote device command.
//
static DWORD
DoDeviceCommand(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes)
{
    DWORD result = ERROR_SUCCESS;
    ce::tstring errorMessage;

    LogDebug(TEXT("[AC] DoDeviceCommand++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Retrieve the device-controller for this socket and perform the command.
    DeviceController_t *pDevice = LookupDevice(Sock);
    if (NULL == pDevice)
    {
        WiFUtils::FmtMessage(&errorMessage, TEXT("Device not initialized"));
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
        messageBuffer.CopyOut(&errorMessage);
    }
    
    return DoCommandCleanup(result, "DoDeviceCommand",
                           &errorMessage, ppReturnData,
                                          pReturnDataBytes);
}

// ----------------------------------------------------------------------------
//
// Gets the current server status.
//
static DWORD
GetStatus(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes)
{
    DWORD result = ERROR_SUCCESS;
    ce::tstring errorMessage;

    LogDebug(TEXT("[AC] GetStatus++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Return the current status.
   *ppReturnData = (BYTE *) malloc(sizeof(DWORD));
    if (NULL == *ppReturnData)
    {
        WiFUtils::FmtMessage(&errorMessage, TEXT("Can't allocate memory"));
        result = APControlData_t::EncodeMessage(ERROR_OUTOFMEMORY,
                                                errorMessage,
                                                ppReturnData,
                                                 pReturnDataBytes);
    }
    else
    {
        errorMessage.clear();
        DWORD oldStatus = InterlockedCompareExchange(&s_ServerStatus, 0, 0);
        memcpy(*ppReturnData, &oldStatus, sizeof(DWORD));
       *pReturnDataBytes = sizeof(DWORD);
    }

    LogDebug(TEXT("[AC] GetStatus-- result: 0x%x(%u): %s"),
             result, result, &errorMessage[0]);
    return result;
}

// ----------------------------------------------------------------------------
//
// Sets the current server status.
//
static DWORD
SetStatus(
    IN  SOCKET  Sock,
    IN  DWORD   Command,
    IN  BYTE  *pCommandData,
    IN  DWORD   CommandDataBytes,
    OUT BYTE **ppReturnData,
    OUT DWORD  *pReturnDataBytes)
{
    DWORD result = ERROR_SUCCESS;
    ce::tstring errorMessage;

    LogDebug(TEXT("[AC] SetStatus++ command %d with %d bytes of data"),
             Command, CommandDataBytes);

    // Get the new server status.
    if (sizeof(DWORD) != CommandDataBytes)
    {
        WiFUtils::FmtMessage(&errorMessage, TEXT("Bad status-code size"));
        result = ERROR_INVALID_PARAMETER;
    }
    else
    {
       *ppReturnData = (BYTE *) malloc(sizeof(DWORD));
        if (NULL == *ppReturnData)
        {
            WiFUtils::FmtMessage(&errorMessage, TEXT("Can't allocate memory"));
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

    if (ERROR_SUCCESS == result)
    {
        errorMessage.clear();
    }
    else
    {
        result = APControlData_t::EncodeMessage(result,
                                                errorMessage,
                                                ppReturnData,
                                                 pReturnDataBytes);
    }

    LogDebug(TEXT("[AC] SetStatus-- result: 0x%x(%u): %s"),
             result, result, &errorMessage[0]);
    return result;
}


// ----------------------------------------------------------------------------
//
// A simple, high speed, hash-table linking sockets and device controllers.
//
struct DeviceTableNode
{
    SOCKET              sock;
    DeviceController_t *pdev;
    bool                temp;
    DeviceTableNode    *next;
};

static enum { DEVICE_TABLE_SIZE = 47 };
static DeviceTableNode *DeviceTable[DEVICE_TABLE_SIZE + 1];
static DeviceTableNode *DeviceTableFreeList = NULL;

static ce::critical_section DeviceTableLocker;

static void
AllocateDeviceTable(void)
{
    ce::gate<ce::critical_section> locker(DeviceTableLocker);
    memset(DeviceTable, 0, sizeof(DeviceTable));
}

static void
ReleaseDeviceTable(void)
{
    ce::gate<ce::critical_section> locker(DeviceTableLocker);
    
    DeviceTable[DEVICE_TABLE_SIZE] = DeviceTableFreeList;
    DeviceTableFreeList = NULL;

    DeviceTableNode *node, *next;
    for (int hash = 0 ; hash <= DEVICE_TABLE_SIZE ; ++hash)
    {
        node = DeviceTable[hash];
        while (node)
        {
            next = node->next;
            free(node);
            node = next;
        }
    }
}

static DeviceController_t *
LookupDevice(
    IN SOCKET Sock)
{
    int hash = (int)((unsigned long)Sock % DEVICE_TABLE_SIZE);
    ce::gate<ce::critical_section> locker(DeviceTableLocker);

    DeviceTableNode *node = DeviceTable[hash];
    while (node)
    {
        if (node->sock == Sock)
            return node->pdev;
        node = node->next;
    }

    return NULL;
}

static DWORD
InsertDevice(
    IN SOCKET              Sock,
    IN DeviceController_t *pDevice, 
    IN bool                fTemporary)
{
    int hash = (int)((unsigned long)Sock % DEVICE_TABLE_SIZE);
    ce::gate<ce::critical_section> locker(DeviceTableLocker);

    DeviceTableNode *node = DeviceTable[hash];
    while (node)
    {
        if (node->sock == Sock)
        {
            if (node->pdev != pDevice)
            {
                if (node->pdev && node->temp)
                    delete node->pdev;
                node->pdev = pDevice;
            }
            node->temp = fTemporary;
            return ERROR_SUCCESS;
        }
        node = node->next;
    }

    node = DeviceTableFreeList;
    if (node)
    {
        DeviceTableFreeList = node->next;
    }
    else
    {
        node = (DeviceTableNode *) malloc(sizeof(DeviceTableNode));
        if (NULL == node)
            return ERROR_OUTOFMEMORY;
    }

    node->sock = Sock;
    node->pdev = pDevice;
    node->temp = fTemporary;
    node->next = DeviceTable[hash];
    DeviceTable[hash] = node;
    return ERROR_SUCCESS;   
}

static bool
RemoveDevice(IN SOCKET Sock)
{
    int hash = (int)((unsigned long)Sock % DEVICE_TABLE_SIZE);
    ce::gate<ce::critical_section> locker(DeviceTableLocker);

    DeviceTableNode **parent = &DeviceTable[hash];
    while (*parent)
    {
        DeviceTableNode *node = *parent;
        if (node->sock == Sock)
        {
           *parent = node->next;
            node->next = DeviceTableFreeList;
            DeviceTableFreeList = node;
            if (node->pdev && node->temp)
                delete node->pdev;
            return true;
        }
        parent = &(node->next);
    }

    return false;
}

// ----------------------------------------------------------------------------
