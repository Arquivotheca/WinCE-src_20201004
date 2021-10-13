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
#include "devices.h"
#include "commctrl.h"
#include "ListSelector.h"
#include "Utilities.h"

CDevices::CDevices(void)
{
}

CDevices::~CDevices(void)
{
}
#ifndef _WIN32_WCE
HRESULT CDevices::Select(HINSTANCE hInst, HWND hWndParent, REFCLSID clsidDeviceClass, LPCTSTR pszCaption, IBaseFilter ** ppBaseFilter)
{
    // Create the System Device Enumerator.
    ICreateDevEnum *pSysDevEnum = NULL;
    
    HRESULT hr = CoCreateInstance(    CLSID_SystemDeviceEnum,
                                    NULL,
                                    CLSCTX_INPROC_SERVER,
                                    IID_ICreateDevEnum,
                                    (void **)&pSysDevEnum);

    if (FAILED(hr))
    {
        return hr;
    }

    // Obtain a class enumerator for the given category.
    IEnumMoniker * pEnumCat = NULL;

    hr = pSysDevEnum->CreateClassEnumerator(clsidDeviceClass, &pEnumCat, 0);

    if (hr == S_OK) 
    {
        INT_PTR dialogResult = CListSelector::Select(    hInst, hWndParent, Enumerate, OnRemoveItem, pEnumCat, pszCaption);

        if(dialogResult != NULL)
        {
            IMoniker * pMoniker = (IMoniker *) dialogResult;

            if(ppBaseFilter != NULL)
            {
                hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**) ppBaseFilter);
            }

            pMoniker->Release();
        }

        pEnumCat->Release();
    }

    pSysDevEnum->Release();

    return hr;
}
#endif

void CDevices::Enumerate(HWND hWndList, void * pData)
{
    TCHAR friendlyName[255];
    LVITEM lvI;
    IEnumMoniker * pEnumCat = (IEnumMoniker *) pData;
    // Initialize LVITEM members that are common to all items. 
    lvI.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE; 
    lvI.state = 0; 
    lvI.stateMask = 0; 

    int index = 0;

    // Enumerate the monikers.
    IMoniker *pMoniker = NULL;
    ULONG cFetched;
    while (pEnumCat->Next(1, &pMoniker, &cFetched) == S_OK)
    {
        // Bind the first moniker to an object
        IPropertyBag *pPropBag;
        HRESULT hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pPropBag);
        if (SUCCEEDED(hr))
        {
            // To retrieve the filter's friendly name, do the following:
            VARIANT varName;
            VariantInit(&varName);
            hr = pPropBag->Read(L"FriendlyName", &varName, 0);
            if (SUCCEEDED(hr))
            {
                // Do a comparison, find out if it's the right one
                //if (wcsncmp(varName.bstrVal, matchName, 
                //    wcslen(matchName)) == 0) {

                //    // We found it, so send it back to the caller
                //    hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**) gottaFilter);
                //    done = true;
                //}
            }

               lvI.iItem = index;
            lvI.iImage = 0;
            lvI.iSubItem = 0;
            lvI.lParam = (LPARAM) pMoniker;//0;//(LPARAM) &rgPetInfo[index];

            CUtilities::Bstr2Tchar(varName.bstrVal, wcslen(varName.bstrVal), friendlyName, 255);
            lvI.pszText = friendlyName;
                //lvI.pszText = BSTR2A(varName.bstrVal);
                //lvI.pszText = COLE2T(varName.bstrVal);//.bstrVal;//rgPetInfo[index,0];//LPSTR_TEXTCALLBACK; // sends an LVN_GETDISPINFO
                                                // message.

            int newIndex = ListView_InsertItem(hWndList, &lvI);

            if(newIndex == -1)
            {
                //return ;
            }

            VariantClear(&varName);    
            pPropBag->Release();
        }
        //pMoniker->Release();

        index++;
    }
}

void CDevices::OnRemoveItem(void * pData, BOOL bSelected)
{
    if(!bSelected)
    {
        IMoniker *pMoniker = (IMoniker *) pData;

        pMoniker->Release();
    }
}

