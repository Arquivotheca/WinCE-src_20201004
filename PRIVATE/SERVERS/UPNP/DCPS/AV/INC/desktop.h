//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef __DESKTOP_H__
#define __DESKTOP_H__

// XP doesn't support IDispatch::Invoke for UPnP actions
#define NO_INVOKE_FOR_UPNP_ACTIONS

#define DEBUGMSG(x, y)

EXTERN_C const IID IID_IUPnPServiceCallbackPrivate;

MIDL_INTERFACE("24ea2515-f612-4528-ba82-7bd3dbbad303")
IUPnPServiceCallbackPrivate : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE AddTransientCallback( 
        /* [in] */ IUnknown __RPC_FAR *pUnkCallback,
        /* [out] */ DWORD __RPC_FAR *pdwCookie) = 0;
    
    virtual HRESULT STDMETHODCALLTYPE RemoveTransientCallback( 
        /* [in] */ DWORD dwCookie) = 0;
    
};

#endif // __DESKTOP_H__
