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
#include <acmi.h>
#include <waveproxyacm.h>


CWaveProxy *CreateWaveProxyACM(
    DWORD dwCallback,
    DWORD dwInstance,
    DWORD fdwOpen,
    BOOL bIsOutput,
    CWaveLib *pCWaveLib
    )
{
    return new CWaveProxyACM(dwCallback,dwInstance,fdwOpen,bIsOutput,pCWaveLib);
}

CWaveProxyACM::CWaveProxyACM(
    DWORD dwCallback,
    DWORD dwInstance,
    DWORD fdwOpen,
    BOOL bIsOutput,
    CWaveLib *pCWaveLib
    ) :
    CWaveProxy(dwCallback, dwInstance, fdwOpen, bIsOutput, pCWaveLib),
    m_mmrClient(MMSYSERR_NOERROR),
    m_uHeuristic(0),
    m_had(0),
    m_has(0),
    m_pwfxSrc(NULL),
    m_pwfxDst(NULL),
    m_fdwSupport(0),
    m_pwfxClient(NULL),
    m_pwfxReal(NULL),
    m_cbwfxReal(0)
{
    return;
}

CWaveProxyACM::~CWaveProxyACM()
{
    return;
}

MMRESULT CWaveProxyACM::waveQueueBuffer( LPWAVEHDR pwh, UINT cbwh )
{
    MMRESULT            mmRet;

    LPWAVEHDR           pwhShadow;
    LPACMSTREAMHEADER   pash;
    DWORD               cbShadowData;

    // Get the conversion stream header...
    pash = (LPACMSTREAMHEADER)pwh->reserved;
    if (NULL == pash)
        {
        DEBUGMSG(ZONE_ERROR, (TEXT("!waveQueueBuffer: very strange--reserved field is 0???")));
        mmRet = WAVERR_UNPREPARED;
        goto Exit;
        }

    // Now get ptr to the shadow header we're going to pass down to the wave API.
    pwhShadow = (LPWAVEHDR)pash->dwSrcUser;

    if (m_bIsOutput)
        {
        // On output we need to do the conversion here and pass the converted data down
        pash->cbDstLengthUsed = 0L;
        if (0L != pwh->dwBufferLength)
            {
            pash->pbSrc       = (LPBYTE)pwh->lpData;        // Shouldn't need to do this- we set it in prepare.
            pash->cbSrcLength = pwh->dwBufferLength;        // Do need to do this, as it can change... should verify it didn't grow from orig size though
            pash->pbDst       = (LPBYTE)pwhShadow->lpData;  // Shouldn't need to do this- we set it in prepare.
            ////////////pash->cbDstLength = xxx;        !!! leave as is !!!

            // Do the conversion
            mmRet = acmStreamConvert(m_has, pash, 0L);
            if (MMSYSERR_NOERROR != mmRet)
                {
                DEBUGMSG(ZONE_ERROR, (TEXT("!waveOutWrite: conversion failed!")));
                goto Exit;
                }
            }

        if (0L == pash->cbDstLengthUsed)
            {
            // Not an error- this can happen in some edge cases. Just proceed as if everything's OK.
            DEBUGMSG(ZONE_VERBOSE, (TEXT("waveOutWrite: nothing converted--no data in output buffer.")));
            }

        pwhShadow->dwBufferLength = pash->cbDstLengthUsed;
        }
    else
        {
        // On input we do the conversion in the callback, so on the waveInAddBuffer call we just
        // need to setup the shadow header params.

        // We must block align the input buffer.
        UINT u = m_pwfxClient->nBlockAlign;
        cbShadowData = (pwh->dwBufferLength / u) * u;

        //  determine amount of data we need from the _real_ device. give a _block aligned_ destination buffer...
        mmRet = acmStreamSize(m_has,
                              cbShadowData,
                              &cbShadowData,
                              ACM_STREAMSIZEF_DESTINATION);

        if (MMSYSERR_NOERROR != mmRet)
            {
            DEBUGMSG(ZONE_ERROR, (TEXT("!waveQueueBuffer: failed to get conversion size!")));
            mmRet = MMSYSERR_NOMEM;
            goto Exit;
            }

        pwhShadow->dwBufferLength  = cbShadowData;
        }

    pwhShadow->dwFlags = pwh->dwFlags;
    pwhShadow->dwLoops = pwh->dwLoops;
    pwhShadow->dwBytesRecorded = 0L;

    //  Clear the done bit of the caller's wave header (not done) and set the inqueue bit
    pwh->dwFlags = (pwh->dwFlags & ~WHDR_DONE) | WHDR_INQUEUE;

    //  Add the shadow buffer to the real (maybe) device's queue...
    mmRet = CWaveProxy::waveQueueBuffer(pwhShadow, sizeof(WAVEHDR));
    if (MMSYSERR_NOERROR != mmRet)
        {
        pwh->dwFlags &= ~WHDR_INQUEUE;
        DEBUGMSG(ZONE_ERROR, (TEXT("!waveQueueBuffer failed!")));
        }

Exit:
    return mmRet;
}


MMRESULT CWaveProxyACM::wavePrepareHeader( LPWAVEHDR pwh, UINT cbwh )
{
    MMRESULT            mmRet;
    LPWAVEHDR           pwhShadow;
    LPACMSTREAMHEADER   pash;
    DWORD               cbShadow;
    DWORD               dwLen;
    DWORD               fdwSize;

    //
    //  if we are in convert mode, allocate a 'shadow' wave header
    //  and buffer to hold the converted wave bits
    //
    //  we need to pagelock the callers header but *not* his buffer
    //  because we touch it in wXdWaveMapCallback (to set the DONE bit)
    //
    //  here is the state of the dwUser and reserved fields in
    //  both buffers.
    //
    //      client's header (sent to the wavemapper by the 'user')
    //
    //          reserved        points to the stream header used for
    //                          conversions with the ACM. the wavemapper
    //                          is the driver so we can use this.
    //          dwUser          for use by the 'user' (client)
    //
    //      shadow header (sent to the real device by the wavemapper)
    //
    //          reserved        for use by the real device
    //          dwUser          points to the client's header. (the
    //                          wavemapper is the user in this case)
    //
    //      acm stream header (created by us for conversion work)
    //
    //          dwUser          points to CWaveProxyACM object (not used?)
    //          dwSrcUser       points to shadow header
    //          dwDstUser       original source buffer size (prepared with)
    //

    dwLen = pwh->dwBufferLength;
    if (m_bIsOutput)
        {
        //  determine size for the shadow buffer (this will be the buffer
        //  that we convert to before writing the data to the underlying
        //  device).
        fdwSize = ACM_STREAMSIZEF_SOURCE;
        }
    else
        {
        //  block align the destination buffer if the caller didn't read
        //  our documentation...
        UINT u = m_pwfxClient->nBlockAlign;
        dwLen = (dwLen / u) * u;

        //
        //  determine size for shadow buffer (the buffer that we will give
        //  to the _real_ device). give a _block aligned_ destination buffer
        //
        fdwSize = ACM_STREAMSIZEF_DESTINATION;
        }

    mmRet = acmStreamSize(m_has, dwLen, &dwLen, fdwSize);
    if (MMSYSERR_NOERROR != mmRet)
        {
        DEBUGMSG(ZONE_ERROR, (TEXT("!mapWavePrepareHeader: failed to get conversion size!")));
        return MMSYSERR_NOMEM;
        }

    //
    //  allocate the shadow WAVEHDR
    //
    //  NOTE: add four bytes to guard against GP faulting with stos/lods
    //  code that accesses the last byte/word/dword in a segment--very
    //  easy to do...
    //

    // Protect against overflow
    if (dwLen >= (UINT_MAX - (sizeof(WAVEHDR) + sizeof(ACMSTREAMHEADER) + 4)))
        {
        DEBUGMSG(ZONE_ERROR, (TEXT("!mapWavePrepareHeader(): could not alloc bytes for shadow!")));
        return MMSYSERR_NOMEM;
        }

    cbShadow  = sizeof(WAVEHDR) + sizeof(ACMSTREAMHEADER) + dwLen + 4;
    pwhShadow = (LPWAVEHDR)LocalAlloc(LMEM_FIXED, cbShadow);
    if (NULL == pwhShadow)
        {
        DEBUGMSG(ZONE_ERROR, (TEXT("!mapWavePrepareHeader(): could not alloc bytes for shadow!")));
        return MMSYSERR_NOMEM;
        }

    pwhShadow->lpNext = 0;
    pwhShadow->reserved = 0;

    // The ACMSTREAMHEADER struct is just after the shadow header in the per-header allocate memory
    pash = (LPACMSTREAMHEADER)(pwhShadow + 1);

    pash->cbStruct  = sizeof(*pash);
    pash->fdwStatus = 0L;
    pash->dwUser    = (DWORD)this;  // Not needed???

    //
    //  fill in the shadow wave header, the dwUser field will point
    //  back to the original header, so we can get back to it
    //
    pwhShadow->lpData          = (LPSTR)(pash + 1);
    pwhShadow->dwBufferLength  = dwLen;
    pwhShadow->dwBytesRecorded = 0;
    pwhShadow->dwUser          = (DWORD)pwh;


    //
    //  now prepare the shadow wavehdr
    //
    if (m_bIsOutput)
        {
        pwhShadow->dwFlags = pwh->dwFlags & (WHDR_BEGINLOOP|WHDR_ENDLOOP);
        pwhShadow->dwLoops = pwh->dwLoops;

        //  output: our source is the client (we get data from the
        //  client and convert it into something for the physical
        //  device)
        pash->pbSrc         = (LPBYTE)pwh->lpData;
        pash->cbSrcLength   = pwh->dwBufferLength;
        pash->cbSrcLengthUsed = 0;
        pash->dwSrcUser     = (DWORD)pwhShadow;
        pash->pbDst         = (LPBYTE)pwhShadow->lpData;
        pash->cbDstLength   = pwhShadow->dwBufferLength;
        pash->cbDstLengthUsed = 0;
        pash->dwDstUser     = pwh->dwBufferLength;
        }
    else
        {
        pwhShadow->dwFlags = 0L;
        pwhShadow->dwLoops = 0L;

        //  input: our source is the shadow (we get data from the
        //  physical device and convert it into the clients buffer)
        pash->pbSrc         = (LPBYTE)pwhShadow->lpData;
        pash->cbSrcLength   = pwhShadow->dwBufferLength;
        pash->cbSrcLengthUsed = 0;
        pash->dwSrcUser     = (DWORD)pwhShadow;
        pash->pbDst         = (LPBYTE)pwh->lpData;
        pash->cbDstLength   = pwh->dwBufferLength;
        pash->cbDstLengthUsed = 0;
        pash->dwDstUser     = pwhShadow->dwBufferLength;
        }

    mmRet = CWaveProxy::wavePrepareHeader(pwhShadow, sizeof(WAVEHDR));
    if (MMSYSERR_NOERROR == mmRet)
        {
        mmRet = acmStreamPrepareHeader(m_has, pash, 0L);
        if (MMSYSERR_NOERROR != mmRet)
            {
            CWaveProxy::waveUnprepareHeader(pwhShadow, sizeof(WAVEHDR));
            }
        }

    if (MMSYSERR_NOERROR != mmRet)
        {
        LocalFree(pwhShadow);
        return (mmRet);
        }

    //  the reserved field of the callers WAVEHDR will contain the
    //  shadow LPWAVEHDR
    pwh->reserved = (DWORD)pash;
    pwh->dwFlags |= WHDR_PREPARED;

    return (MMSYSERR_NOERROR);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT CWaveProxyACM::waveUnprepareHeader( LPWAVEHDR pwh, UINT cbwh )
{
    MMRESULT            mmRet;
    LPWAVEHDR           pwhShadow;
    LPACMSTREAMHEADER   pash;
    DWORD               cbShadowData;

    pash      = (LPACMSTREAMHEADER)pwh->reserved;
    pwhShadow = (LPWAVEHDR)pash->dwSrcUser;

    if (m_bIsOutput)
        {
        cbShadowData = pash->cbDstLength;

        pash->cbSrcLength = pash->dwDstUser;
        ////////pash->cbDstLength = xxx;        !!! don't touch this !!!
        }
    else
        {
        cbShadowData = pash->dwDstUser;

        pash->cbSrcLength = pash->dwDstUser;
        ////////pash->cbDstLength = xxx;        !!! don't touch this !!!
        }

    pwhShadow->dwBufferLength = cbShadowData;
    mmRet = CWaveProxy::waveUnprepareHeader(pwhShadow, sizeof(WAVEHDR));

    //
    //  Unprepare stream and client headers only if shadow header was
    //  unprepared.
    //
    if (0 == (WHDR_PREPARED & pwhShadow->dwFlags))
        {

        acmStreamUnprepareHeader(m_has, pash, 0L);

        //
        //  free the shadow buffer--mark caller's wave header as unprepared
        //  and succeed the call
        //
        LocalFree(pwhShadow);

        pwh->reserved = 0L;
        pwh->dwFlags &= ~WHDR_PREPARED;

        }

    return (mmRet);
}

MMRESULT CWaveProxyACM::waveClose()
{
    MMRESULT mmRet;

    mmRet = CWaveProxy::waveClose();
    if (mmRet == MMSYSERR_NOERROR)
        {
        if (m_has)
            {
            acmStreamClose(m_has, 0L);
            m_has=0;
            }

        if (m_had)
            {
            acmDriverClose(m_had, 0L);
            m_had=0;
            }

        LocalFree(m_pwfxClient);
        m_pwfxClient=NULL;

        LocalFree(m_pwfxReal);
        m_pwfxReal=NULL;

        m_pwfxSrc=NULL;
        m_pwfxDst=NULL;
        }

    return mmRet;
}

// waveCommonOpenMapACM tries to use the ACM converter to convert to a format that the wave driver can understand
MMRESULT CWaveProxyACM::waveOpen( LPCWAVEFORMATEX pwfx )
{
    MMRESULT mmRet = MMSYSERR_NOERROR;

    // Keep ptrs to client & driver (real) wave formats
    m_pwfxClient = CopyWaveformat(pwfx);
    if (!m_pwfxClient)
        {
        mmRet = MMSYSERR_NOMEM;
        goto Exit;
        }

    //  Determine size of largest possible mapped destination format.
    mmRet = acmMetrics(NULL, ACM_METRIC_MAX_SIZE_FORMAT, &m_cbwfxReal);
    if (MMSYSERR_NOERROR != mmRet)
        {
        // if ACM is stubbed out, give up here.
        mmRet = WAVERR_BADFORMAT;
        goto Exit;
        }

    m_pwfxReal = (LPWAVEFORMATEX)LocalAlloc(LMEM_FIXED, m_cbwfxReal);
    if (!m_pwfxReal)
        {
        mmRet = MMSYSERR_NOMEM;
        goto Exit;
        }

    // Direction determines which is input & which is output format
    if (m_bIsOutput)
        {
        m_pwfxSrc = m_pwfxClient;
        m_pwfxDst = m_pwfxReal;
        }
    else
        {
        m_pwfxSrc = m_pwfxReal;
        m_pwfxDst = m_pwfxClient;
        }

    // Now search for a converter that will work with this format
    mmRet = FindConverterMatch();

    Exit:

    if ( (mmRet != MMSYSERR_NOERROR) || (WAVE_FORMAT_QUERY & m_fdwOpen) )
        {
        LocalFree(m_pwfxClient);
        m_pwfxClient=NULL;
        LocalFree(m_pwfxReal);
        m_pwfxReal=NULL;
        }

    return mmRet;
}

//------------------------------------------------------------------------------
//      Test all drivers to see if one can convert the requested format
//      into a format supported by an available wave device
//------------------------------------------------------------------------------
#define MAX_HEURISTIC 4
MMRESULT CWaveProxyACM::FindConverterMatch()
{
    MMRESULT    mmRet;

    // Note that in order for this function to be called, we must
    // not have been able to this device to play this format
    // natively.  If our client format is PCM, then it is likely
    // that it needs a PCM conversion.  So, we'll skip heuristic
    // 0 (the "suggest anything" pass) and start with heuristic 1
    // (the fist of the "suggest pcm" passes).
    //
    // This special casing hopefully finds the required conversion
    // quicker.  And, just as importantly, since a "suggest
    // anything" would include something like msadpcm suggesting
    // pcm->msadpcm, it also will help to avoid querying a bunch of
    // codecs unnecessarily.
    //
    // However, in the even that this fails we'll still go back and try
    // heuristic 0 at the end.
    UINT uHeuristicLast;
    if (WAVE_FORMAT_PCM == m_pwfxClient->wFormatTag)
        {
        // Start at 1, wrap around to 0
        m_uHeuristic = 1;
        uHeuristicLast = 0;
        }
    else
        {
        // Start at 0, go to the end
        m_uHeuristic = 0;
        uHeuristicLast = MAX_HEURISTIC;
        }

    // Assume it's a bad format
    m_mmrClient = WAVERR_BADFORMAT;
    while (1)
        {
        if (0 == m_uHeuristic) // for the 'suggest anything' pass, allow converters and codecs
            {
            m_fdwSupport = ACMDRIVERDETAILS_SUPPORTF_CONVERTER | ACMDRIVERDETAILS_SUPPORTF_CODEC;
            }
        else if (WAVE_FORMAT_PCM == m_pwfxClient->wFormatTag)
            {
            m_fdwSupport = ACMDRIVERDETAILS_SUPPORTF_CONVERTER;
            }
        else
            {
            m_fdwSupport = ACMDRIVERDETAILS_SUPPORTF_CODEC;
            }

        mmRet = acmDriverEnum(CWaveProxyACM::s_DriverEnumCallback, (DWORD)this, 0L);
        if ( (MMSYSERR_NOERROR == mmRet) && (MMSYSERR_NOERROR == m_mmrClient) )
            {
            break;  // Success!
            }

        if (m_uHeuristic==uHeuristicLast)
            {
            break;  // Failed.
            }

        m_uHeuristic++;
        if (m_uHeuristic>MAX_HEURISTIC)
            {
            m_uHeuristic = 0; // Wrap around to 0
            }
        }

    return m_mmrClient;
} // FindConverterMatch()

//------------------------------------------------------------------------------
// DriverEnumCallback is called back from within acmDriverEnum (called from FindConverterMatch)
// once for each ACM driver in the system.
// Return Values:
//      TRUE: continue enumeration.
//      FALSE: halt enumeration (e.g. we found an ACM driver that can do our conversion)
BOOL CWaveProxyACM::s_DriverEnumCallback(HACMDRIVERID hadid, DWORD dwInstance, DWORD fdwSupport)
{
    CWaveProxyACM *pProxyACM = (CWaveProxyACM *)dwInstance;
    return pProxyACM->DriverEnumCallback(hadid, fdwSupport);
}

BOOL CWaveProxyACM::DriverEnumCallback( HACMDRIVERID hadid, DWORD fdwSupport )
{
    MMRESULT mmRet;
    BOOL bRet = TRUE;    // Default to TRUE (continue enumeration)
    BOOL bQuery = (0 != (WAVE_FORMAT_QUERY & m_fdwOpen));

    m_had = 0;

    //  check if support required
    if (0 == (m_fdwSupport & fdwSupport))
        {
        goto Exit; //  skip to next driver..
        }

    ACMFORMATTAGDETAILS aftd;
    aftd.cbStruct    = sizeof(aftd);
    aftd.dwFormatTag = m_pwfxClient->wFormatTag;
    aftd.fdwSupport  = 0L;

    mmRet = acmFormatTagDetails((HACMDRIVER)hadid,
                                &aftd,
                                ACM_FORMATTAGDETAILSF_FORMATTAG);
    if (MMSYSERR_NOERROR != mmRet)
        {
        goto Exit;        //  skip to next driver..
        }

    if (0 == (m_fdwSupport & aftd.fdwSupport))
        {
        goto Exit;        //  skip to next driver..
        }

    // Open this driver and get back a driver handle
    mmRet = acmDriverOpen(&m_had, hadid, 0L);
    if (MMSYSERR_NOERROR != mmRet)
        {
        goto Exit;        //  skip to next driver..
        }

    // Choose which formats we'll try to convert to
    DWORD dwFormatSuggestFlags;
    switch (m_uHeuristic)
        {
        case 0: //  suggest anything!
            dwFormatSuggestFlags=0;
            break;

        case 1: //  suggest PCM format for the Client
            m_pwfxReal->wFormatTag = WAVE_FORMAT_PCM;
            dwFormatSuggestFlags = ACM_FORMATSUGGESTF_WFORMATTAG;
            break;

        case 2: //  suggest MONO PCM format for the Client
            m_pwfxReal->wFormatTag = WAVE_FORMAT_PCM;
            m_pwfxReal->nChannels  = 1;
            dwFormatSuggestFlags = ACM_FORMATSUGGESTF_WFORMATTAG | ACM_FORMATSUGGESTF_NCHANNELS;
            break;

        case 3: //  suggest 8 bit PCM format for the Client
            m_pwfxReal->wFormatTag     = WAVE_FORMAT_PCM;
            m_pwfxReal->wBitsPerSample = 8;
            dwFormatSuggestFlags = ACM_FORMATSUGGESTF_WFORMATTAG | ACM_FORMATSUGGESTF_WBITSPERSAMPLE;
            break;

        case 4: //  suggest 8 bit MONO PCM format for the Client
            m_pwfxReal->wFormatTag     = WAVE_FORMAT_PCM;
            m_pwfxReal->nChannels      = 1;
            m_pwfxReal->wBitsPerSample = 8;
            dwFormatSuggestFlags = ACM_FORMATSUGGESTF_WFORMATTAG | ACM_FORMATSUGGESTF_NCHANNELS | ACM_FORMATSUGGESTF_WBITSPERSAMPLE;
            break;
        default:
            goto Exit;
        }

    // What format does this driver suggest?
    // Format will be returned in the pwfxReal struct...
    mmRet = acmFormatSuggest(m_had,
                             m_pwfxClient,
                             m_pwfxReal,
                             m_cbwfxReal,
                             dwFormatSuggestFlags);

    if (MMSYSERR_NOERROR != mmRet)
        {
        goto Exit;        //  skip to next driver..
        }

    // Try to open converter- can it open real time?
    // Remember that pwfxSrc/pwfxDst are aliased to pwfxClient/pwfxReal depending on whether this is input or output
    // E.g. for output pwfxDst==pwfxReal and pwfxSrc==pwfxClient
    mmRet = acmStreamOpen(&m_has,
                          m_had,
                          m_pwfxSrc,
                          m_pwfxDst,
                          NULL,
                          0L,
                          0L,
                          bQuery ? ACM_STREAMOPENF_QUERY : 0);

    if (MMSYSERR_NOERROR != mmRet)
        {
        goto Exit;        //  skip to next driver..
        }

    // Try to open the wave device with this format
    // If this is just a query, no handle is returned
    mmRet = CWaveProxy::waveOpen(m_pwfxReal);

    if (MMSYSERR_NOERROR != mmRet)
        {
        m_mmrClient = mmRet;
        goto Exit;
        }

    // If no err, then we found a suitable mapping.  Return FALSE to abort
    // the enumeration. The ACM driver, stream, and wave device will be left opened.
    m_mmrClient = MMSYSERR_NOERROR;
    bRet = FALSE;

    Exit:
    if ( (bRet == TRUE) || (bQuery) )
        {
        if (m_has)
            {
            acmStreamClose(m_has, 0L);
            m_has=0;
            }

        if (m_had)
            {
            acmDriverClose(m_had, 0L);
            m_had = 0;
            }
        }

    return bRet;
} // DriverEnumCallback()


//------------------------------------------------------------------------------
//
//  Notes on error code priorities  FrankYe     09/28/94
//
//  The error code that is returned to the client and the error code
//  that is returned by internal functions are not always the same.  The
//  primary reason for this is the way we handle MMSYSERR_ALLOCATED and
//  WAVERR_BADFORMAT in multiple device situations.
//
//  For example, suppose we have two devices.  If one returns ALLOCATED and
//  the other returns BADFORMAT then we prefer to return ALLOCATED to the
//  client because BADFORMAT implies no devices understand the format.  So,
//  for the client, we prefer to return ALLOCATED over BADFORMAT.
//
//  On the other hand, we want the mapper to be able to take advantage of
//  situations where all the devices are allocated.  If all devices are
//  allocated then there is no need to continue trying to find a workable
//  map stream.  So, for internal use, we prefer BADFORMAT over ALLOCATED.
//  That way if we see ALLOCATED then we know _all_ devices are allocated
//  and we can abort trying to create a map stream.  (If the client sees
//  ALLOCATED, it only means that at least one device is allocated.)
//
//  Client return codes are usually stored in the mmrClient member of the
//  MAPSTREAM structure.  Internal return codes are returned via
//  function return values.
//
//  Below are functions that prioritize error codes and update error codes
//  given the last err, the current err, and the priorities of the errs.
//  Notice that the prioritization of the err codes for the client is very
//  similar to for internal use.  The only difference is the ordering of
//  MMSYSERR_ALLOCATED and WAVERR_BADFORMAT for reasons stated above.
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
UINT mapErrGetClientPriority(
                            MMRESULT mmRet
                            )
{
    switch (mmRet)
        {
        case MMSYSERR_NOERROR:
            return 6;
        case MMSYSERR_ALLOCATED:
            return 5;
        case WAVERR_BADFORMAT:
            return 4;
        case WAVERR_SYNC:
            return 3;
        case MMSYSERR_NOMEM:
            return 2;
        default:
            return 1;
        case MMSYSERR_ERROR:
            return 0;
        }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID mapErrSetClientError(
                         MMRESULT *lpmmRet,
                         MMRESULT mmRet
                         )
{
    if (mapErrGetClientPriority(mmRet) > mapErrGetClientPriority(*lpmmRet))
        {
        *lpmmRet = mmRet;
        }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
UINT mapErrGetPriority(
                      MMRESULT mmRet
                      )
{
    switch (mmRet)
        {
        case MMSYSERR_NOERROR:
            return 6;
        case WAVERR_BADFORMAT:
            return 5;
        case MMSYSERR_ALLOCATED:
            return 4;
        case WAVERR_SYNC:
            return 3;
        case MMSYSERR_NOMEM:
            return 2;
        default:
            return 1;
        case MMSYSERR_ERROR:
            return 0;
        }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
VOID mapErrSetError(
                   MMRESULT *lpmmRet,
                   MMRESULT mmRet
                   )
{
    if (mapErrGetPriority(mmRet) > mapErrGetPriority(*lpmmRet))
        {
        *lpmmRet = mmRet;
        }
}

inline DWORD MulDivRN( DWORD a, DWORD b, DWORD c )
{
    return (DWORD)( ((a * b) + c/2) / c );
}

MMRESULT CWaveProxyACM::waveGetPosition( LPMMTIME pmmt, UINT cbmmt )
{
    MMRESULT mmr;

    if ((TIME_SAMPLES != pmmt->wType) && (TIME_BYTES != pmmt->wType))
        {
        DEBUGMSG(ZONE_VERBOSE, (TEXT("mapWaveGetPosition: time format")));
        pmmt->wType = TIME_BYTES;
        }

    mmr = CWaveProxy::waveGetPosition(pmmt, cbmmt);

    if (mmr == MMSYSERR_NOERROR)
        {
        //  convert real time to client's time
        switch (pmmt->wType)
            {
            DWORD dw;
            case TIME_SAMPLES:
                dw = pmmt->u.sample;
                pmmt->u.sample = MulDivRN(dw,
                                          m_pwfxClient->nSamplesPerSec,
                                          m_pwfxReal->nSamplesPerSec);

                break;

            case TIME_BYTES:
                dw = pmmt->u.cb;
                pmmt->u.cb = MulDivRN(dw,
                                      m_pwfxClient->nAvgBytesPerSec,
                                      m_pwfxReal->nAvgBytesPerSec);
                break;

            default:
                DEBUGMSG(ZONE_ERROR, (TEXT("!mapWaveGetPosition() received unrecognized return format!")));
                mmr = MMSYSERR_ERROR;
            }
        }

    return mmr;
}

void CWaveProxyACM::FilterWomDone(WAVECALLBACKMSG *pmsg)
{
    // Let the base class filter the message we got
    CWaveProxy::FilterWomDone(pmsg);

    // Get back the client's real header
    WAVEHDR UNALIGNED * pwhShadow = (WAVEHDR UNALIGNED *)pmsg->dwParam1;
    WAVEHDR UNALIGNED * pwhClient = (WAVEHDR UNALIGNED *)pwhShadow->dwUser;
    pwhClient->dwFlags = pwhShadow->dwFlags;

    // Put it back into the message so it can be sent back to the app
    pmsg->dwParam1 = (DWORD)pwhClient;

    return;
}

void CWaveProxyACM::FilterWimData(WAVECALLBACKMSG *pmsg)
{
    MMRESULT            mmr;

    // Let the base class filter the message we got
    CWaveProxy::FilterWimData(pmsg);

    WAVEHDR UNALIGNED *pwhShadow = (WAVEHDR UNALIGNED *)pmsg->dwParam1;

    //  Client wave header is user data of shadow wave header
    WAVEHDR UNALIGNED *pwhClient  = (LPWAVEHDR)pwhShadow->dwUser;

    //  Stream header for this client/shadow pair is in the client's 'reserved' member
    LPACMSTREAMHEADER pash = (LPACMSTREAMHEADER)pwhClient->reserved;

    //  do the conversion (if there is data in the input buffer)
    pash->cbDstLengthUsed = 0L;
    DWORD dwBytesRecorded = pwhShadow->dwBytesRecorded;
    if (0L != dwBytesRecorded)
        {
        pash->pbSrc       = (LPBYTE)pwhShadow->lpData;
        pash->cbSrcLength = dwBytesRecorded;
        pash->pbDst       = (LPBYTE)pwhClient->lpData;
        ////////pash->cbDstLength = pwhClient->dwBufferLength;

        mmr = acmStreamConvert(m_has, pash, ACM_STREAMCONVERTF_BLOCKALIGN);
        if (MMSYSERR_NOERROR != mmr)
            {
            DEBUGMSG(ZONE_ERROR, (TEXT("!mapWaveInWindow: conversion failed! mmr=%.04Xh, proxy=%.08lXh"), mmr, this));

            pash->cbDstLengthUsed = 0L;
            }
        else if (pash->cbSrcLength != pash->cbSrcLengthUsed)
            {
            DEBUGMSG(ZONE_WARNING, (TEXT("mapWaveInWindow: discarding %lu bytes of input! proxy=%.08lXh"), pash->cbSrcLength - pash->cbSrcLengthUsed, this));
            }
        }

    if (0L == pash->cbDstLengthUsed)
        {
        DEBUGMSG(ZONE_VERBOSE, (TEXT("mapWaveInWindow: nothing converted--no data in input buffer. proxy=%.08lXh"), this));
        }

    //  update the 'real' header and send the WIM_DATA callback
    pwhClient->dwBytesRecorded = pash->cbDstLengthUsed;
    pwhClient->dwFlags = pwhShadow->dwFlags;

    pmsg->dwParam1 = (DWORD)pwhClient;

    return;
}


