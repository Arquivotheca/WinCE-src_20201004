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
// Definitions and declarations for the TelnetHandle_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_TelnetHandle_t_
#define _DEFINED_TelnetHandle_t_
#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "APCUtils.hpp"
#include "TelnetPort_t.hpp"

#include <MemBuffer_t.hpp>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides a contention-controlled interface to a telnet-server.
//
class TelnetHandle_t
{
private:

    // Server host-name,, port, user-name and password:
    ce::tstring m_ServerHost;
    DWORD       m_ServerPort;
    ce::tstring m_UserName;
    ce::tstring m_Password;

    // Connected socket:
    SOCKET m_Socket;

    // Current state:
    enum TelnetState_e {
        TelnetStateDisconnected,       // Totally disconnected
        TelnetStateNegotiating,        // Negotiating protocol options
        TelnetStateUserName,           // Waiting for user-name prompt
        TelnetStatePassword,           // Waiting for password prompt
        TelnetStateVerifyLogin,        // Checking password-entry response
        TelnetStateConnected           // Connected and authenticated
    };
    TelnetState_e m_State;

    // Time of last access:
    static const long sm_InactivityTimeoutPeriodMS = 90*1000;
    DWORD m_LastAccessTime;

    // Read/write buffers:
    MemBuffer_t m_ReadBuffer;
    MemBuffer_t m_WriteBuffer;
    
    // Number connected TelnetPorts:
    int m_NumberConnected;

    // Synchronization object:
    ce::critical_section m_Locker;

    // Pointer to next handle in list:
    TelnetHandle_t *m_Next;

public:

    // Constructor/Destructor:
    TelnetHandle_t(const TCHAR *pServerHost, DWORD ServerPort);
     virtual
   ~TelnetHandle_t(void);

    // Attaches or releases the telnet-server connection:
    static TelnetHandle_t *
    AttachHandle(
        const TCHAR *pServerHost,
        DWORD         ServerPort,
        TelnetHandle_t *(*CreateHandleFunc)(const TCHAR *, DWORD));
    static void
    DetachHandle(
        const TelnetHandle_t *pHandle);

    // Gets the telnet-server address or port (default 23):
    const TCHAR *
    GetServerHost(void) const { return m_ServerHost; }
    DWORD
    GetServerPort(void) const { return m_ServerPort; }

    // Gets or sets the user-name:
    const TCHAR *
    GetUserName(void) const { return m_UserName; }
    void
    SetUserName(const TCHAR *Value) { m_UserName = Value; }

    // Gets or sets the admin password:
    const TCHAR *
    GetPassword(void) const { return m_Password; }
    void
    SetPassword(const TCHAR *Value) { m_Password = Value; }

    // Retrieves an object which can be locked to prevent other threads
    // from using the port:
    // Callers should lock this object before performing any I/O operations.
    ce::critical_section &
    GetLocker(void) { return m_Locker; }

    // Connects to the telnet-server:
    virtual DWORD
    Connect(long MaxWaitTimeMS = TelnetPort_t::DefaultConnectTimeMS);

    // Closes the existing connection:
    virtual void
    Disconnect(void);

    // Is the connection open?
    // Note that the connection will automatically be timed out after
    // a period with no activity.
    bool
    IsConnected(void) const;

    // Flushes the read/write buffers:
    void
    Flush(void);

    // Writes a line to the server:
    DWORD
    WriteLine(
        const char *pFormat, ...);
    DWORD
    WriteLineV(
        const char *pFormat,
        va_list     pArgList,
        long        MaxWaitTimeMS = TelnetPort_t::DefaultWriteTimeMS);
    DWORD
    WriteNewLine(
        long MaxWaitTimeMS = TelnetPort_t::DefaultWriteTimeMS)
    {
        return WriteBlock("\r\n", 2, MaxWaitTimeMS);
    }

    // Writes an unformatted, "raw", block of data to the server:
    DWORD
    WriteBlock(
        const void *pBlock,
        int          BlockChars,
        long         MaxWaitTimeMS = TelnetPort_t::DefaultWriteTimeMS);

    // Sends the specified message (defaults to a newline) and waits for
    // the server to send the indicated prompt in return.
    // Returns ERROR_NOT_FOUND if the specified prompt wasn't forthcoming.
    //
    DWORD
    SendExpect(
        const char    *pExpect,
        int             ExpectChars,
        const char    *pExpectName,
        ce::string    *pPromptBuffer = NULL,
        const char    *pMessage      = "",
        TelnetLines_t *pResponse     = NULL,
        long           MaxWaitTimeMS = TelnetPort_t::DefaultReadTimeMS);
    
private:

    // Read/write buffer size parameters:
    static const DWORD BufferBlockSize = 1024;
    static const DWORD MaxBufferSize = 64 * BufferBlockSize;
    
    // Reads the next command-prompt from the server:
    // If the optional text-buffer is specified, puts everything up to
    // the prompt in the buffer. Otherwise the text is discarded.
    DWORD
    ReadCommandPrompt(
        const char    *pExpect,
        int             ExpectChars,
        ce::string    *pPromptBuffer,
        TelnetLines_t *pTextBuffer   = NULL,
        long           MaxWaitTimeMS = TelnetPort_t::DefaultReadTimeMS);
    
    // Reads raw data from the server into the read-buffer:
    // Returns ERROR_BUFFER_OVERFLOW if the read-buffer is already full.
    // Returns ERROR_CONNECTION_INVALID if the server isn't connected.
    DWORD
    ReadBuffer(
        long MaxWaitTimeMS);

    // Writes raw data to the server from the write-buffer:
    // Writes each newline-separated line individually.
    // Returns ERROR_CONNECTION_INVALID if the server isn't connected.
    DWORD
    WriteBuffer(
        DWORD BytesWritten,  // number buffered bytes which have already been written
        long  MaxWaitTimeMS);
    
};

};
};
#endif /* _DEFINED_TelnetHandle_t_ */
// ----------------------------------------------------------------------------
