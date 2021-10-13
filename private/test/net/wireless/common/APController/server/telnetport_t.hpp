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
// Definitions and declarations for the TelnetPort_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_TelnetPort_t_
#define _DEFINED_TelnetPort_t_
#pragma once

#include <APCUtils.hpp>
#include <MemBuffer_t.hpp>
#include <sync.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Provides a contention-controlled interface to a telnet server.
// Unless external synchronization is provided, each of these objects
// should be used by a single thread at a time.
//
class TelnetHandle_t;
class TelnetLines_t;
class TelnetPort_t
{
private:

    // Copy and assignment are deliberately disabled:
    TelnetPort_t(const TelnetPort_t &src);
    TelnetPort_t &operator = (const TelnetPort_t &src);

protected:

    // Current telnet-server connection:
    TelnetHandle_t *m_pHandle;

    // Special constructor for derived classes which need to create
    // a different handle type:
    TelnetPort_t(
        const TCHAR *pServerHost,
        DWORD         ServerPort,
        TelnetHandle_t *(*CreateHandleFunc)(const TCHAR *, DWORD));

public:

    // Default telnet-server port number:
    static const DWORD DefaultTelnetPort = 23;

    // Default operation times:
    static const long DefaultConnectTimeMS = 10*1000; // 10 seconds
    static const long DefaultReadTimeMS    =  2*1000; //  2 seconds
    static const long DefaultWriteTimeMS   =  1*1000; //  1 second
    
    // Contructor and destructor:
    TelnetPort_t(const TCHAR *pServerHost, DWORD ServerPort);
    virtual
   ~TelnetPort_t(void);

    // Returns true if the port handle is valid:
    bool IsValid(void) const;

    // Gets the telnet-server address or port (default 23):
    const TCHAR *
    GetServerHost(void) const;
    DWORD
    GetServerPort(void) const;

    // Gets or sets the user-name:
    const TCHAR *
    GetUserName(void) const;
    void
    SetUserName(const TCHAR *Value);

    // Gets or sets the admin password:
    const TCHAR *
    GetPassword(void) const;
    void
    SetPassword(const TCHAR *Value);

    // Retrieves an object which can be locked to prevent other threads
    // from accessing this telnet-server:
    // Callers should lock this object before performing any I/O operations.
    ce::critical_section &
    GetLocker(void) const;

    // Connects and logs in to the telnet-server:
    DWORD
    Connect(long MaxWaitTimeMS = DefaultConnectTimeMS);

    // Closes the existing connection:
    void
    Disconnect(void);

    // Is the connection open?
    // Note that the connection will automatically be timed out after
    // a period with no activity.
    bool
    IsConnected(void) const;

    // Flushes the read/write buffers:
    void
    Flush(void);

#if 0
    // Writes a line to the server:
    DWORD
    WriteLine(
        const char *pFormat, ...);
    DWORD
    WriteLineV(
        const char *pFormat,
        va_list     pArgList,
        long        MaxWaitTimeMS = DefaultWriteTimeMS);
    DWORD
    WriteNewLine(
        long MaxWaitTimeMS = DefaultWriteTimeMS)
    {
        return WriteBlock("\r\n", 2, MaxWaitTimeMS);
    }

    // Writes an unformatted, "raw", block of data to the server:
    DWORD
    WriteBlock(
        const void *pBlock,
        int          BlockChars,
        long         MaxWaitTimeMS = DefaultWriteTimeMS);
#endif

    // Sends the specified message (defaults to a newline) and waits for
    // the server to send the indicated prompt in return.
    // Returns ERROR_NOT_FOUND if the specified prompt wasn't forthcoming.
    DWORD
    SendExpect(
        const char    *pExpect,
        int             ExpectChars,
        const char    *pExpectName,
        ce::string    *pPromptBuffer = NULL,
        const char    *pMessage      = "",
        TelnetLines_t *pResponse     = NULL,
        long           MaxWaitTimeMS = DefaultReadTimeMS);
    
};

// ----------------------------------------------------------------------------
//
// Provides a mechanism for the telnet interface to return lines of
// text retrieved from the telnet server.
//
class TelnetWords_t;
class TelnetLines_t
{
protected:

    // Text retrieved from the server:
    MemBuffer_t m_Text;

    // List of separated lines:
    MemBuffer_t m_Lines;

    // Number words in the list:
    // Contains -1 if the text hasn't been parsed yet.
    int m_NumberLines;

    // Parses the text into lines and words:
    DWORD
    ParseLines(void);

    // The telnet interface is a friend:
    friend class TelnetHandle_t;

public:

    // Constructor/destructor:
    TelnetLines_t(void) { Clear(); }
    virtual
   ~TelnetLines_t(void);

    // Clears the list of parsed words:
    void
    Clear(void) { m_Text.Clear(); m_Lines.Clear(); m_NumberLines = 0; }

    // Gets the unparsed text:
    // Note that this method and the following methods are incompatible.
    // When one of the following methods is called, it will parse the
    // text into lines and words. Hence, the unmodified text will no
    // longer be available for retrieval using this method.
    const char *
    GetText(void) const { return (const char *)(m_Text.GetShared()); }

    // Gets the number of lines in the text:
    // If necessary, parses the text first.
    int
    Size(void);

    // Gets the specified line as separated words:
    // If necessary, parses the text first.
    const TelnetWords_t &
    GetLine(int IX); 
     
};

// ----------------------------------------------------------------------------
//
// Provides a mechanism for the telnet interface to return a parsed line
// of text retrieved from the telnet server.
//
class TelnetWords_t
{
private:

    // List of separated words:
    static const int MaxWordsInLine = 16;
    char *m_Words[MaxWordsInLine];

    // Number words in the list:
    int m_NumberWords;

    // Parses the words from a line of text:
    void
    ParseWords(
        char      *&pText,
        const char *pTextEnd);

    // The telnet interface is a friend:
    friend class TelnetLines_t;

public:

    // Gets the number of words in the line:
    int
    Size(void) const { return m_NumberWords; }

    // Retreves the individual words:
    const char *
    operator[](int IX) const { return m_Words[IX]; }

    // Re-merges the line of text into the specified buffer:
    const char *
    MergeLine(char *pBuffer, size_t BufferChars) const;
     
};

};
};
#endif /* _DEFINED_TelnetPort_t_ */
// ----------------------------------------------------------------------------
