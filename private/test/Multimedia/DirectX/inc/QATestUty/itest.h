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

#ifndef __ITEST_H__
#define __ITEST_H__

// ===================
// enum enumTestResult
// ===================
enum 
{
    trPass = 0,
    trSkip = 1,
    trAbort = 2,
    trFail = 4,
    trException = 8,
    trNotImplemented = 16
};

typedef DWORD eTestResult;
typedef eTestResult enumTestResult; // kept for old code

// ===============
// interface ITest
// ===============
class ITest 
{
public:
    virtual ~ITest() {};

    virtual eTestResult Execute()=0;
    virtual LPCTSTR GetTestName()=0;

    virtual void Initialize(DWORD dwThreadCount, DWORD dwRandomSeed, DWORD dwThreadNumber) = 0;
};

#endif
