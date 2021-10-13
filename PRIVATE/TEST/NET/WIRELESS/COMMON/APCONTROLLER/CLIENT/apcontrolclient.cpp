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
// This is the APController client.
//
// This program provides command-line access for manually sending a command
// to an APController server.
//
// Usage:
//      APClient [-v] [-z] [-s ServerHost] [-p Port] [-c command]
//      options:
//         -v   verbose debug output
//         -z   send debug output to standard output
//         -s   name/address of server (default: localhost)
//         -p   port to connect on (default: 33331)
//         -c   command to send (e.g. Azimuth,C1-M1-1A,SetAttenuator=45)
//         -?   show this information
//
//      Commands all contain a device-type (e.g., Azimuth or Weinschel)
//      followed by a device-name (e.g., C1-M1-1A or COM1-PORT3) followed
//      by the command-name (e.g., GetAttenuation or SetAccessPoint)
//      followed, optionally, by the command arguments (e.g., SSID=CEOPEN).
//
//      One special command "stop" exists. This will send a "stop" signal
//      to cause the server to shut itself down.
//
//      For a complete list of commands use "apclient -v -z -?".
//
// ----------------------------------------------------------------------------

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <winsock2.h>
#include <windows.h>

#include <netmain.h>
#include <backchannel.h>
#include <inc/auto_xxx.hxx>
#include <strsafe.h>

#include "APControlClient_t.hpp"
#include "APControlData_t.hpp"
#include "AttenuatorCoders.hpp"
#include "AccessPointCoders.hpp"

#ifndef _PREFAST_
#pragma warning(disable:4068)
#endif

// Enable this to see LOTS of debug output:
#define EXTRA_DEBUG 1

using namespace ce::qa;

// ----------------------------------------------------------------------------
//
// External variables for NetMain:
//
WCHAR *gszMainProgName = L"APClient";
DWORD  gdwMainInitFlags = 0;

// ----------------------------------------------------------------------------
//
// Run-time arguments:
//
// Server's host name or address:
static TCHAR DefaultServerHost[] = TEXT("localhost");
static TCHAR *   s_pServerHost   = DefaultServerHost;

// Server's port number:
static TCHAR DefaultServerPort[] = TEXT("33331");
static TCHAR *   s_pServerPort   = DefaultServerPort;

// ----------------------------------------------------------------------------
//
// Prints the program usage.
//
static void
PrintUsage()
{
    LogAlways(TEXT("++Remote Access Point device-control client++"));
    LogAlways(TEXT("%ls [-s ServerHost] [-p Port] [-c command]"), gszMainProgName);
    LogAlways(TEXT("options:"));
    LogAlways(TEXT("   -v   verbose debug output"));
    LogAlways(TEXT("   -z   send debug output to standard output"));
    LogAlways(TEXT("   -s   name/address of server (default: %s)"), DefaultServerHost);
    LogAlways(TEXT("   -p   port to connect on (default: %s)"), DefaultServerPort);
    LogAlways(TEXT("   -c   command to send (e.g. \"Azimuth,C1-M1-1A,SetAttenuator=45\")"));
    LogAlways(TEXT("        Commands:"));
    const COMMAND_HANDLER_ENTRY *cmd = g_CommandHandlerArray;
    for (; cmd->dwCommand != 0 ; ++cmd)
    {
        LogAlways(TEXT("            {devtype},{devname},%s"), cmd->tszHandlerName);
    }
    LogAlways(TEXT("            stop"));
    LogAlways(TEXT("   -?   show this information"));
}

// ----------------------------------------------------------------------------
//
// Sends an Access Point command to the server.
//
static DWORD
DoAccessPointCommand(
    int CommandCode,
    APControlClient_t        *pClient,
    const AccessPointState_t &NewState,
    ce::tstring              *pErrorMessage)
{
    AccessPointState_t response;
    DWORD result = NewState.SendMessage(CommandCode,
                                        pClient,
                                       &response,
                                        pErrorMessage);
    if (ERROR_SUCCESS == result)
    {
        result = AccessPointCoders::State2String(response, 
                                                 pErrorMessage);
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Sends an RF Attenuator command to the server.
//
static DWORD
DoAttenuatorCommand(
    int CommandCode,
    APControlClient_t         *pClient,
    const RFAttenuatorState_t &NewState,
    ce::tstring               *pErrorMessage)
{
    RFAttenuatorState_t response;
    DWORD result = NewState.SendMessage(CommandCode,
                                        pClient,
                                       &response,
                                        pErrorMessage);
    if (ERROR_SUCCESS == result)
    {
        result = AttenuatorCoders::State2String(response, 
                                                pErrorMessage);
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Sends a status-update command to the server.
//
static DWORD
DoStatusCommand(
    int CommandCode,
    APControlClient_t *pClient,
    DWORD              NewState,
    ce::tstring       *pErrorMessage)
{
    HRESULT hr;
    BYTE *pReturnData;
    DWORD  returnDataBytes;
    DWORD remoteResult;
    DWORD result = pClient->SendPacket(CommandCode,
                                       (BYTE *)&NewState,
                                       sizeof(NewState),
                                       &remoteResult,
                                       &pReturnData,
                                        &returnDataBytes);
    if (ERROR_SUCCESS == result)
    {
        if (ERROR_SUCCESS == remoteResult)
        {
            DWORD status;
            memcpy(&status, pReturnData, sizeof(status));
            TCHAR buff[30];
            hr = StringCchPrintf(buff, COUNTOF(buff), TEXT("%u"), status);
            if (FAILED(hr))
                 pErrorMessage->assign(TEXT("FAILED"));
            else pErrorMessage->assign(buff);
        }
        else
        {
            result = APControlData_t::DecodeMessage(remoteResult,
                                                   pReturnData, 
                                                    returnDataBytes, 
                                                   pErrorMessage);
        }
    }
    if (pReturnData)
    {
        free(pReturnData);
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Connects to the server and sends the specified command.
//
static int
DoClient(const WCHAR *CommandParam)
{
    ce::auto_ptr<APControlClient_t> pClient;

    AccessPointState_t  accessPointState;
    RFAttenuatorState_t attenuatorState;
    
    ce::tstring lCommandParam(CommandParam);
    TCHAR *command = lCommandParam.get_buffer();
    DWORD  commandCode;
    const TCHAR *pDeviceType;
    const TCHAR *pDeviceName;

    HRESULT     hr;
    DWORD       result;
    ce::tstring response;

    // Find and trim the command arguments (if any).
    TCHAR *argPtr = _tcschr(command, TEXT('='));
    const TCHAR *args;
    if (NULL == argPtr)
    {
        args = TEXT("");
    }
    else
    {
       *argPtr = TEXT(' ');
        while (argPtr-- != command && _istspace(*argPtr)) ;
        while (_istspace(*(++argPtr))) *argPtr = TEXT('\0');
        args = argPtr;
    }

    // Handle the "stop" command.
    if (_tcsicmp(TEXT("stop"), command) == 0)
    {
        commandCode = 0;
        pDeviceType = NULL;
        pDeviceName = NULL;
    }

    // Otherwise...
    else
    {
        // Extract the device-type and -name.
        static const TCHAR *comma = TEXT(",");
        
        pDeviceType = pDeviceName = NULL;
        TCHAR *commandName = NULL;
        for (TCHAR *token = _tcstok(command, comma) ;
                    token != NULL;
                    token = _tcstok(NULL, comma))
        {
            if      (NULL == pDeviceType) { pDeviceType = token; }
            else if (NULL == pDeviceName) { pDeviceName = token; }
            else                          { commandName = token; break; }
        }
                    
        if (NULL == pDeviceType)
        {
            LogError(TEXT("Missing device-type in \"%s\""), CommandParam);
            PrintUsage();
            return -1;
        }
        
        if (NULL == pDeviceName)
        {
            LogError(TEXT("Missing device-name in \"%s\""), CommandParam);
            PrintUsage();
            return -1;
        }
        
        if (NULL == commandName)
        {
            LogError(TEXT("Missing command-name in \"%s\""), CommandParam);
            PrintUsage();
            return -1;
        }
        
        command = commandName;

        // Look up the command name in the list.
        commandCode = APControlData_t::GetCommandCode(command);
        if (0 > (long)commandCode)
        {
            LogError(TEXT("Unknown command name \"%s\""), command);
            PrintUsage();
            return -1;
        }
    }
    
    // Allocate the client object.
    pClient = new APControlClient_t(s_pServerHost,
                                    s_pServerPort, pDeviceType,
                                                   pDeviceName);
    if (!pClient.valid())
    {
        LogError(TEXT("Failed allocating APControlClient object"));
        return -1;
    }

    // Connect to the server.
    LogAlways(TEXT("Connecting the BackChannel at %s:%s"),
              s_pServerHost, s_pServerPort);
    hr = pClient->Connect();
    if (FAILED(hr))
    {
        LogError(TEXT("BackChannel_Connect() failed: %s"),
                 HRESULTErrorText(hr));
        return -1;
    }
    
    // Send the command and wait for a response.
    LogAlways(TEXT("Sending command \"%s\" with arguments \"%s\""),
             command, args);
    switch (commandCode)
    {
        case APControlData_t::GetAccessPointCommandCode:

            result = DoAccessPointCommand(commandCode, pClient, 
                                          accessPointState, &response);
            break;

        case APControlData_t::SetAccessPointCommandCode:
            result = AccessPointCoders::String2State(args, &accessPointState);
            if (ERROR_SUCCESS != result)
                return -1;
            result = DoAccessPointCommand(commandCode, pClient,
                                          accessPointState, &response);
            break;
            
        case APControlData_t::GetAttenuatorCommandCode:

            result = DoAttenuatorCommand(commandCode, pClient, 
                                         attenuatorState, &response);
            break;

        case APControlData_t::SetAttenuatorCommandCode:
            result = AttenuatorCoders::String2State(args, &attenuatorState);
            if (ERROR_SUCCESS != result)
                return -1;
            result = DoAttenuatorCommand(commandCode, pClient,
                                         attenuatorState, &response);
            break;

        case APControlData_t::GetStatusCommandCode:
            result = DoStatusCommand(commandCode, pClient,
                                     0, &response);
            break;

        case APControlData_t::SetStatusCommandCode:
            result = DoStatusCommand(commandCode, pClient,
                                    (DWORD)wcstol(args, NULL, 10), &response);
            break;
            
        default:
            result = pClient->SendStringCommand(commandCode,
                                                ce::wstring(args), &response);
            break;
    }

    LogAlways(TEXT("The remote call %hs: result: %s"),
             (result == ERROR_SUCCESS)? "SUCCEEDED" : "FAILED",
             Win32ErrorText(result));
    if (0 == response.length())
    {
        LogAlways(TEXT("No return message"));
    }
    else
    {
        LogAlways(TEXT("Return message (length=%d):"), response.length());
        LogAlways(response);
    }

    // Disconnect from the server.
    hr = pClient->Disconnect();
    if (FAILED(hr))
    {
        LogError(TEXT("BackChannel_Disconnect() failed: %s"),
                 HRESULTErrorText(hr));
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//
//
int
mymain(int argc, WCHAR *argv[])
{
    WSADATA wsaData;
    WCHAR *commandParam = NULL;
        
    CmnPrint_Initialize();
    CmnPrint_RegisterPrintFunctionEx(PT_LOG,     NetLogDebug,   FALSE);
    CmnPrint_RegisterPrintFunctionEx(PT_WARN1,   NetLogWarning, FALSE);
    CmnPrint_RegisterPrintFunctionEx(PT_WARN2,   NetLogWarning, FALSE);
    CmnPrint_RegisterPrintFunctionEx(PT_FAIL,    NetLogError,   FALSE);
    CmnPrint_RegisterPrintFunctionEx(PT_VERBOSE, NetLogMessage, FALSE);

    // Parse the run-time arguments.
    if (QAWasOption(TEXT("?")) || QAWasOption(TEXT("help")))
    {
        PrintUsage();
        return 0;
    }

    QAGetOption(TEXT("s"), &s_pServerHost);
    QAGetOption(TEXT("p"), &s_pServerPort);
    QAGetOption(TEXT("c"), &commandParam);

    // Initialize WinSock.

    int result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (0 != result)
    {
        result = GetLastError();
        LogError(TEXT("WSAStartup failed: %s"), Win32ErrorText(result));
        return -1;
    }

    // Format the command and send it to the server.
    result = DoClient(commandParam);

    // Shut down WinSock.
    WSACleanup();
    return result;
}

// ----------------------------------------------------------------------------
