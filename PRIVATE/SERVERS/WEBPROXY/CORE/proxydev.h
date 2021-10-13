//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __PROXYDEV_H__
#define __PROXYDEV_H__

#include "global.h"
#include "service.h"
#include "sync.hxx"
#include "proxy.h"


class CSrvMgr {
public:
	CSrvMgr(void);
	virtual ~CSrvMgr(void);
	
	__inline void Lock(void) { m_csService.lock(); }
	__inline void Unlock(void) { m_csService.unlock(); }
	void SetState(DWORD dwState);
	DWORD GetState(void);

private:
	ce::critical_section m_csService;
	DWORD m_dwState;	
};

extern CSrvMgr g_SvcMgr;
extern HINSTANCE g_hInst;

#endif // __PROXYDEV_H__

