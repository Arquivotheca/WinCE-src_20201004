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
#include "TUXMAIN.H"
#include "ERRORMAP.H"

BEGINMAP(MMRESULT,lpErrorMap)
ENTRY(WAVERR_BADFORMAT)
ENTRY(MMSYSERR_ERROR)
ENTRY(MMSYSERR_BADDEVICEID)
ENTRY(MMSYSERR_NOTENABLED)
ENTRY(MMSYSERR_ALLOCATED)
ENTRY(MMSYSERR_INVALHANDLE)
ENTRY(MMSYSERR_NODRIVER)
ENTRY(MMSYSERR_NOMEM)
ENTRY(MMSYSERR_NOTSUPPORTED)
ENTRY(MMSYSERR_BADERRNUM)
ENTRY(MMSYSERR_INVALFLAG)
ENTRY(MMSYSERR_INVALPARAM)
ENTRY(MMSYSERR_HANDLEBUSY)
ENTRY(MMSYSERR_INVALIDALIAS)
ENTRY(MMSYSERR_BADDB)
ENTRY(MMSYSERR_KEYNOTFOUND)
ENTRY(MMSYSERR_READERROR)
ENTRY(MMSYSERR_WRITEERROR)
ENTRY(MMSYSERR_DELETEERROR)
ENTRY(MMSYSERR_VALNOTFOUND)
ENTRY(MMSYSERR_NODRIVERCB)
ENTRY(MMSYSERR_LASTERROR)
ENDMAP

Map<MMRESULT> g_ErrorMap(lpErrorMap,MAPSIZE(lpErrorMap));

