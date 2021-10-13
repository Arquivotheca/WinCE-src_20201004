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
// Implementation of the WiFUtils class.
//
// ----------------------------------------------------------------------------

#include "WiFUtils.hpp"

#include <assert.h>
#include <cmnprint.h>
#include <inc/sync.hxx>
#include <strsafe.h>

using namespace ce::qa;

// Status-message logger:
static void * volatile s_pStatusFunction = NULL;

// ----------------------------------------------------------------------------
//
// A simple, high-speed, hash-table linking threads to error-buffers:
//

struct ErrorBufferHashNode
{
    DWORD                threadId;
    ce::tstring         *pErrors;
    ErrorBufferHashNode *next;
};

static const int ERROR_HASH_TABLE_SIZE = 47;
static ErrorBufferHashNode *ErrorBufferHashTable[ERROR_HASH_TABLE_SIZE + 1];
static ErrorBufferHashNode *ErrorBufferHashFreeList = NULL;
static bool                 ErrorBufferHashInited = false;
static ce::critical_section ErrorBufferHashLocker;

static void
ReleaseErrorBuffers(void)
{
    ce::gate<ce::critical_section> locker(ErrorBufferHashLocker);
    if (ErrorBufferHashInited)
    {
        ErrorBufferHashTable[ERROR_HASH_TABLE_SIZE] = ErrorBufferHashFreeList;
        ErrorBufferHashFreeList = NULL;
        ErrorBufferHashInited = false;

        ErrorBufferHashNode *node, *next;
        for (int hash = 0 ; hash <= ERROR_HASH_TABLE_SIZE ; ++hash)
        {
            node = ErrorBufferHashTable[hash];
            while (node)
            {
                next = node->next;
                free(node);
                node = next;
            }
        }
    }
}

static void
InsertErrorBuffer(ce::tstring *pErrorBuffer)
{
    DWORD thisThread = GetCurrentThreadId();
    int hash = (int)((unsigned long)thisThread % ERROR_HASH_TABLE_SIZE);
    ce::gate<ce::critical_section> locker(ErrorBufferHashLocker);

    if (ErrorBufferHashInited == false)
    {
        memset(ErrorBufferHashTable, 0, sizeof(ErrorBufferHashTable));
        ErrorBufferHashInited = true;
        atexit(ReleaseErrorBuffers);
    }

    ErrorBufferHashNode *node = ErrorBufferHashFreeList;
    if (node)
    {
        ErrorBufferHashFreeList = node->next;
    }
    else
    {
        node = (ErrorBufferHashNode *) malloc(sizeof(ErrorBufferHashNode));
        if (NULL == node)
            return;
    }

    node->threadId = thisThread;
    node->pErrors = pErrorBuffer;
    node->next = ErrorBufferHashTable[hash];
    ErrorBufferHashTable[hash] = node;
}

static ce::tstring *
LookupErrorBuffer(void)
{
    if (ErrorBufferHashInited)
    {
        DWORD thisThread = GetCurrentThreadId();
        int hash = (int)((unsigned long)thisThread % ERROR_HASH_TABLE_SIZE);
        ce::gate<ce::critical_section> locker(ErrorBufferHashLocker);

        ErrorBufferHashNode *node = ErrorBufferHashTable[hash];
        while (node)
        {
            if (node->threadId == thisThread)
                return node->pErrors;
            node = node->next;
        }
    }
    return NULL;
}

static void
RemoveErrorBuffer(void)
{
    if (ErrorBufferHashInited)
    {
        DWORD thisThread = GetCurrentThreadId();
        int hash = (int)((unsigned long)thisThread % ERROR_HASH_TABLE_SIZE);
        ce::gate<ce::critical_section> locker(ErrorBufferHashLocker);

        ErrorBufferHashNode **parent = &ErrorBufferHashTable[hash];
        while (*parent)
        {
            ErrorBufferHashNode *node = *parent;
            if (node->threadId == thisThread)
            {
               *parent = node->next;
                node->next = ErrorBufferHashFreeList;
                ErrorBufferHashFreeList = node;
                return;
            }
            parent = &(node->next);
        }
    }
}

// ----------------------------------------------------------------------------
//
// Formats the specified printf-style string and inserts it into the
// specified message buffer.
//
HRESULT
WiFUtils::
FmtMessage(
    OUT ce::tstring *pMessage,
    IN  const TCHAR *pFormat, ...)
{
    va_list varArgs;
    va_start(varArgs, pFormat);
    pMessage->clear();
    HRESULT hr = AddMessageV(pMessage, pFormat, varArgs);
    va_end(varArgs);
    return hr;
}

HRESULT
WiFUtils::
FmtMessageV(
    OUT ce::tstring *pMessage,
    IN  const TCHAR *pFormat,
    IN  va_list      VarArgs)
{
    pMessage->clear();
    return AddMessageV(pMessage, pFormat, VarArgs);
}

// ----------------------------------------------------------------------------
//
// Formats the specified printf-style string and appends it to the end of
// the specified message buffer.
//
HRESULT
WiFUtils::
AddMessage(
    OUT ce::tstring *pMessage,
    IN  const TCHAR *pFormat, ...)
{
    va_list varArgs;
    va_start(varArgs, pFormat);
    HRESULT hr = AddMessageV(pMessage, pFormat, varArgs);
    va_end(varArgs);
    return hr;
}

HRESULT
WiFUtils::
AddMessageV(
    OUT ce::tstring *pMessage,
    IN  const TCHAR *pFormat,
    IN  va_list      VarArgs)
{
    HRESULT hr = S_OK;
    TCHAR *bufferEnd;

    int currentChars = pMessage->length();
    for (int bufferChars = 128 ;; bufferChars *= 2)
    {
        if (!pMessage->reserve(currentChars + bufferChars))
        {
            CmnPrint(PT_FAIL,
                     TEXT("Can't allocate %u bytes of memory"),
                     (currentChars + bufferChars) * sizeof(TCHAR));
            pMessage->clear();
            hr = ERROR_OUTOFMEMORY;
            break;
        }

        _try
        {
            hr = StringCchVPrintfEx(&(*pMessage)[currentChars],
                                    bufferChars - 2,
                                   &bufferEnd, NULL,
                                    STRSAFE_IGNORE_NULLS,
                                    pFormat, VarArgs);
        }
        __except(1)
        {
            CmnPrint(PT_FAIL,
                     TEXT("Exception formatting \"%.128s\""),
                     pFormat);
            hr = E_FAIL;
            break;
        }

        if (SUCCEEDED(hr))
        {
            pMessage->resize(bufferEnd - &(*pMessage)[0]);
            break;
        }

        if (STRSAFE_E_INSUFFICIENT_BUFFER != hr || (128*1024) <= bufferChars)
        {
            CmnPrint(PT_FAIL,
                     TEXT("Message formatting failed: %s"),
                     HRESULTErrorText(hr));
            pMessage->resize(currentChars);
            break;
        }
    }

    return hr;
}

// ----------------------------------------------------------------------------
//
// Logs the specified message without concern for message size, etc.
// Also automatically splits the message into multiple lines and logs
// each separately.
//
void
WiFUtils::
LogMessageF(
    IN int          Severity,   // CmnPrint severity codes
    IN bool         AddOffsets, // true to prepend line-offsets
    IN const TCHAR *pFormat,
    ...)
{
    va_list varArgs;
    va_start(varArgs, pFormat);
    LogMessageV(Severity, AddOffsets, pFormat, varArgs);
    va_end(varArgs);
}

void
WiFUtils::
LogMessageV(
    IN int          Severity,   // CmnPrint severity codes
    IN bool         AddOffsets, // true to prepend line-offsets
    IN const TCHAR *pFormat,
    IN va_list      VarArgs)
{
    ce::tstring message;
    HRESULT hr = AddMessageV(&message, pFormat, VarArgs);
    if (SUCCEEDED(hr))
    {
        LogMessage(Severity, AddOffsets, message);
    }
}

void
WiFUtils::
LogMessage(
    IN int                Severity,   // CmnPrint severity codes
    IN bool               AddOffsets, // true to prepend line-offsets
    IN const ce::tstring &Message)
{
    TCHAR lineBuffer[MAX_PRINT_CHARS];
    const TCHAR *lineEnd = &lineBuffer[MAX_PRINT_CHARS-4];
    const TCHAR *lineFormat = TEXT("%s");

    if (AddOffsets)
    {
        if (Message.length() > 100000)
        {
            lineFormat = TEXT("%06ld: %s");
            lineEnd -= 8;
        }
        else
        if (Message.length() > 10000)
        {
            lineFormat = TEXT("%05ld: %s");
            lineEnd -= 7;
        }
        else
        if (Message.length() > 1000)
        {
            lineFormat = TEXT("%04ld: %s");
            lineEnd -= 6;
        }
        else
        {
            lineFormat = TEXT("%03ld: %s");
            lineEnd -= 5;
        }
    }

    const TCHAR *bufPtr = &Message[0];
    const TCHAR *bufEnd = &Message[Message.length()];
    while (*bufPtr != _T('\0') && bufPtr < bufEnd)
    {
        long lineOffset = bufPtr - &Message[0];
        TCHAR *linePtr = lineBuffer;
        while (*bufPtr != _T('\0') && bufPtr < bufEnd && linePtr < lineEnd)
        {
            TCHAR ch = *(bufPtr++);
            if (ch == _T('\n')) break;
            *(linePtr++) = ch;
        }
       *linePtr = _T('\0');

        _try
        {
            if (AddOffsets)
                 CmnPrint(Severity, lineFormat, lineOffset, lineBuffer);
            else CmnPrint(Severity, lineFormat,             lineBuffer);
        }
        __except(1)
        {
            CmnPrint(PT_FAIL,
                     TEXT("Exception logging \"%.128s\""),
                     lineBuffer);
        }
    }
}

// ----------------------------------------------------------------------------
//
// Simplified message/debugging functions.
//
void
ce::qa::
LogError(IN const TCHAR *pFormat, ...)
{
    va_list varArgs;
    va_start(varArgs, pFormat);
    ce::tstring message;
    HRESULT hr = WiFUtils::AddMessageV(&message, pFormat, varArgs);
    va_end(varArgs);
    if (SUCCEEDED(hr))
    {
        LogError(message);
    }
}

void
ce::qa::
LogError(IN const ce::tstring &Message)
{
    ce::tstring *pErrors = LookupErrorBuffer();
    if (NULL == pErrors)
    {
        WiFUtils::LogMessage(PT_FAIL, false, Message);
        void *pTest = InterlockedCompareExchangePointer(&s_pStatusFunction,
                                                        NULL, NULL);
        if (NULL != pTest)
        {
            void (*pStatus)(const TCHAR *) = (void (*)(const TCHAR *))pTest;
            pStatus(Message);
        }
    }
    else
    {
        if (pErrors->length() != 0)
            pErrors->append(TEXT("\n"));
        pErrors->append(Message);
    }
}

void
ce::qa::
LogWarn(IN const TCHAR *pFormat, ...)
{
    va_list varArgs;
    va_start(varArgs, pFormat);
    WiFUtils::LogMessageV(PT_WARN2, false, pFormat, varArgs);
    va_end(varArgs);
}

void
ce::qa::
LogWarn(IN const ce::tstring &Message)
{
    WiFUtils::LogMessage(PT_WARN2, false, Message);
}

void
ce::qa::
LogDebug(IN const TCHAR *pFormat, ...)
{
    va_list varArgs;
    va_start(varArgs, pFormat);
    WiFUtils::LogMessageV(PT_LOG, false, pFormat, varArgs);
    va_end(varArgs);
}

void
ce::qa::
LogDebug(IN const ce::tstring &Message)
{
    WiFUtils::LogMessage(PT_LOG, false, Message);
}

void
ce::qa::
LogAlways(IN const TCHAR *pFormat, ...)
{
    va_list varArgs;
    va_start(varArgs, pFormat);
    WiFUtils::LogMessageV(PT_VERBOSE, false, pFormat, varArgs);
    va_end(varArgs);
}

void
ce::qa::
LogAlways(IN const ce::tstring &Message)
{
    WiFUtils::LogMessage(PT_VERBOSE, false, Message);
}

// ----------------------------------------------------------------------------
//
// Logs a status message.
// By default, this is the same as calling LogAlways.
// If SetStatusLogger is handed a function to call, status and error
// messages will also be passed to that function. This can be used, for
// example, to capture "important" messages to a GUI status-message area.
//
void
ce::qa::
LogStatus(IN const TCHAR *pFormat, ...)
{
    va_list varArgs;
    va_start(varArgs, pFormat);
    ce::tstring message;
    HRESULT hr = WiFUtils::AddMessageV(&message, pFormat, varArgs);
    va_end(varArgs);
    if (SUCCEEDED(hr))
    {
        LogStatus(message);
    }
}

void
ce::qa::
LogStatus(IN const ce::tstring &Message)
{
    LogAlways(Message);
    void *pTest = InterlockedCompareExchangePointer(&s_pStatusFunction,
                                                    NULL, NULL);
    if (NULL != pTest)
    {
        void (*pStatus)(const TCHAR *) = (void (*)(const TCHAR *))pTest;
        pStatus(Message);
    }
}

void
ce::qa::
SetStatusLogger(
    void (*pLogFunc)(const TCHAR *))
{
    InterlockedExchangePointer(&s_pStatusFunction, pLogFunc);
}

void
ce::qa::
ClearStatusLogger(void)
{
    InterlockedExchangePointer(&s_pStatusFunction, NULL);
}

// ----------------------------------------------------------------------------
//
// Captures all of this thread's LogError error-message output into
// the specified message-buffer.
//
void
ErrorLock::
StartCapture(void)
{
    InsertErrorBuffer(m_pErrorBuffer);
}

void
ErrorLock::
StopCapture(void)
{
    RemoveErrorBuffer();
}

// ----------------------------------------------------------------------------
