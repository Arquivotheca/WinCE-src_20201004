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
// Definitions and declarations for the QADialog_t class.
//
// ----------------------------------------------------------------------------

#pragma once

#include <windows.h>
#include <tchar.h>
#include <tux.h>
#include <katoex.h>
#include <netlog.h>

namespace CENetworkQA {

// Forward references:
class QADialogPrompter_t;

// ----------------------------------------------------------------------------
//
// Provides a simple dialog-window and/or message-box interface for 
// processing question/answer dialogs with the user.
//
// Manages the interaction using a message-box if running in interactive
// mode. In non-interactive mode, the dialog questions are shown in the
// log together with instructions telling the operator how to respond.
//
class QADialog_t
{
private:

    // The "-ok/-cancel-button" command-line argument strings:
    static LPCTSTR s_pOkButtonCommand;
    static LPCTSTR s_pCancelButtonCommand;

    // Debug logger:
    union
    {
        CKato   *pKato;
        CNetLog *pNetLog;
    }
    m_pLogger;
    enum
    {
        KatoLogger,
        NetLogLogger
    }
    m_LogType;

    // Name of executable:
    LPCTSTR m_pExecutableName;

    // Run dialog in an interactive window?
    bool m_Interactive;

    // Are we suppressing log prompts?
    bool m_SuppressLogPrompts;

    // Prompter handling the dialog:
    QADialogPrompter_t *m_pPrompter;

    // OK- and Cancel-button event handles:
    HANDLE m_EventHandles[2];

public:

    // Gets or sets the OK-button or Cancel-button command-line arguments:
    static LPCTSTR
    GetOkButtonCommand(void) { return s_pOkButtonCommand; }
    static void
    SetOkButtonCommand(LPCTSTR v) { s_pOkButtonCommand = v; }
    static LPCTSTR
    GetCancelButtonCommand(void) { return s_pCancelButtonCommand; }
    static void
    SetCancelButtonCommand(LPCTSTR v) { s_pCancelButtonCommand = v; }

    // Combines the specified DLL name, the OK/Cancel-button
    // command-argument strings and the specfied test id to create
    // the standard "use this command to simulate an OK or Cancel
    // button" prompt for the Tux test operator:
    static size_t
    FormatOkCancelPrompt(
        LPCTSTR pDllName,       // Tux DLL file name
        int     TuxTestId,      // ID of noop test within Tux DLL
        LPTSTR  pBuffer,        // buffer for storing formatted string
        size_t  BufferSize);    // length of output buffer (characters)

    // Types of responses from the user:
    enum ResponseCode
    {
        ResponseFailed,     // operation failed - use GetLastError for cause
        ResponseOK,         // user clicked OK button
        ResponseCancel,     // user clicked Cancel button
        ResponseTimeout     // no response before timeout
    };

    // Constructors:
    QADialog_t(
        CKato   *pLogger,           // Kato logger
        LPCTSTR  pExecutableName,   // name of executable
        bool     Interactive);      // run interacive MessageBox
    QADialog_t(
        CNetLog *pLogger,           // NetLog logger
        LPCTSTR  pExecutableName,   // name of executable
        bool     Interactive);      // run interacive MessageBox

    // Destructor:
    virtual
   ~QADialog_t(void);

    // Are we running the dialog in an interactive window?
    bool
    IsInteractive(void) const { return m_Interactive; }
    void
    SetInteractive(bool v) { m_Interactive = v; }

    // Are we suppressing dialog prompts directed to the log?
    bool
    IsSuppressLogPrompts(void) const { return m_SuppressLogPrompts; }
    void
    SetSuppressLogPrompts(bool v) { m_SuppressLogPrompts = v; }

    // Searches the specified command arguments looking for the
    // ok- or cancel-button argument. If either is found, signals
    // the executable's "-ok-button" or "-cancel-button" event:
    bool
    SetButtonEventFromCommandLine(LPCTSTR pCommandLine);
    
    // Asks the user a question:
    // Appends the optional OK/Cancel-button prompt to the end of the
    // prompt sent to the log.
    ResponseCode
    RunDialog(
        LPCTSTR pPrompt,                 // dialog prompt
        long    MaxWaitTimeMS,           // time to wait for response
        LPCTSTR pOkCancelPrompt = NULL); // instructions for simulating
                                         // OK and Cancel buttons
                                         
    // Waits for one of the ok-button or cancel-button events:
    ResponseCode
    AwaitButtonEvent(long MaxWaitTimeMS);

    // Utility methods for sending the specified message(s) to the log.
    // Both methods divide the text into newline-terminated lines before
    // logging each line separately:
    void
    Log(DWORD Verbosity, LPCTSTR pFormat, ...) const;
    void
    LogV(DWORD Verbosity, LPCTSTR pFormat, va_list varArgs) const;

private:

    // Initializes the object after the constructors handle the basics:
    void
    Construct(void);
};

};
// ----------------------------------------------------------------------------
