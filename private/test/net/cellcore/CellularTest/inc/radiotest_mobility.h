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

#ifndef _RADIOTEST_MOBILITY_H_
#define _RADIOTEST_MOBILITY_H_

#include <tapi.h>
#include "radiotest.h"

// Mobility test class
class CMobilityTest
{
public:
    CMobilityTest()
    {
        m_hRIL = NULL;
        m_dwHandoverCount = 0;
    }
    bool Init();    // should be called when application wants to call test function
    bool DeInit(); // should be pair of Init()

    // test functions
    // default is 10 minutes
    bool VoiceWithHandover(const DWORD dwDuration = 10 * 60 * 1000);
    // default is 1 MB
    bool DataWithHandover(const DWORD dwSize = 1 * 1024);

    // RIL call back, this call back should access member function
    friend void HandleRILResult(
        HRIL        hRIL, 
        DWORD       dwCode, 
        HRESULT     hrCmdID,
        const void *lpData, 
        DWORD       cbData, 
        DWORD       dwParam
        );
    friend void HandleRILNotify(
        HRIL        hRIL, 
        DWORD       dwCode, 
        const void *lpData, 
        DWORD       cbData, 
        DWORD       dwParam
        );

private:
    HRIL    m_hRIL;
    DWORD   m_dwHandoverCount;
};
// global mobility test object

LPCWSTR RIL_NotifyToString(const DWORD dwCode);


// Test cases
BOOL Mobility_Voice(DWORD dwUserData);
BOOL Mobility_Data(DWORD dwUserData);

#endif // #ifndef _RADIOTEST_MOBILITY_H_