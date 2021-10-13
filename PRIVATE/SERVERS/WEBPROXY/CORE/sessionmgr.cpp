//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "session.h"
#include "proxydbg.h"

DWORD CSessionMgr::StartSession(SessionSettings* pSettings, DWORD* pdwSessionId)
{
	DWORD dwRetVal = ERROR_SUCCESS;
	CHttpSession session;

	Lock();

	//
	// Insert the session in the list.  This is copying the object and thus invoking the copy
	// constructor of CHttpSession.
	//
	
	if (false == m_SessionList.push_front(session)) {
		dwRetVal = ERROR_OUTOFMEMORY;
		goto exit;
	}

	pSettings->pSessionMgr = this;

	//
	// Have to call the start() method after we insert the item on the list since
	// it will copy the object.  Start() must be called from the same instance
	// which will exist in the list since it stores the instance of the object for
	// use by a static method.
	//
	
	m_SessionList.begin()->SetId(m_dwNextId);
	dwRetVal = m_SessionList.begin()->Start(pSettings);
	if (ERROR_SUCCESS != dwRetVal) {
		m_SessionList.pop_front();
		goto exit;
	}

	if (pdwSessionId) {
		*pdwSessionId = m_dwNextId;
	}

	m_dwNextId++;

exit:	
	Unlock();
	return dwRetVal;
}

DWORD CSessionMgr::RemoveSession(DWORD dwSessionId)
{
	DWORD dwRetVal = ERROR_NOT_FOUND;

	Lock();

	// Traverse the list until we find the node with the specified session id and delete it
	for(list::iterator it = m_SessionList.begin(), itEnd = m_SessionList.end(); it != itEnd;) {
		if(it->GetId() == dwSessionId) {
			m_SessionList.erase(it++);
			dwRetVal = ERROR_SUCCESS;
			break;
		}
		else {
			++it;
		}
	}

	Unlock();
	return dwRetVal;
}

DWORD CSessionMgr::RemoveAllSessions(void)
{
	Lock();
	
	// Remove each node in the list
	for (list::iterator it = m_SessionList.begin(), itEnd = m_SessionList.end(); it != itEnd;) {
		m_SessionList.erase(it++);
	}

	Unlock();
	return ERROR_SUCCESS;
}

DWORD CSessionMgr::ShutdownAllSessions(void)
{
	Lock();
	
	// Traverse the list, shutting down each session.
	for (list::iterator it = m_SessionList.begin(), itEnd = m_SessionList.end(); it != itEnd; ++it) {
		DWORD dwErr = it->Shutdown();
#ifdef DEBUG
		if (ERROR_SUCCESS != dwErr) {
			IFDBG(DebugOut(ZONE_WARN, _T("WebProxy: Did not shutdown session : %d.\n"), dwErr));
		}
#endif // DEBUG
	}

	Unlock();
	return ERROR_SUCCESS;
}

int CSessionMgr::GetSessionCount(void)
{
	Lock();
	int iSessions = m_SessionList.size();
	Unlock();
	return iSessions;
}

