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
#include "filtergraphlist.h"
#include "commctrl.h"
#include "ListSelector.h"
#include "Utilities.h"

CFilterGraphList::CFilterGraphList(void)
    :    pFilterList(NULL),
        pLastElement(NULL),
        pCurrentElement(NULL),
        pEnumeratedElement(NULL)
{
}

CFilterGraphList::~CFilterGraphList(void)
{
    DeleteAll();
}

void CFilterGraphList::Add(CFilterGraph * pFilterGraphToAdd)
{
    FILTER_LIST * pFilterListNewElement = new FILTER_LIST;

    if(pFilterListNewElement)
    {
        pFilterListNewElement->pFilterGraph = pFilterGraphToAdd;
        pFilterListNewElement->pNext = NULL;

        if(this->pFilterList == NULL)
        {
            this->pFilterList     = pFilterListNewElement;
        }
        else
        {
            this->pLastElement->pNext = pFilterListNewElement;
        }

        this->pLastElement    = pFilterListNewElement;
        this->pCurrentElement = pFilterListNewElement;
    }
}

CFilterGraph * CFilterGraphList::GetCurrent()
{
    if(this->pCurrentElement != NULL)
    {
        return this->pCurrentElement->pFilterGraph;
    }
    else
    {
        return NULL;
    }
}

CFilterGraph * CFilterGraphList::GetCurrentEnumerated()
{
    if(this->pEnumeratedElement != NULL)
    {
        return this->pEnumeratedElement->pFilterGraph;
    }
    else
    {
        return NULL;
    }
}

HRESULT CFilterGraphList::RunCurrent()
{
    CFilterGraph * pFilterGraph = GetCurrent();

    if(pFilterGraph != NULL)
    {
        return pFilterGraph->Run();
    }
    else
    {
        return E_FAIL;
    }
}

HRESULT CFilterGraphList::RunAll()
{
    HRESULT hr = E_FAIL;

    CFilterGraph * pFilterGraph = EnumerateFirst();

    while(pFilterGraph != NULL)
    {
        hr = pFilterGraph->Run();

        if(FAILED(hr))
        {
            break;
        }

        pFilterGraph = EnumerateNext();
    }

    return hr;
}

HRESULT CFilterGraphList::PauseCurrent()
{
    CFilterGraph * pFilterGraph = GetCurrent();

    if(pFilterGraph != NULL)
    {
        return pFilterGraph->Pause();
    }
    else
    {
        return E_FAIL;
    }
}

HRESULT CFilterGraphList::StopCurrent()
{
    CFilterGraph * pFilterGraph = GetCurrent();

    if(pFilterGraph != NULL)
    {
        return pFilterGraph->Stop();
    }
    else
    {
        return E_FAIL;
    }
}

void CFilterGraphList::DeleteAll()
{
    pCurrentElement = pFilterList;

    while(pCurrentElement != NULL)
    {
        pLastElement = pCurrentElement;
        pCurrentElement = pLastElement->pNext;

        pLastElement->pFilterGraph->UnInitialize();
        delete pLastElement->pFilterGraph;

        delete pLastElement;
    }

    pLastElement = NULL;
    pFilterList  = NULL;
}

void CFilterGraphList::Enumerate(HWND hWndList, void * pData)
{
    LVITEM lvI;
    FILTER_LIST * pFilterListElement = (FILTER_LIST *) pData;
    // Initialize LVITEM members that are common to all items. 
    lvI.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE; 
    lvI.state = 0; 
    lvI.stateMask = 0; 

    int index = 0;

    while (pFilterListElement != NULL)
    {
           lvI.iItem    = index;
        lvI.iImage   = 0;
        lvI.iSubItem = 0;
        lvI.lParam   = (LPARAM) pFilterListElement;

        WCHAR pszFriendlyName[MAX_PATH] = {NULL};

        if(pFilterListElement->pFilterGraph->GetName())
            wcscpy(pszFriendlyName, pFilterListElement->pFilterGraph->GetName());
        lvI.pszText = pszFriendlyName;

        int newIndex = ListView_InsertItem(hWndList, &lvI);

        if(newIndex == -1)
        {
            //return ;
        }

        pFilterListElement = pFilterListElement->pNext;
        index++;
    }
}

void CFilterGraphList::Select(HINSTANCE hInst, HWND hWndParent, LPCTSTR pszCaption)
{
    INT_PTR dialogResult = CListSelector::Select(hInst, hWndParent, Enumerate, NULL, this->pFilterList, pszCaption);

    if(dialogResult != NULL)
    {
        this->pCurrentElement = (FILTER_LIST *)dialogResult;
    }
}

BOOL CFilterGraphList::Select(INT iTargetIndex)
{
    BOOL retVal = TRUE;
    FILTER_LIST * pTargetElement = this->pFilterList;

    for(INT i=0; i<iTargetIndex; i++)
    {
        if(pTargetElement == NULL)
        {
            retVal = FALSE;
            break;
        }
        else
        {
            pTargetElement = pTargetElement->pNext;
        }
    }

    if(retVal == TRUE)
    {
        this->pCurrentElement = pTargetElement;
    }

    return retVal;
}

BOOL CFilterGraphList::Select(CFilterGraph * pTargetFilterGraph)
{
    BOOL retVal = FALSE;
    FILTER_LIST * pTargetElement = this->pFilterList;

    while(pTargetElement != NULL)
    {
        if(pTargetElement->pFilterGraph  == pTargetFilterGraph)
        {
            retVal = TRUE;
            break;
        }
        else
        {
            pTargetElement = pTargetElement->pNext;
        }
    }

    if(retVal == TRUE)
    {
        this->pCurrentElement = pTargetElement;
    }

    return retVal;
}

CFilterGraph * CFilterGraphList::EnumerateFirst()
{
    this->pEnumeratedElement = this->pFilterList;

    return GetCurrentEnumerated();
}

CFilterGraph * CFilterGraphList::EnumerateNext()
{
    if(this->pEnumeratedElement == NULL)
    {
        return NULL;
    }
    else if(this->pEnumeratedElement->pNext == NULL)
    {
        return NULL;
    }
    else
    {
        this->pEnumeratedElement = this->pEnumeratedElement->pNext;
        return GetCurrentEnumerated();
    }
}

