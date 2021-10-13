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
#pragma once
#include "FilterGraph.h"

typedef struct _FILTER_LIST
{
    CFilterGraph * pFilterGraph;
    _FILTER_LIST * pNext;
} FILTER_LIST;


class CFilterGraphList
{
public:
    CFilterGraphList(void);
    ~CFilterGraphList(void);
    void Add(CFilterGraph * pFilterGraphToAdd);
    CFilterGraph * GetCurrent();
    CFilterGraph * GetCurrentEnumerated();
    HRESULT RunCurrent();
    HRESULT PauseCurrent();
    HRESULT StopCurrent();
    HRESULT RunAll();

    void DeleteAll();
    void Select(HINSTANCE hInst, HWND hWndParent, LPCTSTR pszCaption);
    BOOL Select(INT iTargetIndex);
    BOOL Select(CFilterGraph * pTargetFilterGraph);
    CFilterGraph * EnumerateFirst();
    CFilterGraph * EnumerateNext();
private:
    FILTER_LIST * pFilterList;
    FILTER_LIST * pLastElement;
    FILTER_LIST * pCurrentElement;
    FILTER_LIST * pEnumeratedElement;
    static void Enumerate(HWND hWndList, void * pData);
};

