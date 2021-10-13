//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++

Module Name:  
    SCardMul.h

Description:  
    Smart Card Multi-Access Test
    
Notes: 

--*/
#ifndef __SCARDMUL_H_
#define __SCARDMUL_H_

#include "syncobj.h"

class TestObj {
public:
    TestObj() { m_dwErrorCode=0;};
    virtual ~TestObj() {;};
    virtual DWORD GetErrorCode() { return m_dwErrorCode; };
    virtual DWORD SetErrorCode(DWORD dwErrorCode) { 
            ASSERT(dwErrorCode==0);
        return m_dwErrorCode=dwErrorCode; 
    };
    virtual DWORD RunTest () { return 0; };
private:
    DWORD m_dwErrorCode;
};

class SyncTestObj : public TestObj{
public:
    SyncTestObj(CEvent * pcEvent,DWORD dwSyncCount) {
        m_pcEvent=pcEvent;
        m_dwSyncCount=dwSyncCount;
    };
    void  CheckLock() { 
        if (m_dwSyncCount>0)
            m_dwSyncCount--;
        else
            m_pcEvent->Lock();
    };
private:
    CEvent *m_pcEvent;
    DWORD m_dwSyncCount;
};

#define NAME_BUFFER_SIZE 512
TESTPROCAPI SCardMulti_Test(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) ;

#endif
