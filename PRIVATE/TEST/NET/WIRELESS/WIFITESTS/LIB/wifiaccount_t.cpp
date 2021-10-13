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
// Implementation of the WiFiAccount_t class.
//
// ----------------------------------------------------------------------------

#include "WiFiAccount_t.hpp"

#include <assert.h>
#include <auto_xxx.hxx>
#include <eapol.h>
#include <netcmn.h>
#include <wincrypt.h>

#include "WiFiConfig_t.hpp"

#ifndef _PREFAST_
#pragma warning(disable:4068)
#endif

using namespace ce;
using namespace ce::qa;

// =========================== WiFiAccountDialog =========================== */

// ----------------------------------------------------------------------------
//
// Defines the information required to run a sub-thread which will watch
// for the appearance of a dialog box and, when it comes up, fill in the
// dialog form.
//
namespace ce {
namespace qa {
class WiFiAccountDialog_t
{
private:

    // Authentication credentials:
    const WiFiAccount_t &m_Cred;

    // Event telling this thread to stop:
    const TCHAR *m_pTerminateThreadName;
    ce::auto_handle m_TerminateEvent;

    // Dialog-box title:
    const TCHAR *m_pTitle;

    // String to "type" into the window:
    //   Special characters:
    //     %u = user-name
    //     %p = password
    //     %d = domain
    //     \n = newline = VK_RETURN
    //     \t = horizontal tab = VK_TAB
    //     \v = vertical tab = VK_UP
    //     \1B = escape = VK_ESCAPE
    //     ^X = CTRL+X
    //     ~X = ALT+X
    //     !X = SHIFT+X
    TCHAR m_Dialog[MAX_PATH];

    // True if sub-thread should continue until told to stop:
    bool m_fContinuous;

    // Time to wait after dialog typed for box to close:
    DWORD m_DialogCloseTimeMS;

    // Sub-thread handle:
    ce::auto_handle m_ThreadHandle;

    // Synchronization object:
    ce::mutex m_KeyboardMutex;

    // Sub-thread result-code:
    DWORD m_Result;

    // Result from waiting for dialog-box to close:
    DWORD m_CloseResult;

    // Construction can only be done by CreateDialogWatcher:
    WiFiAccountDialog_t(
        const WiFiAccount_t &Cred,
        const TCHAR *pTerminateThreadName,
        const TCHAR *pDialogTitle,
        const TCHAR *pDialogText,
        bool         fContinuous,
        DWORD        DialogCloseTimeMS); // 0 = don't wait

public:

    // Creates a new dialog-box watcher object:
    static WiFiAccountDialog_t *
    CreateDialogWatcher(
        const WiFiAccount_t &Cred,
        const TCHAR *pTerminateThreadName,
        const TCHAR *pDialogTitle,
        const TCHAR *pDialogText,
        bool         fContinuous,
        DWORD        DialogCloseTimeMS); // 0 = don't wait

    // Destructor:
    virtual
   ~WiFiAccountDialog_t(void);

    // Gets the sub-thread handle:
    HANDLE GetThreadHandle(void) const { return m_ThreadHandle; }

    // Checks or sets the thread-terminate event:
    bool IsTerminateSet(DWORD waitTime = 0);
    void ClearTerminate(void);
    void   SetTerminate(void);

    // Accessors:
    const TCHAR *
    GetDialogTitle(void) const { return m_pTitle; }
    const TCHAR *
    GetDialogText(void) const { return m_Dialog; }
    bool
    IsContinuous(void) const { return m_fContinuous; }
    DWORD
    GetDialogCloseTimeMS(void) const { return m_DialogCloseTimeMS; }

    // Starts the sub-thread running:
    virtual DWORD
    StartThread(void);

    // Gets the sub-thread result-code:
    //   ERROR_INVALID_DATA - dialog-watcher not started
    //   ERROR_NOT_FOUND - dialog-box never seen
    //   ERROR_SUCCESS - dialog-box processed successfully
    DWORD GetResult(void) const { return m_Result; }

    // Gets the result from waiting for dialog-box to close:
    //   ERROR_INVALID_DATA - dialog-box never processed
    //   ERROR_NOT_FOUND - dialog-box did not close
    //   ERROR_TIMEOUT - dialog-box didn't close fast enough
    //   ERROR_SUCCESS - dialog-box closed after processing
    DWORD GetCloseResult(void) const { return m_CloseResult; }

private:

    // Sub-thread run method:
    static DWORD WINAPI
    ThreadProc(LPVOID pParameter);

    // Types the authentication credentials into the dialog box:
    DWORD
    InsertDialog(HWND Window);
};
};
};

// ----------------------------------------------------------------------------
//
// Constructor.
//
WiFiAccountDialog_t::
WiFiAccountDialog_t(
    const WiFiAccount_t &Cred,
    const TCHAR *pTerminateThreadName,
    const TCHAR *pDialogTitle,
    const TCHAR *pDialogText,
    bool         fContinuous,
    DWORD        DialogCloseTimeMS)
    : m_Cred(Cred),
      m_pTerminateThreadName(pTerminateThreadName),
      m_pTitle(pDialogTitle),
      m_fContinuous(fContinuous),
      m_DialogCloseTimeMS(DialogCloseTimeMS),
      m_Result(ERROR_INVALID_DATA),
      m_CloseResult(ERROR_INVALID_DATA)
{
    SafeCopy(m_Dialog, pDialogText, COUNTOF(m_Dialog));
}

// ----------------------------------------------------------------------------
//
// Creates a new dialog-box watcher object.
//
WiFiAccountDialog_t *
WiFiAccountDialog_t::
CreateDialogWatcher(
    const WiFiAccount_t &Cred,
    const TCHAR *pTerminateThreadName,
    const TCHAR *pDialogTitle,
    const TCHAR *pDialogText,
    bool         fContinuous,
    DWORD        DialogCloseTimeMS)
{
    WiFiAccountDialog_t *pDialog = new WiFiAccountDialog_t(Cred,
                                                           pTerminateThreadName,
                                                           pDialogTitle,
                                                           pDialogText,
                                                           fContinuous,
                                                           DialogCloseTimeMS);
    if (NULL == pDialog)
    {
        LogError(TEXT("Can't allocate %u bytes for dialog-box watcher thread"),
                 sizeof(WiFiAccountDialog_t));
    }
    return pDialog;
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
WiFiAccountDialog_t::
~WiFiAccountDialog_t(void)
{
    if (m_ThreadHandle.valid())
    {
        if (WAIT_OBJECT_0 == WaitForSingleObject(m_ThreadHandle, 0))
        {
            SetTerminate();
            WaitForSingleObject(m_ThreadHandle, 1000);
        }
    }
}

// ----------------------------------------------------------------------------
//
// Checks or sets the thread-terminate event.
//
bool
WiFiAccountDialog_t::
IsTerminateSet(DWORD waitTime)
{
    if (m_TerminateEvent.valid())
    {
        DWORD result = WaitForSingleObject(m_TerminateEvent, waitTime);
        if (WAIT_TIMEOUT == result)
            return false;
        if (WAIT_OBJECT_0 != result)
        {
            result = GetLastError();
            LogError(TEXT("Terminate-thread event wait failed: %s"),
                    Win32ErrorText(result));
            m_TerminateEvent.close();
        }
    }
    return true;
}

void
WiFiAccountDialog_t::
ClearTerminate(void)
{
    if (m_TerminateEvent.valid())
    {
        ResetEvent(m_TerminateEvent);
    }
}

void
WiFiAccountDialog_t::
SetTerminate(void)
{
    if (m_TerminateEvent.valid())
    {
        SetEvent(m_TerminateEvent);
    }
}

// ----------------------------------------------------------------------------
//
// Starts the sub-thread running.
//
DWORD
WiFiAccountDialog_t::
StartThread(void)
{
    DWORD result;

    m_TerminateEvent = CreateEvent(NULL, TRUE, FALSE, m_pTerminateThreadName);
    if (!m_TerminateEvent.valid())
    {
        result = GetLastError();
        LogError(TEXT("Can't create event \"%s\": %s"),
                 m_pTerminateThreadName, Win32ErrorText(result));
        return result;
    }
    ClearTerminate();

    if (!m_KeyboardMutex.create(TEXT("keyboardMutex")))
    {
        result = GetLastError();
        LogError(TEXT("Can't create keyboard mutex: %s"),
                 Win32ErrorText(result));
        return result;
    }

    m_ThreadHandle = CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
    if (!m_ThreadHandle.valid())
    {
        result = GetLastError();
        LogError(TEXT("Can't create thread to monitor dialog \"%s\": %s"),
                 m_pTitle, Win32ErrorText(result));
        return result;
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Sub-thread run method.
//
DWORD WINAPI
WiFiAccountDialog_t::
ThreadProc(LPVOID pParameter)
{
    WiFiAccountDialog_t *pThis = (WiFiAccountDialog_t *) pParameter;

    LogDebug(TEXT("  dialog watcher for \"%s\" started"), pThis->m_pTitle);

    pThis->m_Result = ERROR_NOT_FOUND;
    pThis->m_CloseResult = ERROR_INVALID_DATA;

    const long cycleTime = 50;
    while (!pThis->IsTerminateSet(cycleTime))
    {
        HWND hWnd = FindWindow(NULL, pThis->m_pTitle);
        if (NULL == hWnd)
            continue;

        LogDebug(TEXT("    found dialog-box \"%s\""), pThis->m_pTitle);

        pThis->m_Result = pThis->InsertDialog(hWnd);

        LogDebug(TEXT("    entered dialog-box text"));

        if (0 == pThis->m_DialogCloseTimeMS)
        {
            pThis->m_CloseResult = ERROR_SUCCESS;
            LogDebug(TEXT("    not waiting for dialog-box to close"));
        }
        else
        {
            pThis->m_CloseResult = ERROR_NOT_FOUND;
            LogDebug(TEXT("    waiting for dialog-box to close"));

            DWORD waitCycles = pThis->m_DialogCloseTimeMS / cycleTime;
            DWORD cycleIX = 0;
            while (FindWindow(NULL, pThis->m_pTitle)
                && ++cycleIX < waitCycles
                && !pThis->IsTerminateSet(cycleTime))
            {;}

            if (pThis->IsTerminateSet())
                break;

            double duration = (double)(cycleIX * cycleTime) / 1000.0;

            if (cycleIX < waitCycles)
            {
                pThis->m_CloseResult = ERROR_SUCCESS;
                LogDebug(TEXT("    dialog box \"%s\" closed in %.2f seconds"),
                         pThis->m_pTitle, duration);
            }
            else
            {
                pThis->m_CloseResult = ERROR_TIMEOUT;
                LogWarn(TEXT("Dialog box \"%s\" did not close in %.2f seconds"),
                        pThis->m_pTitle, duration);
            }
        }

        if (!pThis->m_fContinuous)
            break;
    }

    LogDebug(TEXT("  dialog watcher thread for \"%s\" stopped"),
             pThis->m_pTitle);
    return pThis->m_Result;
}

// ----------------------------------------------------------------------------
//
// Translates the specified one- or two-byte sequence to a scan-code and
// simulates the corresponding keyboard key-strokes.
//
inline void
SendKeyboardEvent(
    UCHAR KeyChar,
    DWORD Flags)
{
    // Sometimes it is helpful to be able to see the dialog-box interaction
    // actually happening. This slows down the interaction to the point where
    // it becomes visible to normal humans.
    Sleep(50);
    keybd_event((BYTE)KeyChar, (BYTE)MapVirtualKey(KeyChar, 0), Flags, 0);
}
static void
SendKeyboardEvents(
    const UCHAR *KeyChars,
    int          NumChars)
{
    SendKeyboardEvent(KeyChars[0], 0);
    if (0 < --NumChars) SendKeyboardEvents(&(KeyChars[1]), NumChars);
    SendKeyboardEvent(KeyChars[0], KEYEVENTF_KEYUP);
}

// ----------------------------------------------------------------------------
//
// Types the authentication credentials into the dialog box.
//
DWORD
WiFiAccountDialog_t::
InsertDialog(
    HWND Window)
{
    // Only one dialog window can be active at a time.
    ce::gate<ce::mutex> locked(m_KeyboardMutex);

    // Insert credentials into the output dialog.
    TCHAR buffer[MAX_PATH];
          TCHAR *bufPtr = &buffer[0];
    const TCHAR *bufEnd = &buffer[MAX_PATH-1];
    for (const TCHAR *pDialog = m_Dialog ; *pDialog ; ++pDialog)
    {
        TCHAR ch = pDialog[0];
        if (TEXT('%') == ch)
        {
            const TCHAR *pCred;
            switch (pDialog[1])
            {
                case TEXT('u'): pCred = m_Cred.GetUserName(); break;
                case TEXT('p'): pCred = m_Cred.GetPassword(); break;
                case TEXT('d'): pCred = m_Cred.GetDomain();   break;
                default:        pCred = NULL;                 break;
            }
            if (NULL != pCred)
            {
                pDialog++;
                for (; *pCred ; ++pCred)
                    if (bufPtr < bufEnd)
#pragma prefast(suppress: 394, "Potential buffer overrun is impossible") 
                      *(bufPtr++) = *pCred;
                    else break;
                continue;
            }
        }
        if (bufPtr < bufEnd)
          *(bufPtr++) = ch;
        else break;
    }

    // Send the buffer to the keyboard.
    UCHAR keyChars[MAX_PATH];
    int   numChars = 0;
    bufEnd = &bufPtr[0];
    bufPtr = &buffer[0];
    for (; bufPtr < bufEnd ; ++bufPtr)
    {
        TCHAR ch = bufPtr[0];
        switch (ch)
        {
            case TEXT('^'):     // CTRL
                keyChars[numChars++] = VK_CONTROL;
                continue;
            case TEXT('~'):     // ALT
                keyChars[numChars++] = VK_MENU;
                continue;
            case TEXT('!'):     // SHIFT
                keyChars[numChars++] = VK_SHIFT;
                continue;
            case TEXT('\n'):    // newline
                keyChars[numChars++] = VK_RETURN;
                break;
            case TEXT('\t'):    // horizontal tab
                keyChars[numChars++] = VK_TAB;
                break;
            case TEXT('\v'):    // vertical tab
                keyChars[numChars++] = VK_UP;
                break;
            case 0x1B:          // escape
                keyChars[numChars++] = VK_ESCAPE;
                break;
            default:
                keyChars[numChars++] = _totupper(ch);
                break;
        }
        SendKeyboardEvents(keyChars, numChars);
        numChars = 0;
    }

    return ERROR_SUCCESS;
}

// ============================== WiFiAccount ============================== */

// ----------------------------------------------------------------------------
//
// Initializes or cleans up static resources.
//
void
WiFiAccount_t::
StartupInitialize(void)
{
    // nothing to do.
}

void
WiFiAccount_t::
ShutdownCleanup(void)
{
    // nothing to do.
}

// ----------------------------------------------------------------------------
//
// Configuration parameters:
//

//
// Certificate enrollment host name:
//
const  TCHAR WiFiAccount_t::DefaultEnrollHost[]         = TEXT("10.10.0.1");
static TCHAR                     s_EnrollHost[MAX_PATH] = TEXT("10.10.0.1");

const TCHAR *
WiFiAccount_t::
GetEnrollHost(void)
{
    return s_EnrollHost;
}

void
WiFiAccount_t::
SetEnrollHost(const TCHAR *pValue)
{
    SafeCopy(s_EnrollHost, pValue, COUNTOF(s_EnrollHost));
}

//
// Certificate enrollment Command name:
//
const  TCHAR WiFiAccount_t::DefaultEnrollCommand[]         = TEXT("enroll");
static TCHAR                     s_EnrollCommand[MAX_PATH] = TEXT("enroll");

const TCHAR *
WiFiAccount_t::
GetEnrollCommand(void)
{
    return s_EnrollCommand;
}

void
WiFiAccount_t::
SetEnrollCommand(const TCHAR *pValue)
{
    SafeCopy(s_EnrollCommand, pValue, COUNTOF(s_EnrollCommand));
}

//
// Title of "set root certificate" dialog-box
//
const  TCHAR WiFiAccount_t::DefaultEnrollRootDialog[]         = TEXT("Root Certificate Store");
static TCHAR                     s_EnrollRootDialog[MAX_PATH] = TEXT("Root Certificate Store");

const TCHAR *
WiFiAccount_t::
GetEnrollRootDialog(void)
{
    return s_EnrollRootDialog;
}

void
WiFiAccount_t::
SetEnrollRootDialog(const TCHAR *pValue)
{
    SafeCopy(s_EnrollRootDialog, pValue, COUNTOF(s_EnrollRootDialog));
}

//
// Title of "insert new certificate" dialog-box:
//
const  TCHAR WiFiAccount_t::DefaultEnrollToolDialog[]         = TEXT("Enrollment Tool");
static TCHAR                     s_EnrollToolDialog[MAX_PATH] = TEXT("Enrollment Tool");

const TCHAR *
WiFiAccount_t::
GetEnrollToolDialog(void)
{
    return s_EnrollToolDialog;
}

void
WiFiAccount_t::
SetEnrollToolDialog(const TCHAR *pValue)
{
    SafeCopy(s_EnrollToolDialog, pValue, COUNTOF(s_EnrollToolDialog));
}

//
// Milliseconds to wait for enrollment tool to finish:
//
const  long WiFiAccount_t::MinimumEnrollTime =      15*1000; // 15 seconds
const  long WiFiAccount_t::DefaultEnrollTime =    3*60*1000; // 3 minutes
const  long WiFiAccount_t::MaximumEnrollTime = 2*60*60*1000; // 2 hours
static long                     s_EnrollTime =    3*60*1000; // 3 minutes

long
WiFiAccount_t::
GetEnrollTime(void)
{
    return s_EnrollTime;
}

DWORD
WiFiAccount_t::
SetEnrollTime(const TCHAR *pValue)
{
    TCHAR *pValueEnd;
    unsigned long ulValue = _tcstoul(pValue, &pValueEnd, 10);
    if (NULL == pValueEnd || TEXT('\0') != pValueEnd[0]
     || MinimumEnrollTime > (long)ulValue
     || MaximumEnrollTime < (long)ulValue)
    {
        LogError(TEXT("Enroll command wait time \"%s\" bad (min=%ld, max=%ld)"),
                 pValue, MinimumEnrollTime, MaximumEnrollTime);
        return ERROR_INVALID_PARAMETER;
    }
    else
    {
        s_EnrollTime = (long)ulValue;
        return ERROR_SUCCESS;
    }
}

//
// Title of "User Logon" dialog-box:
//
const  TCHAR WiFiAccount_t::DefaultUserLogonDialog[]         = TEXT("User Logon");
static TCHAR                     s_UserLogonDialog[MAX_PATH] = TEXT("User Logon");

const TCHAR *
WiFiAccount_t::
GetUserLogonDialog(void)
{
    return s_UserLogonDialog;
}

void
WiFiAccount_t::
SetUserLogonDialog(const TCHAR *pValue)
{
    SafeCopy(s_UserLogonDialog, pValue, COUNTOF(s_UserLogonDialog));
}

//
// Title of "Network Password" dialog-box:
//
const  TCHAR WiFiAccount_t::DefaultNetPasswordDialog[]         = TEXT("Enter Network Password");
static TCHAR                     s_NetPasswordDialog[MAX_PATH] = TEXT("Enter Network Password");

const TCHAR *
WiFiAccount_t::
GetNetPasswordDialog(void)
{
    return s_NetPasswordDialog;
}

void
WiFiAccount_t::
SetNetPasswordDialog(const TCHAR *pValue)
{
    SafeCopy(s_NetPasswordDialog, pValue, COUNTOF(s_NetPasswordDialog));
}

//
// Milliseconds to wait for user logon dialog to close:
//
const  long WiFiAccount_t::MinimumLogonCloseTime =     5*1000; // 5 seconds
const  long WiFiAccount_t::DefaultLogonCloseTime =  5*60*1000; // 5 minutes
const  long WiFiAccount_t::MaximumLogonCloseTime = 60*60*1000; // 1 hour
static long                     s_LogonCloseTime =  5*60*1000; // 5 minutes

long
WiFiAccount_t::
GetLogonCloseTime(void)
{
    return s_LogonCloseTime;
}

DWORD
WiFiAccount_t::
SetLogonCloseTime(const TCHAR *pValue)
{
    TCHAR *pValueEnd;
    unsigned long ulValue = _tcstoul(pValue, &pValueEnd, 10);
    if (NULL == pValueEnd || TEXT('\0') != pValueEnd[0]
     || MinimumLogonCloseTime > (long)ulValue
     || MaximumLogonCloseTime < (long)ulValue)
    {
        LogError(TEXT("Logon-dialog close time \"%s\" bad (min=%ld, max=%ld)"),
                 pValue, MinimumLogonCloseTime, MaximumLogonCloseTime);
        return ERROR_INVALID_PARAMETER;
    }
    else
    {
        s_LogonCloseTime = (long)ulValue;
        return ERROR_SUCCESS;
    }
}

// ----------------------------------------------------------------------------
//
// Displays the command-argument options.
//
void
WiFiAccount_t::
PrintUsage(void)
{
    LogAlways(TEXT("\ncertificate enrollment options:"));
    LogAlways(TEXT("  -nHost     enrollment host name (default \"%s\")"), DefaultEnrollHost);
    LogAlways(TEXT("  -nCommand  enrollment command name (default \"%s\")"), DefaultEnrollCommand);
    LogAlways(TEXT("  -nRootBox  title of \"set root cert\" dialog-box (default \"%s\")"), DefaultEnrollRootDialog);
    LogAlways(TEXT("  -nToolBox  title of \"insert new cert\" dialog-box (default \"%s\")"), DefaultEnrollToolDialog);
    LogAlways(TEXT("  -nWaitTime milliseconds to wait for command (default %ld (%ld mins))"), DefaultEnrollTime, DefaultEnrollTime / (60*1000));

    LogAlways(TEXT("\nuser logon options:"));
    LogAlways(TEXT("  -uUsrBox   title of user logon dialog-box (default \"%s\")"), DefaultUserLogonDialog);
    LogAlways(TEXT("  -uNetBox   title of network password dialog-box (default \"%s\")"), DefaultNetPasswordDialog);
    LogAlways(TEXT("  -uWaitTime milliseconds to wait for dialog close (default %ld (%ld mins))"), DefaultLogonCloseTime, DefaultLogonCloseTime / (60*1000));
}

// ----------------------------------------------------------------------------
//
// Parses the DLL's command-line arguments.
//
DWORD
WiFiAccount_t::
ParseCommandLine(int argc, TCHAR *argv[])
{
    DWORD result;

    // Define the array of accepted options.
    TCHAR *optionsArray[] = {
        TEXT("nHost"),
        TEXT("nCommand"),
        TEXT("nRootBox"),
        TEXT("nToolBox"),
        TEXT("nWaitTime"),
        TEXT("uUsrBox"),
        TEXT("uNetBox"),
        TEXT("uWaitTime"),
    };
    int numberOptions = COUNTOF(optionsArray);

    // Check each of the options.
    for (int opx = 0 ; opx < numberOptions ; ++opx)
    {
        int wasOpt = WasOption(argc, argv, optionsArray[opx]);
        if (wasOpt < 0)
        {
            if (wasOpt < -1)
            {
                LogError(TEXT("Error parsing command line for option \"-%s\""),
                         optionsArray[opx]);
                return ERROR_INVALID_PARAMETER;
            }
            continue;
        }

        TCHAR *optionStr = NULL;
        int wasArg = GetOption(argc, argv, optionsArray[opx], &optionStr);

        switch (opx)
        {
            case 0: // -nHost - certs host
                if (wasArg < 0 || !optionStr || !optionStr[0])
                {
                    LogError(TEXT("No certificate-host after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                SetEnrollHost(optionStr);
                break;

            case 1: // -nCommand - enroll command
                if (wasArg < 0 || !optionStr || !optionStr[0])
                {
                    LogError(TEXT("No enrollment command after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                SetEnrollCommand(optionStr);
                break;

            case 2: // -nRootBox - root-cert dialog-box
                if (wasArg < 0 || !optionStr || !optionStr[0])
                {
                    LogError(TEXT("No root-cert dialog after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                SetEnrollRootDialog(optionStr);
                break;

            case 3: // -nToolBox - insert-cert dialog box
                if (wasArg < 0 || !optionStr || !optionStr[0])
                {
                    LogError(TEXT("No insert-cert dialog after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                SetEnrollToolDialog(optionStr);
                break;

            case 4: // -nWaitTime - time to wait for enrollment
                if (wasArg < 0 || !optionStr || !optionStr[0])
                {
                    LogError(TEXT("No enroll wait-time after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                result = SetEnrollTime(optionStr);
                if (ERROR_SUCCESS != result)
                    return result;
                break;

            case 5: // -uUsrBox - user logon dialog box
                if (wasArg < 0 || !optionStr || !optionStr[0])
                {
                    LogError(TEXT("No user logon dialog after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                SetUserLogonDialog(optionStr);
                break;

            case 6: // -uNetBox - network password dialog box
                if (wasArg < 0 || !optionStr || !optionStr[0])
                {
                    LogError(TEXT("No network password dialog after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                SetNetPasswordDialog(optionStr);
                break;

            case 7: // -uWaitTime - time to wait for dialog to close
                if (wasArg < 0 || !optionStr || !optionStr[0])
                {
                    LogError(TEXT("No logon close wait-time after \"-%s\""),
                             optionsArray[opx]);
                    return ERROR_INVALID_PARAMETER;
                }
                result = SetEnrollTime(optionStr);
                if (ERROR_SUCCESS != result)
                    return result;
                break;
        }
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Constructor.
//
WiFiAccount_t::
WiFiAccount_t(
    const TCHAR *pUserName,
    const TCHAR *pPassword,
    const TCHAR *pDomain)
    : m_NumberDialogThreads(0)
{
    SetUserName(pUserName);
    SetPassword(pPassword);
    SetDomain  (pDomain);
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
WiFiAccount_t::
~WiFiAccount_t(void)
{
    CloseDialogThreads();
}

// ----------------------------------------------------------------------------
//
// Gets or sets the account credentials.
//
const TCHAR *
WiFiAccount_t::
GetUserName(void) const
{
    return m_UserName;
}

const TCHAR *
WiFiAccount_t::
GetPassword(void) const
{
    return m_Password;
}

const TCHAR *
WiFiAccount_t::
GetDomain(void) const
{
    return m_Domain;
}

DWORD
WiFiAccount_t::
SetUserName(const TCHAR *pUserName)
{
    SafeCopy(m_UserName, pUserName, MaximumUserNameChars);
    return ERROR_SUCCESS;
}

DWORD
WiFiAccount_t::
SetPassword(const TCHAR *pPassword)
{
    SafeCopy(m_Password, pPassword, MaximumPasswordChars);
    return ERROR_SUCCESS;
}

DWORD
WiFiAccount_t::
SetDomain(const TCHAR *pDomain)
{
    SafeCopy(m_Domain, pDomain, MaximumDomainChars);
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Parses the account credentials from the specified string.
//
DWORD
WiFiAccount_t::
Assign(
    const TCHAR *pParams)
{
    assert(NULL != pParams);
    HRESULT hr;
    DWORD result;

    TCHAR buffer[MaximumUserNameChars
               + MaximumPasswordChars
               + MaximumDomainChars + 3];
    hr = StringCchCopyEx(buffer, COUNTOF(buffer), pParams,
                         NULL, NULL, STRSAFE_IGNORE_NULLS);
    if (FAILED(hr))
    {
        result = HRESULT_CODE(hr);
        LogError(TEXT("Error translating credentials \"%s\": %s"),
                 buffer, Win32ErrorText(result));
        return result;
    }

    static const TCHAR separators[] = TEXT(":,\t");
    const TCHAR *pUserName, *pPassword, *pDomain;
    pUserName = pPassword = pDomain = NULL;
    for (TCHAR *token = _tcstok(buffer, separators) ;
                token != NULL ;
                token = _tcstok(NULL, separators))
    {
        if      (NULL == pUserName) { pUserName = Utils::TrimToken(token); }
        else if (NULL == pPassword) { pPassword = Utils::TrimToken(token); }
        else if (NULL == pDomain)   { pDomain   = Utils::TrimToken(token); }
        else {
            LogError(TEXT("Extra chars after end of credentials \"%s\""),
                     pParams);
            return ERROR_INVALID_PARAMETER;
        }
    }

    if (NULL == pUserName)
    {
        LogError(TEXT("User-name missing from credentials \"%s\""),
                 pParams);
        return ERROR_INVALID_PARAMETER;
    }

    if (NULL == pPassword)
    {
        LogError(TEXT("Password missing from credentials \"%s\""),
                 pParams);
        return ERROR_INVALID_PARAMETER;
    }

    if (NULL == pDomain)
    {
        LogError(TEXT("Domain-name missing from credentials \"%s\""),
                 pParams);
        return ERROR_INVALID_PARAMETER;
    }

    result = SetUserName(pUserName);
    if (ERROR_SUCCESS == result)
        result = SetPassword(pPassword);
    if (ERROR_SUCCESS == result)
        result = SetDomain(pDomain);

    return result;
}

// ----------------------------------------------------------------------------
//
// Translates the account credentials to text form.
//
DWORD
WiFiAccount_t::
ToString(
  __out_ecount(BufferChars) TCHAR *pBuffer,
                            int     BufferChars) const
{
    HRESULT hr = StringCchPrintf(pBuffer, BufferChars, TEXT("%s:%s:%s"),
                                 m_UserName, m_Password, m_Domain);
    return FAILED(hr)? HRESULT_CODE(hr) : ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Display the dialog-box watcher results.
//
static void
ShowDialogResult(
    const WiFiAccountDialog_t &Dialog,
    void (*LogFunc)(const TCHAR *pFormat, ...))
{
    DWORD result = Dialog.GetResult();
    switch (result)
    {
        case ERROR_SUCCESS:
            break;
        case ERROR_INVALID_DATA:
            LogFunc(TEXT("%s dialog watcher thread didn't start properly"),
                    Dialog.GetDialogTitle());
            break;
        case ERROR_NOT_FOUND:
            LogFunc(TEXT("%s dialog was not filled in"),
                    Dialog.GetDialogTitle());
            break;
        default:
            LogFunc(TEXT("%s dialog failed: %s"),
                    Dialog.GetDialogTitle(), Win32ErrorText(result));
            break;
    }
}

static void
ShowDialogCloseResult(
    const WiFiAccountDialog_t &Dialog,
    void (*LogFunc)(const TCHAR *pFormat, ...))
{
    DWORD result = Dialog.GetCloseResult();
    switch (result)
    {
        case ERROR_SUCCESS:
        case ERROR_INVALID_DATA:
            break;
        case ERROR_NOT_FOUND:
            LogFunc(TEXT("%s dialog-box did not close"),
                    Dialog.GetDialogTitle());
            break;
        case ERROR_TIMEOUT:
            LogFunc(TEXT("%s dialog-close timed out after %ld seconds"),
                    Dialog.GetDialogTitle(),
                    Dialog.GetDialogCloseTimeMS() / 1000);
            break;
        default:
            LogFunc(TEXT("%s dialog-close failed: %s"),
                    Dialog.GetDialogTitle(), Win32ErrorText(result));
            break;
    }
}

// ----------------------------------------------------------------------------
//
// Retrieves a certificate from the certificate server.
//
DWORD
WiFiAccount_t::
Enroll(void)
{
    DWORD result;

    CloseDialogThreads();

    LogStatus(TEXT("Getting \"%s\" authentication certificate"),
             GetUserName());

    // Write the configuration file.
    TCHAR fileName[64];
    int   fileNameChars = COUNTOF(fileName);
    SafeCopy  (fileName, TEXT("\\"),    fileNameChars);
    SafeAppend(fileName, GetUserName(), fileNameChars);
    SafeAppend(fileName, TEXT(".cfg"),  fileNameChars);

    FILE *fp;
#ifdef UNICODE
    fp = _wfopen(fileName, TEXT("wt"));
#else
    fp = fopen(fileName, "wt");
#endif
    if (NULL == fp)
    {
        LogError(TEXT("cannot create \"%s\" for certificate enrollment"),
                 fileName);
        return ERROR_OPEN_FAILED;
    }
    fwprintf(fp,
             TEXT("SERVER=%s\n")
             TEXT("USERNAME=%s\\%s\n")
             TEXT("PASSWORD=%s\n")
             TEXT("CERT_STORE=MY\n")
             TEXT("DW_KEY_SPEC=1\n")
             TEXT("DW_FLAGS=0\n")
             TEXT("DW_PROV_TYPE=1\n")
             TEXT("CERT_REQ_PAGE=/certsrv/certfnsh.asp\n")
             TEXT("CERT_PICKUP_TEMPLATE=/certsrv/certnew.cer")
             TEXT(                             "?ReqId=%%i&Enc=b64\n")
             TEXT("\n"),
             GetEnrollHost(), GetDomain(), GetUserName(), GetPassword());
    fclose(fp);

    // Create dialog-box watcher threads.
    auto_ptr<WiFiAccountDialog_t> pRootDialog;
    pRootDialog = WiFiAccountDialog_t::CreateDialogWatcher(*this,
                        TEXT("RootCertDailog"), // terminate-event name
                        GetEnrollRootDialog(),  // dialog-box title
                        TEXT("\x02Y"),          // dialog-box text = ALT+Y
                        false,                  // don't restart watcher
                        3*60*1000);             // wait for close 3 mins
    if (!pRootDialog.valid())
        return ERROR_OUTOFMEMORY;

    result = pRootDialog->StartThread();
    if (ERROR_SUCCESS != result)
        return result;

    auto_ptr<WiFiAccountDialog_t> pToolDialog;
    pToolDialog = WiFiAccountDialog_t::CreateDialogWatcher(*this,
                        TEXT("EnrollToolDialog"), // terminate-event name
                        GetEnrollToolDialog(),    // dialog-box title
                        TEXT("\n"),               // dialog-box text = newline
                        false,                    // don't restart watcher
                        3*60*1000);               // wait for close 3 mins
    if (!pToolDialog.valid())
        return ERROR_OUTOFMEMORY;

    result = pToolDialog->StartThread();
    if (ERROR_SUCCESS != result)
        return result;

    m_pDialogThreads[m_NumberDialogThreads++] = pRootDialog.release();
    m_pDialogThreads[m_NumberDialogThreads++] = pToolDialog.release();

    // Run the enrollment command.
    TCHAR arguments[64];
    int   argumentChars = COUNTOF(arguments);
    SafeCopy  (arguments, TEXT("-f"), argumentChars);
    SafeAppend(arguments, fileName,   argumentChars);

    LogDebug(TEXT("Running enrollment command \"%s %s\""),
             GetEnrollCommand(), arguments);

    result = Utils::RunCommand(GetEnrollCommand(), arguments, GetEnrollTime());
    if (ERROR_SUCCESS != result)
        return result;

    // Delete the config file.
    DeleteFile(fileName);

    // Stop the dialog-watcher threads.
    result = StopDialogThreads(GetEnrollTime());
    if (ERROR_SUCCESS != result)
    {
        if (ERROR_TIMEOUT == result)
        {
            LogError(TEXT("Dialog-box watchers didn't stop in %u seconds"),
                    GetEnrollTime() / 1000);
        }
        else
        {
            LogError(TEXT("Cannot wait for dialog-box watchers: %s"),
                     Win32ErrorText(result));
        }
    }

    // Make sure the dialogs were processed successfully.
    pToolDialog = m_pDialogThreads[--m_NumberDialogThreads];
    pRootDialog = m_pDialogThreads[--m_NumberDialogThreads];

    ShowDialogResult     (*pToolDialog, LogError);
    ShowDialogCloseResult(*pToolDialog, LogWarn);
    ShowDialogResult     (*pRootDialog, LogDebug);
    ShowDialogCloseResult(*pRootDialog, LogDebug);

    if (ERROR_SUCCESS != pToolDialog->GetResult())
        return pToolDialog->GetResult();

    LogDebug(TEXT("Generated authentication certificate for \"%s\""),
             GetUserName());

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Creates the registry entries required to tell the security system
// that the user has already been authenticated so it won't pop up the
// user-login dialog.
//
static DWORD
RegisterUserLogin(
    const WiFiAccount_t &Creds,
    const TCHAR         *pSSIDName,
    APEapAuthMode_e      EapAuthMode)
{
    DWORD result;
    HRESULT hr;

    // Create or open the SSID's EAPOL parameters registry entry.
    TCHAR ssidKey[MAX_PATH];
    hr = StringCchPrintf(ssidKey, COUNTOF(ssidKey),
                         TEXT("%s\\%s"), EAPOL_REGISTRY_KEY_CONFIG,
                                         pSSIDName);
    if (FAILED(hr))
    {
        result = HRESULT_CODE(hr);
        LogError(TEXT("Can't format SSID EAPOL registry key: %s"),
                 Win32ErrorText(result));
        return result;
    }

    auto_hkey rootHkey;
    result = RegCreateKeyEx(HKEY_CURRENT_USER,
                            ssidKey, 0, NULL, 0, 0, NULL,
                           &rootHkey, NULL);

    if (ERROR_SUCCESS != result)
    {
        LogWarn(L"Cannot open EAPOL SSID key \"%s\": %s",
                ssidKey, Win32ErrorText(result));
        return result;
    }

    // Turn on the enable-802.1X and "last authentication succeeded" flags.
    hr = WiFUtils::WriteRegDword(rootHkey, ssidKey,
                                 EAPOL_REGISTRY_VALUENAME_ENABLE_8021X, 1);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    hr = WiFUtils::WriteRegDword(rootHkey, ssidKey,
                                 EAPOL_REGISTRY_VALUENAME_LAST_AUTH_SUCCESS, 1);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    // Insert the WZC EAP-authentication type.
    hr = WiFUtils::WriteRegDword(rootHkey, ssidKey,
                                 EAPOL_REGISTRY_VALUENAME_EAPTYPE,
                                 WiFiConfig_t::APEapAuthMode2WZC(EapAuthMode));
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    // Insert the user's identity.
    TCHAR identity[MAX_PATH];
    hr = StringCchPrintf(identity, COUNTOF(identity),
                          TEXT("%s\\%s"), Creds.GetDomain(),
                                          Creds.GetUserName());
    if (FAILED(hr))
    {
        LogError(TEXT("Can't format user-name EAPOL data: %s"),
                 HRESULTErrorText(hr));
        return HRESULT_CODE(hr);
    }

    hr = WiFUtils::WriteRegString(rootHkey, ssidKey,
                                  EAPOL_REGISTRY_VALUENAME_IDENTITY, identity);
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    // Encrypt and insert the password.
    WCHAR passwd[MAX_PATH];
    hr = WiFUtils::ConvertString(passwd, Creds.GetPassword(), 
                         COUNTOF(passwd), "password EAPOL data");
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    DATA_BLOB dataPassword,
              dataEncrypted;

    dataPassword.cbData = wcslen(passwd) * sizeof(WCHAR);
    dataPassword.pbData = (BYTE *)passwd;
    dataEncrypted.cbData = 0;
    dataEncrypted.pbData = NULL;

    if (!CryptProtectData(&dataPassword,
                          L"EAPOL Password", NULL, NULL, NULL,
                          CRYPTPROTECT_SYSTEM,
                          &dataEncrypted))
    {
        result = GetLastError();
        LogError(TEXT("Can't encrypt password EAPOL data: %s"),
                 Win32ErrorText(result));
        return result;
    }

    result = RegSetValueEx(rootHkey, EAPOL_REGISTRY_VALUENAME_PASSWORD,
                           0, REG_BINARY, dataEncrypted.pbData,
                                          dataEncrypted.cbData);
    if (ERROR_SUCCESS != result)
    {
        LogError(TEXT("Cannot update registry value \"%s\\%s\": %s"),
                 ssidKey, EAPOL_REGISTRY_VALUENAME_PASSWORD,
                 Win32ErrorText(result));
    }

    LocalFree(dataEncrypted.pbData);

    return result;
}

// ----------------------------------------------------------------------------
//
// Starts the user logon dialog-box sub-thread to authenticate a
// connection to the specified SSID. If possible, sets the EAPOL
// registry so the system doesn't need to launch the dialog.
//
DWORD
WiFiAccount_t::
StartUserLogon(
    const TCHAR    *pSSIDName,
    APEapAuthMode_e EapAuthMode)
{
    DWORD result;
    assert(NULL != pSSIDName && TEXT('\0') != pSSIDName[0]);

    LogDebug(TEXT("Starting logon dialog for \"%s\" to connect with %s"),
             GetUserName(), pSSIDName);

    // Close all the running threads.
    CloseDialogThreads();

    // If possible, insert the user's login information into the registry.
    result = RegisterUserLogin(*this, pSSIDName, EapAuthMode);
    if (ERROR_SUCCESS != result && ERROR_ACCESS_DENIED != result)
        return result;

    // Start the "User Login" dialog thread.
    auto_ptr<WiFiAccountDialog_t> pUsrDialog;
    pUsrDialog = WiFiAccountDialog_t::CreateDialogWatcher(*this,
                        TEXT("UserLogonDailog"), // terminate-event name
                        GetUserLogonDialog(),    // dialog-box title
                        TEXT("\t\t%u\t%d\n"),    // dialog text
                        true,                    // repeat in case login fails
                        20*1000);                // wait 20 seconds before retry
    if (!pUsrDialog.valid())
        return ERROR_OUTOFMEMORY;

    result = pUsrDialog->StartThread();
    if (ERROR_SUCCESS != result)
        return result;

    // Start the "Enter Network Password" dialog thread.
    auto_ptr<WiFiAccountDialog_t> pNetDialog;
    pNetDialog = WiFiAccountDialog_t::CreateDialogWatcher(*this,
                        TEXT("NetPasswdDailog"), // terminate-event name
                        GetNetPasswordDialog(),  // dialog-box title
                        TEXT("!\t%u\t%p\t%d\n"), // dialog text
                        true,                    // repeat in case login fails
                        20*1000);                // wait 20 seconds before retry
    if (!pNetDialog.valid())
        return ERROR_OUTOFMEMORY;

    result = pNetDialog->StartThread();
    if (ERROR_SUCCESS != result)
        return result;

    m_pDialogThreads[m_NumberDialogThreads++] = pUsrDialog.release();
    m_pDialogThreads[m_NumberDialogThreads++] = pNetDialog.release();
    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Closes the user logon dialog-box sub-thread.
//
void
WiFiAccount_t::
CloseUserLogon(
    DWORD CloseWaitTimeMS)
{
    LogDebug(TEXT("Stopping logon dialog for \"%s\""),
             GetUserName());

    DWORD result = StopDialogThreads(CloseWaitTimeMS);
    if (ERROR_SUCCESS != result)
    {
        if (ERROR_TIMEOUT == result)
        {
            LogWarn(TEXT("Logon dialog-box watcher didn't stop in %u seconds"),
                    CloseWaitTimeMS / 1000);
        }
        else
        {
            LogWarn(TEXT("Cannot wait for logon dialog-box watcher: %s"),
                    Win32ErrorText(result));
        }
    }

    CloseDialogThreads();
}

// ----------------------------------------------------------------------------
//
// Stops the dialog-box sub-threads with a WaitFor.
//
DWORD
WiFiAccount_t::
StopDialogThreads(DWORD CloseWaitTimeMS)
{
    int closed, threads = m_NumberDialogThreads;

    if (threads < 0)
        threads = 0;
    if (threads > MaximumDialogThreads)
        threads = MaximumDialogThreads;
    
    HANDLE dialogHandles[MaximumDialogThreads];
    for (closed = 0 ; closed < threads ; ++closed)
    {
        m_pDialogThreads[closed]->SetTerminate();
        dialogHandles[closed] = m_pDialogThreads[closed]->GetThreadHandle();
    }

    for (; 0 < threads ; --threads)
    {
        DWORD result = WaitForMultipleObjects(threads, dialogHandles,
                                              FALSE, CloseWaitTimeMS);
        switch (result)
        {
            case WAIT_TIMEOUT:
                return ERROR_TIMEOUT;

            case WAIT_FAILED:
                return GetLastError();

            default:
                closed = (int)(result - WAIT_OBJECT_0);
                if (0 <= closed)
                    for (int next = closed ; ++next < threads ; ++closed)
                        dialogHandles[closed] = dialogHandles[next];
                break;
        }
    }

    return ERROR_SUCCESS;
}

// ----------------------------------------------------------------------------
//
// Stops the dialog-box sub-threads without waiting.
//
void
WiFiAccount_t::
CloseDialogThreads(void)
{
    StopDialogThreads(10000);
    while (0 < m_NumberDialogThreads)
    {
        m_NumberDialogThreads--;
        if (NULL != m_pDialogThreads[m_NumberDialogThreads])
        {
            delete m_pDialogThreads[m_NumberDialogThreads];
            m_pDialogThreads[m_NumberDialogThreads] = NULL;
        }
    }
}

// ----------------------------------------------------------------------------
