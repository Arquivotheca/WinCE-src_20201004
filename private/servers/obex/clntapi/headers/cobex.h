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
// Obex.h: interface for the CObex class.
//
//////////////////////////////////////////////////////////////////////

#include "DeviceEnum.h"

#ifndef OBEX_H
#define OBEX_H

enum Stage
{
    Initialized,
    ShuttingDown,
    Stopped
};

class CDeviceEnum;
class CConnectionPoint;

struct TRANSPORT_NODE {
    IObexTransport *pTransport;
    TRANSPORT_NODE *pNext;
};

class CObex : public IObex2, public IConnectionPointContainer, public IObexCaps
{
public:
    CObex();
    ~CObex();

    HRESULT STDMETHODCALLTYPE Initialize();
    HRESULT STDMETHODCALLTYPE Shutdown();
    HRESULT STDMETHODCALLTYPE EnumDevices(LPDEVICEENUM *ppDeviceEnum, REFCLSID uuidTransport);
    HRESULT STDMETHODCALLTYPE EnumTransports(LPPROPERTYBAGENUM *ppTransportEnum);
    HRESULT STDMETHODCALLTYPE BindToDevice(LPPROPERTYBAG pPropertyBag, LPDEVICE *ppDevice);
    HRESULT STDMETHODCALLTYPE RegisterService(LPPROPERTYBAG pPropertyBag, LPSERVICE * pService);
//  HRESULT STDMETHODCALLTYPE RegisterServiceBlob(unsigned long ulSize, byte * pbData, LPSERVICE * pService);
    HRESULT STDMETHODCALLTYPE StartDeviceEnum();
    HRESULT STDMETHODCALLTYPE StopDeviceEnum();
    HRESULT STDMETHODCALLTYPE PauseDeviceEnum(BOOL fPauseOn);


    HRESULT STDMETHODCALLTYPE EnumConnectionPoints(IEnumConnectionPoints **ppEnum);
    HRESULT STDMETHODCALLTYPE FindConnectionPoint(REFIID riid, IConnectionPoint **ppCP);

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);
    HRESULT STDMETHODCALLTYPE SetCaps(DWORD dwCaps);

public:
    BOOL   IsInitialized() { return _stage == Initialized; }

protected:
    DWORD  StartEnumeratingDevices();
    static DWORD WINAPI EnumTest(LPVOID lpvParam);
    static DWORD WINAPI EnumIndividualDeviceThread(LPVOID lpvParam);

private:
    ULONG                _refCount; 
    Stage                _stage;    
    BOOL                _bStopEnum;
    CDeviceEnum            *_pDevices;
    TRANSPORT_NODE         *_pActiveTransports;

    //Thread handle and refcount to number of successful Enums called
    HANDLE                _hEnumThreadArray[10];
    UINT                _uiEnumThreadCnt;
    HANDLE              _hEnumPauseEvent;
    HANDLE              _hEnumPauseEvent_UnlockPauseFunction;
    BOOL                _fPause;
    LONG                _uiRunningENumThreadCnt; //<--this differs from _uiEnumThreadCnt
                                                 //  because that represents the # of 
                                                 //  threads that were STARTED... not
                                                 //  necessarily RUNNING

    
    UINT                _uiDeviceEnumCnt;

    //count for initilization
    UINT                _uiInitCnt;
    
    CConnectionPoint    *_pConnPt;

    HRESULT IsCorpse(DEVICE_PROPBAG_LIST *pDeviceCorpses, LPPROPERTYBAG2 pBag, BOOL *pfIsCorpse);
    HRESULT UpdateDevices(IObexTransport *pTransport, CLSID *pclsid, DEVICE_LIST *pMatchedDeviceList);
    HRESULT PauseEnumIfRequired();

    // an "active" transport is one that is currently being enumeratated
    HRESULT AddActiveTransport(IObexTransport *pTransport);
    HRESULT RemoveActiveTransport(IObexTransport *pTransport);
    HRESULT AbortActiveTransports(BOOL fAbort); //TRUE to abort, FALSE to resume
};

extern CObex  *g_pObex;
extern DWORD g_dwObexCaps;

BOOL GetLock();
void ReleaseLock();

int IsBluetoothGeneratedName (VARIANT *pName, VARIANT *pAddress);

#endif // !defined(OBEX_H)
