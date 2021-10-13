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

#ifndef CAMERAMULTIGRAPHTESTS_H
#define CAMERAMULTIGRAPHTESTS_H

#define MAX_THREADCOUNT 5

struct sTestCaseData
{
    int nGraphType;
    DWORD dwGraphLayout;
    int nFileLength;
    TCHAR *tszFileName;
    HWND hwnd;
};

struct sTestScenarioData
{
    int nThreadCount;
    struct sTestCaseData *pTestData[MAX_THREADCOUNT];
};

extern struct sTestCaseData g_sTestCaseData[];

extern struct sTestScenarioData g_sScenarioTestData[];

enum
{
    NotUsed = 0,
    CaptureGraph,
    PlaybackGraph,
    MessagePump
};

#endif

