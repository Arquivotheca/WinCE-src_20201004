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
// Implentation of the QAMessageBoxPrompter_t classes.
//
// ----------------------------------------------------------------------------

#include <QAMessageBoxPrompter_t.hpp>
#include <assert.h>

using namespace CENetworkQA;

// ----------------------------------------------------------------------------
//
// MessageBox sub-thread.
//
class MessageBoxThread_t
{
private:

    // Thread call-back procedure:
    static DWORD WINAPI
    ThreadProc(LPVOID pParameter);
        
    // Runs the thread.
    QADialog_t::ResponseCode
    Run(void);
    
    // Object running the dialog:
    const QADialog_t &m_rDialog;
    
    // Address of MessageBox procedure:
    CoreDllLoader_t::pMessageBox_t m_pMessageBoxProc;

    // MessageBox caption and contents:
    LPCTSTR m_pCaption;
    LPCTSTR m_pPrompt;

    // Sub-thread handle and id:
    HANDLE m_ThreadHandle;
    DWORD  m_ThreadId;
    
    // Response from the user:
    QADialog_t::ResponseCode m_Response;
    
public:

    // Constructor:
    MessageBoxThread_t(
        const QADialog_t              &rDialog,
        CoreDllLoader_t::pMessageBox_t pMessageBoxProc,
        LPCTSTR                        pCaption,
        LPCTSTR                        pPrompt)
        : m_rDialog(rDialog),
          m_pMessageBoxProc(pMessageBoxProc),
          m_pCaption(pCaption),
          m_pPrompt(pPrompt),
          m_ThreadHandle(NULL)
    { ; }

    // Destructor:
   ~MessageBoxThread_t(void)
    {
        if (m_ThreadHandle != NULL)
        {
            CloseHandle(m_ThreadHandle);
            m_ThreadHandle = NULL;
        }
    }

    // Starts the thread.
    bool
    Start(void);

    // Waits for the thread to finish.
    QADialog_t::ResponseCode
    Wait(HANDLE *pEventHandles, long MaxWaitTimeMS);

    // Stops the thread.
    void
    Stop(void);
};

// ----------------------------------------------------------------------------
//
// Constructor.
//
QAMessageBoxPrompter_t::
QAMessageBoxPrompter_t(const QADialog_t &rDialog)
    : QADialogPrompter_t(rDialog),
      m_pMessageBoxProc(NULL)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
QAMessageBoxPrompter_t::
~QAMessageBoxPrompter_t(void)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Initializes the object (if necessary) and makes sure it's ready.
// False indicates there's a problem - use GetLastError for an explanation.
//
bool
QAMessageBoxPrompter_t::
IsValid(void)
{
    // Load the MessageBox procedure.
    if (m_pMessageBoxProc == NULL)
    {
        m_pMessageBoxProc = m_CoreDll.GetMessageBoxProc();
        if (m_pMessageBoxProc == NULL)
        {
            Log(LOG_FAIL,
                TEXT("ERROR: Can't load MessageBox procedure: err=%u"),
                GetLastError());
            return false;
        }
    }
    return true;
}

// ----------------------------------------------------------------------------
//
// Sends the user a prompt and waits for a response.
//
QADialog_t::ResponseCode 
QAMessageBoxPrompter_t::
RunDialog(
    LPCTSTR pPrompt,        // prompt to show user
    HANDLE *pEventHandles,  // OK/Cancel-button events
    long    MaxWaitTimeMS)  // time to wait for user or events
{
    assert(pEventHandles != NULL);
    assert(pPrompt!= NULL && pPrompt[0] != TEXT('\0'));
    assert(MaxWaitTimeMS >= 0 || MaxWaitTimeMS == INFINITE);
    
    if (IsValid())
    {
        MessageBoxThread_t msgThread(GetDialog(), m_pMessageBoxProc, 
                                     TEXT("Interactive Response"), pPrompt);
        if (msgThread.Start())
        {
            return msgThread.Wait(pEventHandles, MaxWaitTimeMS);
        }
    }
    
    return QADialog_t::ResponseFailed;
}

// ----------------------------------------------------------------------------
//
// Thread call-back procedure.
//
DWORD WINAPI
MessageBoxThread_t::
ThreadProc(LPVOID pParameter)
{
    MessageBoxThread_t *msgThread = (MessageBoxThread_t *) pParameter;
    assert(msgThread != NULL);
    return (DWORD) msgThread->Run();
}

// ----------------------------------------------------------------------------
//
// Runs the sub-thread.
//
QADialog_t::ResponseCode
MessageBoxThread_t::
Run(void)
{
    m_Response = QADialog_t::ResponseFailed;
    switch (m_pMessageBoxProc(NULL, m_pPrompt, m_pCaption,
                              MB_OKCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2))
    {
        case IDOK:
            m_Response = QADialog_t::ResponseOK;
            break;
        case IDCANCEL:;
            m_Response = QADialog_t::ResponseCancel;
            break;
        default:
            m_rDialog.Log(LOG_FAIL, 
                          TEXT("MessageBox failed: err=%u"),
                          GetLastError());
            break;
    }
    
    return m_Response;
}

// ----------------------------------------------------------------------------
//
// Starts the sub-thread.
//
bool
MessageBoxThread_t::
Start(void)
{
    m_ThreadHandle = CreateThread(NULL, 0, ThreadProc,
                                 (PVOID)this, 0, &m_ThreadId);
    if (m_ThreadHandle != NULL)
    {
        return true;
    }   
    else
    {
        m_rDialog.Log(LOG_FAIL,
                      TEXT("ERROR: Can't CreateThread: err=%u"),
                      GetLastError());
        return false;
    }
}

// ----------------------------------------------------------------------------
//
// Waits for the thread to finish.
//
QADialog_t::ResponseCode
MessageBoxThread_t::
Wait(HANDLE *pEventHandles, long MaxWaitTimeMS)
{
    // Wait for the thread or an ok/cancel-button event.
    HANDLE handles[3];
    handles[0] = m_ThreadHandle;
    handles[1] = pEventHandles[0];
    handles[2] = pEventHandles[1];
    QADialog_t::ResponseCode ret;
    switch (WaitForMultipleObjects(3, handles, FALSE, MaxWaitTimeMS))
    {
        case WAIT_OBJECT_0:
            // Thread finished: user response waiting.
           return m_Response;
        
        case WAIT_OBJECT_0 + 1:
            // Ok-button event signalled.
            ResetEvent(handles[1]);
            ret = QADialog_t::ResponseOK;
            break;

        case WAIT_OBJECT_0 + 2:
            // Cancel-button event signalled.
            ResetEvent(handles[2]);
            ret = QADialog_t::ResponseCancel;
            break;
         
        case WAIT_TIMEOUT:
            ret = QADialog_t::ResponseTimeout;
            break;

        default:
            m_rDialog.Log(LOG_FAIL,
                TEXT("ERROR: Wait for msg-box thread failed: err=%u"),
                GetLastError());
            ret = QADialog_t::ResponseFailed;
            break;
    }

    // Forcibly terminate the thread.
    Stop();
    
    return ret;
}

// ----------------------------------------------------------------------------
//
// Stops the thread.
//
void
MessageBoxThread_t::
Stop(void)
{
    if (!PostThreadMessage(m_ThreadId, WM_QUIT, 0, 0))
    {   
        DWORD errorCode = GetLastError();
        if (ERROR_INVALID_THREAD_ID == errorCode)
        {      
            m_rDialog.Log(LOG_FAIL, 
                TEXT("ERROR: terminate msg-box thread failed.")
                TEXT(" PostThreadMessage reported ERROR_INVALID_THREAD_ID."));
        }
        else
        {
            m_rDialog.Log(LOG_FAIL,
                TEXT("ERROR: terminate msg-box thread failed.")
                TEXT(" PostThreadMessage reported a last error of %d"), errorCode);
        }
    }
}

// ----------------------------------------------------------------------------
