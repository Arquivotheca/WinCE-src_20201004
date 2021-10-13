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
//------------------------------------------------------------------------------
//
//  acm.c
//
//  Copyright (c) 1991-2000 Microsoft Corporation
//
//  Description:
//      This module provides the Audio Compression Manager API to the
//      installable audio drivers
//
//  History:
//
//------------------------------------------------------------------------------
#include "acmi.h"

// Function to scan the registry for any ACM drivers installed in the system.
// Driver registry entry is in the following format:
// [HKEY_LOCAL_MACHINE\Drivers32]
// "msacm.<tag>"="<drivername.dll>"
//
MMRESULT acmBootDrivers(VOID)
{
    FUNC("acmBootDrivers");

    // If we've already done this, just return now.
    if (pag->fDriversBooted)
    {
        return MMSYSERR_NOERROR;
    }

    HACMDRIVERID        hadid = NULL;
    MMRESULT            mmRet = MMSYSERR_NOERROR;
    DWORD               dwIndex;
    UINT                uPriority = 1;

    LONG            lRet;
    HKEY            hKey;

    CONST TCHAR cszSecDrivers32[] = TEXT("Drivers32");
    CONST TCHAR cszTagDrivers[]   = TEXT("msacm.");

    //  Open HKLM\Drivers32. This is where we (& the desktop) keep ACM driver info
    lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, cszSecDrivers32, 0, KEY_READ, &hKey);
    if (ERROR_SUCCESS != lRet)
    {
        return MMSYSERR_ERROR;
    }

    // Enumerate through all the values look for any of the form 'msacm.xxxx'
    dwIndex=0;
    for(;;)
    {
        TCHAR szValue[256];
        TCHAR szData[256];

        DWORD cchValue = _countof(szValue);
        DWORD cchData  = _countof(szData);

        lRet = RegEnumValue(hKey, dwIndex, szValue, &cchValue, NULL, NULL, (LPBYTE) szData, &cchData);
        if (ERROR_SUCCESS != lRet)
        {
            break;
        }
        dwIndex++;

        // Check for "msacm."
        if (_wcsnicmp(szValue, cszTagDrivers, _countof(cszTagDrivers)-1))
        {
            continue;
        }
        // Skip dummy driver lines (data starts with '*')
        if ('*' == szData[0])
        {
            continue;
        }

        // Add the driver to our list
        mmRet = IDriverAdd( &hadid, NULL, (LPARAM)szData, 0L, ACM_DRIVERADDF_NAME | ACM_DRIVERADDF_GLOBAL, szValue);
        if (MMSYSERR_NOERROR != mmRet)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("Failed IDriverAdd on ACM init\r\n")));
        }
    }

    lRet = RegCloseKey(hKey);

    pag->fDriversBooted=TRUE;

    mmRet = MMSYSERR_NOERROR;

    FUNC_EXIT();
    return (mmRet);
}

#ifdef DEBUG
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID acm_PrintList()
{
    PACMDRIVERID padid;

    FUNC("acm_PrintList");

// tmp
    goto EXIT;

    acmBootDrivers();
    padid = pag->padidFirst;

    DEBUGMSG(ZONE_VERBOSE, (TEXT("====================== acm_PrintList (pag = 0x%X) (pag->padidFirst = 0x%X)\r\n"),pag, pag->padidFirst));
    for ( ; padid; padid = padid->padidNext) {
        HEXVALUE(padid);
        HEXVALUE(padid->padidNext);
        HEXVALUE(padid->pag);            // ptr back to garb
        HEXVALUE(padid->fRemove);        // this driver needs to be removed
        HEXVALUE(padid->uPriority);      // current global priority
        HEXVALUE(padid->padFirst);       // first open instance of driver
        HEXVALUE(padid->lParam);         // lParam used when 'added'
        HEXVALUE(padid->fdwAdd);         // flags used when 'added'
        HEXVALUE(padid->fdwDriver);      // ACMDRIVERID_DRIVERF_* info bits
        HEXVALUE(padid->fdwSupport);     // ACMDRIVERID_SUPPORTF_* info bits
        HEXVALUE(padid->cFormatTags);
        HEXVALUE(padid->paFormatTagCache);
        HEXVALUE(padid->cFilterTags);    // from aci.cFilterTags
        HEXVALUE(padid->paFilterTagCache);
        HEXVALUE(padid->hDev);
        HEXVALUE(padid->hdrvr);          // open driver handle (if driver)
        HEXVALUE(padid->fnDriverProc);   // function entry (if not driver)
        HEXVALUE(padid->dwInstance);     // instance data for functions..
        HEXVALUE(padid->pszSection);
        //HEXVALUE(padid->szAlias[MAX_DRIVER_NAME_CHARS]);
        HEXVALUE(padid->pstrPnpDriverFilename);
        HEXVALUE(padid->dnDevNode);
        DEBUGMSG(ZONE_VERBOSE, (TEXT("\r\n...next...\r\n")));
    }

EXIT:
    FUNC_EXIT();
}
#endif

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PACMDRIVERID
acm_GetPadidFromHad(
    HACMDRIVER had
    )
{
    PACMDRIVERID padid;

    switch (((PACMDRIVER)had)->uHandleType) {
        case TYPE_HACMDRIVERID :
            padid = (PACMDRIVERID)had;
            break;

        case TYPE_HACMDRIVER :
            padid = (PACMDRIVERID)((PACMDRIVER)had)->hadid;
            break;

        default :
            padid = NULL;
    }

    return(padid);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LRESULT
IDriverMessageId(
    HACMDRIVERID        hadid,
    UINT                uMsg,
    LPARAM              lParam1,
    LPARAM              lParam2
    )
{
    PACMDRIVERID            padid = (PACMDRIVERID) hadid;
    LRESULT                 lRet = MMSYSERR_ERROR;

    //  Better make sure the driver is loaded if we're going to use the hadid
    if (0 == (ACMDRIVERID_DRIVERF_LOADED & padid->fdwDriver))
    {
        if ( (DRV_LOAD != uMsg) && (DRV_ENABLE != uMsg) && (DRV_OPEN != uMsg) )
        {
            lRet = (LRESULT)IDriverLoad(hadid, 0L);
            if (MMSYSERR_NOERROR != lRet)
            {
                return (lRet);
            }
        }
    }

    if (NULL != padid->fnDriverProc)
    {
        lRet = padid->fnDriverProc(padid->dwInstance, hadid, uMsg, lParam1, lParam2);
    }

    return (lRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LRESULT
IDriverMessage(
    HACMDRIVER          had,
    UINT                uMsg,
    LPARAM              lParam1,
    LPARAM              lParam2
    )
{
    PACMDRIVER              pad = (PACMDRIVER) had;
    LRESULT                 lRet = MMSYSERR_ERROR;

    if (NULL != pad->fnDriverProc)
    {
        lRet = pad->fnDriverProc(pad->dwInstance, pad->hadid, uMsg, lParam1, lParam2);
    }

    return (lRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LRESULT
IDriverConfigure(
    HACMDRIVERID            hadid,
    HWND                    hwnd
    )
{
    LRESULT             lRet;
    PACMDRIVERID        padid = (PACMDRIVERID) hadid;
    DRVCONFIGINFOEX     dci;
    HACMDRIVER          had;
    LPARAM              lParam2;

    FUNC("IDriverConfigure");

    if (TYPE_HACMDRIVER == padid->uHandleType) {
        //
        // HADID is really a HACMDRIVER
        //
        had   = (HACMDRIVER) hadid;
        hadid = ((PACMDRIVER) had)->hadid;
    } else if (TYPE_HACMDRIVERID == padid->uHandleType) {
        //
        // HADID is indeed a HACMDRIVERID
        //
        had   = NULL;
    } else {
        lRet = DRVCNF_CANCEL;
        goto EXIT;
    }

    padid = (PACMDRIVERID) hadid;

    if (0 != (ACMDRIVERID_DRIVERF_NOTIFY & padid->fdwDriver)) {
        TESTMSG("acmDriverMessage(): notification handles cannot be configured.");
        lRet = MMSYSERR_INVALHANDLE;
        goto EXIT;
    }

    //
    //
    //
    lParam2 = 0L;
    if (ACM_DRIVERADDF_NAME == (ACM_DRIVERADDF_TYPEMASK & padid->fdwAdd)) {
        dci.dwDCISize          = sizeof(dci);
        dci.lpszDCISectionName = padid->pszSection;
        dci.lpszDCIAliasName   = padid->szAlias;
        dci.dnDevNode          = padid->dnDevNode;

        lParam2 = (LPARAM)(LPVOID)&dci;
    }

    lRet = IDriverMessageId(hadid, DRV_CONFIGURE, (LPARAM)(UINT)hwnd, lParam2);

EXIT:
    FUNC_EXIT();
    return (lRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverDetails(
    HACMDRIVERID            hadid,
    LPACMDRIVERDETAILS      padd,
    DWORD                   fdwDetails
    )
{
    MMRESULT            mmRet;
    PACMDRIVERID        padid = (PACMDRIVERID) hadid;

    FUNC("IDriverDetails");

    VE_COND_NSUP(0 != (ACMDRIVERID_DRIVERF_NOTIFY & padid->fdwDriver));

    //
    //  default all info then call driver to fill in what it wants
    //
    mmRet = (MMRESULT)IDriverMessageId(hadid,
                                     ACMDM_DRIVER_DETAILS,
                                     (LPARAM)(LPACMDRIVERDETAILS)padd,
                                     0L);

    VE_COND_NSUP((MMSYSERR_NOERROR != mmRet) || (0L == padd->vdwACM));

    HEXVALUE(padd->cFormatTags);
    //
    //  Check that the driver didn't set any of the reserved flags; then
    //  set the DISABLED and LOCAL flags.
    //
    VE_COND_ERR(~0x0000001FL & padd->fdwSupport);

    if (0 != (ACMDRIVERID_DRIVERF_DISABLED & padid->fdwDriver)) {
        padd->fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_DISABLED;
    }

    if (0 != (ACMDRIVERID_DRIVERF_LOCAL & padid->fdwDriver)) {
        padd->fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_LOCAL;
    }

EXIT:
    FUNC_EXIT();
    return (mmRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverGetFormatTags(
    PACMDRIVERID            padid
    )
{
    MMRESULT                mmRet = MMSYSERR_ERROR;
    UINT                    u;
    ACMFORMATTAGDETAILS     aftd;
    PACMFORMATTAGCACHE      paftc = NULL;
    UINT                    cb;

    FUNC("IDriverGetFormatTags");

    if (NULL != padid->paFormatTagCache) {
        LocalFree((HLOCAL) padid->paFormatTagCache);
        padid->paFormatTagCache = NULL;
    }

    //
    //  check to see if there are no formats for this driver. if not, dump
    //  them...
    //
    VE_COND_ERR(0 == padid->cFormatTags);

    //
    //  alloc an array of tag data structures to hold info for format tags
    //
    if (padid->cFormatTags > 1024) {
        return MMSYSERR_INVALPARAM;
    }
    cb = padid->cFormatTags * sizeof(ACMFORMATTAGDETAILS);
    paftc = (PACMFORMATTAGCACHE) LocalAlloc(LMEM_FIXED, cb);
    VE_COND_NOMEM(NULL == paftc);

    padid->paFormatTagCache = paftc;
    for (u = 0; u < padid->cFormatTags; u++) {
        aftd.cbStruct         = sizeof(aftd);
        aftd.dwFormatTagIndex = u;

        mmRet = (MMRESULT) IDriverMessageId((HACMDRIVERID)padid,
                                         ACMDM_FORMATTAG_DETAILS,
                                         (LPARAM)(LPVOID)&aftd,
                                         ACM_FORMATTAGDETAILSF_INDEX);

        if (MMSYSERR_NOERROR != mmRet) {
            ERRMSG("IDriverGetFormatTags(): driver failed format tag details query!");
            goto EXIT;
        }

        //
        //  Following switch is just some validation for debug
        //
#ifdef RDEBUG
        switch (aftd.dwFormatTag) {
            case WAVE_FORMAT_UNKNOWN:
                VE_COND_ERR(1);

            case WAVE_FORMAT_PCM:
                if ('\0' != aftd.szFormatTag[0]) {
                    WARNMSG("IDriverGetFormatTags(): driver returned custom PCM format tag name! ignoring it!");
                }
                break;

            case WAVE_FORMAT_DEVELOPMENT:
                WARNMSG("IDriverGetFormatTags(): driver returned DEVELOPMENT format tag--do not ship this way.");
                break;

        }
#endif

        paftc[u].dwFormatTag  = aftd.dwFormatTag;
        paftc[u].cbFormatSize = aftd.cbFormatSize;

    }

EXIT:
    if (MMSYSERR_NOERROR != mmRet) {
        if (NULL != paftc ) {
            LocalFree((HLOCAL) paftc);
        }
    }

    FUNC_EXIT();
    return (mmRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverGetFilterTags(
    PACMDRIVERID    padid
    )
{
    MMRESULT                mmRet = MMSYSERR_ERROR;
    UINT                    u;
    ACMFILTERTAGDETAILS     aftd;
    PACMFILTERTAGCACHE      paftc = NULL;
    DWORD                   cb;

    FUNC("IDriverGetFilterTags");

    if (NULL != padid->paFilterTagCache) {
        LocalFree((HLOCAL)padid->paFilterTagCache);
    }
    padid->paFilterTagCache = NULL;

    //
    //  check to see if there are no filters for this driver. if not, null
    //  our cache pointers and succeed..
    //
    if (0 != (ACMDRIVERDETAILS_SUPPORTF_FILTER & padid->fdwSupport)) {
        VE_COND_ERR(0 == padid->cFilterTags);
    } else {
        VE_COND_NOERR(0 == padid->cFilterTags);

        TESTMSG("IDriverLoad(): non-filter driver reports filter tags?");
        GOTO_EXIT(mmRet = MMSYSERR_ERROR);
    }

    //
    //  alloc an array of details structures to hold info for filter tags
    //
    if (padid->cFilterTags > 1000) {
        // avoid integer overlfow in the computation of the tag cache size
        GOTO_EXIT(mmRet = MMSYSERR_INVALPARAM);
    }

    cb    = sizeof(*paftc) * padid->cFilterTags;
    paftc = (PACMFILTERTAGCACHE) LocalAlloc(LMEM_FIXED, (UINT)cb);
    VE_COND_NOMEM(NULL == paftc);

    padid->paFilterTagCache = paftc;
    for (u = 0; u < padid->cFilterTags; u++) {
        aftd.cbStruct         = sizeof(aftd);
        aftd.dwFilterTagIndex = u;

        mmRet = (MMRESULT)IDriverMessageId((HACMDRIVERID)padid,
                                         ACMDM_FILTERTAG_DETAILS,
                                         (LPARAM)(LPVOID)&aftd,
                                         ACM_FILTERTAGDETAILSF_INDEX);
        if (MMSYSERR_NOERROR != mmRet) {
            ERRMSG("IDriverGetFilterTags(): driver failed filter tag details query!");
            goto EXIT;
        }

        //
        //  Following switch is just some validation for debug
        //
#ifdef RDEBUG
        switch (aftd.dwFilterTag) {
            case WAVE_FILTER_UNKNOWN:
                VE_COND_ERR(1);

            case WAVE_FILTER_DEVELOPMENT:
                WARNMSG("IDriverGetFilterTags(): driver returned DEVELOPMENT filter tag--do not ship this way.");
                break;
        }
#endif

        paftc[u].dwFilterTag = aftd.dwFilterTag;
        paftc[u].cbFilterSize = aftd.cbFilterSize;
    }

EXIT:
    if (MMSYSERR_NOERROR != mmRet) {
        if (NULL != paftc ) {
            LocalFree((HLOCAL)paftc);
        }
    }

    FUNC_EXIT();
    return (mmRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverGetWaveIdentifier(
    HACMDRIVERID            hadid,
    LPDWORD                 pdw,
    BOOL                    fInput
    )
{
    PACMDRIVERID        padid = (PACMDRIVERID) hadid;
    MMRESULT            mmRet;
    UINT                u;
    UINT                cDevs;
    UINT                uId = (UINT)WAVE_MAPPER;

    FUNC("IDriverGetWaveIdentifier");

    *pdw = (DWORD)-1L;

    //
    //  check to see if there is hardware support
    //
    VE_COND_HANDLE(0 == (ACMDRIVERDETAILS_SUPPORTF_HARDWARE & padid->fdwSupport));

    if (fInput)
    {
        WAVEINCAPS          wic;
        WAVEINCAPS          wicSearch;

        memset(&wic, 0, sizeof(wic));
        mmRet = (MMRESULT)IDriverMessageId(hadid,
                                         ACMDM_HARDWARE_WAVE_CAPS_INPUT,
                                         (LPARAM)(LPVOID)&wic,
                                         sizeof(wic));
        if (MMSYSERR_NOERROR == mmRet) {
            mmRet   = MMSYSERR_NODRIVER;

            wic.szPname[SIZEOF(wic.szPname) - 1] = '\0';

            cDevs = waveInGetNumDevs();

            for (u = 0; u < cDevs; u++) {

                memset(&wicSearch, 1, sizeof(wicSearch));

                if (0 != waveInGetDevCaps(u, &wicSearch, sizeof(wicSearch)))
                    continue;

                wicSearch.szPname[SIZEOF(wicSearch.szPname) - 1] = '\0';

                if (0 == lstrcmp(wic.szPname, wicSearch.szPname)) {
                    uId = u;
                    mmRet = MMSYSERR_NOERROR;
                    break;
                }
            }
        }
    } else {
        WAVEOUTCAPS         woc;
        WAVEOUTCAPS         wocSearch;

        memset(&woc, 0, sizeof(woc));
        mmRet = (MMRESULT)IDriverMessageId(hadid,
                                         ACMDM_HARDWARE_WAVE_CAPS_OUTPUT,
                                         (LPARAM)(LPVOID)&woc,
                                         sizeof(woc));
        if (MMSYSERR_NOERROR == mmRet) {
            mmRet   = MMSYSERR_NODRIVER;

            woc.szPname[SIZEOF(woc.szPname) - 1] = '\0';

            cDevs = waveOutGetNumDevs();

            for (u = 0; u < cDevs; u++) {
                memset(&wocSearch, 1, sizeof(wocSearch));
                if (0 != waveOutGetDevCaps(u, (LPWAVEOUTCAPS)&wocSearch, sizeof(wocSearch)))
                    continue;

                wocSearch.szPname[SIZEOF(wocSearch.szPname) - 1] = '\0';

                if (0 == lstrcmp(woc.szPname, wocSearch.szPname)) {
                    uId = u;
                    mmRet = MMSYSERR_NOERROR;
                    break;
                }
            }
        }

    }

    *pdw = (DWORD)(long)(int)uId;

EXIT:
    FUNC_EXIT();
    return (mmRet);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverLoad(
    HACMDRIVERID            hadid,
    DWORD                   fdwLoad
    )
{
    BOOL                f;
    PACMDRIVERID        padid = (PACMDRIVERID) hadid;
    PACMDRIVERDETAILS   padd = NULL;
    MMRESULT            mmRet = MMSYSERR_NOERROR;

    FUNC("IDriverLoad");
    //
    // Are we already loaded?
    //
    VE_COND_NOERR(0 != (ACMDRIVERID_DRIVERF_LOADED & padid->fdwDriver));

    if (NULL == (void *)padid->lParam)
    {
        mmRet = MMSYSERR_NODRIVER;
        goto EXIT;
    }

    HINSTANCE hInst = LoadLibrary((LPCTSTR)padid->lParam);
    if (NULL == hInst)
    {
        mmRet = MMSYSERR_NODRIVER;
        goto EXIT;
    }

    padid->fnDriverProc = (ACMDRIVERPROC) GetProcAddress(hInst, TEXT("DriverProc"));
    if (NULL == padid->fnDriverProc)
    {
        mmRet = MMSYSERR_NODRIVER;
        goto EXIT;
    }

    padid->hdrvr = (HDRVR)hInst;

    //
    //  note that lParam2 is set to 0L in this case to signal the driver
    //  that it is merely being loaded to put it in the list--not for an
    //  actual conversion. therefore, drivers do not need to allocate
    //  any instance data on this initial DRV_OPEN (unless they want to)
    //
    IDriverMessageId(hadid, DRV_ENABLE, 0L, 0L);

    padid->dwInstance = IDriverMessageId(hadid, DRV_OPEN, 0L, 0L);
    VE_COND_NODRV(0L == padid->dwInstance);

    //  mark driver as loaded (although we may dump it back out if something goes wrong below...
    padid->fdwDriver |= ACMDRIVERID_DRIVERF_LOADED;

    VE_COND_NOERR(0 != (ACMDRIVERID_DRIVERF_NOTIFY & padid->fdwDriver));
    //
    //  now get the driver details--we use this all the time, so we will
    //  cache it. this also enables us to free a driver until it is needed
    //  for real work...
    //
    padd = (PACMDRIVERDETAILS) LocalAlloc(LMEM_FIXED, sizeof(*padd));
    VE_COND_NOMEM(NULL == padd);
    padd->cbStruct = sizeof(*padd);

    mmRet = IDriverDetails(hadid, padd, 0L);
    VE_EXIT_ON_ERROR();

    VE_COND_ERR(sizeof(*padd) != padd->cbStruct);

    VE_COND_ERR((ACMDRIVERDETAILS_FCCTYPE_AUDIOCODEC != padd->fccType) ||
                (ACMDRIVERDETAILS_FCCCOMP_UNDEFINED  != padd->fccComp));

    if ((0 == padd->wMid) || (0 == padd->wPid))
        ERRMSG("wMid/wPid must be finalized before shipping.");

    VE_COND_ERR( (!_V_STRING(padd->szShortName, SIZEOF(padd->szShortName))) ||
                 (!_V_STRING(padd->szLongName,  SIZEOF(padd->szLongName)))   ||
                 (!_V_STRING(padd->szCopyright, SIZEOF(padd->szCopyright))) ||
                 (!_V_STRING(padd->szLicensing, SIZEOF(padd->szLicensing))) ||
                 (!_V_STRING(padd->szFeatures,  SIZEOF(padd->szFeatures))) );

    //
    //  Above validation succeeded.  Reset the fRemove flag.
    //
    padid->fRemove = FALSE;

    //
    //  We don't keep the DISABLED flag in the fdwSupport cache.
    //
    padid->fdwSupport = padd->fdwSupport & ~ACMDRIVERDETAILS_SUPPORTF_DISABLED;

    padid->cFormatTags = (UINT)padd->cFormatTags;
    mmRet = IDriverGetFormatTags(padid);
    VE_EXIT_ON_ERROR();

    padid->cFilterTags = (UINT)padd->cFilterTags;
    mmRet = IDriverGetFilterTags(padid);
    VE_EXIT_ON_ERROR();

    //
    //  now get some info about the driver so we don't have to keep
    //  asking all the time...
    //
    f = (0L != IDriverMessageId(hadid, DRV_QUERYCONFIGURE, 0L, 0L));
    if (f)
        padid->fdwDriver |= ACMDRIVERID_DRIVERF_CONFIGURE;

    f = (MMSYSERR_NOERROR == IDriverMessageId(hadid, ACMDM_DRIVER_ABOUT, -1L, 0L));
    if (f)
        padid->fdwDriver |= ACMDRIVERID_DRIVERF_ABOUT;

    mmRet = MMSYSERR_NOERROR;
EXIT:
    if (NULL != padd) {
        LocalFree((HLOCAL)padd);
    }
    FUNC_EXIT();
    DECPARAM(mmRet);
    return (mmRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverGetNext(
    LPHACMDRIVERID          phadidNext,
    HACMDRIVERID            hadid,
    DWORD                   fdwGetNext
    )
{
    MMRESULT            mmRet = MMSYSERR_NOERROR;
    PACMDRIVERID        padid;
    BOOL                fDisabled;
    BOOL                fLocal;
    BOOL                fNotify;
    BOOL                fEverything;
    BOOL                fRemove;

    FUNC("IDriverGetNext");
    HEXPARAM(phadidNext);
    HEXPARAM(hadid);
    HEXPARAM(fdwGetNext);

    acmBootDrivers();

    *phadidNext = NULL;

    if (-1L == fdwGetNext) {
        TESTMSG("Everything");
        fEverything = TRUE;
    } else {
        TESTMSG("not Everything");
        fEverything = FALSE;
        //
        //  put flags in more convenient (cheaper) variables
        //
        fDisabled = (0 != (ACM_DRIVERENUMF_DISABLED & fdwGetNext));
        fLocal    = (0 == (ACM_DRIVERENUMF_NOLOCAL & fdwGetNext));
        fNotify   = (0 != (ACM_DRIVERENUMF_NOTIFY & fdwGetNext));
        fRemove   = (0 != (ACM_DRIVERENUMF_REMOVED & fdwGetNext));
    }

    //
    //  init where to start searching from
    //
    if (NULL != hadid) {
        VE_HANDLE(hadid, TYPE_HACMDRIVERID);

        padid = (PACMDRIVERID)hadid;
        HEXVALUE(padid);
        padid = padid->padidNext;
    } else {
        padid = pag->padidFirst;
    }


    HEXVALUE(padid);

    acm_PrintList();

    TESTMSG("getnext find driver");
//Driver_Get_Next_Find_Driver:

    for ( ; padid; padid = padid->padidNext) {
        HEXVALUE(padid);
        if (fEverything) {
            *phadidNext = (HACMDRIVERID)padid;
            GOTO_EXIT(mmRet = MMSYSERR_NOERROR);
        }

        if (!fNotify && (ACMDRIVERID_DRIVERF_NOTIFY & padid->fdwDriver)) {
            TESTMSG("Notify skipping...");
            continue;
        }

        if (!fLocal && (ACMDRIVERID_DRIVERF_LOCAL & padid->fdwDriver)) {
            TESTMSG("Local skipping...");
            continue;
        }

        //
        //  if we are not supposed to include disabled drivers and
        //  padid is disabled, then skip it
        //
        if (!fDisabled && (ACMDRIVERID_DRIVERF_DISABLED & padid->fdwDriver)) {
            TESTMSG("Disabled skipping...");
            continue;
        }

        //
        //  if we are not supposed to include drivers to be removed and
        //  this padid is to be removed then skip it.
        //
        if (!fRemove && padid->fRemove) {
            TESTMSG("Removing skipping...");
            continue;
        }

        TESTMSG("Setting next");
        *phadidNext = (HACMDRIVERID)padid;

        mmRet = MMSYSERR_NOERROR;
        goto EXIT;
    }

    //
    //  We should be returning NULL in *phadidNext ... let's just make sure.
    //
//    ASSERT( NULL == *phadidNext );

    mmRet = MMSYSERR_BADDEVICEID;
EXIT:
    FUNC_EXIT();
    DECPARAM(mmRet);
    return (mmRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverSupport(
    HACMDRIVERID            hadid,
    LPDWORD                 pfdwSupport,
    BOOL                    fFullSupport
    )
{
    PACMDRIVERID        padid;
    DWORD               fdwSupport;

    FUNC("IDriverSupport");

    *pfdwSupport = 0L;

    padid = (PACMDRIVERID)hadid;

    fdwSupport = padid->fdwSupport;

    if (fFullSupport) {
        if (0 != (ACMDRIVERID_DRIVERF_DISABLED & padid->fdwDriver))
            fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_DISABLED;

        if (0 != (ACMDRIVERID_DRIVERF_LOCAL & padid->fdwDriver))
            fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_LOCAL;

        if (0 != (ACMDRIVERID_DRIVERF_NOTIFY & padid->fdwDriver))
            fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_NOTIFY;
    }

    *pfdwSupport = fdwSupport;

    FUNC_EXIT();
    return (MMSYSERR_NOERROR);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverCount(
    LPDWORD                 pdwCount,
    DWORD                   fdwSupport,
    DWORD                   fdwEnum
    )
{
    MMRESULT            mmRet;
    DWORD               fdw;
    DWORD               cDrivers = 0;
    HACMDRIVERID        hadid = NULL;

    FUNC("IDriverCount");

    *pdwCount = 0;

    ENTER_LIST_SHARED();

    while (MMSYSERR_NOERROR == IDriverGetNext(&hadid, hadid, fdwEnum)) {
        mmRet = IDriverSupport(hadid, &fdw, TRUE);
        if (MMSYSERR_NOERROR != mmRet)
            continue;

        if (fdwSupport == (fdw & fdwSupport))
            cDrivers++;
    }

    LEAVE_LIST_SHARED();

    *pdwCount = cDrivers;

    FUNC_EXIT();
    return (MMSYSERR_NOERROR);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
DWORD
IDriverCountGlobal(VOID)
{
    DWORD               cDrivers = 0;
    HACMDRIVERID        hadid;
    DWORD               fdwEnum;

    FUNC("IDriverCountGlobal");

    ASSERT( NULL != pag );


    //
    //  We can enumerate all global drivers using the following flags.
    //
    fdwEnum = ACM_DRIVERENUMF_DISABLED | ACM_DRIVERENUMF_NOLOCAL;

    hadid   = NULL;


    ENTER_LIST_SHARED();
    while (MMSYSERR_NOERROR == IDriverGetNext(&hadid, hadid, fdwEnum))
        cDrivers++;

    LEAVE_LIST_SHARED();

    FUNC_EXIT();
    return(cDrivers);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID
IDriverRefreshPriority(VOID)
{
    HACMDRIVERID        hadid;
    PACMDRIVERID        padid;
    UINT                uPriority;
    DWORD               fdwEnum;

    FUNC("IDriverRefreshPriority");

    //
    //  We only set priorities for non-local and non-notify drivers.
    //
    fdwEnum   = ACM_DRIVERENUMF_DISABLED | ACM_DRIVERENUMF_NOLOCAL;

    uPriority = 1;

    hadid = NULL;
    while (MMSYSERR_NOERROR == IDriverGetNext(&hadid, hadid, fdwEnum)) {
        padid = (PACMDRIVERID)hadid;
        padid->uPriority = uPriority;
        uPriority++;
    }

    FUNC_EXIT();
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
IDriverBroadcastNotify(VOID)
{
    HACMDRIVERID        hadid;
    PACMDRIVERID        padid;

    FUNC("IDriverBroadcastNotify");

    hadid = NULL;
    while (MMSYSERR_NOERROR == IDriverGetNext(&hadid, hadid, (DWORD)-1L))
    {
        padid = (PACMDRIVERID)hadid;

        //
        //  skip drivers that are not loaded--when we load them, they
        //  can refresh themselves...
        //
        if (0 == (ACMDRIVERID_DRIVERF_LOADED & padid->fdwDriver))
            continue;

        //
        //  skip disabled drivers also
        //
        if (0 != (ACMDRIVERID_DRIVERF_DISABLED & padid->fdwDriver))
            continue;

        if (ACM_DRIVERADDF_NOTIFYHWND == (ACM_DRIVERADDF_TYPEMASK & padid->fdwAdd)) {
            HWND        hwnd;
            UINT        uMsg;

            hwnd = (HWND)(UINT)padid->lParam;
            uMsg = (UINT)padid->dwInstance;

            if (IsWindow(hwnd)) {
                TESTMSG("IDriverBroadcastNotify: posting hwnd notification");
                PostMessage(hwnd, uMsg, 0, 0L);
            }
        } else {
            IDriverMessageId(hadid, ACMDM_DRIVER_NOTIFY, 0L, 0L);
        }
    }


    FUNC_EXIT();
    return (TRUE);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PACMDRIVERID
IDriverFind(
    LPARAM                  lParam,
    DWORD                   fdwAdd
    )
{
    PACMDRIVERID        padid;
    DWORD               fdwAddType;

    FUNC("IDriverFind");

    acmBootDrivers();

    if (NULL == pag->padidFirst) {
        padid = NULL;
        goto EXIT;
    }

    fdwAddType = (ACM_DRIVERADDF_TYPEMASK & fdwAdd);

    //
    //
    //
    //
    for (padid = pag->padidFirst; padid; padid = padid->padidNext) {

        switch (fdwAddType) {
            case ACM_DRIVERADDF_NOTIFY:
            case ACM_DRIVERADDF_FUNCTION:
            case ACM_DRIVERADDF_NOTIFYHWND:
                if (padid->lParam == lParam) {
                    goto EXIT;
                }
                break;

            case ACM_DRIVERADDF_NAME:
                //
                //  This driver's alias is in lParam.
                //
                if( 0==lstrcmp( (LPTSTR)lParam, padid->szAlias ) ) {
                    goto EXIT;
                }
                break;

            default:
                return (NULL);
        }
    }

EXIT:
    FUNC_EXIT();
    return (padid);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverClose(
    HACMDRIVER              had,
    DWORD                   fdwClose)
{
    BOOL                f;
    PACMDRIVER          pad = (PACMDRIVER) had;
    PACMDRIVERID        padid = (PACMDRIVERID) pad->hadid;
    MMRESULT            mmRet;

    FUNC("IDriverClose");
    //
    //  Kill all the streams
    //
    if (NULL != pad->pasFirst) {
        VE_COND(pag->hadDestroy != had, ACMERR_BUSY);
        WARNMSG("Driver has open streams--forcing close!");
    }

    //
    //  if the driver is open for this instance, then close it down...
    //
    if ((NULL != pad->hdrvr) || (0L != pad->dwInstance)) {
        //
        //  clear the rest of the table entry
        //
//        f = (0L != IDriverMessageId((HACMDRIVERID)padid, DRV_CLOSE, 0L, 0L));
        f = (0L != IDriverMessage(had, DRV_CLOSE, 0L, 0L));

        VE_COND_ERR(!f && pag->hadDestroy != had);
    }

    //
    //  remove the driver instance from the linked list and free its memory
    //
    if (pad == padid->padFirst)
    {
        padid->padFirst = pad->padNext;
    }
    else
    {
        PACMDRIVER  padCur;

        //
        //
        //
        for (padCur = padid->padFirst;
             (NULL != padCur) && (pad != padCur->padNext);
             padCur = padCur->padNext)
            ;

        VE_COND_HANDLE(NULL == padCur);

        padCur->padNext = pad->padNext;
    }

    pad->uHandleType = TYPE_HACMNOTVALID;
    pad->Release();

    mmRet = MMSYSERR_NOERROR;

EXIT:
    FUNC_EXIT();
    return(mmRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverOpen(
    LPHACMDRIVER            phadNew,
    HACMDRIVERID            hadid,
    DWORD                   fdwOpen)
{
    ACMDRVOPENDESC      aod;
    PACMDRIVERID        padid = (PACMDRIVERID) hadid;
    PACMDRIVER          pad;
    MMRESULT            mmRet;
    DWORD               dw;

    FUNC("IDriverOpen");

    *phadNew = NULL;

    //
    //  if the driver has never been loaded, load it and keep it loaded.
    //
    if (0L == (ACMDRIVERID_DRIVERF_LOADED & padid->fdwDriver))
    {
        //
        //
        //
        mmRet = IDriverLoad(hadid, 0L);
        if (MMSYSERR_NOERROR != mmRet)
        {
            ERRMSG("acmDriverOpen(): driver had fatal error during load--unloading it now.");

            IDriverRemove((HACMDRIVERID)padid, 0L);
            return (mmRet);
        }
    }

    VE_COND_HANDLE(0 != (ACMDRIVERID_DRIVERF_NOTIFY & padid->fdwDriver));
    //
    //  do not allow opening of a disabled driver
    //
    VE_COND_NOTEN(0 != (ACMDRIVERID_DRIVERF_DISABLED & padid->fdwDriver));

    //
    //  alloc space for the new driver instance.
    //
    pad = new ACMDRIVER;
    VE_COND_NOMEM (NULL == pad);

    pad->uHandleType   = TYPE_HACMDRIVER;
    pad->pasFirst      = NULL;
    pad->hadid         = hadid;
    pad->fdwOpen       = fdwOpen;


    //
    //  add the new driver instance to the head of our list of open driver
    //  instances for this driver identifier.
    //
    pad->padNext       = padid->padFirst;
    padid->padFirst    = pad;

    aod.cbStruct       = sizeof(aod);
    aod.fccType        = ACMDRIVERDETAILS_FCCTYPE_AUDIOCODEC;
    aod.fccComp        = ACMDRIVERDETAILS_FCCCOMP_UNDEFINED;
    aod.dwVersion      = VERSION_MSACM;
    aod.dwFlags        = fdwOpen;
    aod.dwError        = MMSYSERR_NOERROR;
    aod.pszSectionName = padid->pszSection;
    aod.pszAliasName   = padid->szAlias;
    aod.dnDevNode      = padid->dnDevNode;


    dw = IDriverMessageId(hadid, DRV_OPEN, 0L, (LPARAM)(LPVOID)&aod);
    if (0L == dw) {

        IDriverClose((HACMDRIVER)pad, 0L);

        if (MMSYSERR_NOERROR == aod.dwError)
            GOTO_EXIT(mmRet = MMSYSERR_ERROR);

        GOTO_EXIT(mmRet = (MMRESULT) aod.dwError);
    }

    pad->dwInstance   = dw;
    pad->fnDriverProc = padid->fnDriverProc;

    *phadNew = (HACMDRIVER)pad;

    mmRet = MMSYSERR_NOERROR;
EXIT:
    FUNC_EXIT();
    return(mmRet);
}

BOOL acmFreeDrivers ()
{
    if (!pag)
    {
        return FALSE;
    }

    PACMDRIVERID padidT = pag->padidFirst;

    while (padidT)
    {
        PACMDRIVERID padidN = padidT->padidNext;
        IDriverRemove((HACMDRIVERID)padidT, 0);
        padidT = padidN;
    }

    pag->padidFirst = NULL;

    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverFree(
           HACMDRIVERID            hadid,
           DWORD                   fdwFree
           )
{
    MMRESULT            mmRet;
    PACMDRIVERID        padid = (PACMDRIVERID) hadid;
    BOOL                f;

    FUNC("IDriverFree");

    VE_COND_NOERR(0 == (ACMDRIVERID_DRIVERF_LOADED & padid->fdwDriver));
    VE_COND_ALLOC(NULL != padid->padFirst);

    //
    //  clear the rest of the table entry
    //
    f = TRUE;
    if (NULL != padid->fnDriverProc)
    {
        if (0 == (ACMDRIVERID_DRIVERF_NOTIFY & padid->fdwDriver))
        {
            // CloseDriver sequence to the driver function
            //
            //
            f = (0L != IDriverMessageId(hadid, DRV_CLOSE, 0L, 0L));
            if (f)
            {
                IDriverMessageId(hadid, DRV_DISABLE, 0L, 0L);
                IDriverMessageId(hadid, DRV_FREE, 0L, 0L);
            }

        }

        if (padid->hdrvr)
        {
            FreeLibrary((HMODULE)padid->hdrvr);
        }
    }

    VE_COND_ERR(!f);

    padid->fdwDriver  &= ~ACMDRIVERID_DRIVERF_LOADED;
    padid->dwInstance  = 0L;
    padid->hdrvr       = NULL;

    mmRet = MMSYSERR_NOERROR;
EXIT:
    FUNC_EXIT();
    return(mmRet);
}

//--------------------------------------------------------------------------;
//
//  MMRESULT IDriverReadRegistryData
//
//  Description:
//      Reads from the registry data which describes a driver.
//
//  Arguments:
//      PACMDRIVERID padid:
//          Pointer to ACMDRIVERID.
//
//  Return (MMRESULT):
//
//  History:
//      08/30/94    frankye
//
//  Notes:
//      This function will succeed only for drivers added with
//      ACM_DRIVERADDF_NAME.  The function attempts to open a key having
//      the same name as the szAlias member of ACMDRIVERID.  The data
//      stored under that key is described in the comment header for
//      IDriverWriteRegistryData().
//
//--------------------------------------------------------------------------;

MMRESULT FNLOCAL IDriverReadRegistryData(PACMDRIVERID padid)
{
    HKEY                    hkeyDriverCache;
    HKEY                    hkeyCache;
    DWORD                   fdwSupport;
    DWORD                   cFormatTags;
    DWORD                   cFilterTags;
    PACMFORMATTAGCACHE      paFormatTagCache;
    UINT                    cbaFormatTagCache;
    PACMFILTERTAGCACHE      paFilterTagCache;
    UINT                    cbaFilterTagCache;
    DWORD                   dwType;
    DWORD                   cbData;
    LONG                    lr;
    MMRESULT                mmr;

    TCHAR const cszDriverCache[]        = TEXT("AudioCompressionManager\\DriverCache");
    TCHAR const cszValfdwSupport[]      = TEXT("fdwSupport");
    TCHAR const cszValcFormatTags[]     = TEXT("cFormatTags");
    TCHAR const cszValaFormatTagCache[] = TEXT("aFormatTagCache");
    TCHAR const cszValcFilterTags[]     = TEXT("cFilterTags");
    TCHAR const cszValaFilterTagCache[] = TEXT("aFilterTagCache");

    hkeyDriverCache     = NULL;
    hkeyCache           = NULL;
    paFormatTagCache    = NULL;
    paFilterTagCache    = NULL;

    //
    //  We only keep registry data for ACM_DRIVERADDF_NAME drivers.
    //
    if (ACM_DRIVERADDF_NAME != (padid->fdwAdd & ACM_DRIVERADDF_TYPEMASK))
    {
        mmr = MMSYSERR_NOTSUPPORTED;
        goto Destruct;
    }

    //
    //  Open the registry keys under which we store the cache information
    //
    lr = RegOpenKeyEx( HKEY_LOCAL_MACHINE, cszDriverCache, 0, KEY_READ, &hkeyDriverCache );
    if ( ERROR_SUCCESS != lr )
    {
        hkeyDriverCache = NULL;
        mmr = MMSYSERR_NOMEM;   // Can't think of anything better
        goto Destruct;
    }

    lr = RegOpenKeyEx( hkeyDriverCache, padid->szAlias, 0, KEY_READ, &hkeyCache );
    if (ERROR_SUCCESS != lr)
    {
        mmr = ACMERR_NOTPOSSIBLE;    // Can't think of anything better
        goto Destruct;
    }

    //
    //  Attempt to read the fdwSupport for this driver
    //
    cbData = sizeof(fdwSupport);
    lr = RegQueryValueEx( hkeyCache, cszValfdwSupport, 0L, &dwType,
                          (LPBYTE)&fdwSupport, &cbData );

    if ( (ERROR_SUCCESS != lr) ||
         (REG_DWORD != dwType) ||
         (sizeof(fdwSupport) != cbData) )
    {
        mmr = ACMERR_NOTPOSSIBLE;
        goto Destruct;
    }

    //
    //  Attempt to read cFormatTags for this driver.  If more than zero
    //  format tags, then allocate a FormatTagCache array and attempt to
    //  read the cache array from the registry.
    //
    cbData = sizeof(cFormatTags);
    lr = RegQueryValueEx( hkeyCache, cszValcFormatTags, 0L, &dwType,
                          (LPBYTE)&cFormatTags, &cbData );

    if ( (ERROR_SUCCESS != lr) ||
         (REG_DWORD != dwType) ||
         (sizeof(cFormatTags) != cbData) )
    {
        mmr = ACMERR_NOTPOSSIBLE;
        goto Destruct;
    }

    if (0 != cFormatTags)
    {
        cbaFormatTagCache = (UINT)cFormatTags * sizeof(*paFormatTagCache);
        paFormatTagCache = (PACMFORMATTAGCACHE)LocalAlloc(LPTR, cbaFormatTagCache);
        if (NULL == paFormatTagCache) {
            mmr = MMSYSERR_NOMEM;
            goto Destruct;
        }

        cbData = cbaFormatTagCache;
        lr = RegQueryValueEx( hkeyCache, cszValaFormatTagCache, 0L, &dwType,
                              (LPBYTE)paFormatTagCache, &cbData );

        if ( (ERROR_SUCCESS != lr) ||
             (REG_BINARY != dwType) ||
             (cbaFormatTagCache != cbData) )
        {
            mmr = ACMERR_NOTPOSSIBLE;
            goto Destruct;
        }

    }

    //
    //  Attempt to read cFilterTags for this driver.  If more than zero
    //  filter tags, then allocate a FilterTagCache array and attempt to
    //  read the cache array from the registry.
    //
    cbData = sizeof(cFilterTags);
    lr = RegQueryValueEx( hkeyCache, cszValcFilterTags, 0L, &dwType,
                          (LPBYTE)&cFilterTags, &cbData );

    if ( (ERROR_SUCCESS != lr) ||
         (REG_DWORD != dwType) ||
         (sizeof(cFilterTags) != cbData) )
    {
        mmr = ACMERR_NOTPOSSIBLE;
        goto Destruct;
    }

    if (0 != cFilterTags)
    {
        cbaFilterTagCache = (UINT)cFilterTags * sizeof(*paFilterTagCache);
        paFilterTagCache = (PACMFILTERTAGCACHE)LocalAlloc(LPTR, cbaFilterTagCache);
        if (NULL == paFilterTagCache) {
            mmr = MMSYSERR_NOMEM;
            goto Destruct;
        }

        cbData = cbaFilterTagCache;
        lr = RegQueryValueEx( hkeyCache, cszValaFilterTagCache, 0L, &dwType,
                              (LPBYTE)paFilterTagCache, &cbData );

        if ( (ERROR_SUCCESS != lr) ||
             (REG_BINARY != dwType) ||
             (cbaFilterTagCache != cbData) )
        {
            mmr = ACMERR_NOTPOSSIBLE;
            goto Destruct;
        }

    }

    //
    //  Copy all the cache information to the ACMDRIVERID structure for
    //  this driver.  Note that we use the cache arrays that were allocated
    //  in this function.
    //
    padid->fdwSupport       = fdwSupport;
    padid->cFormatTags      = (UINT)cFormatTags;
    padid->paFormatTagCache = paFormatTagCache;
    padid->cFilterTags      = (UINT)cFilterTags;
    padid->paFilterTagCache = paFilterTagCache;

    mmr                     = MMSYSERR_NOERROR;

    //
    //  Clean up and return.
    //
Destruct:
    if (MMSYSERR_NOERROR != mmr)
    {
        if (NULL != paFormatTagCache) {
            LocalFree((HLOCAL)paFormatTagCache);
        }
        if (NULL != paFilterTagCache) {
            LocalFree((HLOCAL)paFilterTagCache);
        }
    }
    if (NULL != hkeyCache) {
        RegCloseKey(hkeyCache);
    }
    if (NULL != hkeyDriverCache) {
        RegCloseKey(hkeyDriverCache);
    }

    return (mmr);
}


MMRESULT
IDriverRemove(
    HACMDRIVERID            hadid,
    DWORD                   fdwRemove
    )
{
    PACMDRIVERID        padid;
    PACMDRIVERID        padidT;
    MMRESULT            mmRet;

    FUNC("IDriverRemove");

    padid   = (PACMDRIVERID)hadid;
    ASSERT( NULL != padid );

    pag     = padid->pag;


    if (0 != (ACMDRIVERID_DRIVERF_LOADED & padid->fdwDriver)) {
        mmRet = IDriverFree(hadid, 0L);
        if (MMSYSERR_NOERROR != mmRet) {
            if (pag->hadidDestroy != hadid) {
                ERRMSG("Driver cannot be removed while in use.");
                goto EXIT;
            }

            WARNMSG("Driver in use--forcing removal.");
        }
    }

    //
    //  remove the driver from the linked list and free its memory
    //
    if (padid == pag->padidFirst) {
        pag->padidFirst = padid->padidNext;
    } else {
        for (padidT = pag->padidFirst;
             padidT && (padidT->padidNext != padid);
             padidT = padidT->padidNext)
            ;

        if (NULL == padidT) {
            ERRMSG("Driver not in list!!!");
            mmRet = MMSYSERR_INVALHANDLE;
            goto EXIT;
        }

        padidT->padidNext = padid->padidNext;
    }

    padid->padidNext = NULL;

    //
    //  free all resources allocated for this thing
    //
    if (NULL != padid->paFormatTagCache)
        LocalFree((HLOCAL)padid->paFormatTagCache);

    if (NULL != padid->paFilterTagCache)
        LocalFree((HLOCAL)padid->paFilterTagCache);

    if (NULL != padid->pstrPnpDriverFilename)
        LocalFree((HLOCAL)padid->pstrPnpDriverFilename);

    if (0 != padid->lParam)
        LocalFree((HLOCAL)padid->lParam);

    //
    //  set handle type to 'dead'
    //
    padid->uHandleType = TYPE_HACMNOTVALID;
    LocalFree((HLOCAL)padid);

    mmRet = MMSYSERR_NOERROR;

EXIT:
    FUNC_EXIT();
    return(mmRet);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverAdd(
    LPHACMDRIVERID          phadidNew,
    HINSTANCE               hinstModule,
    LPARAM                  lParam,
    DWORD                   dwPriority,
    DWORD                   fdwAdd,
    WCHAR                   *szAlias
    )
{
    PACMDRIVERID        padid;
    PACMDRIVERID        padidT;
    MMRESULT            mmRet = MMSYSERR_ERROR;

    FUNC("IDriverAdd");

    *phadidNew = NULL;

    //  new driver - Alloc space for the new driver identifier.
    padid = new ACMDRIVERID;
    if (NULL == padid)
    {
        return mmRet;
    }

    //
    //  save the filename, function ptr or hinst, and ptr back to garb
    //
    padid->pag          = pag;
    padid->uHandleType  = TYPE_HACMDRIVERID;
    padid->uPriority    = 0;
    padid->fdwAdd       = fdwAdd;
    padid->htask        = NULL;
    if (lParam)
    {
        int iLen = sizeof(TCHAR)*(lstrlen((LPCTSTR)lParam) + 2);
        LPTSTR lpszDllName = (LPTSTR)LocalAlloc(LPTR, (long)iLen);
        if (lpszDllName)
        {
            StringCbCopy(lpszDllName, iLen, (LPCTSTR)lParam);
        }
        padid->lParam = (LPARAM)lpszDllName;
    }

    if (szAlias)
    {
        StringCbCopy(padid->szAlias, sizeof(padid->szAlias), szAlias);
    }

    padidT = pag->padidFirst;
    for ( ; padidT && padidT->padidNext; padidT = padidT->padidNext)
        ;

    if (NULL != padidT)
        padidT->padidNext = padid;
    else {
        pag->padidFirst = padid;
    }

    acm_PrintList();

    //
    //  We need to get some data about this driver into the ACMDRIVERID
    //  for this driver.  First see if we can get this data from the
    //  registry.  If that doesn't work, then we'll load the driver
    //  and that will load the necessary data into ACMDRIVERID.
    //
    mmRet = IDriverReadRegistryData(padid);
    if (MMSYSERR_NOERROR != mmRet)
    {
        //
        //  Registry information doesn't exist or appears out of date.  Load
        //  the driver now so we can get some information about the driver
        //  into the ACMDRIVERID for this driver.
        //
        WARNMSG("IDriverAdd: Couldn't load registry data for driver.  Attempting to load.");
        mmRet = IDriverLoad((HACMDRIVERID)padid, 0L);
    }

    if (MMSYSERR_NOERROR != mmRet)
    {
        ERRMSG("IDriverAdd(): driver had fatal error during load--unloading it now.");
        IDriverRemove((HACMDRIVERID)padid, 0L);
        return (mmRet);
    }


    //
    //  Success!  Store the new handle, notify 16-bit side of driver change,
    //  and return.
    //
    *phadidNew = (HACMDRIVERID)padid;

    mmRet = MMSYSERR_NOERROR;

    FUNC_EXIT();
    return (mmRet);
}

//------------------------------------------------------------------------------
//
//  BOOL IDriverLockPriority
//
//  Description:
//      This routine manages the htaskPriority lock (pag->htaskPriority).
//
//      ACMPRIOLOCK_GETLOCK:     If the lock is free, set it to this task.
//      ACMPRIOLOCK_RELEASELOCK: If the lock is yours, release it.
//      ACMPRIOLOCK_ISMYLOCK:    Return TRUE if this task has the lock.
//      ACMPRIOLOCK_ISLOCKED:    Return TRUE if some task has the lock.
//      ACMPRIOLOCK_LOCKISOK:    Return TRUE if it's unlocked, or if it's
//                                  my lock - ie. if it's not locked for me.
//
//  Arguments:
//      PACMGARB pag:
//      HTASK htask:    The current task.
//      UINT flags:
//
//  Return (BOOL):  Success or failure.  Failure on RELEASELOCK means that
//                  the lock didn't really belong to this task.
//
//------------------------------------------------------------------------------
BOOL
IDriverLockPriority(
    HTASK                   htask,
    UINT                    uRequest
    )
{
//    ASSERT( uRequest >= ACMPRIOLOCK_FIRST );
//    ASSERT( uRequest <= ACMPRIOLOCK_LAST );
//    ASSERT( htask == GetCurrentTask() );
    FUNC("IDriverLockPriority");

    switch( uRequest )
    {
        case ACMPRIOLOCK_GETLOCK:
            if( NULL != pag->htaskPriority )
                return FALSE;
            pag->htaskPriority = htask;
            return TRUE;

        case ACMPRIOLOCK_RELEASELOCK:
            if( htask != pag->htaskPriority )
                return FALSE;
            pag->htaskPriority = NULL;
            return TRUE;

        case ACMPRIOLOCK_ISMYLOCK:
            return ( htask == pag->htaskPriority );

        case ACMPRIOLOCK_ISLOCKED:
            return ( NULL != pag->htaskPriority );

        case ACMPRIOLOCK_LOCKISOK:
            return ( htask == pag->htaskPriority ||
                     NULL == pag->htaskPriority );
    }

    FUNC_EXIT();
    return FALSE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IDriverPriority(
    PACMDRIVERID            padid,
    DWORD                   dwPriority,
    DWORD                   fdwPriority)
{
    PACMDRIVERID        padidT;
    PACMDRIVERID        padidPrev;
    DWORD               fdwAble;
    UINT                uCurPriority;
    MMRESULT            mmRet;

    FUNC("IDriverPriority");

    //
    //  Enable or disable the driver.
    //
    fdwAble = ( ACM_DRIVERPRIORITYF_ABLEMASK & fdwPriority );

    switch (fdwAble)
    {
        case ACM_DRIVERPRIORITYF_ENABLE:
            padid->fdwDriver &= ~ACMDRIVERID_DRIVERF_DISABLED;
            break;

        case ACM_DRIVERPRIORITYF_DISABLE:
            padid->fdwDriver |= ACMDRIVERID_DRIVERF_DISABLED;
            break;
    }


    //
    //  Change the priority.  If dwPriority==0, then we only want to
    //  enable/disable the driver - leave the priority alone.
    //
    if( 0L != dwPriority  &&  dwPriority != padid->uPriority )
    {
        //
        //  first remove the driver from the linked list
        //
        if (padid == pag->padidFirst)
        {
            pag->padidFirst = padid->padidNext;
        }
        else
        {
            padidT = pag->padidFirst;

            for ( ; NULL != padidT; padidT = padidT->padidNext)
            {
                if (padidT->padidNext == padid)
                    break;
            }

            VE_COND_HANDLE(NULL == padidT);
            padidT->padidNext = padid->padidNext;
        }

        padid->padidNext = NULL;


        //
        //  now add the driver at the correct position--this will be in
        //  the position of the current global driver
        //
        uCurPriority = 1;

        padidPrev = NULL;
        for (padidT = pag->padidFirst; NULL != padidT; )
        {
            //
            //  skip local and notify handles
            //
            if (0 == ((ACMDRIVERID_DRIVERF_LOCAL | ACMDRIVERID_DRIVERF_NOTIFY) & padidT->fdwDriver))
            {
                if (uCurPriority == dwPriority)
                {
                    break;
                }

                uCurPriority++;
            }

            padidPrev = padidT;
            padidT = padidT->padidNext;
        }

        if (NULL == padidPrev)
        {
            padid->padidNext = pag->padidFirst;
            pag->padidFirst = padid;
        }
        else
        {
            padid->padidNext = padidPrev->padidNext;
            padidPrev->padidNext = padid;
        }
    }

    mmRet = MMSYSERR_NOERROR;
EXIT:
    FUNC_EXIT();
    return(mmRet);
}




