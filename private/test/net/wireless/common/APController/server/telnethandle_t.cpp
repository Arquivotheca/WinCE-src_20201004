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
// Implementation of the TelnetPort_t class.
//
// ----------------------------------------------------------------------------

#include "TelnetHandle_t.hpp"

#include <assert.h>
#include <intsafe.h>
#include <tchar.h>
#include <strsafe.h>

#include <auto_xxx.hxx>

// Turns on/off various types of logging:
//
//#define DEBUG_SEND_RECV      1
//#define DEBUG_SEND_RECV_RAW  1

using namespace ce::qa;


// ----------------------------------------------------------------------------
//
// Constructor.
//
TelnetHandle_t::
TelnetHandle_t(
    const TCHAR *pServerHost,
    DWORD         ServerPort)
    : m_ServerHost(pServerHost),
      m_ServerPort(ServerPort),
      m_NumberConnected(1),
      m_State(TelnetStateDisconnected),
      m_LastAccessTime(0),
      m_Next(NULL)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
TelnetHandle_t::
~TelnetHandle_t(void)
{
    Disconnect();
}

// ----------------------------------------------------------------------------
//
// Existing handles:
//
static TelnetHandle_t      *s_TelnetHandles = NULL;
static ce::critical_section s_TelnetHandlesLocker;

// ----------------------------------------------------------------------------
//
// Attaches the telnet-server handle.
//
TelnetHandle_t *
TelnetHandle_t::
AttachHandle(
    const TCHAR *pServerHost,
    DWORD         ServerPort,
    TelnetHandle_t *(*CreateHandleFunc)(const TCHAR *, DWORD))
{
    ce::gate<ce::critical_section> locker(s_TelnetHandlesLocker);

    TelnetHandle_t *hand = s_TelnetHandles;
    for (;;)
    {
        if (NULL == hand)
        {
            hand = CreateHandleFunc(pServerHost, ServerPort);
            if (NULL == hand)
            {
                LogError(TEXT("[AC] Can't allocate TelnetHandle for %s:%lu"),
                         pServerHost, ServerPort);
            }
            else
            {
                hand->m_Next = s_TelnetHandles;
                s_TelnetHandles = hand;
            }
            break;
        }
        if (ServerPort == hand->GetServerPort()
         && _tcscmp(pServerHost, hand->GetServerHost()) == 0)
        {
            hand->m_NumberConnected++;
            break;
        }
        hand = hand->m_Next;
    }

    return hand;
}

// ----------------------------------------------------------------------------
//
// Releases the telnet-server handle.
//
void
TelnetHandle_t::
DetachHandle(const TelnetHandle_t *pHandle)
{
    ce::gate<ce::critical_section> locker(s_TelnetHandlesLocker);

    TelnetHandle_t **parent = &s_TelnetHandles;
    for (;;)
    {
        TelnetHandle_t *hand = *parent;
        if (NULL == hand)
        {
            assert(!"Tried to detach unknown TelnetHandle");
            break;
        }
        if (hand == pHandle)
        {
            if (--hand->m_NumberConnected <= 0)
            {
               *parent = hand->m_Next;
                delete hand;
            }
            break;
        }
        parent = &(hand->m_Next);
    }
}

// ----------------------------------------------------------------------------
//
// Opens a socket to the specified destination.
//
static DWORD
ConnectToAddress(
    const TCHAR *pServerHost,
    DWORD         ServerPort,
    SOCKET      *pSocket,   // can be NULL
    sockaddr    *pSockAddr) // can be NULL
{
    HRESULT hr;
    DWORD result;

    assert(NULL != pServerHost && TEXT('\0') != pServerHost[0]);

    // Translate the host and port to ASCII.
    char asciiName[MAX_PATH];
    char asciiPort[30];
    hr = WiFUtils::ConvertString(asciiName, pServerHost, 
                         COUNTOF(asciiName), "telnet host name");
    if (SUCCEEDED(hr))
        hr = StringCchPrintfA(asciiPort, COUNTOF(asciiPort), "%lu", ServerPort);
    if (FAILED(hr))
    {
        LogError(TEXT("Can't translate IP address \"%s:%lu\": %s"),
                 pServerHost, ServerPort,
                 HRESULTErrorText(hr));
        return HRESULT_CODE(hr);
    }

    // Resolve the address.
    ADDRINFO hints, *pAddrInfo = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    result = getaddrinfo(asciiName, asciiPort, &hints, &pAddrInfo);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Cannot resolve address \"%hs:%hs\": wsa %s"),
                 asciiName, asciiPort, Win32ErrorText(result));
    }
    else
    {
        // Attempt to connect each address in turn.
        result = WSANO_DATA;
        for (ADDRINFO *pAI = pAddrInfo ; NULL != pAI ; pAI = pAI->ai_next)
        {
            if (PF_INET == pAI->ai_family || PF_INET6 == pAI->ai_family)
            {
                ce::auto_socket sock = socket(pAI->ai_family,
                                              pAI->ai_socktype,
                                              pAI->ai_protocol); 

                result = NO_ERROR;
                if (!sock.valid()
                 || SOCKET_ERROR == connect(sock, pAI->ai_addr,
                                                  pAI->ai_addrlen))
                {
                    result = WSAGetLastError();
                    if (NO_ERROR == result)
                        result = ERROR_INVALID_PARAMETER;
                }
                
                if (NO_ERROR == result)
                {
                    if (NULL != pSocket)
                       *pSocket = sock.release();
                    if (NULL != pSockAddr)
                       *pSockAddr = *pAI->ai_addr;
                    break;
                }
            }
        }

        if (NO_ERROR != result)
        {
            LogError(TEXT("Can't create TCP socket for %hs:%hs: wsa %s"),
                     asciiName, asciiPort, Win32ErrorText(result));
        }
    }

    // Deallocate resources.
    if (NULL != pAddrInfo)
    {
        freeaddrinfo(pAddrInfo);
    }
    return result;
}

// ----------------------------------------------------------------------------
//
// Connects to the device's telnet-server to access the specified page.
//
DWORD
TelnetHandle_t::
Connect(long MaxWaitTimeMS /* = TelnetPort_t::DefaultConnectTimeMS */)
{        
    DWORD result = NO_ERROR;

    // Make sure we're actually disconnected. If we're still connected,
    // but the line has timed out, close it first before reconnecting.
    if (IsConnected())
        return NO_ERROR;
    if (m_State != TelnetStateDisconnected)
    {
        long inactivityTime = WiFUtils::SubtractTickCounts(GetTickCount(),
                                                           m_LastAccessTime);
        LogDebug(TEXT("[AC] Timing out connection to %s:%lu after %lds of inactivity"), 
                 GetServerHost(), GetServerPort(),
                 inactivityTime / 1000);
        Disconnect();
    }
    
    LogDebug(TEXT("[AC] Connecting to %s:%lu"),
             GetServerHost(), GetServerPort());
    
    DWORD startTime = GetTickCount();
    TelnetLines_t textBuffer;
    do
    {
        // Make sure we haven't already waited too long.
        long timeWaited = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
        if (MaxWaitTimeMS <= timeWaited)
        {   
            LogError(TEXT("[AC] Tried %lu seconds to connect to %s:%lu"),
                     MaxWaitTimeMS / 1000,
                     GetServerHost(), GetServerPort());
            result = ERROR_TIMEOUT;
            break;
        }
        long remainingTime = MaxWaitTimeMS - timeWaited;
        
        switch (m_State)
        {
        case TelnetStateDisconnected:
            result = ConnectToAddress(GetServerHost(),
                                      GetServerPort(),
                                     &m_Socket,
                                      NULL); // Don't need the sockaddr
            if (NO_ERROR == result)
            {
                LogDebug(TEXT("[AC]   connected with sock=0x%08lX"),
                        (ULONG)m_Socket);
                m_State = TelnetStateNegotiating;
            }
            break;
            
        case TelnetStateNegotiating:
            result = SendExpect("username:", 9,     // expected prompt
                                "user-name prompt", // prompt description
                                 NULL,              // prompt buffer
                                 NULL,              // attention message
                                &textBuffer,        // response buffer
                                 remainingTime);    // time to wait
            if (NO_ERROR != result)
                Disconnect();
            else
            {
                m_State = TelnetStateUserName;

                // We expect to find the IAC (Interpret As Command) string
                // at the beginning of the initial prompt from the server.
                // If it's there, we respond with either "will->do/dont" or
                // "do->will/wont" depending whether we want the server to
                // do something or we want do do womething the server wants.
                //
                // In reality, we're okay with everything the server wants
                // to do (even echo). The only thing we treat specially is
                // the window-size request. We don't want to have "smart"
                // servers send their reports with "--More--" lines in the
                // middle, so we send a huge window-size.
                
                // Telnet commands:
                static const UCHAR TelnetCmdSE   = (UCHAR)0xF0;
                static const UCHAR TelnetCmdSB   = (UCHAR)0xFA;
                static const UCHAR TelnetCmdWill = (UCHAR)0xFB;
                static const UCHAR TelnetCmdWont = (UCHAR)0xFC;
                static const UCHAR TelnetCmdDo   = (UCHAR)0xFD;
                static const UCHAR TelnetCmdDont = (UCHAR)0xFE;
                static const UCHAR TelnetCmdIAC  = (UCHAR)0xFF;

                // Telnet options:
                static const UCHAR TelnetOptEcho            = (UCHAR)0x01; 
                static const UCHAR TelnetOptSuppressGoAhead = (UCHAR)0x03; 
                static const UCHAR TelnetOptWindowSize      = (UCHAR)0x1F; 
                static const UCHAR TelnetOptLineMode        = (UCHAR)0x22; 
                
                const UCHAR *pIn = (const UCHAR *)(textBuffer.GetText());
                if (pIn[0] == TelnetCmdIAC)
                {
                    UCHAR response[256];
                          UCHAR *pOut    = &response[0];
                    const UCHAR *pOutEnd = &response[COUNTOF(response) - 5];
                    while (pOut < pOutEnd)
                    {
                        if (pIn[0] == '\0' || (pIn[0] == '\r' && pIn[1] == '\n'))
                            break;
                        if (*(pIn++) == TelnetCmdIAC)
                        {
                            UCHAR operation = pIn[0];
                            UCHAR option    = pIn[1];
                            
                            // Supported option?
                            bool supported;
                            if (TelnetOptEcho            == option
                             || TelnetOptSuppressGoAhead == option
                             || TelnetOptWindowSize      == option)
                                 supported = true;
                            else supported = false;

                            // Prepare response.
                            UCHAR answer;
                            switch (operation)
                            {
                            case TelnetCmdWill:
                                answer = supported? TelnetCmdDo  
                                                  : TelnetCmdDont;
                                break;
                            case TelnetCmdDo:
                                answer = supported? TelnetCmdWill
                                                  : TelnetCmdWont;
                                break;
                                
                            default:
                                // Ignore anything else.
                                continue;
                            }

                            // Send a huge window size to avoid "--More--".
                            if (TelnetOptWindowSize == option)
                            {
                               *(pOut++) = TelnetCmdIAC;
                               *(pOut++) = TelnetCmdSB;
                               *(pOut++) = TelnetOptWindowSize;
                               *(pOut++) = 0x01;    // Terminal width = 300
                               *(pOut++) = 0x2C;
                               *(pOut++) = 0x01;    // Terminal length = 500
                               *(pOut++) = 0xF2;
                               *(pOut++) = TelnetCmdIAC;
                               *(pOut++) = TelnetCmdSE;
                            }
                            else
                            {
                               *(pOut++) = TelnetCmdIAC;
                               *(pOut++) = answer;
                               *(pOut++) = option;
                            }

                            pIn += 2;
                        }
                    }

                    if (pOut > response)
                    {
                        result = WriteBlock(response, pOut - response);
                        if (NO_ERROR != result)
                            Disconnect();
                    }
                }
            }
            break;

        case TelnetStateUserName:
            result = SendExpect("username:", 9,     // expected prompt
                                "user-name prompt", // prompt description
                                 NULL,              // prompt buffer
                                 "",                // attention message
                                 NULL,              // response buffer
                                 remainingTime);    // time to wait
            if (NO_ERROR != result)
                Disconnect();
            else
            {
                result = WriteLine("%ls", GetUserName());
                if (NO_ERROR == result)
                    m_State = TelnetStatePassword;
            }
            break;

        case TelnetStatePassword:
            result = SendExpect("password:", 9,     // expected prompt
                                "password prompt",  // prompt description
                                 NULL,              // prompt buffer
                                 NULL,              // attention message
                                 NULL,              // response buffer
                                 remainingTime);    // time to wait
            if (NO_ERROR != result)
                Disconnect();
            else
            {
                result = WriteLine("%ls", GetPassword());
                if (NO_ERROR == result)
                    m_State = TelnetStateVerifyLogin;
            }
            break;

        case TelnetStateVerifyLogin:
            result = SendExpect(">", 1,             // expected prompt
                                "password-accepted indication",
                                 NULL,              // prompt buffer
                                 NULL,              // attention message
                                 NULL,              // response buffer
                                 remainingTime);    // time to wait
            if (NO_ERROR != result)
                Disconnect();
            else
            {
                m_State = TelnetStateConnected;
                Flush();
            }
            break;
        }
    }
    while (TelnetStateConnected != m_State
       && (NO_ERROR == result || ERROR_CONNECTION_INVALID == result));
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Disconnects the current connection to cause a reconnection next time.
//
void
TelnetHandle_t::
Disconnect(void)
{
    if (m_State != TelnetStateDisconnected)
    {
        LogDebug(TEXT("[AC] Disconnecting from %s:%lu: sock=0x%08lX"), 
                 GetServerHost(), GetServerPort(), (ULONG)m_Socket);

        if (m_Socket)
        {
            shutdown(m_Socket, SD_BOTH);
        	closesocket(m_Socket);
            m_Socket = NULL;
        }
        
        m_State = TelnetStateDisconnected;
        m_LastAccessTime = 0;

        Flush();
    }
}

// ----------------------------------------------------------------------------
//
// Is the connection open?
// Note that the connection will automatically be timed out after
// a period with no activity.
//
bool
TelnetHandle_t::
IsConnected(void) const
{
    if (m_State != TelnetStateConnected)
        return false;

    long inactivityTime = WiFUtils::SubtractTickCounts(GetTickCount(),
                                                       m_LastAccessTime);
    return inactivityTime < sm_InactivityTimeoutPeriodMS;
}

// ----------------------------------------------------------------------------
//
// Flushes the read/write buffers:
//
void
TelnetHandle_t::
Flush(void)
{
    if (TelnetStateConnected == m_State)
    {
        // Flush unread data.
        static const long FlushWaitTimeMS = 20;
        ReadBuffer(FlushWaitTimeMS);
    }
    
    m_ReadBuffer.Clear();
    m_WriteBuffer.Clear();
}

// ----------------------------------------------------------------------------
//
// Writes a line to the server:
//
DWORD
TelnetHandle_t::
WriteLine(
    const char *pFormat, ...)
{
    va_list pArgList;
    va_start(pArgList, pFormat);
    DWORD result = WriteLineV(pFormat, pArgList, TelnetPort_t::DefaultWriteTimeMS);
    va_end(pArgList);
    return result;
}

DWORD
TelnetHandle_t::
WriteLineV(
    const char *pFormat,
    va_list     pArgList,
    long        MaxWaitTimeMS /* = TelnetPort_t::DefaultWriteTimeMS */)
{
    DWORD result = NO_ERROR;
    char buff[2*1024], *buffEnd = NULL;

    // Format the line(s).
  __try
    {
        HRESULT hr = StringCchVPrintfExA(buff, COUNTOF(buff) - 2,
                                        &buffEnd, NULL, STRSAFE_IGNORE_NULLS,
                                         pFormat, pArgList);
        if (FAILED(hr))
        {
            LogError(TEXT("Can't format line \"%.64hs...\" to send to %s:%lu: %s"),
                     pFormat, GetServerHost(), GetServerPort(),
                     HRESULTErrorText(hr));
            return HRESULT_CODE(hr);
        }
    }
  __except(1)
    {
        LogError(TEXT("Exception formatting \"%.64hs...\" to send to %s:%lu"),
                 pFormat, GetServerHost(), GetServerPort());
        return ERROR_INVALID_PARAMETER;
    }

    // Append a newline.
   *(buffEnd++) = '\r';
   *(buffEnd++) = '\n';

    // Write it to the server.
    return WriteBlock(buff, buffEnd - buff, MaxWaitTimeMS);
}

// ----------------------------------------------------------------------------
//
// Writes unformatted, "raw", data to the server:
//
DWORD
TelnetHandle_t::
WriteBlock(
    const void *pBlock,
    int          BlockChars,
    long         MaxWaitTimeMS /* = TelnetPort_t::DefaultWriteTimeMS */)
{
    DWORD result;

    // Save the echo-cancellation data size.
    DWORD bytesWritten = m_WriteBuffer.Length();
    
    // Append the raw data to the buffer.
    result = m_WriteBuffer.Append((BYTE *)pBlock, BlockChars, sizeof(char));
    if (NO_ERROR != result)
    {
        LogError(TEXT("Can't add %d bytes to %s:%lu write-buffer: %s"),
                 BlockChars, GetServerHost(), GetServerPort(),
                 Win32ErrorText(result));
        return result;
    }

    // Write it to the server.
    return WriteBuffer(bytesWritten, MaxWaitTimeMS);
}

// ----------------------------------------------------------------------------
//
// Sends the specified message (defaults to a newline) and waits for
// the server to send the indicated prompt in return.
// Returns ERROR_NOT_FOUND if the specified prompt wasn't forthcoming.
//
//
DWORD
TelnetHandle_t::
SendExpect(
    const char    *pExpect,
    int             ExpectChars,
    const char    *pExpectName,
    ce::string    *pPromptBuffer /* = NULL */,
    const char    *pMessage      /* = "" */,
    TelnetLines_t *pResponse     /* = NULL */,
    long           MaxWaitTimeMS /* = TelnetPort_t::DefaultReadTimeMS */)
{
    DWORD result = NO_ERROR;
    DWORD startTime = GetTickCount();

    ce::string localBuffer;
    if (pPromptBuffer == NULL)
        pPromptBuffer = &localBuffer;

    if (pResponse)
        pResponse->Clear();

    // If required, send the message to the server.
    if (pMessage != NULL)
    {
        result = WriteLine("%s", pMessage);
        if (NO_ERROR != result)
            return result;
    }
    
    for (;;)
    {
        // Get the command prompt.
        result = ReadCommandPrompt(pExpect,
                                    ExpectChars,
                                   pPromptBuffer,
                                   pResponse,
                                   MaxWaitTimeMS);
        if (ERROR_TIMEOUT != result)
            break;

        // If we've already sent a message, we're not seeing the
        // response we expected.
        if (pMessage != NULL)
            break;

        // Otherwise, send a newline to get the server's attention.
        pMessage = "";
        result = WriteNewLine();
        if (NO_ERROR != result)
            break;
    }

    if (ERROR_TIMEOUT == result)
    {
        if (pPromptBuffer->length() != 0)
            result = ERROR_NOT_FOUND;
        else
        {
            long timeWaited = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
            LogError(TEXT("Did not get prompt from %s:%lu after")
                     TEXT(" waiting %.02fs - expected \"%hs\""),
                     GetServerHost(),
                     GetServerPort(),
                    (double)timeWaited / 1000.0,
                     pExpectName);
        }
    }
    
    return result;
}

// ----------------------------------------------------------------------------
//
// Reads the next command-prompt from the server:
// If the optional text-buffer is specified, puts everything up to
// the prompt in the buffer. Otherwise, the text is discarded.
//
DWORD
TelnetHandle_t::
ReadCommandPrompt(
    const char    *pExpect,
    int             ExpectChars,
    ce::string    *pPromptBuffer,
    TelnetLines_t *pTextBuffer,
    long           MaxWaitTimeMS /* = TelnetPort_t::DefaultReadTimeMS */)
{    
    static const char   Terminators[] = ":>#";
    static const size_t TerminatorChars = COUNTOF(Terminators)-1;
    
    DWORD result = NO_ERROR;
    DWORD startTime = GetTickCount();
    
    pPromptBuffer->clear();
    for (;;)
    {
        // See if there's anything interesting in the read-buffer.
        if (m_ReadBuffer.Length() >= 2)
        {
            // First, remove the terminated line(s).
            char *pLine; const char *pLineEnd, *pLineMax;
            pLine = (char *)m_ReadBuffer.GetPrivate();
            pLineMax = &pLine[m_ReadBuffer.Length()];
            for (pLineEnd = pLineMax-2 ; pLineEnd > pLine ; --pLineEnd)
                if (pLineEnd[0] == '\r' && pLineEnd[1] == '\n')
                    break;

            if (pLineEnd[0] == '\r' && pLineEnd[1] == '\n')
            {
                DWORD lineLen = (pLineEnd - pLine) + 2;

                // If they supplied a text-buffer, copy the lines
                // before removing them from the read buffer.
                if (pTextBuffer)
                {
                    // Flag the text as unparsed.
                    pTextBuffer->m_NumberLines = -1;

                    // Tack on a few nulls to terminate the text.
                    static const DWORD TextTerminationNulls = sizeof(WCHAR);
                    result = pTextBuffer->m_Text.Append((BYTE *)pLine, lineLen,
                                                         TextTerminationNulls);
                    if (NO_ERROR != result)
                    {
                        LogError(TEXT("Can't allocate %lu bytes to copy")
                                 TEXT(" text from %s:%lu"),
                                 lineLen,
                                 GetServerHost(), GetServerPort());
                        break;
                    }
                }

                if (m_ReadBuffer.Length() <= lineLen)
                {
                    m_ReadBuffer.Clear();
                    continue;
                }
                else
                {
                    DWORD lineLeft = m_ReadBuffer.Length() - lineLen;
                    memmove(pLine, &pLine[lineLen], lineLeft);
                    m_ReadBuffer.SetLength(lineLeft);

                    // If there's enough text left, drop through to
                    // check for a command-prompt.
                    if (lineLeft < 2)
                        continue;
                    else
                        pLineMax = &pLine[lineLeft];
                }
            }

            // Check what remains for a command-prompt.
            pLineEnd = pLineMax;
            pLineEnd--;
            if (isspace((unsigned char)pLineEnd[0]))
                pLineEnd--;
            int termIX = TerminatorChars;
            while (--termIX >= 0)
                if (pLineEnd[0] == Terminators[termIX])
                    break;

            if (termIX >= 0)
            {
                pLineEnd++;
                DWORD lineLen = pLineEnd - pLine;
                if (!pPromptBuffer->assign(pLine, lineLen))
                {
                    LogError(TEXT("Can't copy %lu-byte")
                             TEXT(" command-prompt from %s:%lu"),
                             lineLen,
                             GetServerHost(), GetServerPort());
                    result = ERROR_OUTOFMEMORY;
                    break;
                }

#ifdef DEBUG_SEND_RECV
                LogDebug(TEXT("[AC]   Recv \"%hs\""), &(*pPromptBuffer)[0]);
#endif

                // If it's the prompt we wanted, return it.
                // Otherwise, keep waiting.
                if (lineLen >= (DWORD)ExpectChars
                 && _strnicmp(pLineEnd-ExpectChars, pExpect, ExpectChars) == 0)
                {
                    // We've read everything there is.
                    m_ReadBuffer.Clear();
                    m_WriteBuffer.Clear();  // flush echo-cancellation buffer, too
                    break;
                }
            }
        }

        // Make sure we haven't already waited too long.
        long timeWaited = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
        if (MaxWaitTimeMS <= timeWaited)
        {
            result = ERROR_TIMEOUT;
            break;
        }

        // Get more response from the server.
        result = ReadBuffer(MaxWaitTimeMS - timeWaited);
        if (NO_ERROR != result)
        {
            if (ERROR_BUFFER_OVERFLOW != result)
                break;
            else
            {
                LogWarn(TEXT("Read %lu bytes without command-prompt from %s:%lu"),
                        MaxBufferSize,
                        GetServerHost(), GetServerPort());
                m_ReadBuffer.Clear();

                // Do this in case the server has gone into some kind
                // of error state sending garbage to us.
                Sleep(500);
            }
        }
    }

    return result;
}

#if defined(DEBUG_SEND_RECV_RAW)
// ----------------------------------------------------------------------------
//
// Logs the specified read/write block.
//
static void
LogIOBlock(const void *pBlock, int Length)
{
    const char *pChars = (const char *)pBlock;
    for (int bx = 0 ; bx < Length ;)
    {
        static const int LineChars = 20;
        int lineOffset = bx;
        char  hexValues[LineChars * 3 + 1], *px =  hexValues;
        char charValues[LineChars * 3 + 1], *pc = charValues;
        for (int lx = 0 ; lx < LineChars && bx < Length ; ++lx, ++bx)
        {
            static const char HexValues[] = "0123456789ABCDEF";
            unsigned int ch = (unsigned int)pChars[bx] & 0xFF;
           *(pc++) = *(px++) = ' ';
            if (isprint(ch))
            {
                *(pc++) = ' ';
                *(pc++) = ch;
            }
            else
            {
                switch (ch)
                {
                case '\b': *(pc++) = '\\'; *(pc++) = 'b'; break;
                case '\n': *(pc++) = '\\'; *(pc++) = 'n'; break;
                case '\r': *(pc++) = '\\'; *(pc++) = 'r'; break;
                case '\t': *(pc++) = '\\'; *(pc++) = 't'; break;
                default:   *(pc++) = ' ';  *(pc++) = '?'; break;
                }
            }
           *(px++) = HexValues[(ch >> 4) & 0xF];
           *(px++) = HexValues[ ch       & 0xF];
        }
       *px = *pc = '\0';
        LogDebug(TEXT("[AC]     %04d %hs"), lineOffset, charValues);
        LogDebug(TEXT("[AC]          %hs"),               hexValues);
    }
}
#endif

// ----------------------------------------------------------------------------
//
// Reads raw data from the server into the read-buffer:
// Returns ERROR_BUFFER_OVERFLOW if the read-buffer is already full.
// Returns ERROR_CONNECTION_INVALID if the server isn't connected.
//
DWORD
TelnetHandle_t::
ReadBuffer(
    long MaxWaitTimeMS)
{
    HRESULT hr;
    DWORD result = NO_ERROR;
    DWORD startTime = GetTickCount();

    // Make sure we're connected.
    if (TelnetStateDisconnected == m_State)
    {
        LogError(TEXT("Connection dropped to %s:%lu"),
                 GetServerHost(), GetServerPort());
        return ERROR_CONNECTION_INVALID;
    }

    // The first select waits this long.
    TIMEVAL timeval = { MaxWaitTimeMS / 1000L,
                       (MaxWaitTimeMS % 1000L) * 1000L };
    
    for (;;)
    {
        // Wait for more characters from the server.
        fd_set recvFdSet;
        FD_ZERO(&recvFdSet);
        FD_SET(m_Socket, &recvFdSet);

        int numFds = select(0, &recvFdSet, NULL, NULL, &timeval);
        if (SOCKET_ERROR == numFds)
        {
            result = WSAGetLastError();
            LogError(TEXT("Select error for recv from %s:%lu: wsa %s"),
                     GetServerHost(), GetServerPort(),
                     Win32ErrorText(result));
            Disconnect();
            return result;
        }

        // Return when there's nothing left to read.
        if (0 >= numFds)
            break;

        // If necessary, increase the read buffer size.
        DWORD newLength = m_ReadBuffer.Length();
        DWORD capacity  = m_ReadBuffer.Capacity();
     
        hr = DWordAdd(newLength, BufferBlockSize, &newLength);
        if (FAILED(hr))
            result = HRESULT_CODE(hr);
        else
        if (newLength > capacity)
        {
            hr = DWordAdd(capacity, BufferBlockSize, &capacity);
            if (FAILED(hr))
                result = HRESULT_CODE(hr);
            else
            if (capacity > MaxBufferSize)
                return ERROR_BUFFER_OVERFLOW;
            else
                result = m_ReadBuffer.Reserve(capacity);
        }
        if (NO_ERROR != result)
        {
            LogError(TEXT("Can't allocate %lu bytes for %s:%lu read buffer"),
                     capacity,
                     GetServerHost(), GetServerPort());
            return result;
        }

        // Read the data.
        char *readPtr = (char *)m_ReadBuffer.GetPrivate();
        readPtr += m_ReadBuffer.Length();
        
        int readResult = recv(m_Socket, readPtr, BufferBlockSize, 0);
        if (SOCKET_ERROR == readResult)
        {
            result = WSAGetLastError();
            LogError(TEXT("Can't read packet from %s:%lu: wsa %s"),
                     GetServerHost(), GetServerPort(), 
                     Win32ErrorText(result));
            Disconnect();
            return result;
        }
        else
        if (0 >= readResult)
        {
            result = ERROR_CONNECTION_INVALID;    
            LogDebug(TEXT("Connection closed by %s:%lu"),
                     GetServerHost(), GetServerPort());
            Disconnect();
            return result;
        }
        else
        {
#ifdef DEBUG_SEND_RECV_RAW
            LogDebug(TEXT("[AC]   recv (length=%d):"), readResult);
            LogIOBlock(readPtr, readResult);
#endif
            m_ReadBuffer.SetLength(m_ReadBuffer.Length() + readResult);
            m_LastAccessTime = GetTickCount();         
        }

        // Subsequent selects are semi-non-blocking.
        static const long SendCompletionTimeUS = 1;
        timeval.tv_sec  = 0;
        timeval.tv_usec = SendCompletionTimeUS;
    }

    // Remove echoes.
    DWORD echoLen = m_ReadBuffer.Length();
    DWORD sendLen = m_WriteBuffer.Length();
    if (echoLen && sendLen)
    {
        if (echoLen > sendLen)
            echoLen = sendLen;
        if (memcmp(m_WriteBuffer.GetShared(),
                   m_ReadBuffer.GetShared(), echoLen) == 0)
        {
#ifdef DEBUG_SEND_RECV_RAW
            LogDebug(TEXT("[AC]   echo (length=%d):"), echoLen);
            LogIOBlock(m_ReadBuffer.GetShared(), echoLen);
#endif            
            result = m_ReadBuffer.Remove(0, echoLen);
            if (NO_ERROR == result)
                result = m_WriteBuffer.Remove(0, echoLen);
            if (NO_ERROR != result)
            {
                LogWarn(TEXT("Can't remove %lu bytes from")
                        TEXT(" %s:%lu echo-cancellation buffer"),
                        echoLen,
                        GetServerHost(), GetServerPort());
            }
        }
    }
    
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Writes raw data to the server from the write-buffer:
// Writes each newline-separated line individually.
// Returns ERROR_CONNECTION_INVALID if the server isn't connected.
//
DWORD
TelnetHandle_t::
WriteBuffer(
    DWORD BytesWritten,  // number buffered bytes which have already been written
    long  MaxWaitTimeMS)
{
    DWORD result = NO_ERROR;    
    DWORD startTime = GetTickCount();

    // Make sure we're connected.
    if (TelnetStateDisconnected == m_State)
    {
        LogError(TEXT("Connection dropped to %s:%lu"),
                 GetServerHost(), GetServerPort());
        return ERROR_CONNECTION_INVALID;
    }

    while (m_WriteBuffer.Length() > BytesWritten)
    {
        // Make sure we haven't already waited too long.
        long timeWaited = WiFUtils::SubtractTickCounts(GetTickCount(), startTime);
        if (MaxWaitTimeMS <= timeWaited)
            return ERROR_TIMEOUT;
        
        // Wait for the server to empty its read buffer.
        fd_set sendFdSet;
        FD_ZERO(&sendFdSet);
        FD_SET(m_Socket, &sendFdSet);

        long remainingTime = MaxWaitTimeMS - timeWaited;
        TIMEVAL timeval = { remainingTime / 1000L,
                           (remainingTime % 1000L) * 1000L };

        int numFds = select(0, NULL, &sendFdSet, NULL, &timeval);
        if (SOCKET_ERROR == numFds)
        {
            result = WSAGetLastError();
            LogError(TEXT("Select error for send to %s:%lu: wsa %s"),
                     GetServerHost(), GetServerPort(),
                     Win32ErrorText(result));
            Disconnect();
            return result;
        }

        // Write the data.
        if (0 >= numFds)
        {
            LogWarn(TEXT("Unable to write to %s:%lu for %.02f seconds"),
                    GetServerHost(), GetServerPort(),
                   (double)remainingTime / 1000.0);
        }
        else
        {
            char *writePtr = (char *)m_WriteBuffer.GetPrivate();
            DWORD dataLeft = m_WriteBuffer.Length();

            writePtr += BytesWritten;
            dataLeft -= BytesWritten;

            // Write one line at a time.
            DWORD lineLen = 0;
            for (const char *pLineEnd = writePtr ; lineLen < dataLeft ;)
                if (*(pLineEnd++) != '\r')
                    ++lineLen;
                else
                {
                    if (++lineLen >= dataLeft)
                        break;
                    else
                    if (*pLineEnd == '\n')
                    {
                        ++lineLen;
                        break;
                    }
                }

#ifdef DEBUG_SEND_RECV
            if (writePtr[0] == (char)0xFF)
                LogDebug(TEXT("[AC]   Send IAC, etc."));
            else
            {
                DWORD logLen = lineLen;
                if (logLen >= 2 && writePtr[logLen-2] == '\r'
                                && writePtr[logLen-1] == '\n')
                    logLen -= 2;
                ce::string buffer;
                if (buffer.append(writePtr, logLen))
                    LogDebug(TEXT("[AC]   Send \"%hs\""), &buffer[0]);
            }
#endif      

            // Write the line.
            int sendResult = send(m_Socket, writePtr, lineLen, 0);
            if (SOCKET_ERROR == sendResult)
            {
                result = WSAGetLastError();
                LogError(TEXT("Can't write line to %s:%lu: wsa %s"),
                         GetServerHost(), GetServerPort(), 
                         Win32ErrorText(result));
                Disconnect();
                return result;
            }
            else
            if (0 >= sendResult)
            {
                LogWarn(TEXT("Wrote %d bytes to %s:%lu ???"), sendResult,
                        GetServerHost(), GetServerPort());

                // Slow down the errors in case the socket is
                // in a strange state.
                Sleep(500);
            }
            else
            {
#ifdef DEBUG_SEND_RECV_RAW
                LogDebug(TEXT("[AC]   send (length=%d):"), sendResult);
                LogIOBlock(writePtr, sendResult);
#endif
                BytesWritten += sendResult;
                m_LastAccessTime = GetTickCount();
            }
        }
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
