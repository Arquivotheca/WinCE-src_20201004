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
//   acmstrm.c
//
//   Copyright (c) 1991-2000 Microsoft Corporation
//
//   This module provides the Buffer to Buffer API's
//
//------------------------------------------------------------------------------
#include "acmi.h"
#include "wavelib.h"

PWAVEFILTER CopyWaveFilter(PWAVEFILTER pwfltr)
{
    PWAVEFILTER pwfltrCopy = NULL;

    if (!pwfltr)
        {
        goto Exit;
        }

    __try
    {
        DWORD cbwfltr = pwfltr->cbStruct;
        pwfltrCopy = (LPWAVEFILTER)LocalAlloc(LMEM_FIXED, cbwfltr);
        if (!pwfltrCopy)
            {
            goto Exit;
            }

        memcpy(pwfltrCopy,pwfltr,cbwfltr);
        pwfltrCopy->cbStruct = cbwfltr;
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        if (pwfltrCopy)
            {
            LocalFree(pwfltrCopy);
            pwfltrCopy = NULL;
            }
    }

    Exit:
    return pwfltrCopy;
}

//------------------------------------------------------------------------------
// @doc INTERNAL
//
// @func MMRESULT | IStreamOpenQuery | Helper fn to do a stream query.
//
// @parm LPWAVEFORMATEX | pwfxSrc | Source format.
//
// @parm LPWAVEFORMATEX | pwfxDst | Destination format.
//
// @parm LPWAVEFILTER  | pwfltr | Filter to apply.
//
// @parm DWORD | fdwOpen |
//
// @rdesc Returns error number.
//
//------------------------------------------------------------------------------
MMRESULT FNLOCAL
IStreamOpenQuery(
    HACMDRIVER          had,
    LPWAVEFORMATEX      pwfxSrc,
    LPWAVEFORMATEX      pwfxDst,
    LPWAVEFILTER        pwfltr,
    DWORD               fdwOpen
    )
{
    ACMDRVSTREAMINSTANCE_INT  adsi;
    MMRESULT                mmRet;

    FUNC("IStreamOpenQuery");

    memset(&adsi, 0, sizeof(adsi));

    adsi.cbStruct           = sizeof(adsi);
    adsi.pwfxSrc            = pwfxSrc;
    adsi.pwfxDst            = pwfxDst;
    adsi.pwfltr             = pwfltr;
    adsi.fdwOpen            = fdwOpen | ACM_STREAMOPENF_QUERY;

    EnterHandle(had);
    mmRet = (MMRESULT)IDriverMessage(had,
                                   ACMDM_STREAM_OPEN,
                                   (LPARAM)(LPVOID)&adsi,
                                   0L);
    LeaveHandle(had);

    FUNC_EXIT();
    return (mmRet);
}


//------------------------------------------------------------------------------
//
//  @doc EXTERNAL ACM_API
//
//  @func MMRESULT | acmFormatSuggest | This function asks the Audio Compression Manager
//      (ACM) or a specified ACM driver to suggest a destination format for
//      the supplied source format. For example, an application can use this
//      function to determine one or more valid PCM formats to which a
//      compressed format can be decompressed.
//
//  @parm HACMDRIVER | had | Identifies an optional open instance of a
//      driver to query for a suggested destination format. If this
//      argument is NULL, the ACM attempts to find the best driver to suggest
//      a destination format.
//
//  @parm LPWAVEFORMATEX | pwfxSrc | Specifies a pointer to a <t WAVEFORMATEX>
//      structure that identifies the source format to suggest a destination
//      format to be used for a conversion.
//
//  @parm LPWAVEFORMATEX | pwfxDst | Specifies a pointer to a <t WAVEFORMATEX>
//      data structure that will receive the suggested destination format
//      for the <p pwfxSrc> format. Note that based on the <p fdwSuggest>
//      argument, some members of the structure pointed to by <p pwfxDst>
//      may require initialization.
//
//  @parm   DWORD | cbwfxDst | Specifies the size in bytes available for
//      the destination format. The <f acmMetrics> and <f acmFormatTagDetails>
//      functions can be used to determine the maximum size required for any
//      format available for the specified driver (or for all installed ACM
//      drivers).
//
//  @parm DWORD | fdwSuggest | Specifies flags for matching the desired
//      destination format.
//
//      @flag ACM_FORMATSUGGESTF_WFORMATTAG | Specifies that the
//      <e WAVEFORMATEX.wFormatTag> member of the <p pwfxDst> structure is
//      valid.  The ACM will query acceptable installed drivers that can
//      suggest a destination format matching the <e WAVEFORMATEX.wFormatTag>
//      member or fail.
//
//      @flag ACM_FORMATSUGGESTF_NCHANNELS | Specifies that the
//      <e WAVEFORMATEX.nChannels> member of the <p pwfxDst> structure is
//      valid.  The ACM will query acceptable installed drivers that can
//      suggest a destination format matching the <e WAVEFORMATEX.nChannels>
//      member or fail.
//
//      @flag ACM_FORMATSUGGESTF_NSAMPLESPERSEC | Specifies that the
//      <e WAVEFORMATEX.nSamplesPerSec> member of the <p pwfxDst> structure
//      is valid.  The ACM will query acceptable installed drivers that can
//      suggest a destination format matching the <e WAVEFORMATEX.nSamplesPerSec>
//      member or fail.
//
//      @flag ACM_FORMATSUGGESTF_WBITSPERSAMPLE | Specifies that the
//      <e WAVEFORMATEX.wBitsPerSample> member of the <p pwfxDst> structure
//      is valid.  The ACM will query acceptable installed drivers that can
//      suggest a destination format matching the <e WAVEFORMATEX.wBitsPerSample>
//      member or fail.
//
//  @rdesc Returns zero if the function was successful. Otherwise, it returns
//      a non-zero error number. Possible error returns are:
//
//      @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//      @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//      @flag MMSYSERR_INVALPARAM | One or more arguments passed are invalid.
//
//  @xref <f acmDriverOpen> <f acmFormatTagDetails> <f acmMetrics>
//      <f acmFormatEnum>
//
//------------------------------------------------------------------------------
MMRESULT
acmFormatSuggest(
    HACMDRIVER              had,
    LPWAVEFORMATEX          pwfxSrc,
    LPWAVEFORMATEX          pwfxDst,
    DWORD                   cbwfxDst,
    DWORD                   fdwSuggest
    )
{
    MMRESULT            mmRet;
    HACMDRIVERID        hadid;
    PACMDRIVERID        padid;
    UINT                i,j;
    BOOL                fFound;
    ACMDRVFORMATSUGGEST_INT adfs;
    DWORD               cbwfxDstRqd;
    ACMFORMATTAGDETAILS_INT aftd;

    FUNC("acmFormatSuggest");

    VE_FLAGS(fdwSuggest, ACM_FORMATSUGGESTF_VALID);
    V_RWAVEFORMAT(pwfxSrc, MMSYSERR_INVALPARAM);
    VE_WPOINTER(pwfxDst, cbwfxDst);

    //
    //  if the source format is PCM, and we aren't restricting the destination
    //  format, and we're not requesting a specific driver, then first try to
    //  suggest a PCM format.  This is kinda like giving the PCM converter
    //  priority for this case.
    //
    if ( (NULL == had) &&
         (WAVE_FORMAT_PCM == pwfxSrc->wFormatTag) &&
         (0 == (ACM_FORMATSUGGESTF_WFORMATTAG & fdwSuggest)) )
    {
        //
        //  I'll be a bit paranoid and restore pwfxDst->wFormatTag
        //  if this fails.
        //
        WORD wDstFormatTagSave;

        wDstFormatTagSave = pwfxDst->wFormatTag;
        pwfxDst->wFormatTag = WAVE_FORMAT_PCM;
        mmRet = acmFormatSuggest(NULL, pwfxSrc, pwfxDst, cbwfxDst, fdwSuggest | ACM_FORMATSUGGESTF_WFORMATTAG);
        if (MMSYSERR_NOERROR == mmRet)
            goto EXIT;

        pwfxDst->wFormatTag = wDstFormatTagSave;
    }

    //
    //
    //
    if (0 == (ACM_FORMATSUGGESTF_WFORMATTAG & fdwSuggest))
    {
        mmRet = IMetricsMaxSizeFormat(had, &cbwfxDstRqd );
        if (MMSYSERR_NOERROR != mmRet)
            goto EXIT;
    }
    else
    {
        memset(&aftd, 0, sizeof(aftd));
        aftd.cbStruct    = sizeof(aftd);
        aftd.dwFormatTag = pwfxDst->wFormatTag;

        mmRet = acmFormatTagDetails(had,
                                  (PACMFORMATTAGDETAILS) &aftd,
                                  ACM_FORMATTAGDETAILSF_FORMATTAG);
        if (MMSYSERR_NOERROR != mmRet)
            goto EXIT;

        cbwfxDstRqd = aftd.cbFormatSize;
    }

    VE_COND_PARAM(cbwfxDst < cbwfxDstRqd);

    adfs.cbStruct   = sizeof(adfs);
    adfs.fdwSuggest = fdwSuggest;
    adfs.pwfxSrc    = pwfxSrc;
    adfs.cbwfxSrc   = SIZEOF_WAVEFORMATEX(pwfxSrc);
    adfs.pwfxDst    = pwfxDst;
    adfs.cbwfxDst   = cbwfxDst;

    if (NULL != had)
    {
        VE_HANDLE(had, TYPE_HACMDRIVER);

        //
        //  we were given a driver handle
        //

        EnterHandle(had);
        mmRet = (MMRESULT)IDriverMessage(had,
                                       ACMDM_FORMAT_SUGGEST,
                                       (LPARAM)(LPVOID)&adfs,
                                       0L);
        LeaveHandle(had);

        goto EXIT;
    }

    //
    //  if we are being asked to 'suggest anything from any driver'
    //  (that is, (0L == fdwSuggest) and (NULL == had)) AND the source format
    //  is PCM, then simply return the same format as the source... this
    //  keeps seemingly random destination suggestions for a source of PCM
    //  from popping up..
    //
    //  note that this is true even if ALL drivers are disabled!
    //
    if ((0L == fdwSuggest) && (WAVE_FORMAT_PCM == pwfxSrc->wFormatTag))
    {
        memcpy(pwfxDst, pwfxSrc, sizeof(PCMWAVEFORMAT));
        GOTO_EXIT(mmRet = MMSYSERR_NOERROR);
    }

    //
    //  find a driver to match the formats
    //
    //
    mmRet  = MMSYSERR_NODRIVER;
    hadid = NULL;

    ENTER_LIST_SHARED();

    while (MMSYSERR_NOERROR == IDriverGetNext(&hadid, hadid, 0L))
    {
        padid = (PACMDRIVERID)hadid;
        fFound = FALSE;
        for(i = 0; i < padid->cFormatTags; i++ ) {
            //
            //  for every FormatTag in the driver
            //
            if (pwfxSrc->wFormatTag == padid->paFormatTagCache[i].dwFormatTag){
                //
                //  This driver supports the source format.
                //
                if( fdwSuggest & ACM_FORMATSUGGESTF_WFORMATTAG ) {
                    //
                    //  See if this driver supports the desired dest format.
                    //
                    for(j = 0; j < padid->cFormatTags; j++ ) {
                        //
                        //  for every FormatTag in the driver
                        //
                        if (pwfxDst->wFormatTag ==
                                padid->paFormatTagCache[j].dwFormatTag){
                            //
                            //  This driver supports the dest format.
                            //
                            fFound = TRUE;
                            break;
                        }
                    }
                } else {
                    fFound = TRUE;
                }
                break;
            }
        }

        if( fFound ) {
            EnterHandle(hadid);
            mmRet = (MMRESULT)IDriverMessageId(hadid,
                                            ACMDM_FORMAT_SUGGEST,
                                            (LPARAM)(LPVOID)&adfs,
                                            0L );
            LeaveHandle(hadid);
            if (MMSYSERR_NOERROR == mmRet)
                break;
        }
    }

    LEAVE_LIST_SHARED();

EXIT:
    FUNC_EXIT();
    return (mmRet);
}


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   void CALLBACK | acmStreamConvertCallback | The
//          <f acmStreamConvertCallback> function is a placeholder for an
//          application-supplied function name and refers to the callback
//          function used with an asynchronous Audio Compression Manager (ACM)
//          conversion stream.
//
//  @parm   HACMSTREAM | has | Specifies a handle to the ACM conversion stream
//          associated with the callback.
//
//  @parm   UINT | uMsg | Specifies an ACM conversion stream message.
//
//          @flag MM_ACM_OPEN | Specifies that the ACM has successfully opened
//          the conversion stream identified by <p has>.
//
//          @flag MM_ACM_CLOSE | Specifies that the ACM has successfully closed
//          the conversion stream identified by <p has>. The <t HACMSTREAM>
//          handle (<p has>) is no longer valid after receiving this message.
//
//          @flag MM_ACM_DONE | Specifies that the ACM has successfully converted
//          the buffer identified by <p lParam1> (which is a pointer to the
//          <t ACMSTREAMHEADER> structure) for the stream handle specified by <p has>.
//
//  @parm   DWORD | dwInstance | Specifies the user-instance data given
//          as the <p dwInstance> argument of <f acmStreamOpen>.
//
//  @parm   LPARAM | lParam1 | Specifies a parameter for the message.
//
//  @parm   LPARAM | lParam2 | Specifies a parameter for the message.
//
//  @xref   <f acmStreamOpen> <f acmStreamConvert> <f acmStreamClose>
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmStreamOpen | The acmStreamOpen function opens
//          an Audio Compression Manager (ACM) conversion stream. Conversion
//          streams are used to convert data from one specified audio format
//          to another.
//
//  @parm   LPHACMSTREAM | phas | Specifies a pointer to a <t HACMSTREAM>
//          handle that will receive the new stream handle that can be used to
//          perform conversions. Use this handle to identify the stream
//          when calling other ACM stream conversion functions. This parameter
//          should be NULL if the ACM_STREAMOPENF_QUERY flag is specified.
//
//  @parm   HACMDRIVER | had | Specifies an optional handle to an ACM driver.
//          If specified, this handle identifies a specific driver to be used
//          for a conversion stream. If this argument is NULL, then all suitable
//          installed ACM drivers are queried until a match is found.
//
//  @parm   LPWAVEFORMATEX | pwfxSrc | Specifies a pointer to a <t WAVEFORMATEX>
//          structure that identifies the desired source format for the
//          conversion.
//
//  @parm   LPWAVEFORMATEX | pwfxDst | Specifies a pointer to a <t WAVEFORMATEX>
//          structure that identifies the desired destination format for the
//          conversion.
//
//  @parm   LPWAVEFILTER | pwfltr | Specifies a pointer to a <t WAVEFILTER>
//          structure that identifies the desired filtering operation to perform
//          on the conversion stream. This argument can be NULL if no filtering
//          operation is desired. If a filter is specified, the source
//          (<p pwfxSrc>) and destination (<p pwfxDst>) formats must be the same.
//
//  @parm   DWORD | dwCallback | Specifies the address of a callback function
//          or a handle to a window called after each buffer is converted. A
//          callback will only be called if the conversion stream is opened with
//          the ACM_STREAMOPENF_ASYNC flag. If the conversion stream is opened
//          without the ACM_STREAMOPENF_ASYNC flag, then this parameter should
//          be set to zero.
//
//  @parm   DWORD | dwInstance | Specifies user-instance data passed on to the
//          callback specified by <p dwCallback>. This argument is not used with
//          window callbacks. If the conversion stream is opened without the
//          ACM_STREAMOPENF_ASYNC flag, then this parameter should be set to zero.
//
//  @parm   DWORD | fdwOpen | Specifies flags for opening the conversion stream.
//
//          @flag ACM_STREAMOPENF_QUERY | Specifies that the ACM will be queried
//          to determine whether it supports the given conversion. A conversion
//          stream will not be opened and no <t HACMSTREAM> handle will be
//          returned.
//
//          @flag ACM_STREAMOPENF_NONREALTIME | Specifies that the ACM will not
//          consider time constraints when converting the data. By default, the
//          driver will attempt to convert the data in real time. Note that for
//          some formats, specifying this flag might improve the audio quality
//          or other characteristics.
//
//          @flag ACM_STREAMOPENF_ASYNC | Specifies that conversion of the stream should
//          be performed asynchronously. If this flag is specified, the application
//          can use a callback to be notified on open and close of the conversion
//          stream, and after each buffer is converted. In addition to using a
//          callback, an application can examine the <e ACMSTREAMHEADER.fdwStatus>
//          of the <t ACMSTREAMHEADER> structure for the ACMSTREAMHEADER_STATUSF_DONE
//          flag.
//
//          @flag CALLBACK_WINDOW | Specifies that <p dwCallback> is assumed to
//          be a window handle.
//
//          @flag CALLBACK_FUNCTION | Specifies that <p dwCallback> is assumed to
//          be a callback procedure address. The function prototype must conform
//          to the <f acmStreamConvertCallback> convention.
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag MMSYSERR_INVALPARAM | One or more arguments passed are invalid.
//
//          @flag MMSYSERR_NOMEM | Unable to allocate resources.
//
//          @flag ACMERR_NOTPOSSIBLE | The requested operation cannot be performed.
//
//  @comm   Note that if an ACM driver cannot perform real-time conversions,
//          and the ACM_STREAMOPENF_NONREALTIME flag is not specified for
//          the <p fdwOpen> argument, the open will fail returning an
//          ACMERR_NOTPOSSIBLE error code. An application can use the
//          ACM_STREAMOPENF_QUERY flag to determine if real-time conversions
//          are supported for the input arguments.
//
//          If a window is chosen to receive callback information, the
//          following messages are sent to the window procedure function to
//          indicate the progress of the conversion stream: <m MM_ACM_OPEN>,
//          <m MM_ACM_CLOSE>, and <m MM_ACM_DONE>. The <p wParam>  parameter identifies
//          the <t HACMSTREAM> handle. The <p lParam>  parameter identifies the
//          <t ACMSTREAMHEADER> structure for <m MM_ACM_DONE>, but is not used
//          for <m MM_ACM_OPEN> and <m MM_ACM_CLOSE>.
//
//          If a function is chosen to receive callback information, the
//          following messages are sent to the function to indicate the progress
//          of waveform output: <m MM_ACM_OPEN>, <m MM_ACM_CLOSE>, and
//          <m MM_ACM_DONE>.
//
//  @xref   <f acmStreamClose> <f acmStreamConvert> <f acmDriverOpen>
//          <f acmFormatSuggest> <f acmStreamConvertCallback>
//
//------------------------------------------------------------------------------
MMRESULT
acmStreamOpen(
    LPHACMSTREAM            phas,
    HACMDRIVER              had,
    LPWAVEFORMATEX          pwfxSrcApp,
    LPWAVEFORMATEX          pwfxDstApp,
    LPWAVEFILTER            pwfltrApp,
    DWORD                   dwCallback,
    DWORD                   dwInstance,
    DWORD                   fdwOpen
    )
{
    PACMSTREAM          pas;
    PACMDRIVERID        padid;
    HACMDRIVERID        hadid;
    MMRESULT            mmRet;
    DWORD               fdwSupport;
    DWORD               fdwStream;
    BOOL                fAsync;
    BOOL                fQuery;
    UINT                uFormatTag;
    HANDLE              hEvent;

    FUNC(acmStreamOpen);

    LPWAVEFORMATEX pwfxSrc = CopyWaveformat(pwfxSrcApp);
    LPWAVEFORMATEX pwfxDst = CopyWaveformat(pwfxDstApp);
    LPWAVEFILTER pwfltr  = CopyWaveFilter(pwfltrApp);
    if (!pwfxSrc || !pwfxDst || (pwfltrApp && !pwfltr))
    {
        GOTO_EXIT(mmRet = MMSYSERR_INVALPARAM);
    }

    if (NULL != phas) {
        VE_WPOINTER(phas, sizeof(HACMSTREAM));
        *phas = NULL;
    }

    VE_FLAGS(fdwOpen, ACM_STREAMOPENF_VALID);

    fQuery = (0 != (fdwOpen & ACM_STREAMOPENF_QUERY));

    if (fQuery) {
        //
        //  ignore what caller gave us--set to NULL so we will FAULT if
        //  someone messes up this code
        //
        phas = NULL;
    } else {
        //
        //  cause a rip if NULL pointer..
        //
        if (NULL == phas)
            VE_WPOINTER(phas, sizeof(HACMSTREAM));
    }

    hEvent = NULL;
    fAsync = (0 != (fdwOpen & ACM_STREAMOPENF_ASYNC));

    if (!fAsync) {
        VE_COND_PARAM((0L != dwCallback) || (0L != dwInstance));
        }
        else {
                // BUGBUG - Async mode not supported. Until we either run in-proc or
                // do proper header shadowing, we can't allow apps to fiddle with
                // a header while it's in use. So we just disallow async mode
                // and make temp copies of the header during conversion
                // 11/12/01
                ERRMSG("ACM_STREAMOPENF_ASYNC not supported");
        GOTO_EXIT(mmRet = MMSYSERR_INVALPARAM);
        }

    V_DCALLBACK(dwCallback, HIWORD(fdwOpen), MMSYSERR_INVALPARAM);


    fdwSupport = fAsync ? ACMDRIVERDETAILS_SUPPORTF_ASYNC : 0;

    hadid = NULL;

    fdwStream  = (NULL == had) ? 0L : ACMSTREAM_STREAMF_USERSUPPLIEDDRIVER;

    //
    //  if a filter is given, then check that source and destination formats
    //  are the same...
    //
    if (NULL != pwfltr) {
        VE_RWAVEFILTER(pwfltr);

        //
        //  filters do not allow different geometries between source and
        //  destination formats--verify that they are _exactly_ the same.
        //  this includes avg bytes per second!
        //
        //  however, only validate up to wBitsPerSample (the size of a
        //  PCM format header). cbSize can be verified (if necessary) by
        //  the filter driver if it supports non-PCM filtering.
        //
        if (0 != _fmemcmp(pwfxSrc, pwfxDst, sizeof(PCMWAVEFORMAT)))
            GOTO_EXIT(mmRet = ACMERR_NOTPOSSIBLE);

        fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_FILTER;

        uFormatTag = pwfxSrc->wFormatTag;
    } else {
        if (pwfxSrc->wFormatTag == pwfxDst->wFormatTag) {
            fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_CONVERTER;
            uFormatTag = pwfxSrc->wFormatTag;
        } else {
            fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_CODEC;

            //
            //  choose the most common case in an attempt to reduce the
            //  number of driver opens we do--note that even if one of
            //  the tags is not PCM everything will still work..
            //
            if (WAVE_FORMAT_PCM == pwfxSrc->wFormatTag)
                uFormatTag = pwfxDst->wFormatTag;
            else
                uFormatTag = pwfxSrc->wFormatTag;
        }
    }


    if (NULL != had)
    {
        PACMDRIVER      pad;

        VE_HANDLE(had, TYPE_HACMDRIVER);

        pad   = (PACMDRIVER)had;
        padid = (PACMDRIVERID)pad->hadid;

        if (fdwSupport != (fdwSupport & padid->fdwSupport))
            GOTO_EXIT(mmRet = ACMERR_NOTPOSSIBLE);

        if (fQuery)
        {
            EnterHandle(had);
            mmRet = IStreamOpenQuery(had, pwfxSrc, pwfxDst, pwfltr, fdwOpen);
            //
            //  We only support async conversion to sync conversion
            //  on the 32-bit side.
            //
            if (MMSYSERR_NOERROR != mmRet)
            {
                //
                //  If this driver supports async conversions, and we're
                //  opening for sync conversion, then attempt to open
                //  async and the acm will handle making the async conversion
                //  look like a sync conversion.
                //
                if ( !fAsync &&
                     (ACMDRIVERDETAILS_SUPPORTF_ASYNC & padid->fdwSupport) )
                {
                    mmRet = IStreamOpenQuery(had, pwfxSrc, pwfxDst, pwfltr, fdwOpen | ACM_STREAMOPENF_ASYNC);
                }
            }
            LeaveHandle(had);
            if (MMSYSERR_NOERROR != mmRet)
                goto EXIT;
        }
    } else {
        //
        //  we need to find a driver to match the formats--so enumerate
        //  all drivers until we find one that works. if none can be found,
        //  then fail..
        //
        hadid = NULL;

        ENTER_LIST_SHARED();

        while (MMSYSERR_NOERROR == IDriverGetNext(&hadid, hadid, 0L))
        {
            ACMFORMATTAGDETAILS_INT aftd;

            //
            //  if this driver does not support the basic function we
            //  need, then don't even attempt to open it..
            //
            padid = (PACMDRIVERID)hadid;

            if (fdwSupport != (fdwSupport & padid->fdwSupport))
                continue;

            //
            //
            //
            aftd.cbStruct    = sizeof(aftd);
            aftd.dwFormatTag = uFormatTag;
            aftd.fdwSupport  = 0L;

            mmRet = acmFormatTagDetails((HACMDRIVER)hadid,
                                      (PACMFORMATTAGDETAILS) &aftd,
                                      ACM_FORMATTAGDETAILSF_FORMATTAG);
            if (MMSYSERR_NOERROR != mmRet)
                continue;

            if (fdwSupport != (fdwSupport & aftd.fdwSupport))
                continue;


            //
            //
            //
            //
            EnterHandle(hadid);
            mmRet = IDriverOpen(&had, hadid, 0L);
            LeaveHandle(hadid);
            if (MMSYSERR_NOERROR != mmRet)
                continue;

            EnterHandle(had);
            mmRet = IStreamOpenQuery(had, pwfxSrc, pwfxDst, pwfltr, fdwOpen);
            //
            //  We only support async conversion to sync conversion
            //  on the 32-bit side.
            //
            if (MMSYSERR_NOERROR != mmRet)
            {
                //
                //  If this driver supports async conversions, and we're
                //  opening for sync conversion, then attempt to open
                //  async and the acm will handle making the async conversion
                //  look like a sync conversion.
                //
                if ( !fAsync &&
                     (ACMDRIVERDETAILS_SUPPORTF_ASYNC & padid->fdwSupport) )
                {
                    mmRet = IStreamOpenQuery(had, pwfxSrc, pwfxDst, pwfltr, fdwOpen | ACM_STREAMOPENF_ASYNC);
                }
            }
            LeaveHandle(had);

            if (MMSYSERR_NOERROR == mmRet)
                break;

            EnterHandle(hadid);
            IDriverClose(had, 0L);
            LeaveHandle(hadid);
            had = NULL;
        }

        LEAVE_LIST_SHARED();

        if (NULL == had)
            GOTO_EXIT(mmRet = ACMERR_NOTPOSSIBLE);
    }


    //
    //  if just being queried, then we succeeded this far--so succeed
    //  the call...
    //
    if (fQuery) {
        mmRet = MMSYSERR_NOERROR;
        goto Stream_Open_Exit_Error;
    }


    //
    //  alloc an ACMSTREAM structure--we need to alloc enough space for
    //  both the source and destination format headers.
    //
    //  size of one format is sizeof(WAVEFORMATEX) + size of extra format
    //  (wfx->cbSize) informatation. for PCM, the size is simply the
    //  sizeof(PCMWAVEFORMAT)
    //
    pas = new ACMSTREAM;
    if (NULL == pas)
    {
        //DPF(0, "!acmStreamOpen: cannot allocate ACMSTREAM--local heap full!");

        mmRet = MMSYSERR_NOMEM;
        goto Stream_Open_Exit_Error;
    }

    //
    //  initialize the ACMSTREAM structure
    //
    //
    pas->adsi.pwfxSrc = pwfxSrc;
    pas->adsi.pwfxDst = pwfxDst;
    pas->adsi.pwfltr  = pwfltr;

    // ACMSTREAM owns them now, make sure we don't free them by mistake
    pwfxSrc = NULL;
    pwfxDst = NULL;
    pwfltr = NULL;

    pas->uHandleType            = TYPE_HACMSTREAM;
////pas->pasNext                = NULL;
    pas->fdwStream              = fdwStream;
    pas->had                    = had;
    pas->adsi.cbStruct          = sizeof(pas->adsi);

    pas->adsi.dwCallback        = dwCallback;
    pas->adsi.dwInstance        = dwInstance;
    pas->adsi.fdwOpen           = fdwOpen;
////pas->adsi.dwDriverFlags     = 0L;
////pas->adsi.dwDriverInstance  = 0L;
    pas->adsi.has               = (HACMSTREAM)pas;


    //
    //
    //
    //
    //
    EnterHandle(had);
    mmRet = (MMRESULT)IDriverMessage(had,
                                   ACMDM_STREAM_OPEN,
                                   (LPARAM)(LPVOID)&pas->adsi,
                                   0L);

    if ( (MMSYSERR_NOERROR != mmRet) &&
         (!fAsync) &&
         (padid->fdwSupport & ACMDRIVERDETAILS_SUPPORTF_ASYNC) )
    {
        //
        //  Try making async look like sync
        //
        TESTMSG("Trying async to sync");
        hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (NULL != hEvent)
        {
            pas->fdwStream |= ACMSTREAM_STREAMF_ASYNCTOSYNC;
            pas->adsi.dwCallback = (DWORD)(UINT)hEvent;
            pas->adsi.fdwOpen &= ~CALLBACK_TYPEMASK;
            pas->adsi.fdwOpen |= CALLBACK_EVENT;
            pas->adsi.fdwOpen |= ACM_STREAMOPENF_ASYNC;

// BUGBUG setup a local callback to handle callbacks from driver.

            mmRet = (MMRESULT)IDriverMessage(had,
                                           ACMDM_STREAM_OPEN,
                                           (LPARAM)(LPVOID)&pas->adsi,
                                           0L);
            if (MMSYSERR_NOERROR == mmRet)
            {
                TESTMSG("Succeeded async to sync open, waiting for CALLBACK_EVENT");
                WaitForSingleObject(hEvent, INFINITE);
            }
        } else {
            TESTMSG("CreateEvent failed, can't make async codec look like sync codec");
        }
    }

    LeaveHandle(had);

    if (MMSYSERR_NOERROR == mmRet)
    {
        PACMDRIVER      pad;

        pad = (PACMDRIVER)had;

        pas->pasNext  = pad->pasFirst;
        pad->pasFirst = pas;


        //
        //  succeed!
        //
        *phas = (HACMSTREAM) pas;

        GOTO_EXIT(mmRet = MMSYSERR_NOERROR);
    }


    //
    //  we are failing, so free the instance data that we alloc'd!
    //
    pas->uHandleType = TYPE_HACMNOTVALID;
    pas->Release();


Stream_Open_Exit_Error:

    //
    //  Close the event handle if it was created
    //
    if (hEvent)
    {
        CloseHandle(hEvent);
    }

    //
    //  only close driver if _we_ opened it...
    //
    if (0 == (fdwStream & ACMSTREAM_STREAMF_USERSUPPLIEDDRIVER))
    {
        hadid = ((PACMDRIVER)had)->hadid;
        EnterHandle(hadid);
        IDriverClose(had, 0L);
        LeaveHandle(hadid);
    }

EXIT:

    // Remember to free the structures we were given if there's an error
    // If no error, they'll all be set to NULL already
    LocalFree(pwfxSrc);
    LocalFree(pwfxDst);
    LocalFree(pwfltr);

    FUNC_EXIT();
    DECPARAM(mmRet);
    return (mmRet);
}



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmStreamClose | The acmStreamClose function closes
//          a previously opened Audio Compression Manager (ACM) conversion
//          stream. If the function is successful, the handle is invalidated.
//
//  @parm   HACMSTREAM | has | Identifies the open conversion stream to be
//          closed.
//
//  @parm   DWORD | fdwClose | This argument is not used and must be set to
//          zero.
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag ACMERR_BUSY | The conversion stream cannot be closed because
//          an asynchronous conversion is still in progress.
//
//  @xref   <f acmStreamOpen> <f acmStreamReset>
//
//------------------------------------------------------------------------------
MMRESULT
acmStreamClose(
    HACMSTREAM          has,
    DWORD               fdwClose
    )
{
    MMRESULT        mmRet;
    PACMSTREAM      pas = (PACMSTREAM) has;
    PACMDRIVER      pad;
    PACMDRIVERID    padid;
    HANDLE          hEvent;

    FUNC("acmStreamClose");

    VE_HANDLE(has, TYPE_HACMSTREAM);
    VE_FLAGS(fdwClose, ACM_STREAMCLOSEF_VALID);

    pad = (PACMDRIVER) pas->had;
    padid = (PACMDRIVERID) pad->hadid;

    if (0 != pas->cPrepared)
    {
//        VE_COND_PARAM(pag->hadDestroy != pas->had);

        WARNMSG("Stream contains prepared headers--forcing close");
        pas->cPrepared = 0;
    }


    //
    //  Callback event if we are converting async conversion to sync conversion
    //
    hEvent = (pas->fdwStream & ACMSTREAM_STREAMF_ASYNCTOSYNC) ? (HANDLE)pas->adsi.dwCallback : NULL;


    //
    //  tell driver that conversion stream is terminating
    //
    //

    EnterHandle(pas->had);
#ifdef RDEBUG
    if ( (hEvent) && (WAIT_OBJECT_0 == WaitForSingleObject(hEvent, 0)) )
    {
        //
        //  The event is already signaled!  Bad bad!
        //
        ERRMSG("asynchronous codec called callback unexpectedly");
    }
#endif
    mmRet = (MMRESULT)IDriverMessage(pas->had,
                                   ACMDM_STREAM_CLOSE,
                                   (LPARAM)(LPVOID)&pas->adsi,
                                   fdwClose);
    if ( (hEvent) && (MMSYSERR_NOERROR == mmRet) ) {
        TESTMSG("waiting for CALLBACK_EVENT");
        WaitForSingleObject(hEvent, INFINITE);
    }
    LeaveHandle(pas->had);

    if ((MMSYSERR_NOERROR == mmRet) || (pag->hadDestroy == pas->had))
    {
        if (MMSYSERR_NOERROR != mmRet)
        {
            WARNMSG("Forcing close of stream handle!");
        }

        //
        //  Close the event handle
        //
        if (hEvent) {
            CloseHandle(hEvent);
        }

        //
        //  remove the stream handle from the linked list and free its memory
        //
        pad = (PACMDRIVER)pas->had;

        EnterHandle(pad);
        if (pas == pad->pasFirst)
        {
            pad->pasFirst = pas->pasNext;

            //
            //  if this was the last open stream on this driver, then close
            //  the driver instance also...
            //
            if (NULL == pad->pasFirst)
            {
                LeaveHandle(pad);
                if (0 == (pas->fdwStream & ACMSTREAM_STREAMF_USERSUPPLIEDDRIVER))
                {
                    IDriverClose(pas->had, 0L);
                }
            }
            else
            {
                LeaveHandle(pad);
            }
        }
        else
        {
            PACMSTREAM  pasCur;

            for (pasCur = pad->pasFirst;
                (NULL != pasCur) && (pas != pasCur->pasNext);
                pasCur = pasCur->pasNext)
                ;

            if (NULL == pasCur)
            {
                WARNMSG("Stream handle not in list!!!");
                LeaveHandle(pad);
                GOTO_EXIT(mmRet = MMSYSERR_INVALHANDLE);
            }

            pasCur->pasNext = pas->pasNext;

            LeaveHandle(pad);
        }

        //
        //  finally free the stream handle
        //
        pas = (PACMSTREAM)has;
        pas->uHandleType = TYPE_HACMNOTVALID;
        pas->Release();
    }

EXIT:
    FUNC_EXIT();
    return (mmRet);
}



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmStreamMessage | This function sends a user-defined
//          message to a given Audio Compression Manager (ACM) stream instance.
//
//  @parm   HACMSTREAM | has | Specifies the conversion stream.
//
//  @parm   UINT | uMsg | Specifies the message that the ACM stream must
//          process. This message must be in the <m ACMDM_USER> message range
//          (above or equal to <m ACMDM_USER> and less than
//          <m ACMDM_RESERVED_LOW>). The exception to this restriction is
//          the <m ACMDM_STREAM_UPDATE> message.
//
//  @parm   LPARAM | lParam1 | Specifies the first message parameter.
//
//  @parm   LPARAM | lParam2 | Specifies the second message parameter.
//
//  @rdesc  The return value is specific to the user-defined ACM driver
//          message <p uMsg> sent. However, the following return values are
//          possible:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALPARAM | <p uMsg> is not in the ACMDM_USER range.
//
//          @flag MMSYSERR_NOTSUPPORTED | The ACM driver did not process the
//          message.
//
//------------------------------------------------------------------------------
MMRESULT
acmStreamMessage(
    HACMSTREAM              has,
    UINT                    uMsg,
    LPARAM                  lParam1,
    LPARAM                  lParam2
    )
{
    MMRESULT    mmRet;
    PACMSTREAM  pas = (PACMSTREAM) has;

    FUNC("acmStreamMessage");

    VE_HANDLE(has, TYPE_HACMSTREAM);

    //
    //  do not allow non-user range messages through!
    //
    VE_COND_PARAM( ((uMsg < ACMDM_USER) ||
                    (uMsg >= ACMDM_RESERVED_LOW)) &&
                    (uMsg != ACMDM_STREAM_UPDATE) );

    EnterHandle(pas);
    mmRet = (MMRESULT)IDriverMessage(pas->had,
                                   uMsg,
                                   lParam1,
                                   lParam2 );
    LeaveHandle(pas);

EXIT:
    FUNC_EXIT();
    return (mmRet);
}



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmStreamReset | The acmStreamReset function stops
//          conversions for a given Audio Compression Manager (ACM) stream.
//          All pending buffers are marked as done and returned to the
//          application.
//
//  @parm   HACMSTREAM | has | Specifies the conversion stream.
//
//  @parm   DWORD | fdwReset | This argument is not used and must be set to
//          zero.
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//  @comm   Resetting an ACM conversion stream is only necessary to reset
//          asynchronous conversion streams. However, resetting a synchronous
//          conversion stream will succeed, but no action will be taken.
//
//  @xref   <f acmStreamConvert> <f acmStreamClose>
//
//------------------------------------------------------------------------------
MMRESULT
acmStreamReset(
    HACMSTREAM          has,
    DWORD               fdwReset
    )
{
    MMRESULT        mmRet;
    PACMSTREAM      pas = (PACMSTREAM) has;

    FUNC("acmStreamReset");

    VE_HANDLE(has, TYPE_HACMSTREAM);
    VE_FLAGS(fdwReset, ACM_STREAMRESETF_VALID);

    //
    //  if the stream was not opened as async, then just succeed the reset
    //  call--it only makes sense with async streams...
    //
    if (0 == (ACM_STREAMOPENF_ASYNC & pas->adsi.fdwOpen))
        GOTO_EXIT(mmRet = MMSYSERR_NOERROR);

    EnterHandle(pas);
    mmRet = (MMRESULT) IDriverMessage(pas->had,
                                   ACMDM_STREAM_RESET,
                                   (LPARAM)(LPVOID)&pas->adsi,
                                   fdwReset);
    LeaveHandle(pas);

EXIT:
    FUNC_EXIT();
    return (mmRet);
}


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmStreamSize | The acmStreamSize function returns
//          a recommended size for a source or destination buffer on an
//          Audio Compression Manager (ACM) stream.
//
//  @parm   HACMSTREAM | has | Specifies the conversion stream.
//
//  @parm   DWORD | cbInput | Specifies the size in bytes of either the source
//          or destination buffer. The <p fdwSize> flags specify what the
//          input argument defines. This argument must be non-zero.
//
//  @parm   LPDWORD | pdwOutputBytes | Specifies a pointer to a <t DWORD>
//          that contains the size in bytes of the source or destination buffer.
//          The <p fdwSize> flags specify what the output argument defines.
//          If the <f acmStreamSize> function succeeds, this location will
//          always be filled with a non-zero value.
//
//  @parm   DWORD | fdwSize | Specifies flags for the stream-size query.
//
//          @flag ACM_STREAMSIZEF_SOURCE | Indicates that <p cbInput> contains
//          the size of the source buffer. The <p pdwOutputBytes> argument will
//          receive the recommended destination buffer size in bytes.
//
//          @flag ACM_STREAMSIZEF_DESTINATION | Indicates that <p cbInput>
//          contains the size of the destination buffer. The <p pdwOutputBytes>
//          argument will receive the recommended source buffer size in bytes.
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag MMSYSERR_INVALPARAM | One or more arguments passed are invalid.
//
//          @flag ACMERR_NOTPOSSIBLE | The requested operation cannot be performed.
//
//  @comm   An application can use the <f acmStreamSize> function to determine
//          suggested buffer sizes for either source or destination buffers.
//          The buffer sizes returned might be only an estimation of the
//          actual sizes required for conversion. Because actual conversion
//          sizes cannot always be determined without performing the conversion,
//          the sizes returned will usually be overestimated.
//
//          In the event of an error, the location pointed to by
//          <p pdwOutputBytes> will receive zero. This assumes that the pointer
//          specified by <p pdwOutputBytes> is valid.
//
//  @xref   <f acmStreamPrepareHeader> <f acmStreamConvert>
//
//------------------------------------------------------------------------------
MMRESULT
acmStreamSize(
    HACMSTREAM          has,
    DWORD               cbInput,
    LPDWORD             pdwOutputBytes,
    DWORD               fdwSize
    )
{
    MMRESULT            mmRet;
    PACMSTREAM          pas = (PACMSTREAM) has;
    ACMDRVSTREAMSIZE_INT    adss;

    FUNC("acmStreamSize");

    *pdwOutputBytes = 0L;

    VE_HANDLE(has, TYPE_HACMSTREAM);
    VE_FLAGS(fdwSize, ACM_STREAMSIZEF_VALID);
    VE_COND_PARAM(0L == cbInput);

    adss.cbStruct = sizeof(adss);
    adss.fdwSize  = fdwSize;

    switch (ACM_STREAMSIZEF_QUERYMASK & fdwSize) {

        case ACM_STREAMSIZEF_SOURCE:
            adss.cbSrcLength = cbInput;
            adss.cbDstLength = 0L;
            break;

        case ACM_STREAMSIZEF_DESTINATION:
            adss.cbSrcLength = 0L;
            adss.cbDstLength = cbInput;
            break;

        default:
            WARNMSG("unknown query type requested.");
            GOTO_EXIT(mmRet = MMSYSERR_NOTSUPPORTED);
    }

    EnterHandle(pas);
    mmRet = (MMRESULT) IDriverMessage(pas->had,
                                   ACMDM_STREAM_SIZE,
                                   (LPARAM)(LPVOID)&pas->adsi,
                                   (LPARAM)(LPVOID)&adss);
    LeaveHandle(pas);
    if (MMSYSERR_NOERROR == mmRet)
    {
        switch (ACM_STREAMSIZEF_QUERYMASK & fdwSize)
        {
            case ACM_STREAMSIZEF_SOURCE:
                *pdwOutputBytes  = adss.cbDstLength;
                break;

            case ACM_STREAMSIZEF_DESTINATION:
                *pdwOutputBytes  = adss.cbSrcLength;
                break;
        }


        if (0L == *pdwOutputBytes) {
            ERRMSG("buggy driver returned zero bytes for output?!?");
            GOTO_EXIT(mmRet = ACMERR_NOTPOSSIBLE);
        }
    }

EXIT:
    FUNC_EXIT();
    DECPARAM(mmRet);
    return (mmRet);
}



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmStreamPrepareHeader | The acmStreamPrepareHeader
//          function prepares an <t ACMSTREAMHEADER> for an Audio Compression
//          Manager (ACM) stream conversion. This function must be called for
//          every stream header before it can be used in a conversion stream. An
//          application only needs to prepare a stream header once for the life of
//          a given stream; the stream header can be reused as long as the same
//          source and destiniation buffers are used, and the size of the source
//          and destination buffers do not exceed the sizes used when the stream
//          header was originally prepared.
//
//  @parm   HACMSTREAM | has | Specifies a handle to the conversion steam.
//
//  @parm   LPACMSTREAMHEADER | pash | Specifies a pointer to an <t ACMSTREAMHEADER>
//          structure that identifies the source and destination data buffers to
//          be prepared.
//
//  @parm   DWORD | fdwPrepare | This argument is not used and must be set to
//          zero.
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALPARAM | One or more arguments passed are invalid.
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag MMSYSERR_NOMEM | Unable to allocate resources.
//
//  @comm   Preparing a stream header that has already been prepared has no
//          effect, and the function returns zero. However, an application should
//          take care to structure code so multiple prepares do not occur.
//
//  @xref   <f acmStreamUnprepareHeader> <f acmStreamOpen>
//
//------------------------------------------------------------------------------
MMRESULT
acmStreamPrepareHeader(
    HACMSTREAM              has,
    LPACMSTREAMHEADER       pashi,
    DWORD                   fdwPrepare
    )
{
    MMRESULT                mmRet;
    PACMSTREAM              pas = (PACMSTREAM) has;
    PACMDRVSTREAMHEADER_INT padsh = (PACMDRVSTREAMHEADER_INT) pashi;
    DWORD                   cbDstRequired;

    FUNC("acmStreamPrepareHeader");


    VE_HANDLE(has, TYPE_HACMSTREAM);
    VE_WPOINTER(pashi, sizeof(DWORD));
    VE_WPOINTER(pashi, pashi->cbStruct);

    VE_WPOINTER(pashi, sizeof(ACMSTREAMHEADER));

    VE_COND_TEXT(pashi-> cbStruct != sizeof(ACMSTREAMHEADER), MMSYSERR_ERROR, "sizes not right");

    VE_FLAGS(fdwPrepare, ACM_STREAMPREPAREF_VALID);

    VE_COND_PARAM(pashi->cbStruct < sizeof(ACMDRVSTREAMHEADER));

    if (0 != (pashi->fdwStatus & ~ACMSTREAMHEADER_STATUSF_VALID)) {
        ERRMSG("Header contains invalid status flags.");
        GOTO_EXIT(mmRet = MMSYSERR_INVALFLAG);
    }

    if (0 != (pashi->fdwStatus & ACMSTREAMHEADER_STATUSF_PREPARED)) {
        WARNMSG("Header is already prepared.");
        GOTO_EXIT(mmRet = MMSYSERR_NOERROR);
    }

    mmRet = acmStreamSize(has, pashi->cbSrcLength, &cbDstRequired, ACM_STREAMSIZEF_SOURCE);
    if (MMSYSERR_NOERROR != mmRet)
        goto EXIT;


    //
    //  after all the size verification stuff done above, now we check
    //  the src and dst buffer pointers...
    //
    VE_RPOINTER(pashi->pbSrc, pashi->cbSrcLength);
    VE_WPOINTER(pashi->pbDst, pashi->cbDstLength);


    //
    //  init a couple of things for the driver
    //
    padsh->fdwConvert           = fdwPrepare;
    padsh->padshNext            = NULL;
    padsh->fdwDriver            = 0L;
    padsh->dwDriver             = 0L;

    padsh->fdwPrepared          = 0L;
    padsh->dwPrepared           = 0L;
    padsh->pbPreparedSrc        = NULL;
    padsh->cbPreparedSrcLength  = 0L;
    padsh->pbPreparedDst        = NULL;
    padsh->cbPreparedDstLength  = 0L;


    //
    //  set up driver instance info--copy over driver data that is saved
    //  in ACMSTREAM
    //
    EnterHandle(pas);
    mmRet = (MMRESULT) IDriverMessage(pas->had,
                                   ACMDM_STREAM_PREPARE,
                                   (LPARAM)(LPVOID)&pas->adsi,
                                   (LPARAM)(LPVOID)padsh);
    LeaveHandle(pas);

    if (MMSYSERR_NOTSUPPORTED == mmRet ||
        MMSYSERR_NOERROR      == mmRet &&
        (((PACMDRIVERID)((PACMDRIVER)pas->had)->hadid)->fdwAdd &
         ACM_DRIVERADDF_32BIT))
    {
        //
        //  the driver doesn't seem to think it needs anything special
        //  so just succeed the call
        //
        //  note that if the ACM needs to do something special, it should
        //  do it here...
        //

        mmRet = MMSYSERR_NOERROR;
    }

    //
    //
    //
    if (MMSYSERR_NOERROR == mmRet)
    {
        //
        //  set the prepared bit (and also kill any invalid flags that
        //  the driver might have felt it should set--when the driver
        //  writer sees that his flags are not being preserved he will
        //  probably read the docs and use pash->fdwDriver instead).
        //
        pashi->fdwStatus  = pashi->fdwStatus | ACMSTREAMHEADER_STATUSF_PREPARED;
        pashi->fdwStatus &= ACMSTREAMHEADER_STATUSF_VALID;


        //
        //  save the original prepared pointers and sizes so we can keep
        //  track of this stuff for the calling app..
        //
        padsh->fdwPrepared          = fdwPrepare;
        padsh->dwPrepared           = (DWORD)(UINT)has;
        padsh->pbPreparedSrc        = padsh->pbSrc;
        padsh->cbPreparedSrcLength  = padsh->cbSrcLength;
        padsh->pbPreparedDst        = padsh->pbDst;
        padsh->cbPreparedDstLength  = padsh->cbDstLength;
        pas->cPrepared++;
    }

EXIT:

    FUNC_EXIT();
    return (mmRet);
}


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmStreamUnprepareHeader | The acmStreamUnprepareHeader function
//          cleans up the preparation performed by the <f acmStreamPrepareHeader>
//          function for an Audio Compression Manager (ACM) stream. This function must
//          be called after the ACM is finished with the given buffers. An
//          application must call this function before freeing the source and
//          destination buffers.
//
//  @parm   HACMSTREAM | has | Specifies a handle to the conversion steam.
//
//  @parm   LPACMSTREAMHEADER | pash | Specifies a pointer to an <t ACMSTREAMHEADER>
//          structure that identifies the source and destination data buffers to
//          be unprepared.
//
//  @parm   DWORD | fdwUnprepare | This argument is not used and must be set to
//          zero.
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALPARAM | One or more arguments passed are invalid.
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag ACMERR_BUSY | The stream header <p pash> is currently in use
//          and cannot be unprepared.
//
//          @flag ACMERR_UNPREPARED | The stream header <p pash> is currently
//          not prepared by the <f acmStreamPrepareHeader> function.
//
//  @comm   Unpreparing a stream header that has already been unprepared is
//          an error. An application must specify the source and destination
//          buffer lengths (<e ACMSTREAMHEADER.cbSrcLength> and
//          <e ACMSTREAMHEADER.cbDstLength> respectively) that were used
//          during the corresponding <f acmStreamPrepareHeader> call. Failing
//          to reset these member values will cause <f acmStreamUnprepareHeader>
//          to fail with MMSYSERR_INVALPARAM.
//
//          Note that there are some errors that the ACM can recover from. The
//          ACM will return a non-zero error, yet the stream header will be
//          properly unprepared. To determine whether the stream header was
//          actually unprepared an application can examine the
//          ACMSTREAMHEADER_STATUSF_PREPARED flag. The header will always be
//          unprepared if <f acmStreamUnprepareHeader> returns success.
//
//  @xref   <f acmStreamPrepareHeader> <f acmStreamClose>
//
//------------------------------------------------------------------------------
MMRESULT
acmStreamUnprepareHeader(
    HACMSTREAM              has,
    LPACMSTREAMHEADER       pashi,
    DWORD                   fdwUnprepare
    )
{
    MMRESULT                mmRet;
    PACMSTREAM              pas = (PACMSTREAM) has;
    PACMDRVSTREAMHEADER_INT padsh = (PACMDRVSTREAMHEADER_INT) pashi;
    BOOL                    fStupidApp;
    FUNC("acmStreamUnprepareHeader");

    VE_HANDLE(has, TYPE_HACMSTREAM);
    VE_FLAGS(fdwUnprepare, ACM_STREAMPREPAREF_VALID);

    VE_COND_PARAM(pashi->cbStruct < sizeof(ACMDRVSTREAMHEADER));

    if (0 != (pashi->fdwStatus & ~ACMSTREAMHEADER_STATUSF_VALID)) {
        ERRMSG("Header contains invalid status flags.");
        GOTO_EXIT(mmRet = MMSYSERR_INVALFLAG);
    }

    if (0 != (pashi->fdwStatus & ACMSTREAMHEADER_STATUSF_INQUEUE)) {
        ERRMSG("Header is still in use.");
        GOTO_EXIT(mmRet = ACMERR_BUSY);
    }

    if (0 == (pashi->fdwStatus & ACMSTREAMHEADER_STATUSF_PREPARED)) {
        ERRMSG("Header is not prepared.");
        GOTO_EXIT(mmRet = ACMERR_UNPREPARED);
    }

    if ((UINT)has != padsh->dwPrepared) {
        ERRMSG("Header prepared for different stream.");
        GOTO_EXIT(mmRet = MMSYSERR_INVALHANDLE);
    }

    fStupidApp = FALSE;
    if ((padsh->pbSrc != padsh->pbPreparedSrc) ||
        (padsh->cbSrcLength != padsh->cbPreparedSrcLength))
    {
        ERRMSG("Header prepared with different source buffer/length.");
        DEBUGCHK(0);

        if (padsh->pbSrc != padsh->pbPreparedSrc)
            GOTO_EXIT(mmRet = MMSYSERR_INVALPARAM);

        padsh->cbSrcLength = padsh->cbPreparedSrcLength;
        fStupidApp = TRUE;
    }

    if ((padsh->pbDst != padsh->pbPreparedDst) ||
        (padsh->cbDstLength != padsh->cbPreparedDstLength))
    {
        ERRMSG("Header prepared with different destination buffer/length.");
        DEBUGCHK(0);

        if (padsh->pbDst != padsh->pbPreparedDst)
            GOTO_EXIT(mmRet = MMSYSERR_INVALPARAM);

        padsh->cbDstLength = padsh->cbPreparedDstLength;
        fStupidApp = TRUE;
    }



    //
    //  init things for the driver
    //
    padsh->fdwConvert = fdwUnprepare;

    EnterHandle(pas);
    mmRet = (MMRESULT)IDriverMessage(pas->had,
                                   ACMDM_STREAM_UNPREPARE,
                                   (LPARAM)(LPVOID)&pas->adsi,
                                   (LPARAM)(LPVOID)padsh);
    LeaveHandle(pas);

    if (MMSYSERR_NOTSUPPORTED == mmRet)
    {
        //
        //  note that if the ACM needs to undo something special, it should
        //  do it here...
        //
        mmRet = MMSYSERR_NOERROR;
    }

    //
    //
    //
    if (MMSYSERR_NOERROR == mmRet)
    {
        //
        //  UNset the prepared bit (and also kill any invalid flags that
        //  the driver might have felt it should set--when the driver
        //  writer sees that his flags are not being preserved he will
        //  probably read the docs and use pashi->fdwDriver instead).
        //
        pashi->fdwStatus  = pashi->fdwStatus & ~ACMSTREAMHEADER_STATUSF_PREPARED;
        pashi->fdwStatus &= ACMSTREAMHEADER_STATUSF_VALID;

        padsh->fdwPrepared          = 0L;
        padsh->dwPrepared           = 0L;
        padsh->pbPreparedSrc        = NULL;
        padsh->cbPreparedSrcLength  = 0L;
        padsh->pbPreparedDst        = NULL;
        padsh->cbPreparedDstLength  = 0L;

        pas->cPrepared--;

        //
        //  if we fixed up a bug for the app, still return an error...
        //
        if (fStupidApp)
        {
            mmRet = MMSYSERR_INVALPARAM;
        }
    }

EXIT:

    FUNC_EXIT();
    return (mmRet);
}



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API_STRUCTURE
//
//  @struct ACMSTREAMHEADER | The <t ACMSTREAMHEADER> structure defines the
//          header used to identify an Audio Compression Manager (ACM) conversion
//          source and destination buffer pair for a conversion stream.
//
//  @field  DWORD | cbStruct | Specifies the size, in bytes, of the
//          <t ACMSTREAMHEADER> structure. This member must be initialized
//          before calling any ACM stream functions using this structure.
//          The size specified in this member must be large enough to contain
//          the base <t ACMSTREAMHEADER> structure.
//
//  @field  DWORD | fdwStatus | Specifies flags giving information about
//          the conversion buffers. This member must be initialized to zero
//          before calling <f acmStreamPrepareHeader> and should not be modified
//          by the application while the stream header remains prepared.
//
//          @flag ACMSTREAMHEADER_STATUSF_DONE | Set by the ACM or driver to
//          indicate that it is finished with the conversion and is returning it
//          to the application.
//
//          @flag ACMSTREAMHEADER_STATUSF_PREPARED | Set by the ACM to indicate
//          that the data buffers have been prepared with <f acmStreamPrepareHeader>.
//
//          @flag ACMSTREAMHEADER_STATUSF_INQUEUE | Set by the ACM or driver to
//          indicate that the data buffers are queued for conversion.
//
//  @field  DWORD | dwUser | Specifies 32 bits of user data. This can be any
//          instance data specified by the application.
//
//  @field  LPBYTE | pbSrc | Specifies a pointer to the source data buffer.
//          This pointer must always refer to the same location while the stream
//          header remains prepared. If an application needs to change the
//          source location, it must unprepare the header and re-prepare it
//          with the alternate location.
//
//  @field  DWORD | cbSrcLength | Specifies the length, in bytes, of the source
//          data buffer pointed to by <e ACMSTREAMHEADER.pbSrc>. When the
//          header is prepared, this member must specify the maximum size
//          that will be used in the source buffer. Conversions can be performed
//          on source lengths less than or equal to the original prepared size.
//          However, this member must be reset to the original size when
//          unpreparing the header.
//
//  @field  DWORD | cbSrcLengthUsed | Specifies the amount of data, in bytes,
//          used for the conversion. This member is not valid until the
//          conversion is complete. Note that this value can be less than or
//          equal to <e ACMSTREAMHEADER.cbSrcLength>. An application must use
//          the <e ACMSTREAMHEADER.cbSrcLengthUsed> member when advancing to
//          the next piece of source data for the conversion stream.
//
//  @field  DWORD | dwSrcUser | Specifies 32 bits of user data. This can be
//          any instance data specified by the application.
//
//  @field  LPBYTE | pbDst | Specifies a pointer to the destination data
//          buffer. This pointer must always refer to the same location while
//          the stream header remains prepared. If an application needs to change
//          the destination location, it must unprepare the header and re-prepare
//          it with the alternate location.
//
//  @field  DWORD | cbDstLength | Specifies the length, in bytes, of the
//          destination data buffer pointed to by <e ACMSTREAMHEADER.pbDst>.
//          When the header is prepared, this member must specify the maximum
//          size that will be used in the destination buffer. Conversions can be
//          performed to destination lengths less than or equal to the original
//          prepared size. However, this member must be reset to the original
//          size when unpreparing the header.
//
//  @field  DWORD | cbDstLengthUsed | Specifies the amount of data, in bytes,
//          returned by a conversion. This member is not valid until the
//          conversion is complete. Note that this value may be less than or
//          equal to <e ACMSTREAMHEADER.cbDstLength>. An application must use
//          the <e ACMSTREAMHEADER.cbDstLengthUsed> member when advancing to
//          the next destination location for the conversion stream.
//
//  @field  DWORD | dwDstUser | Specifies 32 bits of user data. This can be
//          any instance data specified by the application.
//
//  @field  DWORD | dwReservedDriver[10] | This member is reserved and should
//          not be used. This member requires no initialization by the
//          application and should never be modified while the header
//          remains prepared.
//
//  @comm   Before an <t ACMSTREAMHEADER> structure can be used for a
//          conversion, it must be prepared with <f acmStreamPrepareHeader>.
//          When an application is finished with an <t ACMSTREAMHEADER>
//          structure, the <f acmStreamUnprepareHeader> function must be
//          called before freeing the source and destination buffers.
//
//  @xref   <f acmStreamPrepareHeader> <f acmStreamUnprepareHeader>
//          <f acmStreamConvert>
//
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmStreamConvert | The acmStreamConvert function
//          requests the Audio Compression Manager (ACM) to perform a
//          conversion on the specified conversion stream. A conversion may
//          be synchronous or asynchronous depending on how the stream
//          was opened.
//
//  @parm   HACMSTREAM | has | Identifies the open conversion stream.
//
//  @parm   LPACMSTREAMHEADER | pash | Specifies a pointer to a stream header
//          that describes source and destination buffers for a conversion. This
//          header must have been prepared previously using the
//          <f acmStreamPrepareHeader> function.
//
//  @parm   DWORD | fdwConvert | Specifies flags for doing the conversion.
//
//          @flag ACM_STREAMCONVERTF_BLOCKALIGN | Specifies that only integral
//          numbers of blocks will be converted. Converted data will end on
//          block aligned boundaries. An application should use this flag for
//          all conversions on a stream until there is not enough source data
//          to convert to a block-aligned destination. In this case, the last
//          conversion should be specified without this flag.
//
//          @flag ACM_STREAMCONVERTF_START | Specifies that the ACM conversion
//          stream should reinitialize its instance data. For example, if a
//          conversion stream holds instance data, such as delta or predictor
//          information, this flag will restore the stream to starting defaults.
//          Note that this flag can be specified with the ACM_STREAMCONVERTF_END
//          flag.
//
//          @flag ACM_STREAMCONVERTF_END | Specifies that the ACM conversion
//          stream should begin returning pending instance data. For example, if
//          a conversion stream holds instance data, such as the tail end of an
//          echo filter operation, this flag will cause the stream to start
//          returning this remaining data with optional source data. Note that
//          this flag can be specified with the ACM_STREAMCONVERTF_START flag.
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag MMSYSERR_INVALPARAM | One or more arguments passed are invalid.
//
//          @flag ACMERR_BUSY | The stream header <p pash> is currently in use
//          and cannot be reused.
//
//          @flag ACMERR_UNPREPARED | The stream header <p pash> is currently
//          not prepared by the <f acmStreamPrepareHeader> function.
//
//  @comm   The source and destination data buffers must be prepared with
//          <f acmStreamPrepareHeader> before they are passed to <f acmStreamConvert>.
//
//          If an asynchronous conversion request is successfully queued by
//          the ACM or driver, and later the conversion is determined to
//          be impossible, then the <t ACMSTREAMHEADER> will be posted back to
//          the application's callback with the <e ACMSTREAMHEADER.cbDstLengthUsed>
//          member set to zero.
//
//  @xref   <f acmStreamOpen> <f acmStreamReset> <f acmStreamPrepareHeader>
//          <f acmStreamUnprepareHeader>
//
//------------------------------------------------------------------------------
MMRESULT
acmStreamConvert(
    HACMSTREAM              has,
    LPACMSTREAMHEADER       pashi,
    DWORD                   fdwConvert
    )
{
    MMRESULT                mmRet;
    PACMSTREAM              pas = (PACMSTREAM) has;
    PACMDRVSTREAMHEADER_INT padsh;
    HANDLE                  hEvent;

    FUNC("acmStreamConvert");

    VE_HANDLE(has, TYPE_HACMSTREAM);

    VE_FLAGS(fdwConvert, ACM_STREAMCONVERTF_VALID);
    VE_COND_PARAM(pashi->cbStruct < sizeof(ACMDRVSTREAMHEADER));
    VE_COND(0 != (pashi->fdwStatus & ACMSTREAMHEADER_STATUSF_INQUEUE),  ACMERR_BUSY);
    VE_COND(0 == (pashi->fdwStatus & ACMSTREAMHEADER_STATUSF_PREPARED), ACMERR_UNPREPARED);

    padsh = (PACMDRVSTREAMHEADER_INT) pashi;

    padsh->cbSrcLengthUsed = 0L;
    padsh->cbDstLengthUsed = 0L;


    //
    //  validate that the header is appropriate for conversions.
    //
    //  NOTE! do not allow a destination buffer length that is smaller than
    //  it was prepared for--this keeps drivers from having to validate
    //  whether the destination buffer is large enough for the conversion
    //  from the source. so don't break this code!!!
    //
    VE_COND_HANDLE((UINT) has != padsh->dwPrepared);

    VE_COND_PARAM((padsh->pbSrc != padsh->pbPreparedSrc) ||
                  (padsh->cbSrcLength > padsh->cbPreparedSrcLength));

    VE_COND_PARAM((padsh->pbDst != padsh->pbPreparedDst) ||
        (padsh->cbDstLength != padsh->cbPreparedDstLength));

    //
    //  Callback event if we are converting async conversion to sync conversion.
    //
    hEvent = (ACMSTREAM_STREAMF_ASYNCTOSYNC & pas->fdwStream) ? (HANDLE)pas->adsi.dwCallback : NULL;

    //
    //  init things for the driver
    //
    padsh->fdwStatus  &= ~ACMSTREAMHEADER_STATUSF_DONE;
    padsh->fdwConvert  = fdwConvert;
    padsh->padshNext   = NULL;

    EnterHandle(pas);
    mmRet = (MMRESULT)IDriverMessage(pas->had,
                                   ACMDM_STREAM_CONVERT,
                                   (LPARAM)(LPVOID)&pas->adsi,
                                   (LPARAM)(LPVOID)padsh);
    if ( (hEvent) && (MMSYSERR_NOERROR == mmRet) )
    {
        TESTMSG("waiting for CALLBACK_EVENT");
        WaitForSingleObject(hEvent, INFINITE);
        ResetEvent(hEvent);
    }
    LeaveHandle(pas);

    if (MMSYSERR_NOERROR == mmRet) {
        if (pashi->cbSrcLength < pashi->cbSrcLengthUsed) {
            ERRMSG("Buggy driver returned more data used than given!?!");
            pashi->cbSrcLengthUsed = pashi->cbSrcLength;
        }

        if (pashi->cbDstLength < pashi->cbDstLengthUsed) {
            ERRMSG("buggy driver used more destination space than allowed!?!");
            pashi->cbDstLengthUsed = pashi->cbDstLength;
        }

        //
        //  if sync conversion succeeded, then mark done bit for the
        //  driver...
        //
        if (0 == (ACM_STREAMOPENF_ASYNC & pas->adsi.fdwOpen))
            padsh->fdwStatus |= ACMSTREAMHEADER_STATUSF_DONE;
    }

    //
    //  don't allow driver to set bits that we don't want them to!
    //
    pashi->fdwStatus &= ACMSTREAMHEADER_STATUSF_VALID;

EXIT:

    FUNC_EXIT();
    return (mmRet);
}



