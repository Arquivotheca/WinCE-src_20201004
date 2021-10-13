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
#include "TelnetPort_t.hpp"

#include "APCUtils.hpp"

#include <assert.h>
#include <intsafe.h>
#include <tchar.h>
#include <strsafe.h>

#include <auto_xxx.hxx>

// Turns on/off various types of logging:
//
//#define DEBUG_SEND_RECV 1

using namespace ce::qa;


// ----------------------------------------------------------------------------
//
// Constructors.
//
static TelnetHandle_t *
CreateDefaultHandle(
    const TCHAR *pServerHost,
    DWORD         ServerPort)
{
    return new TelnetHandle_t(pServerHost, ServerPort);
}

TelnetPort_t::
TelnetPort_t(
    const TCHAR *pServerHost,
    DWORD         ServerPort,
    TelnetHandle_t *(*CreateHandleFunc)(const TCHAR *, DWORD))
    : m_pHandle(TelnetHandle_t::AttachHandle(pServerHost, ServerPort, CreateHandleFunc))
{
    assert(IsValid());
}

TelnetPort_t::
TelnetPort_t(
    const TCHAR *pServerHost,
    DWORD         ServerPort)
    : m_pHandle(TelnetHandle_t::AttachHandle(pServerHost, ServerPort, CreateDefaultHandle))
{
    assert(IsValid());
}

// ----------------------------------------------------------------------------
//
// Destructors.
//
TelnetPort_t::
~TelnetPort_t(void)
{
    if (NULL != m_pHandle)
    {
        TelnetHandle_t::DetachHandle(m_pHandle);
        m_pHandle = NULL;
    }
}

// ----------------------------------------------------------------------------
//
// Returns true if the port handle is valid.
//
bool
TelnetPort_t::
IsValid(void) const
{
    return NULL != m_pHandle;
}

// ----------------------------------------------------------------------------
//
// Gets the telnet-server name or address.
//
const TCHAR *
TelnetPort_t::
GetServerHost(void) const
{
    if (NULL == m_pHandle)
        return TEXT("");
    
    return m_pHandle->GetServerHost();
}

// ----------------------------------------------------------------------------
//
// Gets the telnet-server port (default 23).
//
DWORD 
TelnetPort_t::
GetServerPort(void) const
{
    if (NULL == m_pHandle)
        return 23;
    
    return m_pHandle->GetServerPort();
}

// ----------------------------------------------------------------------------
//
// Gets or sets the user-name.
//
const TCHAR *
TelnetPort_t::
GetUserName(void) const
{
    if (NULL == m_pHandle)
        return TEXT("");
    
    return m_pHandle->GetUserName();
}

void
TelnetPort_t::
SetUserName(const TCHAR *Value)
{
    if (NULL != m_pHandle)
    {
        if (0 != _tcscmp(Value, m_pHandle->GetUserName()))
        {
            m_pHandle->Disconnect();
            m_pHandle->SetUserName(Value);
        }
    }
}

// ----------------------------------------------------------------------------
//
// Gets or sets the admin password.
//
const TCHAR *
TelnetPort_t::
GetPassword(void) const
{
    if (NULL == m_pHandle)
        return TEXT("");
    
    return m_pHandle->GetPassword();
}
void
TelnetPort_t::
SetPassword(const TCHAR *Value)
{
    if (NULL != m_pHandle)
    {
        if (0 != _tcscmp(Value, m_pHandle->GetPassword()))
        {
            m_pHandle->Disconnect();
            m_pHandle->SetPassword(Value);
        }
    }
}

// ----------------------------------------------------------------------------
//
// Retrieves an object which can be locked to prevent other threads
// from using the port.
//
ce::critical_section &
TelnetPort_t::
GetLocker(void) const
{
    static ce::critical_section s_EmergencyCS;
    if (NULL == m_pHandle)
        return s_EmergencyCS;

    return m_pHandle->GetLocker();
}

// ----------------------------------------------------------------------------
//
// Connects to the device's telnet-server to access the specified page.
//
DWORD
TelnetPort_t::
Connect(long MaxWaitTimeMS /* DefaultConnectTimeMS */)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;

    return m_pHandle->Connect(MaxWaitTimeMS);
}

// ----------------------------------------------------------------------------
//
// Disconnects the current connection to cause a reconnection next time.
//
void
TelnetPort_t::
Disconnect(void)
{
    if (NULL != m_pHandle)
    {
        m_pHandle->Disconnect();
    }
}

// ----------------------------------------------------------------------------
//
// Is the connection open?
// Note that the connection will automatically be timed out after
// a period with no activity.
//
bool
TelnetPort_t::
IsConnected(void) const
{
    return (NULL == m_pHandle)? false
                              : m_pHandle->IsConnected();
}

// ----------------------------------------------------------------------------
//
// Flushes the read/write buffers:
//
void
TelnetPort_t::
Flush(void)
{
    if (NULL != m_pHandle)
    {
        m_pHandle->Flush();
    }
}

#if 0
// ----------------------------------------------------------------------------
//
// Writes a line to the server:
//
DWORD
TelnetPort_t::
WriteLine(
    const char *pFormat, ...)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;

    va_list pArgList;
    va_start(pArgList, pFormat);
    DWORD result = m_pHandle->WriteLineV(pFormat, pArgList, DefaultWriteTimeMS);
    va_end(pArgList);
    return result;
}

DWORD
TelnetPort_t::
WriteLineV(
    const char *pFormat,
    va_list     pArgList,
    long        MaxWaitTimeMS /* = DefaultWriteTimeMS */)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;

    return m_pHandle->WriteLineV(pFormat, pArgList, MaxWaitTimeMS);
}

// ----------------------------------------------------------------------------
//
// Writes an unformatted, "raw", block of text to the server:
//
DWORD
TelnetPort_t::
WriteBlock(
    const void *pBlock,
    int          BlockChars,
    long         MaxWaitTimeMS /* = DefaultWriteTimeMS */)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;

    return m_pHandle->WriteBlock(pBlock, BlockChars, MaxWaitTimeMS);
}
#endif

// ----------------------------------------------------------------------------
//

// Sends the specified message (defaults to a newline) and waits for
// the server to send the indicated prompt in return.
// Returns ERROR_NOT_FOUND if the specified prompt wasn't forthcoming.
//
DWORD
TelnetPort_t::
SendExpect(
    const char    *pExpect,
    int             ExpectChars,
    const char    *pExpectName,
    ce::string    *pPromptBuffer /* = NULL */,
    const char    *pMessage      /* = "" */,
    TelnetLines_t *pResponse     /* = NULL */,
    long           MaxWaitTimeMS /* = DefaultReadTimeMS */)
{
    if (NULL == m_pHandle)
        return ERROR_INVALID_HANDLE;

    return m_pHandle->SendExpect(pExpect,
                                  ExpectChars,
                                 pExpectName,
                                 pPromptBuffer,
                                 pMessage,
                                 pResponse,
                                 MaxWaitTimeMS);
}

/* ========================================================================= */
/* ============================= TelnetLines =============================== */
/* ========================================================================= */

// ----------------------------------------------------------------------------
//
// Destructor:
//
TelnetLines_t::
~TelnetLines_t(void)
{
    // nothing to do.
}

// ----------------------------------------------------------------------------
//
// Gets the number of lines in the text:
// If necessary, parses the text first.
//
int
TelnetLines_t::
Size(void)
{
    if (m_NumberLines < 0)
        ParseLines();
    
    return m_NumberLines;
}

// ----------------------------------------------------------------------------
//
// Gets the specified line as separated words:
// If necessary, parses the text first.
//
const TelnetWords_t &
TelnetLines_t::
GetLine(int IX)
{
    if (m_NumberLines < 0)
        ParseLines();

    assert(0 <= IX && IX < m_NumberLines);
    
    const TelnetWords_t *pLines = (const TelnetWords_t *)(m_Lines.GetShared());
    return pLines[IX];
}

// ----------------------------------------------------------------------------
//
// Parses the text into lines and words
//
DWORD
TelnetLines_t::
ParseLines(void)
{
    DWORD result = NO_ERROR;

    m_NumberLines = 0;
    
    char *pText = (char *)m_Text.GetPrivate();
    const char *pTextEnd = &pText[m_Text.Length()];
    while (pText < pTextEnd)
    {
        DWORD newSize, capacity;
        
        // Grow the list capacity in multiples of 32 lines.    
        newSize = (m_NumberLines + 0x20) & ~0x1F;
     
        HRESULT hr = DWordMult(newSize, sizeof(TelnetWords_t), &capacity);
        if (FAILED(hr))
            result = HRESULT_CODE(hr);
        else if (capacity > m_Lines.Capacity())
            result = m_Lines.Reserve(capacity);

        if (NO_ERROR != result)
        {
            LogError(TEXT("Can't allocate %lu bytes for text-lines list"), capacity);
            break;
        }

        // Add the new line to the list.
        TelnetWords_t *pWords = (TelnetWords_t *)(m_Lines.GetPrivate());
        pWords += m_NumberLines;
        pWords->ParseWords(pText, pTextEnd);

        // Ignore blank lines.
        if (pWords->Size() > 0)
            m_NumberLines++;

#ifdef DEBUG_SEND_RECV
        if (pWords->Size() > 0)
        {
            char buff[200];
            LogDebug(TEXT("[AC]   recv line = \"%hs\""),
                     pWords->MergeLine(buff, COUNTOF(buff)));
        }
#endif
    }

    return result;
}

/* ========================================================================= */
/* ============================= TelnetWords =============================== */
/* ========================================================================= */

// ----------------------------------------------------------------------------
//
// Parses the words within a line of text.
//
void
TelnetWords_t::
ParseWords(
    char      *&pText,
    const char *pTextEnd)
{
    for (m_NumberWords = 0 ;
         m_NumberWords < MaxWordsInLine ;)
    {
        // Skip over leading white-space and watch for newlines.
        for (;; ++pText)
        {
            if (pText >= pTextEnd)
                return;
            if (!isspace((UCHAR)pText[0]))
                break;
            if (pText[0] == '\r' && &pText[1] < pTextEnd
                                 &&  pText[1] == '\n')
            {
                pText += 2;
                return;
            }
        }

        // Capture the next "word".
        m_Words[m_NumberWords++] = pText;
        for (;; ++pText)
        {
            if (pText >= pTextEnd)
                return;
            if (isspace((UCHAR)pText[0]))
            {
                if (pText[0] == '\r' && &pText[1] < pTextEnd
                                     &&  pText[1] == '\n')
                {
                    pText[0] = '\0';
                    pText += 2;
                    return;
                }
                break;
            }
        }

        // Null-terminate the word.
       *(pText++) = '\0';
    }
}

// ----------------------------------------------------------------------------
//
// Re-merges the parsed words into a line of text.
//
const char *
TelnetWords_t::
MergeLine(
    char *pBuffer,
    size_t BufferChars) const
{
    char       *pBuffPtr = &pBuffer[0];
    const char *pBuffEnd = &pBuffer[BufferChars-1];
    for (int wx = 0 ; wx < m_NumberWords && pBuffPtr < pBuffEnd ; ++wx, --pBuffPtr)
    {
        if (wx > 0) *(pBuffPtr++) = ' ';
        const char *pWord = m_Words[wx];
        while ((*(pBuffPtr++) = *(pWord++)) != '\0')
            if (pBuffPtr >= pBuffEnd)
            {
               *(pBuffPtr++) = '\0';
                break;
            }
    }
   *pBuffPtr = '\0';
    return pBuffer;
}

// ----------------------------------------------------------------------------
