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
//
// Obex.cpp: implementation of the CObex class.
//
//////////////////////////////////////////////////////////////////////
#include "common.h"
#include "ObexDevice.h"
#include "CObex.h"
#include "DeviceEnum.h"
#include "conpoint.h"
#include "ObexIRDATransport.h"
#include "ObexBTHTransport.h"
#include "PropertyBag.h"
#include "PropertyBagEnum.h"
#include "HeaderEnum.h"
#include "HeaderCollection.h"
#include "ObexTCPTransport.h"
#include "ObexTransportConnection.h"
#include "ObexStream.h"
#include <olectl.h>


#include "ObexStrings.h"

extern UINT g_uiIRDAMaxRetries;

#if defined(DEBUG) || defined(_DEBUG)

    DWORD dwData = 'OBEX';

    void *operator new(UINT size)
    {
         return g_funcAlloc(size, &dwData);
    }

    void operator delete(void *pVoid)
    {
        if (pVoid)
            g_funcFree(pVoid, &dwData);
    }
#endif


/*----------globals----------------*/

    //g_TransportSocketCS locks the internal structures of the
    //  transport socket (for example the linked list of
    //  transport connections)
    CRITICAL_SECTION g_TransportSocketCS;
    CRITICAL_SECTION g_PropBagCS;


CSynch             *gpSynch = NULL;
CObex              *g_pObex = NULL;
DWORD                   g_dwObexCaps = 0;
UINT                g_uiMaxFileChunk;

int                 g_iCredits = OBEX_INQUIRY_FAILURES;

const LPCTSTR k_szRegTransportKey         = TEXT("Software\\Microsoft\\Obex\\Transports");
const LPCTSTR k_szRegObex                   = TEXT("Software\\Microsoft\\Obex");


BOOL setKeyAndValue(const WCHAR * pszPath, const WCHAR * szSubkey, const WCHAR * szValue);
BOOL setValueInKey(const WCHAR * szKey, const WCHAR * szNamedValue, const WCHAR * szValue);
HRESULT CLSIDtochar(REFCLSID clsid, WCHAR * szCLSID, unsigned int length);
LONG recursiveDeleteKey(HKEY hKeyParent, const WCHAR * szKeyChild);

HRESULT RegisterServer(const WCHAR * szModuleName,      // DLL module handle
                       REFCLSID clsid,                  // Class ID
                       const WCHAR * szFriendlyName,    // Friendly Name
                       const WCHAR * szVerIndProgID,    // Programmatic
                       const WCHAR * szProgID,          // IDs
                       const WCHAR * szThreadingModel); // ThreadingModel

LONG UnregisterServer(REFCLSID clsid,                  // Class ID
                      const WCHAR * szVerIndProgID,    // Programmatic
                      const WCHAR * szProgID);         // IDs

const int CLSID_STRING_SIZE = 39;


static CObexIRDATransport *g_pIRDATransport = 0;
static CObexBTHTransport  *g_pBTHTransport = 0;

long g_cComponents = 0;
long g_cServerLocks = 0;



BOOL GetLock()
{
    PREFAST_ASSERT(gpSynch);

    SVSUTIL_ASSERT(!gpSynch->IsLocked());
    gpSynch->Lock();
    if ((!g_pObex) || (!g_pObex->IsInitialized()))
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"GetLock::No valid IObex interface\n"));
        gpSynch->Unlock();
        return FALSE;
    }

    return TRUE;
}

void ReleaseLock()
{
    SVSUTIL_ASSERT(gpSynch);
    SVSUTIL_ASSERT(gpSynch->IsLocked());
    gpSynch->Unlock();
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CObex::CObex() : _refCount(1)
{
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::CObex()\n"));
    InterlockedIncrement (&g_cComponents);
    _bStopEnum = FALSE;
    _pDevices = NULL;
    _pConnPt = NULL;
    _stage = Stopped;
    _fPause = FALSE;
    _pActiveTransports = NULL;

    //enumeration thead
    _uiRunningENumThreadCnt = 0; //<-- num of STILL running enum threads
    _uiEnumThreadCnt = 0;        //<--total enum threads (some might have exited)
    _hEnumPauseEvent = NULL;
    _hEnumPauseEvent_UnlockPauseFunction = NULL;

    //number of times enumeration has been requested
    _uiDeviceEnumCnt = 0;

    //number of times we've been inited
    _uiInitCnt = 0;
}

CObex::~CObex()
{
    SVSUTIL_ASSERT(_refCount == 0);
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::~CObex()\n"));

    PREFAST_ASSERT(gpSynch);
    SVSUTIL_ASSERT(NULL == _pActiveTransports);

    gpSynch->Lock();

    if (g_pBTHTransport)
    {
        g_pBTHTransport->Release();
        g_pBTHTransport = NULL;
    }

    if (g_pIRDATransport)
    {
        g_pIRDATransport->Release();
        g_pIRDATransport = NULL;
    }

    if (_pDevices)
    {
        _pDevices->Release();
        _pDevices = NULL;
    }

    g_pObex = NULL;


    gpSynch->Unlock();

    InterlockedDecrement (&g_cComponents);
}


HRESULT STDMETHODCALLTYPE
CObex::SetCaps(DWORD dwCaps)
{
       g_dwObexCaps = dwCaps;
        return S_OK;
}


HRESULT STDMETHODCALLTYPE
CObex::Initialize()
{
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::Initialize()\n"));
    HRESULT hr = E_FAIL;
    SVSUTIL_ASSERT(!gpSynch->IsLocked());
    gpSynch->Lock();

    if (_stage == ShuttingDown)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::Initialize FAILS....ShuttingDown()\n"));
        return E_FAIL; //<--dont goto Done since that will clean up memory!
    }

    if (_stage == Initialized)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::Initialize....Already Initialized\n"));

        //inc the initialization ref count
        _uiInitCnt++;
        hr = S_OK;
        goto Done;
    }

    // load the # of times to retry IRDA from the registry
    g_uiIRDAMaxRetries = 3;

    HKEY hk;
    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"software\\microsoft\\obex", 0, KEY_READ, &hk)) {
        DWORD dw = 0;
        DWORD dwType = REG_DWORD;
        DWORD dwSize = sizeof(dw);
        if (ERROR_SUCCESS == RegQueryValueEx (hk, L"IRDARetries", NULL, &dwType, (LPBYTE)&dw, &dwSize)) {
            if ((dwType == REG_DWORD) && (dwSize == sizeof(dw)) && (dw > 0) && (((int)dw) > 0)) {
                g_uiIRDAMaxRetries = (UINT)dw;
            }
        }
        RegCloseKey (hk);
    }

    ASSERT(NULL == _hEnumPauseEvent);
    if(NULL == (_hEnumPauseEvent = CreateEvent(NULL, TRUE, TRUE, NULL))) {
        hr = E_FAIL;
        goto Done;
    }

    ASSERT(NULL == _hEnumPauseEvent_UnlockPauseFunction);
    if(NULL == (_hEnumPauseEvent_UnlockPauseFunction = CreateEvent(NULL, FALSE, FALSE, NULL))) {
        hr = E_FAIL;
        goto Done;
    }

    SVSUTIL_ASSERT(_pConnPt == NULL);
    if (NULL == (_pConnPt = new CConnectionPoint((IConnectionPointContainer *)this)))
    {
        hr = E_OUTOFMEMORY;
        goto Done;
    }

    SVSUTIL_ASSERT(_uiEnumThreadCnt ==  NULL);
    SVSUTIL_ASSERT(_bStopEnum == FALSE);

    //inc the initialization ref count, set stage
    _stage = Initialized;
    _uiInitCnt++;

    hr = S_OK;
    Done:
        if(FAILED(hr)) {
            if(_pConnPt)
                _pConnPt->Release();
            if(_hEnumPauseEvent)
                CloseHandle(_hEnumPauseEvent);
            if(_hEnumPauseEvent_UnlockPauseFunction)
                CloseHandle(_hEnumPauseEvent_UnlockPauseFunction);
            _pConnPt = NULL;
            _hEnumPauseEvent = NULL;
        }
        //unlock and return
        SVSUTIL_ASSERT(gpSynch->IsLocked());
        gpSynch->Unlock();
        return hr;
}

HRESULT STDMETHODCALLTYPE
CObex::Shutdown()
{
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::Shutdown()\n"));

        //make sure we are not paused
    PauseDeviceEnum(FALSE);

    gpSynch->Lock();

    if (_stage != Initialized)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::Shutdown:ERROR_SERVICE_NOT_ACTIVE()\n"));
        gpSynch->Unlock();
        return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WINDOWS, ERROR_SERVICE_NOT_ACTIVE);
    }

    //dec the increment ref count
    _uiInitCnt --;

    //if we are still referenced, dont delete!  just return OK
    if(_uiInitCnt)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::Shutdown: initilization refcount(%d)\n", _uiInitCnt));
        gpSynch->Unlock();
        return S_OK;
    }

    //if we are no longer referenced, start the shutdown process
    _stage = ShuttingDown;

    if (_uiEnumThreadCnt)
    {
        _bStopEnum = TRUE;

        gpSynch->Unlock();

        //
        //wait for all threads to exit (they get the _bStopEnum flag and quit)
        //
        for(UINT i=0; i<_uiEnumThreadCnt; i++)
        {
            DWORD waitRet = WaitForSingleObject(_hEnumThreadArray[i], INFINITE);
            CloseHandle(_hEnumThreadArray[i]);
            SVSUTIL_ASSERT(WAIT_FAILED != waitRet);

            if(WAIT_FAILED == waitRet)
            {
                DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::WaitForMultipleObject on thread failed: %x\n", GetLastError()));
                return E_FAIL;
            }
            else
            {
                DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::WaitForMultipleObject thread %d down\n", i));
            }
        }


        gpSynch->Lock();

        _uiEnumThreadCnt = 0;
    }

        //kill the pause event (think of a cattle gate)
    ASSERT(_hEnumPauseEvent);
    if(_hEnumPauseEvent) {
        CloseHandle(_hEnumPauseEvent);
        _hEnumPauseEvent = NULL;
    }
    ASSERT(_hEnumPauseEvent_UnlockPauseFunction);
    if(_hEnumPauseEvent_UnlockPauseFunction) {
        CloseHandle(_hEnumPauseEvent_UnlockPauseFunction);
        _hEnumPauseEvent_UnlockPauseFunction = NULL;
    }

    PREFAST_ASSERT(_pConnPt);

    _pConnPt->ContainerReleased();

    ((IConnectionPoint*)_pConnPt)->Release();
    _pConnPt = NULL;

    if (_pDevices)
    {
        _pDevices->Release();
        _pDevices = NULL;
    }


    _stage = Stopped;

    gpSynch->Unlock();
    return S_OK;
}


HRESULT
CObex::IsCorpse(DEVICE_PROPBAG_LIST *pDeviceCorpses, LPPROPERTYBAG2 pBag, BOOL *isCorpse)
{
    //
    //  See if this device has already been found not to have OBEX
    //
    HRESULT hr;
    DEVICE_PROPBAG_LIST *pTemp = pDeviceCorpses;
    VARIANT varDev1Address;
    VARIANT varDev2Address;

    VariantInit(&varDev1Address);
    VariantInit(&varDev2Address);

    hr = pBag->Read(c_szDevicePropAddress, &varDev1Address, NULL);

    if(FAILED(hr))
    {
        return hr;
    }

    BOOL bHasBeenCorpsed = FALSE;
    while(pTemp)
    {
        PREFAST_ASSERT(pTemp->pBag);

        if(FAILED((hr = pTemp->pBag->Read(c_szDevicePropAddress, &varDev2Address, NULL))))
        {
            VariantClear(&varDev1Address);
            return hr;
        }


        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumIndividualDevicesThread -- checking for corpse!\n"));
        if(varDev1Address.vt == varDev2Address.vt && varDev1Address.vt == VT_BSTR)
        {
           DEBUGMSG(OBEX_COBEX_ZONE,(L"    Checking: %s to %s\n", varDev1Address.bstrVal, varDev2Address.bstrVal));
        }

        bHasBeenCorpsed =
            (
              ((varDev1Address.vt == VT_I4) && (varDev1Address.vt == varDev2Address.vt) && (varDev1Address.lVal == varDev2Address.lVal)) ||
              ((varDev1Address.vt == VT_BSTR) && (varDev1Address.vt == varDev2Address.vt) && (0 == wcscmp(varDev1Address.bstrVal, varDev2Address.bstrVal)))
            );

        DEBUGMSG(OBEX_COBEX_ZONE,(L"    %s\n", bHasBeenCorpsed ? L"CORPSE" : L"NOT CORPSE"));


        VariantClear(&varDev2Address);

        //if a corpse has been found, skip this device
        if(bHasBeenCorpsed)
        {
            DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumIndividualDevicesThread - Corpse found for device -- skipping!\n"));
             break;
        }

        pTemp = pTemp->pNext;
    }

    VariantClear(&varDev1Address);
    *isCorpse = bHasBeenCorpsed;
    return S_OK;
}



HRESULT
CObex::UpdateDevices(IObexTransport *pTransport, CLSID *pclsid, DEVICE_LIST *pMatchedDeviceList)
{
    CObexDevice *pObexDevice = NULL;
    HRESULT hRes = S_OK;

    while(pMatchedDeviceList)
    {
        SVSUTIL_ASSERT(gpSynch->IsLocked());

        DEVICE_LIST *pDel = pMatchedDeviceList;
        pObexDevice = pMatchedDeviceList->pDevice;
        pMatchedDeviceList = pMatchedDeviceList->pNext;
        delete pDel;

        PREFAST_ASSERT(pObexDevice);

        //because we are here, this device still exists
        pObexDevice->SetPresent(TRUE);

        IPropertyBag *pDeviceBag = pObexDevice->GetPropertyBag();

        VARIANT varCorpse;
        VariantInit(&varCorpse);
        BOOL fIsCorpse = FALSE;

        if(SUCCEEDED(pDeviceBag->Read(L"Corpse", &varCorpse, NULL)))
        {
            if(varCorpse.lVal == 1)
                fIsCorpse = TRUE;
            VariantClear(&varCorpse);
        }

        // finish updating the device
        //  (but only if it needs updating...)
        if(!fIsCorpse && 0xFFFFFFFF != pObexDevice->GetUpdateStatus())
        {
            DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumIndividualDevicesThread - finishing update \n"));

            HRESULT hr;
            SVSUTIL_ASSERT(pDeviceBag);

            //go update the devices properties.. this will go out
            //  and preform the (possible) time consuming chore
            //  of figuring out if OBEX is supported on the device
            //  as well as build the human name


            IPropertyBagEnum *pNewBagEnum = NULL;

            UINT uiUpdateStatus;
            UINT uiPrevStatus = pObexDevice->GetUpdateStatus();

            ReleaseLock();
            hr = pTransport->UpdateDeviceProperties(pDeviceBag, &pNewBagEnum, FALSE, &uiUpdateStatus);

            if (! GetLock ())
            {
                hRes = E_FAIL;
                pObexDevice->Release();
                goto done;
            }

            //if we succeeded, progress has been made (or could have been)
            //  so notify the client and possibly SetModified
            if(SUCCEEDED(hr))
            {
                if(S_OK != hr)
                    pObexDevice->SetUpdateStatus(uiUpdateStatus);
                else
                    pObexDevice->SetUpdateStatus(0xFFFFFFFF);

                //special case this to maintain bkwrds compat with ppc2002
                if(!(g_dwObexCaps & SEND_DEVICE_UPDATES))
                                {
                                        if(uiUpdateStatus != uiPrevStatus && (S_OK == hr))
                                        {
                                                DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumIndividualDevicesThread - sending update message - status (before/after) = (0x%x/0x%x)\n", uiUpdateStatus, uiPrevStatus));

                                                pObexDevice->SetModified(TRUE);

                                                if (!g_pObex->_bStopEnum)
                                                        g_pObex->_pConnPt->Notify(OE_DEVICE_ARRIVAL, pDeviceBag, NULL);
                                        }
                                        else if(uiUpdateStatus != uiPrevStatus && S_OK == hr)
                                        {
                                                pObexDevice->SetModified(TRUE);
                                        }
                                        else
                                        {
                                                pObexDevice->SetModified(FALSE);
                                        }
                                }
                                else
                                {
                                        if(uiUpdateStatus != uiPrevStatus)
                                        {
                                                DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumIndividualDevicesThread - sending update message - status (before/after) = (0x%x/0x%x)\n", uiUpdateStatus, uiPrevStatus));

                                                pObexDevice->SetModified(TRUE);

                                                if (!g_pObex->_bStopEnum)
                                                        g_pObex->_pConnPt->Notify(OE_DEVICE_UPDATE, pDeviceBag, NULL);
                                        }
                                        else
                                        {
                                                pObexDevice->SetModified(FALSE);
                                        }
                                }

            }


            //if we have failed, or if we havent been modified in a while
            //  corpse the device
            if(E_ABORT != hr && (FAILED(hr) || (!(uiUpdateStatus & CAN_CONNECT) && !pObexDevice->GetModified())))
            {
                DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumIndividualDevicesThread - device doesnt support OBEX -- making it a corpse\n"));

                //if an error occured, mark the device as a corpse in its bag
                VARIANT varCorpseWrite;
                VariantInit(&varCorpseWrite);
                varCorpseWrite.vt = VT_I4;
                varCorpseWrite.lVal = 1;

                hr = pDeviceBag->Write(L"Corpse", &varCorpseWrite);
                   SVSUTIL_ASSERT(SUCCEEDED(hr));
                VariantClear(&varCorpseWrite);

                pObexDevice->Release();
                pObexDevice=NULL;
                break;
            }



            //
            //  If devices were discovered while updating a previous device
            //      they will be cloned and will have information.
            //        so place them into the list of devices and send
            //        a NOTIFY to any Sink
            //
            if(pNewBagEnum)
            {
                pNewBagEnum->Reset();

                ULONG ulFetched = 0;
                LPPROPERTYBAG2 pNewDeviceBag = NULL;

                while(SUCCEEDED(pNewBagEnum->Next(1, &pNewDeviceBag, &ulFetched)) && ulFetched)
                {
                    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumIndividualDevicesThread - New device found during Update of prev found device!!\n"));

                    //create a new device with our clsid and propertybag (this marks it
                    //  updated and present)
                    CObexDevice *pNewDevice = new CObexDevice(pNewDeviceBag, *pclsid);
                    if (!pNewDevice)
                    {
                        pNewBagEnum->Release();
                        pObexDevice->Release();
                        hRes = E_FAIL;
                        goto done;
                    }

                    //insert it into the device list
                    hr = g_pObex->_pDevices->Insert(pNewDevice);
                    SVSUTIL_ASSERT(SUCCEEDED(hr));

                    //if all went okay, notify any connection points
                                        //  <-- dont send arrival here on PPC2002
                    if ((g_dwObexCaps & SEND_DEVICE_UPDATES) && SUCCEEDED(hr))
                    {
                        if (!g_pObex->_bStopEnum)
                            g_pObex->_pConnPt->Notify(OE_DEVICE_ARRIVAL, pNewDeviceBag, NULL);
                    }

                    pNewDeviceBag->Release();
                    pNewDevice->Release();
                    pNewDevice = NULL;
                }

                pNewBagEnum->Release();
                pNewBagEnum = NULL;
            }
        }
        else if(!fIsCorpse)
        {
            SVSUTIL_ASSERT(pObexDevice);
            pObexDevice->SetModified(TRUE);
        }

        pObexDevice->Release();
        pObexDevice=NULL;
    }


done:

    // safety net -- clean up memory (only will happen on bad error)
    while(pMatchedDeviceList)
    {
        DEVICE_LIST *pDel = pMatchedDeviceList;
        pMatchedDeviceList->pDevice->Release();
        pMatchedDeviceList = pMatchedDeviceList->pNext;
        delete pDel;
    }

    return hRes;
}

HRESULT
CObex::PauseEnumIfRequired()
{
        HRESULT hr = E_FAIL;
        SVSUTIL_ASSERT(!gpSynch->IsLocked());
        //
        // Handle pausing
        //    this is done by first getting the lock, then if we were
        //    told to pause, we decrement the pause thread count (the number
        //    of instances of EnumIndividualDeviceThread running) and then
        //    if 0 we wake up the initiating Pause() function -- then
        //    we wait for a pauseEvent
        if (!GetLock())
            goto Done;

            if(TRUE == g_pObex->_fPause) {
                g_pObex->_uiRunningENumThreadCnt --;
                if(0==g_pObex->_uiRunningENumThreadCnt)
                    SetEvent(g_pObex->_hEnumPauseEvent_UnlockPauseFunction);
            }
        ReleaseLock();

        //note: this is a manual reset event!!
        if(WAIT_FAILED == WaitForSingleObject(g_pObex->_hEnumPauseEvent, INFINITE)) {
            ASSERT(FALSE);
            goto Done;
        }

        hr = S_OK;
        Done:
            return hr;
}


//
//  Note: because we monitor the number of instances of this
//     thread that are running (via _uiRunningEnumThreadCnt)
//     on *EVERY* exit (return) we have to first decrement
//     the _uiRunninEnumThreadCnt (while under the main lock -- this is important)
//     THEN once we have left that lock we MUST call PauseEnumIfRequired
//     so that we wont race
//
DWORD WINAPI
CObex::EnumIndividualDeviceThread(LPVOID lpvParam)
{
    PREFAST_ASSERT(g_pObex);

    IObexTransport *pTransport = NULL;

    ULONG ulFetched = 0;

    HRESULT hr = E_FAIL;
    CLSID clsid;
    VARIANT var;
    BOOL bOK;

    LPPROPERTYBAG2 pBag = NULL;
    DEVICE_PROPBAG_LIST *pDeviceCorpses = NULL;
    IObexAbortTransportEnumeration *pAbortEnum = NULL;

    //we are GIVEN the propery bag for the device, DO NOT ref count it!
    LPPROPERTYBAG2 pPropBag = (LPPROPERTYBAG2) lpvParam;
    PREFAST_ASSERT(pPropBag);

    //init the COM library
    if (FAILED(CoInitializeEx(NULL,COINIT_MULTITHREADED)))
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumDevicesThread - CANT CoInitializeEx\n"));
        g_pObex->Release();

        //
        //  Because we are leaving the thread, the # of running threads
        //     must be decremented--- and because this has to be dec'ed
        //     we must call PauseEnumIfRequired
        if (!GetLock())
                goto done;
            //decrement the # of threads running
            g_pObex->_uiRunningENumThreadCnt --;
        ReleaseLock();
        g_pObex->PauseEnumIfRequired();
        return static_cast<DWORD>(E_FAIL);
    }


     //fetch the GUID from the prop bag, if it doesnt exist, quit
    VariantInit(&var);

    bOK = ( (SUCCEEDED(pPropBag->Read(L"GUID", &var, 0))) && (SUCCEEDED(CLSIDFromString(var.bstrVal, &clsid))) );
    if (!bOK)
        goto done;

    VariantClear(&var);

    //get a  transport (for seeing what devices are up)
    hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, IID_IObexTransport,
        (void **)&pTransport);

    if ( (FAILED(hr)) || (!pTransport) )
    {
        pTransport = NULL;
        goto done;
    }

    // make sure the transport is not in the abort state
    if(SUCCEEDED(pTransport->QueryInterface(IID_IObexAbortTransportEnumeration, (LPVOID *)&pAbortEnum))) {
         pAbortEnum->Resume();
         pAbortEnum->Release();
    }


    if(FAILED(g_pObex->AddActiveTransport(pTransport))) {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumIndividualDevicesThread - Cant add active transport to list! -- can continue w/o this but enumeration stopping may be slow\n"));
    }

    for(;;)
    {
        SVSUTIL_ASSERT(pTransport && g_pObex && g_pObex->_pDevices);

        // pause enumeration if required
        if(FAILED(g_pObex->PauseEnumIfRequired()))
            goto done;

        //if we are told to stop... do so
        if (!GetLock())
            goto done;
        if (g_pObex->_bStopEnum)
        {
            ReleaseLock();
            goto done;
        }

        //start an enumeration run for all devices that are of the
        //  transport from 'clsid'... this mean the SetPresent flag is
        //  set false -- thus the credits go down by 1.  if they go to
        //  0 the device is deleted (well, marked as a corpse)
        g_pObex->_pDevices->StartEnumRun(clsid);

        ReleaseLock();

        //
        //get all the devices on a particular transport (pTransport)
        //
        LPPROPERTYBAGENUM pPropBagEnum = NULL;
        if( (SUCCEEDED(pTransport->EnumDevices(&pPropBagEnum))) && (pPropBagEnum))
        {
            //
            //  quick check to see if we've been stopped.
            //
            if (g_pObex->_bStopEnum)
            {
                pPropBagEnum->Release();
                goto done;
            }

            //look through each discoverd device, creating devices and inserting them into the
            //  device enumerator
            ulFetched = 0;
            pPropBagEnum->Reset();

            //
            //  Loop through all devices discovered, find devices that might match
            //    (-- if they share addresses) and then figure out if multiple 'devices'
            //      live under that one device (1 BT device + multiple SDP records)
            while(SUCCEEDED(pPropBagEnum->Next(1, &pBag, &ulFetched) ) && ulFetched == 1)
            {
                if (! GetLock ())
                {
                    pBag->Release();
                    pBag = NULL;
                    pPropBagEnum->Release();
                    goto done;
                }

                //quick check to see if we've been stopped.
                if (g_pObex->_bStopEnum)
                {
                    pBag->Release();
                    pBag = NULL;
                    pPropBagEnum->Release();
                    ReleaseLock();
                    goto done;
                }


                //
                //  Check to see if this device has been found to be a corpse
                //     if it has,  skip it
                //
                BOOL fIsCorpse = FALSE;
                if(FAILED(g_pObex->IsCorpse(pDeviceCorpses,pBag,&fIsCorpse)))
                {
                    ReleaseLock();
                    pBag->Release();
                    pBag = NULL;
                    pPropBagEnum->Release();
                    goto done;
                }
                else if(fIsCorpse)
                {
                    SVSUTIL_ASSERT(gpSynch->IsLocked());
                    ulFetched = 0;
                    ReleaseLock();
                    pBag->Release();
                    pBag = NULL;
                    continue;
                }

                //
                //   Hunt out the obex device with the passed in property bag
                //      if it exists, set its present flag
                DEVICE_LIST *pMatchedDeviceList = NULL;

                hr = g_pObex->_pDevices->FindDevicesThatMatch (pBag, &pMatchedDeviceList);

                if(FAILED(hr))
                {
                    ReleaseLock();
                    pBag->Release();
                    pBag = NULL;
                    pPropBagEnum->Release();
                    goto done;
                }

                //
                //  If there are no devices, this is a new one, so insert it
                //
                if(NULL == pMatchedDeviceList)
                {
                    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumIndividualDevicesThread - New device found!\n"));

                    //create a new device with our clsid and propertybag
                    // NOTE: creating the device effectivly calles SetModified
                    CObexDevice *pObexDevice = new CObexDevice(pBag, clsid);

                    if (!pObexDevice)
                    {
                        ReleaseLock();
                        pBag->Release();
                        pBag = NULL;
                        pPropBagEnum->Release();
                        goto done;
                    }

                    //insert it into the device list
                    hr = g_pObex->_pDevices->Insert(pObexDevice);

                    //if all went okay, notify any connection points
                    if ((g_dwObexCaps & SEND_DEVICE_UPDATES) && SUCCEEDED(hr))
                    {
                        if (!g_pObex->_bStopEnum)
                            g_pObex->_pConnPt->Notify(OE_DEVICE_ARRIVAL, pBag, NULL);
                    }

                    pObexDevice->Release();
                    pObexDevice = NULL;
                }

                //
                //  Otherwise, there are multiple devices, so update them individually
                //
                else
                {
                    if(FAILED(g_pObex->UpdateDevices(pTransport, &clsid, pMatchedDeviceList)))
                    {
                        ReleaseLock();
                        pBag->Release();
                        pBag = NULL;
                        pPropBagEnum->Release();
                        goto done;
                    }

                }//must leave this if locked!



                SVSUTIL_ASSERT(gpSynch->IsLocked());

                ulFetched = 0;
                ReleaseLock();

                pBag->Release();
                pBag = NULL;
            }
            SVSUTIL_ASSERT(!pBag);

            //end of while
            pPropBagEnum->Release();
            pPropBagEnum = NULL;
        }
        else
        {
            DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumDevicesThread - EnumDevices failed for transport %s...()\n", var.bstrVal));
            goto done;
        }
        SVSUTIL_ASSERT(!pPropBagEnum);


        if (!GetLock())
            goto done;

        //finish the enumeration run by building a list of all devices
        //  that have been removed (as determined by the Present flag)
        DEVICE_PROPBAG_LIST *pDevicesToRemove = NULL;
        hr = g_pObex->_pDevices->FinishEnumRun(&pDevicesToRemove, clsid);

        //now, iterate throuh the removed devices notifying the connection
        //  point of their departure.
        if (SUCCEEDED(hr) && pDevicesToRemove)
        {
            SVSUTIL_ASSERT(gpSynch->IsLocked());

            //loop through all devices to be removed and notify the connectionpts
            while(pDevicesToRemove)
            {
                SVSUTIL_ASSERT(pDevicesToRemove->pBag);
                g_pObex->_pConnPt->Notify(OE_DEVICE_DEPARTURE, pDevicesToRemove->pBag, NULL);

                //if an error occured, mark the device as a corpse in its bag
                VARIANT varCorpse;
                VariantInit(&varCorpse);

                hr = pDevicesToRemove->pBag->Read(L"Corpse", &varCorpse, NULL);

                //if the device isnt a corpse, delete it & move on
                if(FAILED(hr) || VT_I4 != varCorpse.vt || 1 != varCorpse.lVal)
                {
                    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumDevicesThread - found device to remove (it walked off)... but not corpse\n"));
                    DEVICE_PROPBAG_LIST *pDel = pDevicesToRemove;
                    pDevicesToRemove->pBag->Release();
                    pDevicesToRemove = pDevicesToRemove->pNext;
                    delete pDel;
                }
                else
                {
                    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumDevicesThread - found device to remove (its a corpse)\n"));
                    DEVICE_PROPBAG_LIST *pPBTemp = pDevicesToRemove->pNext;
                    pDevicesToRemove->pNext = pDeviceCorpses;
                    pDeviceCorpses = pDevicesToRemove;
                    pDevicesToRemove = pPBTemp;
                }

                VariantClear(&varCorpse);
            }
        }

        ReleaseLock();
    }

done:
    SVSUTIL_ASSERT(!gpSynch->IsLocked());

    //
    //  Clean our corpse list (devices that dont support obex but still get discovered
    //    but the transport enumerator)
    //
    while(pDeviceCorpses)
    {
        DEVICE_PROPBAG_LIST *pDel = pDeviceCorpses;
        pDeviceCorpses = pDeviceCorpses->pNext;

        pDel->pBag->Release();
        delete pDel;
    }

    // remove this transport from the active list (there is no recovery for error)
    g_pObex->RemoveActiveTransport(pTransport);

    if (pPropBag)
        pPropBag->Release();
    if (pTransport)
        pTransport->Release();

    SVSUTIL_ASSERT(g_pObex && !pBag);

    //
    //  Because we are leaving the thread, the # of running threads
    //     must be decremented--- and because this has to be dec'ed
    //     we must call PauseEnumIfRequired

    gpSynch->Lock();
    //decrement the # of threads running
    g_pObex->_uiRunningENumThreadCnt --;
    gpSynch->Unlock();
    g_pObex->PauseEnumIfRequired();

    //close up shop
    if(g_pObex)
       g_pObex->Release();
    CoUninitialize();
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumDevicesThread - FINISHED -- going away\n"));
    return 0;
}




DWORD
//note: we are already under gpSynch->Lock()!
CObex::StartEnumeratingDevices()
{
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumDevices()\n"));

    LPPROPERTYBAGENUM pTransportEnum = NULL;
    LPPROPERTYBAG2 pPropBag = NULL;
    HKEY hk;
    HRESULT hr = E_FAIL;
    ULONG ulFetched;


    if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"software\\microsoft\\obex", 0, KEY_READ, &hk))
    {
        DWORD dw = 0;
        DWORD dwType = REG_DWORD;
        DWORD dwSize = sizeof(dw);
        if (ERROR_SUCCESS == RegQueryValueEx (hk, L"clientcredits", NULL, &dwType, (LPBYTE)&dw, &dwSize))
        {
            if ((dwType == REG_DWORD) && (dwSize == sizeof(dw)) && (dw > 0) && (((int)dw) > 0))
                g_iCredits = (int)dw;
        }
        RegCloseKey (hk);
    }

    if ((!g_pObex) || (!g_pObex->IsInitialized()))
        goto done;

    //get a property bag of transports
    hr = g_pObex->EnumTransports(&pTransportEnum);

    if((FAILED(hr)) || (0 == pTransportEnum))
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumDevicesThread - EnumTransports failed(err=0x%08x)()\n", hr));
        goto done;
    }


    //build a linked list of thread handles (and then copy them into an array)
    pTransportEnum->Reset();

    gpSynch->Lock();
     _uiEnumThreadCnt = 0;
     while((SUCCEEDED(pTransportEnum->Next(1, &pPropBag, &ulFetched)) && ulFetched))
     {        //note: we are passing off ownership of the pPropBag!!!!!
         SVSUTIL_ASSERT(g_pObex);
         g_pObex->AddRef();
        _hEnumThreadArray[_uiEnumThreadCnt] = CreateThread(NULL, 0, EnumIndividualDeviceThread, (void *)pPropBag, 0, 0);
        _uiEnumThreadCnt++;
        _uiRunningENumThreadCnt++;
     }
    gpSynch->Unlock();


done:
    if (pTransportEnum)
        pTransportEnum->Release();

    SVSUTIL_ASSERT(gpSynch);
    return 0;
}


HRESULT STDMETHODCALLTYPE
CObex::PauseDeviceEnum(BOOL fPauseOn)
{
    HRESULT hr = S_OK;
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::PauseDeviceEnum()\n"));
    if(fPauseOn)  {
        BOOL fBlock = FALSE;
        UINT uiPreviousThreadCnt;
        gpSynch->Lock();
            uiPreviousThreadCnt = _uiRunningENumThreadCnt;
            if(FALSE == this->_fPause) {
                ResetEvent(_hEnumPauseEvent);
                this->_fPause = TRUE;
                AbortActiveTransports(TRUE);
                if(0 != _uiRunningENumThreadCnt) //dont block if no threads are running!
                    fBlock = TRUE;
            }
        gpSynch->Unlock();

        if(fBlock) {
            if(WAIT_OBJECT_0 != WaitForSingleObject(_hEnumPauseEvent_UnlockPauseFunction, INFINITE))
            {
                hr = E_FAIL;
            }
            gpSynch->Lock();
                _uiRunningENumThreadCnt = uiPreviousThreadCnt;
            gpSynch->Unlock();
        }


    }
    else  {
        gpSynch->Lock();
            this->_fPause = FALSE;
        gpSynch->Unlock();
        SetEvent(_hEnumPauseEvent);
    }

    gpSynch->Lock();
    AbortActiveTransports(FALSE);
    gpSynch->Unlock();
    return hr;
}

HRESULT STDMETHODCALLTYPE
CObex::StartDeviceEnum()
{
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::StartDeviceEnum()\n"));
    gpSynch->Lock();
    if (_stage != Initialized)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::StartDeviceEnum - Not yet initialized...()\n"));
        gpSynch->Unlock();
        return OBEX_E_NOT_INITIALIZED;
    }

    while (_bStopEnum)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::StartDeviceEnum - In the process of stopping previous enumeration...()\n"));

        SVSUTIL_ASSERT(_uiEnumThreadCnt);

        //wait for all threads to exit
        gpSynch->Unlock();

        //
        //wait for all threads to exit (they get the _bStopEnum flag and quit)
        //
        for(UINT i=0; i<_uiEnumThreadCnt; i++)
        {
            DWORD waitRet = WaitForSingleObject(_hEnumThreadArray[i], INFINITE);
            CloseHandle(_hEnumThreadArray[i]);
            SVSUTIL_ASSERT(WAIT_FAILED != waitRet);

            if(WAIT_FAILED == waitRet)
            {
                DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::WaitForMultipleObject on thread failed: %x\n", GetLastError()));
                return E_FAIL;
            }
            else
            {
                DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::WaitForMultipleObject thread %d down\n", i));
            }
        }

        gpSynch->Lock();

        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::StartDeviceEnum - Previous enumeration stopped()\n"));

        if (_stage != Initialized)
        {
            DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::StartDeviceEnum - Device shutdown in the meanwhile...()\n"));
            gpSynch->Unlock();
            return E_FAIL;
        }
    }

    if (_uiEnumThreadCnt)
    {
        //inc the device ref count to the thread (to prevent removing it later)
        _uiDeviceEnumCnt ++;

        gpSynch->Unlock();
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::StartDeviceEnum - Enum already running...()\n"));
        return S_OK;
    }

    SVSUTIL_ASSERT(_uiEnumThreadCnt == 0);

    if (_pDevices)
    {
        _pDevices->Release();
        _pDevices = NULL;
    }

    _pDevices = new CDeviceEnum();
    if (!_pDevices)
    {
        gpSynch->Unlock();
        return E_OUTOFMEMORY;
    }
    _pDevices->Reset();

    //make sure we are not paused
    PauseDeviceEnum(FALSE);

    //crank up the device enumeration threads
    StartEnumeratingDevices();

    //inc the ref count to the thread (to prevent removing it later)
    _uiDeviceEnumCnt ++;

    gpSynch->Unlock();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CObex::StopDeviceEnum()
{
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::StopDeviceEnum()\n"));
    gpSynch->Lock();
    if (_stage != Initialized || !_uiDeviceEnumCnt)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::StopDeviceEnum - Not yet initialized...()\n"));
        gpSynch->Unlock();
        return OBEX_E_NOT_INITIALIZED;
    }

    //dec the ref count
    if(_uiDeviceEnumCnt)
        _uiDeviceEnumCnt--;

    if (_bStopEnum || !_uiEnumThreadCnt || _uiDeviceEnumCnt)
    {
        gpSynch->Unlock();
        return S_OK;
    }

    //set the stop enum flag
    _bStopEnum = TRUE;

    gpSynch->Unlock();

    //make sure we are not paused
    PauseDeviceEnum(FALSE);

    //abort any active connections
    gpSynch->Lock();
    AbortActiveTransports(TRUE);
    gpSynch->Unlock();

    //
    //wait for all threads to exit (they get the _bStopEnum flag and quit)
    //
    for(UINT i=0; i<_uiEnumThreadCnt; i++)
    {
        DWORD waitRet = WaitForSingleObject(_hEnumThreadArray[i], INFINITE);
        CloseHandle(_hEnumThreadArray[i]);
        SVSUTIL_ASSERT(WAIT_FAILED != waitRet);

        if(WAIT_FAILED == waitRet)
        {
            DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::WaitForMultipleObject on thread failed: %x\n", GetLastError()));
            return E_FAIL;
        }
        else
        {
            DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::WaitForMultipleObject thread %d down\n", i));
        }
    }

    //enter lock
    gpSynch->Lock();

    //make the thread array as empty
    _uiEnumThreadCnt = 0;
    _bStopEnum = FALSE;

    //exit lock and return
    gpSynch->Unlock();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CObex::EnumTransports(LPPROPERTYBAGENUM *ppTransportEnum)
{
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumTransports()\n"));
    HKEY hkey;
    DWORD dwRes;

    //bail if the property bag is null
    if (!ppTransportEnum)
        return E_INVALIDARG;

    gpSynch->Lock();

    //check to see if we are initialized
    if (_stage != Initialized)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::XXX - Not yet initialized...()\n"));
        gpSynch->Unlock();
        return OBEX_E_NOT_INITIALIZED;
    }

    *ppTransportEnum = 0;
    HRESULT hr = S_OK;

    CPropertyBagEnum *pPropBagEnum = new CPropertyBagEnum();
    if( !pPropBagEnum )
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }

    //open the key
    if ((ERROR_SUCCESS != (dwRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
        k_szRegTransportKey, 0, KEY_ENUMERATE_SUB_KEYS, &hkey)))) {
        hr = E_FAIL;
        goto done;
    }

    DWORD dwIndex = 0;
    WCHAR lpName[MAX_PATH];
    DWORD cbName;
    LONG lregRetVal;

    do
    {
        cbName = MAX_PATH;

        lregRetVal = RegEnumKeyEx(hkey,dwIndex,lpName,&cbName,NULL,NULL,NULL,NULL);

        if(ERROR_SUCCESS == lregRetVal)
        {
            CPropertyBag *pPropBag = new CPropertyBag();
             if( !pPropBag )
             {
                RegCloseKey(hkey);
                hr = E_OUTOFMEMORY;
                goto done;
             }
            VARIANT var;
            VariantInit(&var);
            var.vt = VT_BSTR;
            var.bstrVal = SysAllocString(lpName);
            pPropBag->Write(L"GUID", &var);
            VariantClear(&var);

            pPropBagEnum->Insert(pPropBag);
            pPropBag->Release();
            DEBUGMSG(OBEX_COBEX_ZONE, (L"Found transport: %s\n", lpName));
        }

        dwIndex ++;
    } while(ERROR_SUCCESS == lregRetVal);

    RegCloseKey(hkey);

done:
    *ppTransportEnum = pPropBagEnum;
    gpSynch->Unlock();
    return hr;
}


HRESULT STDMETHODCALLTYPE
CObex::EnumDevices(LPDEVICEENUM *ppDeviceEnum, REFCLSID uuidTransport)
{
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumDevices()\n"));
     if (!ppDeviceEnum)
        return E_INVALIDARG;
    *ppDeviceEnum = NULL;

    HRESULT hr = E_FAIL;

    gpSynch->Lock();

    if (_stage != Initialized)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumDevices - Not yet initialized...()\n"));
        gpSynch->Unlock();
        return OBEX_E_NOT_INITIALIZED;
    }

    if (_pDevices)
        hr = _pDevices->EnumDevices(ppDeviceEnum, uuidTransport);
    if ((SUCCEEDED(hr)) && (*ppDeviceEnum))
        (*ppDeviceEnum)->Reset();

    gpSynch->Unlock();

    return hr;
}

HRESULT STDMETHODCALLTYPE
CObex::BindToDevice(LPPROPERTYBAG pPropertyBag, LPDEVICE *ppDevice)
{
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::BindToDevice()\n"));
    if (!pPropertyBag || !ppDevice)
        return E_POINTER;
    *ppDevice = NULL;


    gpSynch->Lock();
    if (_stage != Initialized)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::BindToDevice - Not yet initialized...()\n"));
        gpSynch->Unlock();
        return OBEX_E_NOT_INITIALIZED;
    }
    gpSynch->Unlock();


    VARIANT var;
    VariantInit(&var);
    HRESULT hr = pPropertyBag->Read(c_szDevicePropTransport, &var, NULL);
    if ((FAILED(hr)) || (var.vt != VT_BSTR))
    {
        VariantClear(&var);
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::BindToDevice - invalid arg...()\n"));
        return E_INVALIDARG;
    }

    CLSID clsid;
    if ((SUCCEEDED(CLSIDFromString(var.bstrVal, &clsid))) &&
        ( (0 == memcmp(&clsid, &CLSID_BthTransport, sizeof(CLSID))) ||
          (0 == memcmp(&clsid, &CLSID_IpTransport, sizeof(CLSID))) ||
          (0 == memcmp(&clsid, &CLSID_IrdaTransport, sizeof(CLSID)))
        ))
    {
        VariantClear(&var);

        CObexDevice *pDevice = new CObexDevice(pPropertyBag, clsid);
        if (!pDevice)
        {
            DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::BindToDevice - out of memory...()\n"));
            return E_OUTOFMEMORY;
        }

        hr = pDevice->QueryInterface(IID_IObexDevice, (LPVOID *)ppDevice);

                //dont connect up if SEND_DEVICE_UPDATES is set... this is to make the
                //  obex client work with PPC2002
        if(SUCCEEDED(hr) && (g_dwObexCaps & SEND_DEVICE_UPDATES))
        {
            hr = pDevice->ConnectSocket();

            if(FAILED(hr))
            {
                DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumDevices - ConnectSocket failed...()\n"));
                (*ppDevice)->Release();
                *ppDevice = NULL;
            }
            else
            {
                DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumDevices - ConnectSocket SUCCEEDED...()\n"));
            }
        }
        pDevice->Release();
        return hr;
    }
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::BindToDevice - somethings messed up!  tried to connect to non BT/IRDA!\n"));
    VariantClear(&var);
    return E_FAIL;
}

HRESULT STDMETHODCALLTYPE
CObex::RegisterService(LPPROPERTYBAG pPropertyBag, LPSERVICE *ppService)
{
    return E_NOTIMPL;
}

/*
HRESULT STDMETHODCALLTYPE
CObex::RegisterServiceBlob(unsigned long ulSize, byte * pbData, LPSERVICE *ppService)
{
    return E_NOTIMPL;
}
*/

HRESULT STDMETHODCALLTYPE
CObex::EnumConnectionPoints(IEnumConnectionPoints **ppEnum)
{
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumConnectionPoints()\n"));
    if (!ppEnum)
        return E_POINTER;

    *ppEnum = NULL;
    HRESULT hr = E_FAIL;

    gpSynch->Lock();

    if (_stage != Initialized)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::EnumConnectionPoints - Not yet initialized...()\n"));
        gpSynch->Unlock();
        return E_FAIL;
    }

    if (_pConnPt)
    {
        CEnumConnectionPoints *pEnumPts = new CEnumConnectionPoints((IConnectionPoint *)_pConnPt);
        if (pEnumPts)
        {
            *ppEnum = (IEnumConnectionPoints *)pEnumPts;
            hr = S_OK;
        }
        else
            hr = E_OUTOFMEMORY;
    }

    gpSynch->Unlock();
    return hr;
}

HRESULT STDMETHODCALLTYPE
CObex::FindConnectionPoint(REFIID riid, IConnectionPoint **ppCP)
{
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::FindConnectionPoint()\n"));
    if (!ppCP)
        return E_POINTER;

    *ppCP = NULL;

    HRESULT hr = E_FAIL;

    gpSynch->Lock();

    if (_stage != Initialized)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::FindConnectionPoint - Not yet initialized...()\n"));
        gpSynch->Unlock();
        return E_FAIL;
    }

    if ( (riid == IID_IObexSink) && (_pConnPt) )
    {
        *ppCP = _pConnPt;
        _pConnPt->AddRef();
        hr = S_OK;
    }
    else
        hr = CONNECT_E_NOCONNECTION;

    gpSynch->Unlock();
    return hr;
}


HRESULT STDMETHODCALLTYPE
CObex::QueryInterface(REFIID riid, void** ppv)
{
    DEBUGMSG(OBEX_COBEX_ZONE,(L"CObex::QueryInterface()\n"));
       if(!ppv)
        return E_POINTER;
    if(riid == IID_IUnknown)
        *ppv = this;
    else if(riid == IID_IObex)
        *ppv = static_cast<IObex*>(this);
    else if(riid == IID_IObex2)
        *ppv = static_cast<IObex2*>(this);
    else if(riid == IID_IConnectionPointContainer)
        *ppv = static_cast<IConnectionPointContainer*>(this);
    else if(riid == IID_IObexCaps)
         *ppv = static_cast<IObexCaps *>(this);
    else
        return *ppv = 0, E_NOINTERFACE;

    AddRef();
    return S_OK;
}

HRESULT
CObex::AddActiveTransport(IObexTransport *pTransport)
{
    if(FALSE == GetLock())
       return E_UNEXPECTED;

    TRANSPORT_NODE *pNew;
    if(NULL == (pNew = new TRANSPORT_NODE())) {
        return E_OUTOFMEMORY;
    }
    pTransport->AddRef();
    pNew->pTransport = pTransport;
    pNew->pNext =_pActiveTransports;
    _pActiveTransports = pNew;

    ReleaseLock();
        return S_OK;
}

HRESULT
CObex::RemoveActiveTransport(IObexTransport *pTransport)
{
    TRANSPORT_NODE *pDel;
    TRANSPORT_NODE *pParent;
    HRESULT hr;

    if(FALSE == GetLock())
        return E_UNEXPECTED;

    if(NULL == _pActiveTransports) {
        hr = E_FAIL;
        goto Done;
    }

    pParent = NULL;
    pDel = _pActiveTransports;

    while (pDel && (pDel->pTransport != pTransport))
    {
        pParent = pDel;
        pDel = pDel->pNext;
    }

    if (pDel)
    {
        if (pParent)
            pParent->pNext = pDel->pNext;
        else
            _pActiveTransports = pDel->pNext;

        pDel->pTransport->Release();
        delete pDel;
        hr = S_OK;
    }
    else
        hr = E_FAIL;

    Done:
        ReleaseLock();
        ASSERT(SUCCEEDED(hr));
        return hr;
}

HRESULT
CObex::AbortActiveTransports(BOOL fAbort)
{
    TRANSPORT_NODE *pTemp = _pActiveTransports;
    ASSERT(TRUE == gpSynch->IsLocked());

    while(NULL != pTemp) {
        IObexAbortTransportEnumeration *pAbortEnum;

        if(SUCCEEDED(pTemp->pTransport->QueryInterface(IID_IObexAbortTransportEnumeration, (LPVOID *)&pAbortEnum))) {
            if(TRUE == fAbort) {
                pAbortEnum->Abort();
            } else {
                pAbortEnum->Resume();
            }
            pAbortEnum->Release();
        }
        pTemp = pTemp->pNext;
    }
    return S_OK;
}

ULONG STDMETHODCALLTYPE
CObex::AddRef()
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObex::AddRef()\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE
CObex::Release()
{
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount);
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CObex::Release(%d)\n", ret));
    if(!ret)
        delete this;
    return ret;
}


template<typename T> class CFactory : public IClassFactory
{
public:
    // IUnknown
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

    // IClassFactory
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *pUnknownOuter, REFIID riid, void** ppv);
    HRESULT STDMETHODCALLTYPE LockServer(BOOL bLock);

    CFactory() : m_cRef(1) {
        InterlockedIncrement(&g_cServerLocks);
    }
    ~CFactory() {
        InterlockedDecrement(&g_cServerLocks);
    }

    static HRESULT GetClassObject(REFIID riid, void** ppv) {
        if(!ppv) return E_POINTER;

        CFactory<T>* pFactory = new CFactory<T>;

        if(!pFactory)
            return E_OUTOFMEMORY;

        HRESULT hr = pFactory->QueryInterface(riid, ppv);
        pFactory->Release();
        return hr;
    }

private:
    LONG m_cRef;
};



template<typename T> ULONG STDMETHODCALLTYPE CFactory<T>::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}


template<typename T> ULONG STDMETHODCALLTYPE CFactory<T>::Release()
{
    SVSUTIL_ASSERT(m_cRef != 0xFFFFFFFF);
    unsigned cRef = InterlockedDecrement(&m_cRef);
    if(cRef != 0)
        return cRef;
    delete this;
    return 0;
}


template<typename T> HRESULT STDMETHODCALLTYPE CFactory<T>::QueryInterface(REFIID riid, void** ppv)
{
    if(riid == IID_IUnknown || riid == IID_IClassFactory)
    {
        *ppv = (IClassFactory *)this;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}


HRESULT STDMETHODCALLTYPE
CFactory<CObex>::CreateInstance(IUnknown *pUnknownOuter, REFIID riid, void** ppv)
{
    if(pUnknownOuter != NULL)
        return CLASS_E_NOAGGREGATION;

    if (!ppv)
        return E_POINTER;
    *ppv = NULL;

    HRESULT hr;
    PREFAST_ASSERT(gpSynch);
    gpSynch->Lock();

    if (!g_pObex)
    {
        SVSUTIL_ASSERT(!g_pIRDATransport && !g_pBTHTransport); //their lifetimes depend on g_pObex

        g_pIRDATransport = new CObexIRDATransport();
        g_pBTHTransport = new CObexBTHTransport();
        g_pObex = new CObex;

        if (!g_pIRDATransport || !g_pBTHTransport || !g_pObex)
            goto done;

    }
    else
        g_pObex->AddRef();

    hr = g_pObex->QueryInterface(riid, ppv);
    g_pObex->Release();
    gpSynch->Unlock();
    return hr;

done:
    if (g_pObex)
    {
        g_pObex->Release();
        g_pObex = NULL;
    }
    if (g_pBTHTransport)
    {
        g_pBTHTransport->Release();
        g_pBTHTransport = NULL;
    }
    if (g_pIRDATransport)
    {
        g_pIRDATransport->Release();
        g_pIRDATransport = NULL;
    }
    gpSynch->Unlock();
    return E_OUTOFMEMORY;
}

HRESULT STDMETHODCALLTYPE
CFactory<CObexIRDATransport>::CreateInstance(IUnknown *pUnknownOuter, REFIID riid, void** ppv)
{
    if(pUnknownOuter != NULL)
        return CLASS_E_NOAGGREGATION;

    gpSynch->Lock();
    SVSUTIL_ASSERT(g_pObex && g_pIRDATransport);
    HRESULT hr;

    if(g_pObex && g_pIRDATransport)
        hr = g_pIRDATransport->QueryInterface(riid, ppv);
    else
        hr = E_POINTER;

    gpSynch->Unlock();
    return hr;

}


HRESULT STDMETHODCALLTYPE
CFactory<CObexBTHTransport>::CreateInstance(IUnknown *pUnknownOuter, REFIID riid, void** ppv)
{
    if(pUnknownOuter != NULL)
        return CLASS_E_NOAGGREGATION;

    gpSynch->Lock();
    SVSUTIL_ASSERT(g_pObex && g_pBTHTransport);
    HRESULT hr;

    if(g_pObex && g_pBTHTransport)
        hr = g_pBTHTransport->QueryInterface(riid, ppv);
    else
        hr = E_POINTER;

    gpSynch->Unlock();
    return hr;

}


template<typename T> HRESULT STDMETHODCALLTYPE
CFactory<T>::CreateInstance(IUnknown *pUnknownOuter, REFIID riid, void** ppv)
{
    if(pUnknownOuter != NULL)
        return CLASS_E_NOAGGREGATION;

    T* t = new T;

    if(!t)
        return E_OUTOFMEMORY;

    HRESULT hr = t->QueryInterface(riid, ppv);
    t->Release();
    return hr;

}




template<typename T> HRESULT STDMETHODCALLTYPE CFactory<T>::LockServer(BOOL bLock)
{
    if(bLock)
        g_cServerLocks++;
    else
        g_cServerLocks--;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DllCanUnloadNow()
{
    if(g_cServerLocks == 0 && g_cComponents == 0)
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"DllCanUnloadNow -- unloading\n"));
        return S_OK;
    }
    else
    {
        DEBUGMSG(OBEX_COBEX_ZONE,(L"DllCanUnloadNow -- CANT UNLOAD\n"));
        return S_FALSE;
    }
}

HRESULT STDMETHODCALLTYPE DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if(!ppv) return E_POINTER;

    if(rclsid == CLSID_Obex)
        return CFactory<CObex>::GetClassObject(riid, ppv);

    if(rclsid == CLSID_HeaderCollection)
        return CFactory<CHeaderCollection>::GetClassObject(riid, ppv);

    if(rclsid == CLSID_PropertyBag)
        return CFactory<CPropertyBag>::GetClassObject(riid, ppv);

    if(rclsid == CLSID_IrdaTransport)
        return CFactory<CObexIRDATransport>::GetClassObject(riid, ppv);

    if(rclsid == CLSID_BthTransport)
        return CFactory<CObexBTHTransport>::GetClassObject(riid, ppv);

    if(rclsid == CLSID_IpTransport)
        return CFactory<CObexTCPTransport>::GetClassObject(riid, ppv);


    // repeat if statements here

    return *ppv = 0, CLASS_E_CLASSNOTAVAILABLE;


}

HRESULT STDMETHODCALLTYPE DllRegisterServer()
{
    HRESULT hRes = RegisterServer(L"ObexAPI.dll", CLSID_Obex, L"ObexAPI", L"Sum.OBEXAPI", L"Obex.OBEXAPI.1", L"Free");

    if(SUCCEEDED(hRes))
        hRes = RegisterServer(L"ObexAPI.dll", CLSID_HeaderCollection, L"ObexHeaderCollection", L"Obex.OBEXHeaderCollection", L"Sum.OBEXHeaderCollection.1", L"Free");

    if(SUCCEEDED(hRes))
        hRes = RegisterServer(L"ObexAPI.dll",  CLSID_IrdaTransport, L"ObexIrdaTransport", L"Obex.ObexIrdaTransport", L"Sum.ObexIrdaTransport.1", L"Free");

    if(SUCCEEDED(hRes))
        hRes = RegisterServer(L"ObexAPI.dll",  CLSID_BthTransport, L"ObexBthTransport", L"Obex.ObexBthTransport", L"Sum.ObexBthTransport.1", L"Free");

    return hRes;
 }

HRESULT STDMETHODCALLTYPE DllUnregisterServer()
{
    return UnregisterServer(IID_IObex, L"Sum.OBEXAPI", L"Sum.OBEXAPI.1");
}

// Register the component in the registry.
HRESULT RegisterServer(const WCHAR * szModuleName,     // DLL module handle
                       REFCLSID clsid,                 // Class ID
                       const WCHAR * szFriendlyName,   // Friendly Name
                       const WCHAR * szVerIndProgID,   // Programmatic
                       const WCHAR * szProgID,         // IDs
                       const WCHAR * szThreadingModel) // ThreadingModel
{
    // Get server location.
    WCHAR szModule[512];
    HMODULE hModule = GetModuleHandle(szModuleName);
    DWORD dwResult = GetModuleFileName(hModule, szModule, sizeof(szModule)/sizeof(WCHAR));

    SVSUTIL_ASSERT(dwResult != 0);

    if (dwResult == 0)
    {
        szModule[0] = L'\0';
    }

    // Convert the CLSID into a char.
    WCHAR szCLSID[CLSID_STRING_SIZE];
    if(FAILED(CLSIDtochar(clsid, szCLSID, sizeof(szCLSID) / sizeof(WCHAR))))
        return E_FAIL;

    // Build the key CLSID\\{...}
    WCHAR szKey[64];

    StringCchCopyW(szKey, _countof(szKey), L"CLSID\\");
    StringCchCatW(szKey, _countof(szKey), szCLSID);

    // Add the CLSID to the registry.
    setKeyAndValue(szKey, NULL, szFriendlyName);

    // Add the server filename subkey under the CLSID key.
    if(wcsstr(szModuleName, L".exe") == NULL)
    {
        setKeyAndValue(szKey, L"InprocServer32", szModule);
        WCHAR szInproc[64];
        StringCchCopyW(szInproc, _countof(szInproc), szKey);
        StringCchCatW(szInproc, _countof(szInproc), L"\\InprocServer32");
        setValueInKey(szInproc, L"ThreadingModel", szThreadingModel);
    }
    else
        setKeyAndValue(szKey, L"LocalServer32", szModule);

    // Add the ProgID subkey under the CLSID key.
    setKeyAndValue(szKey, L"ProgID", szProgID);

    // Add the version-independent ProgID subkey under CLSID key.
    setKeyAndValue(szKey, L"VersionIndependentProgID", szVerIndProgID);

    // Add the version-independent ProgID subkey under HKEY_CLASSES_ROOT.
    setKeyAndValue(szVerIndProgID, NULL, szFriendlyName);
    setKeyAndValue(szVerIndProgID, L"CLSID", szCLSID);
    setKeyAndValue(szVerIndProgID, L"CurVer", szProgID);

    // Add the versioned ProgID subkey under HKEY_CLASSES_ROOT.
    setKeyAndValue(szProgID, NULL, szFriendlyName);
    setKeyAndValue(szProgID, L"CLSID", szCLSID);

    return S_OK;
}


// Remove the component from the registry.
LONG UnregisterServer(REFCLSID clsid,             // Class ID
                      const WCHAR * szVerIndProgID, // Programmatic
                      const WCHAR * szProgID)       // IDs
{
    // Convert the CLSID into a char.
    WCHAR szCLSID[CLSID_STRING_SIZE];
    if(FAILED(CLSIDtochar(clsid, szCLSID, sizeof(szCLSID)/sizeof(WCHAR))))
        return E_UNEXPECTED;

    // Build the key CLSID\\{...}
    WCHAR szKey[64];

    StringCchCopyW(szKey, _countof(szKey), L"CLSID\\");
    StringCchCatW(szKey, _countof(szKey), szCLSID);

    // Delete the CLSID Key - CLSID\{...}
    LONG lResult = recursiveDeleteKey(HKEY_CLASSES_ROOT, szKey);
    SVSUTIL_ASSERT((lResult == ERROR_SUCCESS) || (lResult == ERROR_FILE_NOT_FOUND)); // Subkey may not exist.

    // Delete the version-independent ProgID Key.
    lResult = recursiveDeleteKey(HKEY_CLASSES_ROOT, szVerIndProgID);
    SVSUTIL_ASSERT((lResult == ERROR_SUCCESS) || (lResult == ERROR_FILE_NOT_FOUND)); // Subkey may not exist.

    // Delete the ProgID key.
    lResult = recursiveDeleteKey(HKEY_CLASSES_ROOT, szProgID);
    SVSUTIL_ASSERT((lResult == ERROR_SUCCESS) || (lResult == ERROR_FILE_NOT_FOUND)); // Subkey may not exist.

    return S_OK;
}

// Convert a CLSID to a char string.
HRESULT CLSIDtochar(REFCLSID clsid, WCHAR * szCLSID, unsigned int length)
{
    SVSUTIL_ASSERT(length >= CLSID_STRING_SIZE);
    // Get CLSID
    LPOLESTR wszCLSID = NULL;
    HRESULT hr = StringFromCLSID(clsid, &wszCLSID);
        SVSUTIL_ASSERT(SUCCEEDED(hr));

    if(SUCCEEDED(hr))
    {
        // Covert from wide characters to non-wide.
        if(wcslen(wszCLSID) <= length) {
            StringCchCopyNW(szCLSID, STRSAFE_MAX_CCH, wszCLSID, length);
        }
        else {
            ASSERT(FALSE);
            return E_FAIL;
        }

        // Free memory.
        CoTaskMemFree(wszCLSID);
    }

    return hr;
}


// Delete a key and all of its descendents.
LONG recursiveDeleteKey(HKEY hKeyParent,           // Parent of key to delete
                        const WCHAR* lpszKeyChild)  // Key to delete
{
    // Open the child.
    HKEY hKeyChild;
    LONG lRes = RegOpenKeyEx(hKeyParent, lpszKeyChild, 0, KEY_ALL_ACCESS, &hKeyChild);
    if(lRes != ERROR_SUCCESS)
        return lRes;

    // Enumerate all of the decendents of this child.
    FILETIME time;
    WCHAR szBuffer[256];
    DWORD dwSize = 256;
    while(RegEnumKeyEx(hKeyChild, 0, szBuffer, &dwSize, NULL, NULL, NULL, &time) == S_OK)
    {
        // Delete the decendents of this child.
        lRes = recursiveDeleteKey(hKeyChild, szBuffer);
        if(lRes != ERROR_SUCCESS)
        {
            // Cleanup before exiting.
            RegCloseKey(hKeyChild);
            return lRes;
        }
        dwSize = 256;
    }

    // Close the child.
    RegCloseKey(hKeyChild);

    // Delete this child.
    return RegDeleteKey(hKeyParent, lpszKeyChild);
}


// Create a key and set its value.
BOOL setKeyAndValue(const WCHAR* szKey, const WCHAR* szSubkey, const WCHAR* szValue)
{
    HKEY hKey;
    WCHAR szKeyBuf[1024];

    if(1024 <= wcslen(szKey))
        return FALSE;

    // Copy keyname into buffer.
    StringCchCopyNW(szKeyBuf, _countof(szKeyBuf), szKey, 1024 - 1);
    szKeyBuf[1024 - 1] = 0;

    // Add subkey name to buffer.
    if(szSubkey != NULL)
    {
        StringCchCopyW(szKeyBuf, _countof(szKeyBuf), L"\\");
        StringCchCatW(szKeyBuf, _countof(szKeyBuf), szSubkey);
    }

    // Create and open key and subkey.
    long lResult = RegCreateKeyEx(HKEY_CLASSES_ROOT, szKeyBuf, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);
    if(lResult != ERROR_SUCCESS)
        return FALSE;

    // Set the Value.
    if(szValue != NULL)
        RegSetValueEx(hKey, NULL, 0, REG_SZ, (BYTE *)szValue, (wcslen(szValue)+1) * sizeof(WCHAR));

    RegCloseKey(hKey);
    return TRUE;
}


// Open a key and set a value.
BOOL setValueInKey(const WCHAR* szKey, const WCHAR* szNamedValue, const WCHAR* szValue)
{
    HKEY hKey;
    WCHAR szKeyBuf[1024];

    if(1024 <= wcslen(szKey))
        return FALSE;

    // Copy keyname into buffer.
    StringCchCopyNW(szKeyBuf, _countof(szKeyBuf), szKey, 1024 - 1);
    szKeyBuf[1024 - 1] = 0;

    // Create and open key and subkey.
    long lResult = RegOpenKeyEx(HKEY_CLASSES_ROOT, szKeyBuf, 0, KEY_SET_VALUE, &hKey);
    if(lResult != ERROR_SUCCESS)
        return FALSE;

    // Set the Value.
    if(szValue != NULL)
        RegSetValueEx(hKey, szNamedValue, 0, REG_SZ, (BYTE*)szValue, (wcslen(szValue)+1) * sizeof(WCHAR));

    RegCloseKey(hKey);
    return TRUE;
}






/*****************************************************************************/
/*   DllMain                                                                 */
/*     Initilize critical sections                                           */
/*****************************************************************************/
BOOL APIENTRY DllMain(HANDLE hInst, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH) {
        DEBUGREGISTER((HINSTANCE)hInst);
        DEBUGMSG(CONSTRUCT_ZONE,(L"Obex DLL Main\n"));
        InitializeCriticalSection(&g_TransportSocketCS);
        InitializeCriticalSection(&g_PropBagCS);

#if defined(DEBUG) || defined(_DEBUG)
                svsutil_Initialize ();
                svsutil_SetAllocData(&dwData, &dwData);
#endif

#if ! defined (SDK_BUILD)
        DisableThreadLibraryCalls((HINSTANCE)hInst);
#endif

                //load OBEX capabilities
                HKEY hkey;
                DWORD cb, dwType, dwRes;

#ifdef SDK_BUILD
                g_dwObexCaps = 0;
#else
                g_dwObexCaps = SEND_DEVICE_UPDATES;
#endif

                g_ResponseTimeout = OBEX_DEFAULT_RESPONSE_TIMEOUT;
                g_uiMaxFileChunk = OBEX_DEFAULT_MAX_FILE_CHUNK_SIZE;

                if ((ERROR_SUCCESS == (dwRes = RegOpenKeyEx(HKEY_LOCAL_MACHINE, k_szRegObex, 0, KEY_QUERY_VALUE, &hkey))))
                {
                        //look for the CAPS value
                        cb= sizeof(DWORD);
                        dwRes = RegQueryValueEx(hkey, L"ObexCaps", NULL, &dwType, (LPBYTE)&g_dwObexCaps, &cb);
                        if ((dwRes != ERROR_SUCCESS) || (dwType != REG_DWORD))
                        {
#ifdef SDK_BUILD
                                g_dwObexCaps = 0;
#else
                                g_dwObexCaps = SEND_DEVICE_UPDATES;
#endif
                        }

                        // Read ResponseTimeout
                        cb= sizeof(DWORD);
                        dwRes = RegQueryValueEx(hkey, L"ResponseTimeout", NULL, &dwType, (LPBYTE)&g_ResponseTimeout, &cb);
                        if ((dwRes != ERROR_SUCCESS) || (dwType != REG_DWORD))
                        {
                                g_ResponseTimeout = OBEX_DEFAULT_RESPONSE_TIMEOUT;
                        }
                        else
                        {
                                // Convert the time from milliseconds to seconds.
                                g_ResponseTimeout = g_ResponseTimeout / 1000;
                        }

                        // Read MaxFileChunk.
                        cb= sizeof(DWORD);
                        dwRes = RegQueryValueEx(hkey, L"MaxFileChunkSize", NULL, &dwType, (LPBYTE)&g_uiMaxFileChunk, &cb);
                        if ((dwRes != ERROR_SUCCESS) || (dwType != REG_DWORD))
                        {
                                g_uiMaxFileChunk = OBEX_DEFAULT_MAX_FILE_CHUNK_SIZE;
                        }
                        else
                        {
                                // The protocol limits this to a SHORT, so double check we're legal.
                                if (g_uiMaxFileChunk >= USHRT_MAX) {
                                        DEBUGMSG(1,(L"OBEX: Error: MaxFileChunk is > USHORT, illegal.  Setting to default <%d>\r\n",OBEX_DEFAULT_MAX_FILE_CHUNK_SIZE));
                                        g_uiMaxFileChunk = OBEX_DEFAULT_MAX_FILE_CHUNK_SIZE;
                                }
                        }

                        RegCloseKey(hkey);
                }

                gpSynch = new CSynch();
                if( !gpSynch )
                {
                        return FALSE;
                }

    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        SVSUTIL_ASSERT(gpSynch && (!gpSynch->IsLocked()));

        delete gpSynch;
        gpSynch = NULL;


        DeleteCriticalSection(&g_TransportSocketCS);
        DeleteCriticalSection(&g_PropBagCS);

#if defined(DEBUG) || defined(_DEBUG)
        DEBUGMSG(1, (L"\n\n\nUNFREED MEMORY = %d bytes\n\n\n", svsutil_TotalAlloc()));
        svsutil_LogCallStack();
        SVSUTIL_ASSERT(0 == svsutil_TotalAlloc());
        DEBUGMSG(CONSTRUCT_ZONE, (_T("Obex DLL Going away\n")));
#endif
        svsutil_DeInitialize();

    }

    return TRUE;
}


#ifdef DEBUG
//note: these are all represented as bits, the first in the list is the least sig
//  of the 16
DBGPARAM dpCurSettings = {
    TEXT("ObexApi"), {
    /*D*/
    TEXT("Construct"),
    TEXT("TransportSocket"),
    TEXT("IrdaTransport"),
    TEXT("BthTransport"),
    /*C*/
    TEXT("TransportConn"),
    TEXT("Network"),
    TEXT("DeviceEnum"),
    TEXT("HeaderCollection"),
    /*B*/
    TEXT("HeaderEnum"),
    TEXT("CObex"),
    TEXT("ObexDevice"),
    TEXT("ObexStream"),
    /*A*/
    TEXT("PropertyBag"),
    TEXT("Addref Rel"),
    TEXT("Print Packet Contents"),
    TEXT("")
    },0x0000
    //0x060A
    //0x0448
    //0x0608
    //0x0408
    //0x0880
    //0x0E34
    /*0xABCD*/
};
//0x4080 does 7 and 14 (header collection and first empty...remember to start cnting at 0)

#endif
