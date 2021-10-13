//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
#include "TUXMAIN.H"
#include "ERRORMAP.H"

BEGINMAP(HRESULT,lpErrorMap)
ENTRY(DS_OK)
ENTRY(DSERR_ALLOCATED)
ENTRY(DSERR_CONTROLUNAVAIL)
ENTRY(DSERR_INVALIDPARAM)
ENTRY(DSERR_INVALIDCALL)
ENTRY(DSERR_GENERIC)
ENTRY(DSERR_PRIOLEVELNEEDED)
ENTRY(DSERR_OUTOFMEMORY)
ENTRY(DSERR_BADFORMAT)
ENTRY(DSERR_UNSUPPORTED)
ENTRY(DSERR_NODRIVER)
ENTRY(DSERR_ALREADYINITIALIZED)
ENTRY(DSERR_NOAGGREGATION)
ENTRY(DSERR_BUFFERLOST)
ENTRY(DSERR_OTHERAPPHASPRIO)
ENTRY(DSERR_UNINITIALIZED)
ENTRY(DSERR_NOINTERFACE)
ENDMAP

Map<HRESULT> g_ErrorMap(lpErrorMap,MAPSIZE(lpErrorMap));