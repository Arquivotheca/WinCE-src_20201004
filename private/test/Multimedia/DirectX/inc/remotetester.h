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
#pragma once

#include <windows.h>
#include <map>
#ifndef UNDER_CE
#include <tchar.h>
#include <strsafe.h>
#endif

// Types
class  Task_t;
class  RemoteMaster_t;
class  RemoteSlave_t;
class  Event_t;
class  EventQueue_t;
struct RemoteParameters_t;

typedef HRESULT (*PFNRemoteSlaveCallback_t)(Task_t * pTask, void * pvContext);
HRESULT ParseRemoteParameters(TCHAR * tszParams, RemoteParameters_t * prp);

#define RT_E_TIMEOUT HRESULT_FROM_WIN32(ERROR_TIMEOUT)

// #defines used
#define REMOTETESTER_MAX_NAME_LEN 256

// Windows Messages used with the RemoteTester framework
// EVENTCOMPLETED: wparam: Event ID
#define UWM_EVENTCOMPLETED    (WM_USER + 1)
// Event ID 0 is reserved for the EVENT_READY message.
#define EVENT_READY 0
#define EVENT_DONE 1
#define EVENT_USER 2

// GOODBYE: no used params
#define UWM_GOODBYE            (UWM_EVENTCOMPLETED + 1)

// SLAVECOMM: Message for sending miscellaneous information between two windows in slave
// wParam: Reason
// lParam: context-sensitive
#define UWM_SLAVECOMM        (UWM_GOODBYE + 1)

// The remote command window is ready, send event 0 to master
#define SLAVECOMM_READY 1

// The remote command window failed to open. lParam: HRESULT value
#define SLAVECOMM_FAILURE 2

// The remote command window has closed. lParam: HRESULT value
#define SLAVECOMM_FINISHED 3

// The remote command window received a task, but it was invalid. lParam: EventID
#define SLAVECOM_INVALIDTASK 5

// NEWTASK: Message that indicates there is a new task
// wParam: Event ID
// lParam: Task_t *
#define UWM_NEWTASK            (UWM_SLAVECOMM + 1)

struct RemoteParameters_t
{
    bool fRunAsMaster;
    TCHAR tszMasterName[REMOTETESTER_MAX_NAME_LEN];
    TCHAR tszSlaveName[REMOTETESTER_MAX_NAME_LEN];
};


class Task_t
{
public:
    static Task_t * CreateNewTask(DWORD dwTaskID, DWORD dwCbData, void * pData);
    static Task_t * CreateNewTask(const Task_t * pTask);
    static void DestroyTask(Task_t * pTask);

    Task_t * GetNextTask();
    void SetNextTask(Task_t * pNext);
    DWORD GetTaskSize();
    void FixupDataPointer();

    // Returns the Task ID associated with this task
    DWORD GetTaskID();

    // If dwCbData is not what is expected, this will fail
    HRESULT GetTaskData(DWORD dwCbData, void * pData);
private:
    // The constructor and destructor are not called (CreateNewTask possibly 
    // allocates extra data, so it just allocates a buffer).
    Task_t() : m_dwTaskID(0), m_dwCbData(0), m_pNext(0), m_pData(NULL) {};
    ~Task_t() {};
    
    DWORD    m_dwTaskID;
    DWORD    m_dwCbData;
    Task_t * m_pNext;
    void   * m_pData;
};

class Event_t
{
public:
    Event_t(DWORD dwEventID, HRESULT hrEventResult);
    
    DWORD     GetEventID();
    HRESULT   GetEventResult();
    Event_t * GetNext();
    void      SetNext(Event_t * pNext);
private:
    Event_t * m_pNext;
    DWORD     m_dwEventID;
    HRESULT   m_hrEventResult;
};

class EventQueue_t
{
public:
    EventQueue_t();
    ~EventQueue_t();
    void Cleanup();
    HRESULT AddEvent(DWORD dwEventID, HRESULT hrEventResult);
    HRESULT RemoveEvent(DWORD dwEventID, HRESULT * phrEventResult);
private:
    Event_t * m_pHead;
    Event_t * m_pTail;
};

class CommWindow_t
{
public:
    CommWindow_t() : m_hwndComm(NULL) {};
    ~CommWindow_t();

    // Non-blocking: will use PeekMessage to flush out any messages
    // in the message queue
    HRESULT FlushWindowMessages();

    // Blocking: will use GetMessage.
    HRESULT MessageLoop();

    HRESULT OpenWindow(
        HINSTANCE hInst, 
        const TCHAR * tszClassName, 
        const TCHAR * tszTitle, 
        WNDPROC WndProc);
    HWND GetWindow();

    void CloseWindow();

private:
    HWND m_hwndComm;
    ATOM m_aClassAtom;
};

class RemoteMaster_t
{
public:
    RemoteMaster_t();
    ~RemoteMaster_t();

    HRESULT Initialize(
        HINSTANCE hInst, 
        const TCHAR * tszMasterName, 
        const DWORD dwMasterID);
    HRESULT Shutdown();
    HRESULT CreateRemoteTestProcess(const TCHAR * tszProcessName);

    // With the Task_t version, the caller is responsible for creation and 
    // destruction of the task.
    // NOTE: Tasks are not guaranteed to be handled in the order they were sent.
    // If order is important, use WaitForCompletedEvent after SendTask, or
    // use SendTaskSynchronous.
    HRESULT SendTask(
        Task_t * pTask, 
        DWORD  * pdwEventID);

    // SendTask is a wrapper for SendTask. It creates a task, calls
    // SendTask, then destroys the task.
    HRESULT SendTask(
        DWORD   dwTaskID, 
        DWORD   dwCbData, 
        void  * pvData, 
        DWORD * pdwEventID);

    // SendTaskSynchronous will send the task and then wait for the 
    // event complete. On successful return, phrTaskResult will contain the
    // result returned with the event.
    HRESULT SendTaskSynchronous(
        Task_t  * pTask,
        HRESULT * phrTaskResult,
        DWORD     dwTimeOut);

    HRESULT SendTaskSynchronous(
        DWORD     dwTaskID, 
        DWORD     dwCbData, 
        void    * pvData, 
        HRESULT * phrTaskResult,
        DWORD     dwTimeOut);

    
    HRESULT WaitForCompletedEvent(
        DWORD     dwEventID, 
        HRESULT * phrEventResult, 
        DWORD     dwTimeOut);

    static LRESULT CALLBACK RemoteMaster_WndProc(
        HWND hWnd, 
        UINT message, 
        WPARAM wParam, 
        LPARAM lParam);

private:
    RemoteMaster_t(RemoteMaster_t &) {}; // unused
    typedef std::pair <HWND, RemoteMaster_t *> ContextPair_t;
    static std::map<HWND, RemoteMaster_t *> m_mapContext;

    CommWindow_t m_cwMasterIncomingEventWindow;
    EventQueue_t m_CompletedEventQueue;

    bool  m_fInitialized;
    DWORD m_dwNextEvent;
    HWND  m_hwndSlaveCommWindow;

    TCHAR m_tszMasterName[REMOTETESTER_MAX_NAME_LEN];
    TCHAR m_tszSlaveName[REMOTETESTER_MAX_NAME_LEN];
    TCHAR m_tszRemoteProcess[MAX_PATH];

    PROCESS_INFORMATION m_piRemoteProcess;
};


class RemoteSlave_t
{
public:
    RemoteSlave_t();
    ~RemoteSlave_t();
    
    // Initialize Function: takes our name, master's name, Callback
    // Creates window for local task queue. The Callback will be called for each
    // task that is received. The tasks will be delivered synchronously (i.e.
    // the callback will not be called more than once before it returns). To
    // ensure synchonous operation, do not call GetMessage or PeekMessage with
    // a NULL hWnd. The Callback will be called in the same thread as Initialize
    // and Run are called.
    HRESULT Initialize(
        HINSTANCE hInst,
        const TCHAR * const tszMasterName,
        const TCHAR * const tszSlaveName,
        PFNRemoteSlaveCallback_t pfnCallback);

    HRESULT InitializeFromCommandLine(
        HINSTANCE hInst,
        TCHAR * tszCommandLine,
        PFNRemoteSlaveCallback_t pfnCallback);

    // Run Function: Creates remote comm thread, starts waiting in message loop
    // Call this after calling Initialize, and after setting up everything needed
    // by the Callback function.
    HRESULT Run(void * pvContext);

    HRESULT Shutdown();

    const TCHAR * GetMasterName();
    const TCHAR * GetSlaveName();
private:
    RemoteSlave_t(RemoteSlave_t & unused) {};
    typedef std::pair <HWND, RemoteSlave_t *> ContextPair_t;
    static std::map<HWND, RemoteSlave_t *> m_mapContext;
    
    // static Functions:
    // Need two wndprocs: one to handle remote command requests
    static LRESULT CALLBACK WndProcRemoteCommands(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam);

    // One for the local task window
    static LRESULT CALLBACK WndProcLocalTasks(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam);

    // Need a thread proc for the Comm thread (takes pointer to RemoteSlave_t class)
    static DWORD WINAPI StaticRemoteCommThread(LPVOID lpParameter);

    DWORD RemoteCommThread();

    HRESULT HandleIncomingTaskRequest(COPYDATASTRUCT * pcds);

    // Other privates:
    // Incoming remote command window
    CommWindow_t m_cwRemoteCommandWindow;

    // Local task window
    CommWindow_t m_cwLocalTaskWindow;

    TCHAR m_tszRemoteCommandWindowName[REMOTETESTER_MAX_NAME_LEN];
    TCHAR m_tszLocalTaskWindowName[REMOTETESTER_MAX_NAME_LEN];
    TCHAR m_tszMasterEventWindowName[REMOTETESTER_MAX_NAME_LEN];

    TCHAR m_tszMasterName[REMOTETESTER_MAX_NAME_LEN];
    TCHAR m_tszSlaveName[REMOTETESTER_MAX_NAME_LEN];

    PFNRemoteSlaveCallback_t m_pfnTaskCallback;

    bool      m_fInitialized;
    HINSTANCE m_hInst;
    HANDLE    m_hCommThread;
    HWND      m_hwndMasterEventWindow;
    void    * m_pvCallbackContext;
};
