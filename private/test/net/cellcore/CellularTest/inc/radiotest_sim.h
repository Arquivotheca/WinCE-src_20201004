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

#ifndef _radiotest_sim_h_
#define _radiotest_sim_h_

#include <simmgr.h>
#include "radiotest.h"

// class CSIMSyn - SIM card operation normally uses lock mechanism
class CSIMSyn
{
public:
    CSIMSyn();
    ~CSIMSyn();

private:
    HANDLE m_hMutex;
};

// SIM test class
// NOTE: CSIMTest should focus on scenario test only.
//       To SIM operation, phonebook is the most usual scenario.
//       Message in SIM is another common user scenario.
class CSIMTest
{
public:
    CSIMTest()
    {
        m_dwNotifySize = 0;     // notification size
        m_lpNotifyData = 0;     // notification data
        m_dwNotifyCode = 0;     // notification
        m_hNotifyEvent = NULL;  // event for waiting notification
        m_hSIM = NULL;          // SIM handle
        for (DWORD dwIter = 0; dwIter < MAX_RADIONUM; dwIter++)
        {
            m_rgSIMArray[dwIter] = NULL;
        }

        FakeMessage(m_message); // fake one message for test usage
        Print(&m_message);      // print faked message
    }
    BOOL Init();    // should be called after object of CSIMTest is created
    BOOL DeInit(); // should be pair of Init()

    // SIM test atom: BOOL XXXX();
    // message operation
    BOOL WriteMessage(DWORD * pIndex);
    BOOL ReadMessage(const DWORD dwIndex);
    BOOL DeleteMessage(const DWORD dwIndex);
    BOOL GetMessageStorage();
    void Print(const LPSIMMESSAGE pSMS);
    BOOL Compare(const SIMMESSAGE &message);

    // phone book operation
    BOOL WritePhoneBookEx(DWORD &dwIndex);
    BOOL ReadPhoneBookEx(const DWORD dwIndex);
    BOOL DeletePhoneBookEx(const DWORD dwIndex);

    BOOL WritePhoneBook(DWORD dwIndex);
    BOOL ReadPhoneBook(DWORD dwIndex);
    
    BOOL GetPhoneBookStorage();
    void Print(const LPSIMPHONEBOOKENTRYEX ppbeex);
    BOOL Compare(const SIMPHONEBOOKENTRYEX &pbeex);

    void Print(const LPSIMPHONEBOOKCAPS pPBCaps);
    BOOL GetPhoneBookCapabilities(SIMPHONEBOOKCAPS *pPBCaps = NULL);
    BOOL SimType(DWORD &dwType);


    // SIM call back, this call back should access member function
    friend void HandleNotify(
        HSIM hSim, 
        DWORD dwCode, 
        const void *lpData, 
        DWORD cbData, 
        DWORD dwParam
        );

    // common function for SIM test
    static BOOL Compare(
        LPCWSTR wszPrefix,
        const SYSTEMTIME stNow, 
        const SYSTEMTIME stRef
        );

private:
    void Print(const LPSIMPHONEBOOKENTRY ppbe);
    BOOL Compare(const SIMPHONEBOOKENTRY &pbe);
    void CSIMTest::SetNotification(const DWORD dwNotify);
    BOOL WaitForNotifiation();
    void FakeMessage(SIMMESSAGE &message);
    void FakePhoneBook(SIMPHONEBOOKENTRYEX *pbeex);
    BOOL GetPBCaps(DWORD dwParams, DWORD & dwRes);

private:
    HSIM    m_rgSIMArray[MAX_RADIONUM];
    HSIM    m_hSIM;
    SIMPHONEBOOKCAPS m_simPBCaps;
    DWORD   m_dwNotifySize;
    LPVOID  m_lpNotifyData;
    DWORD   m_dwNotifyCode;
    HANDLE  m_hNotifyEvent;
    DWORD   m_dwTimer;
    SIMMESSAGE m_message; // for faked message
    SIMPHONEBOOKENTRYEX m_pbeex; // for faked phone book entry.
    SIMPHONEBOOKADDITIONALNUMBER m_pban; // for faked additional number of phone book entry.
    SIMPHONEBOOKEMAILADDRESS m_pbea;  // for faked email address of phone book entry.

};

// common global functions for SIM test
LPCWSTR SIM_NotifyToString(const DWORD dwCode);
BOOL VerifyNotification(
    const DWORD dwCode, 
    const void *lpData, 
    const DWORD cbData
    );

BOOL SIM_SMSWRD(DWORD dwUserData);
BOOL SIM_PhoneBookWRD(DWORD dwUserData);
BOOL SIM_PhoneBookRead(DWORD dwUserData);
#endif
