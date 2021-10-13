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
// Implentation of the QADialog_t class.
//
// ----------------------------------------------------------------------------

#include <QADialog_t.hpp>
#include <QADialogPrompter_t.hpp>
#include <QAMessageBoxPrompter_t.hpp>

#include <assert.h>

using namespace CENetworkQA;

// The "-ok/-cancel-button" command-line argument strings:
LPCTSTR
QADialog_t::
s_pOkButtonCommand = TEXT("-ok-button");
LPCTSTR
QADialog_t::
s_pCancelButtonCommand = TEXT("-cancel-button");

// ----------------------------------------------------------------------------
//
// Combines the specified DLL name, the OK/Cancel-button strings and the
// specfied test id to create the standard "use this command to simulate
// an OK or Cancel button" prompt for the Tux test operator:
//
size_t
QADialog_t::
FormatOkCancelPrompt(
    LPCTSTR pDllName,       // Tux DLL file name
    int     TuxTestId,      // ID of noop test within Tux DLL
    LPTSTR  pBuffer,        // buffer for storing formatted string
    size_t  BufferSize)     // length of output buffer (characters)
{
    int size;
    size = _sntprintf(
                pBuffer, BufferSize,
                TEXT("Use 'tux -d %s -c \"%s|%s\" -x %d' to skip this test."),
                pDllName, s_pOkButtonCommand,
                          s_pCancelButtonCommand, TuxTestId);
    if (size < 0 || size >= (int)BufferSize)
    {
        size = BufferSize - 1;
    }
    pBuffer[size] = TEXT('\0');
    return size;
}

// ----------------------------------------------------------------------------
// 
// Constructors.
//
QADialog_t::
QADialog_t(
    CKato   *pLogger,           // Kato logger
    LPCTSTR  pExecutableName,   // name of executable
    bool     Interactive)       // run interacive MessageBox
    : m_LogType(KatoLogger),
      m_pExecutableName(pExecutableName),
      m_Interactive(Interactive),
      m_SuppressLogPrompts(false),
      m_pPrompter(NULL)
{
    assert(pLogger != NULL);
    m_pLogger.pKato = pLogger;
    Construct();
}
QADialog_t::
QADialog_t(
    CNetLog *pLogger,           // NetLog logger
    LPCTSTR  pExecutableName,   // name of executable
    bool     Interactive)       // run interacive MessageBox
    : m_LogType(NetLogLogger),
      m_pExecutableName(pExecutableName),
      m_Interactive(Interactive),
      m_SuppressLogPrompts(false),
      m_pPrompter(NULL)
{
    assert(pLogger != NULL);
    m_pLogger.pNetLog = pLogger;
    Construct();
}
void
QADialog_t::
Construct(void)
{
    memset(m_EventHandles, 0, sizeof(m_EventHandles));
    
    assert(m_pExecutableName != NULL && m_pExecutableName[0] != TEXT('\0')); 
    if (m_pExecutableName == NULL || m_pExecutableName[0] == TEXT('\0'))
    {
        m_pExecutableName = TEXT("unknownExec");
    }
    
    size_t execNameSize = _tcslen(m_pExecutableName);

    LPCTSTR eventSuffixes[2];
    eventSuffixes[0] = s_pOkButtonCommand;
    eventSuffixes[1] = s_pCancelButtonCommand;

    size_t eventNameSize = 0;
    for (int evix = 0 ; evix < 2 ; ++evix)
    {
        size_t suffixSize = execNameSize + _tcslen(eventSuffixes[evix]);
        if (eventNameSize < suffixSize)
            eventNameSize = suffixSize;
    }

    LPTSTR pEventName = (LPTSTR)LocalAlloc(0, (eventNameSize+2) * sizeof(TCHAR));
    assert (pEventName != NULL);
    if (pEventName == NULL)
    {
        Log(LOG_FAIL,
            TEXT("Can't allocate %u bytes for event-name buffer"),
            eventNameSize * sizeof(TCHAR));
        return;
    }
    
    for (int evix = 0 ; evix < 2 ; ++evix)
    {
        _tcscpy(pEventName, m_pExecutableName);
        _tcscat(pEventName, eventSuffixes[evix]);
        m_EventHandles[evix] = CreateEvent(NULL, TRUE, FALSE, pEventName);
        assert(m_EventHandles[evix] != NULL);
        if (m_EventHandles[evix] == NULL)
        {
            Log(LOG_FAIL,
                TEXT("Can't create \"%s\" event: err=%u"),
                pEventName, GetLastError());
        }
        else
        {
            Log(LOG_COMMENT, 
                TEXT("Created event \"%s\": handle=0x%X"), 
                pEventName, (unsigned int)m_EventHandles[evix]);
            ResetEvent(m_EventHandles[evix]);
        }
    }

    LocalFree(pEventName);
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
QADialog_t::
~QADialog_t(void)
{
    if (m_pPrompter != NULL)
    {
        delete m_pPrompter;
        m_pPrompter = NULL;
    }
    for (int evix = 0 ; evix < 2 ; ++evix)
    {
        if (m_EventHandles[evix] != NULL)
        {
            CloseHandle(m_EventHandles[evix]);
            m_EventHandles[evix] = NULL;
        }
    }
}

// ----------------------------------------------------------------------------
//
// Searches the specified command arguments looking for the
// ok- or cancel-button argument. If either is found, signals
// the executable's "-ok-button" or "-cancel-button" event:
//
bool
QADialog_t::
SetButtonEventFromCommandLine(LPCTSTR pCommandLine)
{
    assert(pCommandLine != NULL);
    if (_tcsstr(pCommandLine, s_pOkButtonCommand) != NULL)
    {
        SetEvent(m_EventHandles[0]);
        return true;
    }
    else
    if (_tcsstr(pCommandLine, s_pCancelButtonCommand) != NULL)
    {
        SetEvent(m_EventHandles[1]);
        return true;
    }
    else
    {
        return false;
    }
}

// ----------------------------------------------------------------------------
//
// Asks the user a question.
//
QADialog_t::ResponseCode
QADialog_t::
RunDialog(
    LPCTSTR pPrompt,          // dialog prompt
    long    MaxWaitTimeMS,    // time to wait for response
    LPCTSTR pOkCancelPrompt)  // instructions for simulating OK and Cancel buttons
{
    assert(pPrompt != NULL && pPrompt[0] != TEXT('\0'));
    assert(MaxWaitTimeMS >= 0 || MaxWaitTimeMS == INFINITE);

    // Send the prompt and instructions (if any) to the log.
    if (!m_SuppressLogPrompts)
     {
         Log(LOG_DETAIL, TEXT("%s"), pPrompt);
         if (pOkCancelPrompt != NULL && pOkCancelPrompt[0] != TEXT('\0'))
         {
             Log(LOG_DETAIL, TEXT("%s"), pOkCancelPrompt);
         }
     }

    // If we're in interactive mode, try to pop up a MessageBox.
    if (m_Interactive)
    {
        if (m_pPrompter == NULL)
        {
            m_pPrompter = new QAMessageBoxPrompter_t(*this);
            assert(m_pPrompter != NULL);
            if (m_pPrompter == NULL)
            {
                Log(LOG_FAIL, TEXT("Can't allocate a QADialogPrompter!"));
            }
            else
            if (!m_pPrompter->IsValid())
            {
                delete m_pPrompter;
                m_pPrompter = NULL;
                m_Interactive = false;
            }
        }

        if (m_pPrompter != NULL)
        {
            return m_pPrompter->RunDialog(pPrompt, 
                                          m_EventHandles, 
                                          MaxWaitTimeMS);
        }
    }

    // Otherwise, just wait for one of the ok/cancel-button events.
    return AwaitButtonEvent(MaxWaitTimeMS);
}

// ----------------------------------------------------------------------------
//
// Waits for one of the ok-button or cancel-button events.
//
QADialog_t::ResponseCode 
QADialog_t::
AwaitButtonEvent(long MaxWaitTimeMS)
{
    assert(MaxWaitTimeMS >= 0 || MaxWaitTimeMS == INFINITE);
    QADialog_t::ResponseCode ret;
    switch (WaitForMultipleObjects(2, m_EventHandles, FALSE, MaxWaitTimeMS))
    {
        case WAIT_OBJECT_0:
            ResetEvent(m_EventHandles[0]);
            ret = QADialog_t::ResponseOK;
            break;
        case WAIT_OBJECT_0 + 1:
            ResetEvent(m_EventHandles[1]);
            ret = QADialog_t::ResponseCancel;
            break;
        case WAIT_TIMEOUT:
            ret = QADialog_t::ResponseTimeout;
            break;
        default:
            Log(LOG_FAIL,
                TEXT("Wait for button-events failed: err=%u"),
                GetLastError());
            ret = QADialog_t::ResponseFailed;
            break;
    }
    return ret;
}

// ----------------------------------------------------------------------------
//
// Utility methods for sending the specified message(s) to the log.
// Both methods divide the text into newline-terminated lines before
// logging each separately.
//
void
QADialog_t::
Log(DWORD Verbosity, LPCTSTR pFormat, ...) const
{
    va_list va;
    va_start(va, pFormat);
    LogV(Verbosity, pFormat, va);
    va_end(va);
}
void
QADialog_t::
LogV(DWORD Verbosity, LPCTSTR pFormat, va_list va) const
{
    // Format the message(s) into a local buffer.
    LPTSTR pBuffer;
    for (size_t bufferSize = 1024 ;; bufferSize *= 2)
    {
        pBuffer = (LPTSTR) LocalAlloc(0, (bufferSize + 2) * sizeof(TCHAR));
        assert (pBuffer != NULL);
        if (pBuffer == NULL)
        {
            if (m_LogType == KatoLogger)
                 m_pLogger.pKato->  LogV(Verbosity, pFormat, va);
            else m_pLogger.pNetLog->LogV(Verbosity, pFormat, va);
            return;
        }
        if (_vsntprintf(pBuffer, bufferSize, pFormat, va) >= 0 ||
            bufferSize >= 16*1024)
        {
            pBuffer[bufferSize] = TEXT('\0');
            break;
        }
        LocalFree(pBuffer);
    }

    // Log each newline-terminated message separately.
    LPTSTR pMessage = pBuffer;
    while (*pMessage != TEXT('\0'))
    {
        LPTSTR lineStart = pMessage;
        while (*pMessage != TEXT('\0'))
        {
            LPTSTR lastChar = pMessage;
            pMessage = CharNext(pMessage);
            if (_istcntrl(*lastChar))
            {
                *lastChar = TEXT('\0');
                break;
            }
        }
        if (m_LogType == KatoLogger)
             m_pLogger.pKato->  Log(Verbosity, TEXT("%s"), lineStart);
        else m_pLogger.pNetLog->Log(Verbosity, TEXT("%s"), lineStart);
    }

    LocalFree(pBuffer);
}

// ----------------------------------------------------------------------------
