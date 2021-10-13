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
