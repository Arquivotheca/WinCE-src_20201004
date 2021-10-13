//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#pragma once

#include <windows.h>
#include <imaging.h>
#include <tux.h>
#include <list>

namespace UnknownTests
{
    typedef std::list<const GUID *> GuidPList;
    INT TestQueryInterface(IUnknown* pUnknown, GuidPList lstExpected, GuidPList lstUnexpected);
    INT TestAddRefRelease(IUnknown* pUnknown);

    typedef INT (*PFNUNKNOWNTEST_QI)(IUnknown*, GuidPList, GuidPList);
    typedef INT (*PFNUNKNOWNTEST_AR)(IUnknown*);
}


