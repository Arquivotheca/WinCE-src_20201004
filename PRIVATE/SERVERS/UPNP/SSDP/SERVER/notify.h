//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __NOTIFY__
#define __NOTIFY__

#include "httpext.h"

#include "auto_xxx.hxx"
#include "string.hxx"
#include "list.hxx"
#include "sync.hxx"


// msg_queue
class msg_queue
{
public:
    msg_queue(LPCWSTR pwszName)
        : m_strName(pwszName),
          m_bWriteTerminateMsg(false)
    {}
    
    // init
    void init()
    {
        MSGQUEUEOPTIONS options = {0};

		options.dwSize = sizeof(MSGQUEUEOPTIONS);

		// create message queue
		m_handle = CreateMsgQueue(m_strName, &options);

		Assert(GetLastError() == ERROR_ALREADY_EXISTS);
		Assert(m_handle.valid());
    }
    
    // operator HANDLE
    operator HANDLE()
        {return m_handle; }
    
    // write_terminate_msg
    void write_terminate_msg()
        {m_bWriteTerminateMsg = true; }
    
    // check_terminate_msg
    bool check_terminate_msg()
    {
        if(m_bWriteTerminateMsg)
	    {
		    int x = 0;

		    // write terminate msg so that reader knows that previous event message wasn't finished and must be discarded
		    if(!WriteMsgQueue(m_handle, &x, 1, 0, 0))
                return false;
	    }

        m_bWriteTerminateMsg = false;

        return true;
    }
    
    // lock
    void lock()
		{m_cs.lock(); }

	// unlock
    void unlock()
		{m_cs.unlock(); }

    // name
    const ce::wstring& name()
        {return m_strName; }

private:    
    ce::auto_xxx<HANDLE, BOOL (__stdcall *)(HANDLE), CloseMsgQueue,	NULL, ce::ref_counting>
                                    m_handle;
    ce::critical_section_with_copy  m_cs;
    ce::wstring		                m_strName;
    bool							m_bWriteTerminateMsg;
};


// notification_sink
class notification_sink
{
public:
	notification_sink(DWORD dwType, LPCWSTR pwszUSN, LPCWSTR pwszQueryString, msg_queue& MsgQueue, HANDLE hOwner);

	void event(LPCWSTR pwszMessage, DWORD dwEventSEQ);
	void byebye(PSSDP_REQUEST pSsdpRequest);
	void alive(PSSDP_REQUEST pSsdpRequest);

	const ce::wstring& getQueryString()
		{return m_strQueryString; }

	const ce::wstring& getUSN()
		{return m_strUSN; }

	const DWORD getType()
		{return m_dwType; }

	HANDLE getHandle()
		{return this; }

    HANDLE getOwner()
        {return m_hOwner; }

private:
	bool init_msg_queue();

private:
	DWORD							m_dwType;
	ce::wstring						m_strQueryString;
	ce::wstring						m_strUSN;
	ce::critical_section_with_copy	m_cs;
    HANDLE                          m_hOwner;
    msg_queue                      &m_MsgQueue; 
};


// notification_mgr
class notification_mgr
{
public:
	HANDLE	register_notification(DWORD dwNotificationType, LPCWSTR pwszUSN, LPCWSTR pwszQueryString, LPCWSTR pwszMsgQueue, HANDLE hOwner);
	void	deregister_notification(HANDLE hSubscription);
    void    cleanup_notifications(HANDLE hOwner);
	bool	alive_byebye(PSSDP_REQUEST pSsdpRequest);
	DWORD	event(LPEXTENSION_CONTROL_BLOCK pecb);

private:
    ce::list<msg_queue>             m_listMsgQueues;
	ce::list<notification_sink>		m_listSinks;
	ce::critical_section			m_cs;
};

extern notification_mgr* g_pNotificationMgr;

#endif //__NOTIFY__
