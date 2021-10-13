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
#pragma once

#ifndef __DDRAWUTY_H__
#define __DDRAWUTY_H__

// Disable bogus warning messags
#pragma warning(disable : 4250)  // warning C4250: 'XXXX' : inherits 'YYYY' via dominance

// External Dependencies
#include "main.h"

// DDrawUty Dependencies
#include "DDrawUty_Macros.h"
#include "DDrawUty_Maps.h"
#include "DDrawUty_IIDs.h"
#include "DDrawUty_Types.h"
#include "DDrawUty_Config.h"
#include "DDrawUty_Output.h"
#include "DDrawUty_Singletons.h"
#include "DDrawUty_Helpers.h"
#include "DDrawUty_App.h"
#include "PixelConverter.h"

#define SafeRelease(x) if (x) {(x)->Release(); (x) = NULL;}
    
    typedef HRESULT (WINAPI * PFNDDRAWCREATE)(GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter);
    typedef HRESULT (WINAPI * PFNDDRAWCREATECLIPPER)(DWORD dwFlags,LPDIRECTDRAWCLIPPER FAR *lplpDDClipper,IUnknown FAR *pUnkOuter);
    typedef HRESULT (WINAPI * PFNDDEnum)(LPDDENUMCALLBACKEX lpCallback, LPVOID lpContext, DWORD dwFlags);

#endif // !defined(__DDRAWUTY_H__)
