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
#include "StdAfx.h"
#include "listselector.h"
#include "Resource.h"
#include "commctrl.h"

P_FUNC_ENUMERATE      CListSelector::Enumerate    = NULL;
P_FUNC_ON_REMOVE_ITEM CListSelector::OnRemoveItem = NULL;
LPCTSTR               CListSelector::pszCaption   = NULL;
void *                CListSelector::pData        = NULL;

CListSelector::CListSelector()
{
}

CListSelector::~CListSelector(void)
{
}

BOOL CALLBACK CListSelector::SelectionDialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    INT_PTR retVal = NULL;
    LVITEM lvI;
    HWND hwndList  = GetDlgItem(hwndDlg, IDC_LIST);
    INT selection  = -1;

    switch (message) 
    { 
        case WM_INITDIALOG:
            SetWindowText(hwndDlg, pszCaption);
            if(Enumerate != NULL)
            {
                Enumerate(hwndList, pData);
            }
            return TRUE; 
 
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
                case IDOK:
                    selection = ListView_GetSelectionMark(hwndList);

                    if(selection > -1)
                    {
                        lvI.iItem = selection;
                        lvI.iSubItem = 0;
                        if(ListView_GetItem(hwndList, &lvI) == TRUE)
                        {
                            retVal = lvI.lParam;
                        }
                    }
 
                case IDCANCEL:

                    for(int i=0; i< ListView_GetItemCount(hwndList); i++)
                    {
                        if(i != selection)
                        {
                            lvI.iItem   = i;
                            lvI.iSubItem = 0;
                            if(ListView_GetItem(hwndList, &lvI) == TRUE)
                            {
                                if(OnRemoveItem != NULL)
                                {
                                    OnRemoveItem((void *)lvI.lParam, i == selection);
                                }
                            }
                        }
                    }

                    EndDialog(hwndDlg, retVal);
                    //hwndGoto = NULL; 
                    return TRUE; 
            } 
    } 

    return FALSE; 
}

INT_PTR CListSelector::Select(HINSTANCE hInst, HWND hWndParent, P_FUNC_ENUMERATE pFuncEnumerate, P_FUNC_ON_REMOVE_ITEM pFuncOnRemoveItem, void * pDataToPassBack, LPCTSTR pszDialogCaption)
{
    Enumerate    = pFuncEnumerate;
    OnRemoveItem = pFuncOnRemoveItem;
    pData        = pDataToPassBack;
    pszCaption   = pszDialogCaption;

    INITCOMMONCONTROLSEX initFlags;
    initFlags.dwICC = ICC_LISTVIEW_CLASSES;
    initFlags.dwSize = sizeof(initFlags);

    InitCommonControlsEx(&initFlags);

    INT_PTR retVal = DialogBox(    hInst,
                        (LPCTSTR)IDD_DIALOG_SELECT,
                        hWndParent,
                        SelectionDialogProc);

    if(retVal == -1)
    {
        retVal = NULL;
        DWORD error = GetLastError();
        WCHAR errMsg[250];
        wcscpy(errMsg, TEXT("DialogBox failed with error"));
        wsprintf(errMsg, TEXT("DialogBox failed with error %d"), error);
        MessageBox(NULL, errMsg, TEXT("Error"), MB_OK);
    }

    return retVal;
}

