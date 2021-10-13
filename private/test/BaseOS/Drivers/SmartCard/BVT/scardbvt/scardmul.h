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
