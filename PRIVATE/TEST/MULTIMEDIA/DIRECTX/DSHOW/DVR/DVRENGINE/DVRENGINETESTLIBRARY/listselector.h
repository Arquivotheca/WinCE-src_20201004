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

typedef void (*P_FUNC_ENUMERATE)(HWND hWndList, void * pData);
typedef void (*P_FUNC_ON_REMOVE_ITEM) (void * pItem, BOOL bSelected);

class CListSelector
{
public:
    CListSelector(void);
    ~CListSelector(void);
    static INT_PTR Select(HINSTANCE hInst, HWND hWndParent, P_FUNC_ENUMERATE pFuncEnumerate, P_FUNC_ON_REMOVE_ITEM pFuncOnRemoveItem, void * pDataToPassBack, LPCTSTR pszDialogCaption);

private:
    static P_FUNC_ENUMERATE Enumerate;
    static P_FUNC_ON_REMOVE_ITEM OnRemoveItem;
    static LPCTSTR pszCaption;
    static void * pData;
    static BOOL CALLBACK SelectionDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
};

