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
/**************************************************************************/
/*  DeviceEnum.cpp                                                        */
/*                                                                        */
/*  This file contains routines to enumerate through devices              */
/*                                                                        */
/*  Functions included:                                                   */
/*                                                                        */
/*  Other related files:                                                  */
/*                                                                        */
/*                                                                        */
/**************************************************************************/
#include "common.h"
#include "obexdevice.h"
#include "cobex.h"
#include "DeviceEnum.h"
#include "PropertyBagEnum.h"
#include "ObexStrings.h"
#include "ObexBTHTransport.h"

/*****************************************************************************/
/*   CDeviceEnum                                                             */
/*   construct a device enumerator                                           */
/*****************************************************************************/
CDeviceEnum::CDeviceEnum() :
    _refCount(1),
    pListHead(0),
    pListCurrent(0)
{
   DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::CDeviceEnum\n"));
}


/*****************************************************************************/
/*   CDeviceEnum                                                             */
/*   destruct a device enumerator by looping through cache calling release   */
/*****************************************************************************/
CDeviceEnum::~CDeviceEnum()
{
    DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::~CDeviceEnum\n"));
        SVSUTIL_ASSERT(_refCount == 0);

    DEVICE_NODE *pTemp = pListHead;
    while(pTemp)
    {
        DEVICE_NODE *pDel = pTemp;
        pDel->pItem->Release();
        pTemp = pTemp->pNext;
        delete pDel;
    }
}


/*****************************************************************************/
/*   CDeviceEnum::Next                                                       */
/*   return 'celt' items (each return device must be Release()               */
/*****************************************************************************/
HRESULT STDMETHODCALLTYPE
CDeviceEnum::Next (ULONG celt, LPDEVICE *rgelt, ULONG *pceltFetched)
{
    DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::Next()\n"));
    if ((celt == 0) || ((celt > 1) && (pceltFetched == NULL)))
        return S_FALSE;

    if (pceltFetched)
                *pceltFetched = 0;

    memset(rgelt, 0, sizeof(LPDEVICE)*celt);

    for(ULONG i=0; pListCurrent && (i<celt); i++)
    {
        rgelt[i] = pListCurrent->pItem;
        rgelt[i]->AddRef ();
        pListCurrent = pListCurrent->pNext;
    }

    if (pceltFetched)
                *pceltFetched = i;

    return (i == celt) ? S_OK : S_FALSE;
}


/*****************************************************************************/
/*   CDeviceEnum::Skip                                                       */
/*   Skip 'celt' items in the list                                           */
/*****************************************************************************/
HRESULT STDMETHODCALLTYPE
CDeviceEnum::Skip(ULONG celt)
{
    DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::Skip()\n"));
    if(celt == 0)
        return S_FALSE;

    for(ULONG i=0; pListCurrent && (i<celt); i++)
        pListCurrent = pListCurrent->pNext;

    return i == celt ? S_OK : S_FALSE ;
}


/*****************************************************************************/
/*   CDeviceEnum::Reset                                                      */
/*   reset the internal iterator to the beginning                            */
/*****************************************************************************/
HRESULT STDMETHODCALLTYPE
CDeviceEnum::Reset()
{
    DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::Reset()\n"));
    pListCurrent = pListHead;
    return S_OK;
}


/*****************************************************************************/
/*   CDeviceEnum::Clone                                                      */
/*   clone the enumerator                                                    */
/*                                                                           */
/*****************************************************************************/
HRESULT STDMETHODCALLTYPE
CDeviceEnum::Clone(IDeviceEnum **ppenum)
{
    return E_NOTIMPL;
}


/*****************************************************************************/
/*   CDeviceEnum::Insert                                                     */
/*   insert a device into the enumerator                                     */
/*****************************************************************************/
HRESULT STDMETHODCALLTYPE
CDeviceEnum::Insert(CObexDevice *pDevice)
{
    DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::Insert()\n"));

        PREFAST_ASSERT(pDevice);
        SVSUTIL_ASSERT(gpSynch->IsLocked());

        DEVICE_NODE *pNode = new DEVICE_NODE();
    if (! pNode)
        return E_OUTOFMEMORY;

        //addref it only if we are going to insert it into the list
    pDevice->AddRef ();

    pNode->pItem = pDevice;
        pNode->pNext = pListHead;
    pListHead = pNode;

    DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::Insert-Added new device"));

    return S_OK;
}

/*****************************************************************************/
/*   CDeviceEnum::FindDevice                                                 */
/*   check to see if the given device (pBag) is already in our list of       */
/*      devices.  if not, return an S_OK and set *pDeviceList=NULL...        */
/*      otherwise, if it IS in our list, then AddRef() it and put it on      */
/*      pDeviceList                                                          */
/*****************************************************************************/
HRESULT CDeviceEnum::FindDevicesThatMatch(LPPROPERTYBAG2 pBag, DEVICE_LIST **pDeviceList)
{
        PREFAST_ASSERT(pDeviceList);
        *pDeviceList = NULL;

    DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::FindDevice()\n"));

        VARIANT varTransport;
        VARIANT varAddress;
        VARIANT varName;

        VariantInit(&varTransport);
        VariantInit(&varAddress);
        VariantInit(&varName);

        BOOL fIsIRDA = FALSE;


        if(!pBag)
                return E_FAIL;


        //get the device address from the passed in property bag
        HRESULT hr = pBag->Read(c_szDevicePropAddress, &varAddress, NULL);
        if (FAILED(hr))
        {
                DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::FindDevice() : ERROR - no address in the bag?\n"));
                return hr;
        }

        hr = pBag->Read(c_szDevicePropTransport, &varTransport, NULL);
        if(FAILED(hr))
        {
                DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::FindDevice() : ERROR - no transport in the bag?\n"));
                VariantClear(&varAddress);
                return hr;

        }

        //
        //  if the address is an int, we are dealing with IRDA
        //    since on IRDA the address doesnt always mean something (Palm)
        //    we will use the name instead
        //
        if(varAddress.vt == VT_I4)
        {
                fIsIRDA = TRUE;

                hr = pBag->Read(c_szDevicePropName, &varName, NULL);
                if (FAILED(hr))
                {
                        DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::FindDevice() : this is irda so we have to use name instead of address\n"));
                        VariantClear(&varAddress);
                        VariantClear(&varTransport);
                        return hr;
                }
        }


#if defined (DEBUG) || defined (_DEBUG)
        DWORD dwNAP=0, dwSAP=0;
        if (varAddress.vt == VT_I4)
        {
                dwSAP = varAddress.lVal;
                dwNAP = 0;
        }
        else
        {
                BT_ADDR ba;
            // move in the device id for the selected device
            SVSUTIL_ASSERT(CObexBTHTransport::GetBA(varAddress.bstrVal, &ba));
                dwNAP = GET_NAP(ba);
                dwSAP = GET_SAP(ba);
        }
#endif

        SVSUTIL_ASSERT(gpSynch->IsLocked());

        //loop through all previously discovered devices
        DEVICE_NODE *pNode = pListHead;
        DEVICE_LIST *pRetList = NULL;

        while(pNode)
        {
                VARIANT var2;
                VariantInit(&var2);

                BOOL bEqual = FALSE;

                //check transport
                if(SUCCEEDED(pNode->pItem->GetDeviceTransport(&var2)))
                {
                        if(0 == wcscmp(var2.bstrVal, varTransport.bstrVal))
                                bEqual = TRUE;

                        VariantClear(&var2);
                }



                //set a flag if the passed in device is equal to the one at this
                //  iteration

                if(bEqual && !fIsIRDA)
                {
                        bEqual = ( (SUCCEEDED(pNode->pItem->GetDeviceAddress(&var2))) &&
                                                        (
                                                          ((varAddress.vt == VT_BSTR) && (varAddress.vt == var2.vt) && (0 == wcscmp(varAddress.bstrVal, var2.bstrVal)))
                                                        )
                                                  );
                }
                else if(bEqual)
                {
                        bEqual = ( (SUCCEEDED(pNode->pItem->GetDeviceName(&var2))) &&
                                (
                                  ((varName.vt == VT_BSTR) && (varName.vt == var2.vt) && (0 == wcscmp(varName.bstrVal, var2.bstrVal)))
                                )
                          );
                }

                VariantClear(&var2);

                if (bEqual)
                {
                    DEBUGMSG(OBEX_DEVICEENUM_ZONE, (L"FindDeviceThatMatch -- Device (%08x%08x) already present\n", dwNAP, dwSAP) );

                        //insert the device on the return list
                        DEVICE_LIST *pNew = new DEVICE_LIST();
                        if( !pNew )
                        {
                                VariantClear(&varName);
                                VariantClear(&varAddress);
                                VariantClear(&varTransport);
                                return E_OUTOFMEMORY;
                        }
                        pNode->pItem->AddRef();
                        pNew->pDevice = pNode->pItem;
                        pNew->pNext = pRetList;
                        pRetList = pNew;
                }

                pNode = pNode->pNext;
        }
        VariantClear(&varName);
    VariantClear(&varAddress);
        VariantClear(&varTransport);

        //set the list pointer and return
        *pDeviceList = pRetList;

    return S_OK;
}


/*****************************************************************************/
/*   CDeviceEnum::QueryInterface                                             */
/*   Query for a interface                                                   */
/*****************************************************************************/
HRESULT STDMETHODCALLTYPE
CDeviceEnum::QueryInterface(REFIID riid, void** ppv)
{
    DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::QueryInterface()\n"));
        if(!ppv)
                return E_POINTER;
        if(riid == IID_IUnknown)
                *ppv = this;
        else if(riid == IID_IDeviceEnum)
                *ppv = static_cast<IDeviceEnum *>(this);
        else
                return *ppv = 0, E_NOINTERFACE;

        return AddRef(), S_OK;
}


/*****************************************************************************/
/*   CDeviceEnum::AddRef                                                     */
/*   inc the ref count                                                       */
/*****************************************************************************/
ULONG STDMETHODCALLTYPE
CDeviceEnum::AddRef()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CDeviceEnum::AddRef()\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}


/*****************************************************************************/
/*   CDeviceEnum::Release                                                    */
/*   dec the ref count, on zero delete the object                            */
/*****************************************************************************/
ULONG STDMETHODCALLTYPE
CDeviceEnum::Release()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CDeviceEnum::Release()\n"));
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);
    if(!ret)
        delete this;
    return ret;
}

/*****************************************************************************/
/*   CDeviceEnum::EnumDevices                                                */
/*   helper function to return devices of a particular transport             */
/*****************************************************************************/
HRESULT
CDeviceEnum::EnumDevices(LPDEVICEENUM *ppDeviceEnum, REFCLSID uuidTransport)
{
    DEBUGMSG(OBEX_DEVICEENUM_ZONE,(L"CDeviceEnum::EnumDevices()\n"));

        if (!ppDeviceEnum)
                return E_POINTER;

        *ppDeviceEnum = NULL;

        CDeviceEnum *pEnum = new CDeviceEnum;
        if (!pEnum)
                return E_OUTOFMEMORY;

        SVSUTIL_ASSERT(gpSynch->IsLocked());

        BOOL bNull = (0 == memcmp(&uuidTransport, &CLSID_NULL, sizeof(CLSID)));

        HRESULT hr = S_OK;
        DEVICE_NODE *pNode = pListHead;
        while (pNode)
        {
                CLSID uuidTrans = pNode->pItem->GetTransport();
                if  ( (bNull) || (0 == memcmp(&uuidTransport, &uuidTrans, sizeof(CLSID))) )
                        hr = pEnum->Insert(pNode->pItem);

                if (FAILED(hr))
                        break;

                pNode = pNode->pNext;
        }

        if (SUCCEEDED(hr))
                hr = pEnum->QueryInterface(IID_IDeviceEnum, (LPVOID *)ppDeviceEnum);

        pEnum->Release();

        return hr;
}

HRESULT
CDeviceEnum::StartEnumRun(REFCLSID clsid)
{
        SVSUTIL_ASSERT(clsid != CLSID_NULL);

        SVSUTIL_ASSERT(gpSynch->IsLocked());

        DEVICE_NODE *pNode = pListHead;
        while (pNode)
        {
                PREFAST_ASSERT(pNode->pItem);
                CLSID uuidTrans = pNode->pItem->GetTransport();
                if (0 == memcmp(&clsid, &uuidTrans, sizeof(CLSID)))
                {
                        pNode->pItem->SetPresent (FALSE);
                }

                pNode = pNode->pNext;
        }

        return S_OK;
}


/*****************************************************************************/
/*   CDeviceEnum::FinishEnumRun                                              */
/*   Finish the enumeration run by building a propertybag enumerator that    */
/*      contains all the devices that are no longer present                              */
/*              this is determined by !GetPresent()                                                                      */
/*****************************************************************************/
HRESULT
CDeviceEnum::FinishEnumRun(DEVICE_PROPBAG_LIST **ppDevicesToRemove, REFCLSID uuidTransport)
{
        SVSUTIL_ASSERT(uuidTransport != CLSID_NULL);
        PREFAST_ASSERT(ppDevicesToRemove);

        *ppDevicesToRemove = NULL;

        SVSUTIL_ASSERT(gpSynch->IsLocked());

        DEVICE_PROPBAG_LIST *pDevicesToRemove = NULL;

        HRESULT hr = S_OK;
        DEVICE_NODE *pNode = pListHead;
        DEVICE_NODE *pPrev = NULL;
        while (pNode)
        {
                PREFAST_ASSERT(pNode->pItem);
                CLSID uuidTrans = pNode->pItem->GetTransport();

                if (0==memcmp(&uuidTransport, &uuidTrans, sizeof(CLSID)))
                {
                        LPPROPERTYBAG2 pBag = NULL;

                        //if we cant get a properybag, something is up so just continue
                        if(FAILED(pNode->pItem->GetPropertyBag(&pBag)) || !pBag)
                        {
                                SVSUTIL_ASSERT(FALSE);
                                pPrev = pNode;
                                pNode = pNode->pNext;
                                continue;
                        }

                        BOOL fIsCorpse = FALSE;
                        VARIANT varCorpse;
                        VariantInit(&varCorpse);

                        if(SUCCEEDED(pBag->Read(L"Corpse", &varCorpse, NULL)))
                        {
                                if(varCorpse.lVal == 1)
                                        fIsCorpse = 1;
                                VariantClear(&varCorpse);
                        }

                        //if this bag hasnt timed out AND its not a corpse then its good
                        //  so skip it
                        if(pNode->pItem->GetPresent() && !fIsCorpse)
                        {
                                pBag->Release();
                                pPrev = pNode;
                                pNode = pNode->pNext;
                                continue;
                        }


                        // unchain the node from list of active devices
                        if (!pPrev)
                                pListHead = pNode->pNext;
                        else
                                pPrev->pNext = pNode->pNext;

                        DEVICE_NODE *pCurrent = pNode->pNext;

                        //create a new node and chain to removal list
                        DEVICE_PROPBAG_LIST *pNew = new DEVICE_PROPBAG_LIST();
                        if( !pNew )
                                return E_OUTOFMEMORY;

                        pNew->pNext = pDevicesToRemove;
                        pNew->pBag = pBag;
                        pDevicesToRemove = pNew;

                        //clean up
                        pBag = NULL;
                        pNode->pItem->Release();
                        delete pNode;

                        if (FAILED(hr))
                                break;

                        pNode = pCurrent;
                }
                else
                {
                        pPrev = pNode;
                        pNode = pNode->pNext;
                }
        }

        *ppDevicesToRemove = pDevicesToRemove;
        return hr;
}
