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
//   acmfmt.c
//
//   Copyright (c) 1991-2000 Microsoft Corporation
//
//   This module provides the wave format enumeration and string API's.
//
//------------------------------------------------------------------------------
#include "acmi.h"

//
//  array of _standard_ sample rates supported
//
//
static const UINT auFormatIndexToSampleRate[] =
{
    8000,
    11025,
    22050,
    44100
};

#define CODEC_MAX_SAMPLE_RATES  _countof(auFormatIndexToSampleRate)

//
//
//
//
#define CODEC_MAX_CHANNELS      2

//
//  array of bits per sample supported
//
//
static const UINT auFormatIndexToBitsPerSample[] =
{
    8,
    16
};

#define CODEC_MAX_BITSPERSAMPLE_PCM _countof(auFormatIndexToBitsPerSample)

//
//  number of formats we enumerate per channels is number of sample rates
//  times number of channels times number of
//  (bits per sample) types.
//
#define CODEC_MAX_STANDARD_FORMATS_PCM  (CODEC_MAX_SAMPLE_RATES *   \
                                         CODEC_MAX_CHANNELS *       \
                                         CODEC_MAX_BITSPERSAMPLE_PCM)

//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API_STRUCTURE
//
//  @struct WAVEFORMATEX | The <t WAVEFORMATEX> structure defines the
//          format of waveform data. Only format information common to all
//          waveform data formats is included in this structure. For formats
//          that require additional information, this structure is included
//          as the first member in another structure, along with the additional
//          information.
//
//  @field  WORD | wFormatTag | Specifies the waveform format type. Format
//          tags are registered with Microsoft for many compression algorithms.
//          A complete list of format tags can be found in the MMREG.H header
//          file available from Microsoft. For more information on format
//          tags, contact Microsoft for availability of the Multimedia Developer
//          Registration Kit:
//
//              Microsoft Corporation           <nl>
//              Advanced Consumer Technology    <nl>
//              Product Marketing               <nl>
//              One Microsoft Way               <nl>
//              Redmond, WA 98052-6399          <nl>
//
//  @field  WORD | nChannels | Specifies the number of channels in the
//          waveform data. Monaural data uses one channel and stereo data uses
//          two channels.
//
//  @field  DWORD | nSamplesPerSec | Specifies the sample rate, in samples
//          per second (Hertz), that each channel should be played or recorded.
//          If <e WAVEFORMATEX.wFormatTag> is WAVE_FORMAT_PCM, then common values
//          for <e WAVEFORMATEX.nSamplesPerSec> are 8.0 kHz, 11.025 kHz,
//          22.05 kHz, and 44.1 kHz. For non-PCM formats, this member must be
//          computed according to the manufacturer's specification of the format
//          tag.
//
//  @field  DWORD | nAvgBytesPerSec | Specifies the required average
//          data-transfer rate, in bytes per second, for the format tag. If
//          <e WAVEFORMATEX.wFormatTag> is WAVE_FORMAT_PCM, then
//          <e WAVEFORMATEX.nAvgBytesPerSec> should be equal to the product
//          of <e WAVEFORMATEX.nSamplesPerSec> and <e WAVEFORMATEX.nBlockAlign>.
//          For non-PCM formats, this member must be computed according to the
//          manufacturer's specification of the format tag.
//
//          Playback and record software can estimate buffer sizes using the
//          <e WAVEFORMATEX.nAvgBytesPerSec> member.
//
//  @field  WORD | nBlockAlign | Specifies the block alignment, in bytes.
//          The block alignment is the minimum atomic unit of data for the
//          <e WAVEFORMATEX.wFormatTag>. If <e WAVEFORMATEX.wFormatTag> is
//          WAVE_FORMAT_PCM, then <e WAVEFORMATEX.nBlockAlign> should be equal
//          to the product of <e WAVEFORMATEX.nChannels> and
//          <e WAVEFORMATEX.wBitsPerSample> divided by 8 (bits per byte).
//          For non-PCM formats, this member must be computed according to the
//          manufacturer's specification of the format tag.
//
//          Playback and record software must process a multiple of
//          <e WAVEFORMATEX.nBlockAlign> bytes of data at a time. Data written
//          and read from a device must always start at the beginning of a
//          block. For example, it is illegal to start playback of PCM data
//          in the middle of a sample (that is, on a non-block-aligned boundary).
//
//  @field  WORD | wBitsPerSample | Specifies the bits per sample for the
//          <e WAVEFORMATEX.wFormatTag>. If <e WAVEFORMATEX.wFormatTag> is
//          WAVE_FORMAT_PCM, then <e WAVEFORMATEX.wBitsPerSample> should be
//          equal to 8 or 16. For non-PCM formats, this member must be set
//          according to the manufacturer's specification of the format tag.
//          Note that some compression schemes cannot define a value for
//          <e WAVEFORMATEX.wBitsPerSample>, so this member can be zero.
//
//  @field  WORD | cbSize | Specifies the size, in bytes, of extra format
//          information appended to the end of the <t WAVEFORMATEX> structure.
//          This information can be used by non-PCM formats to store extra
//          attributes for the <e WAVEFORMATEX.wFormatTag>. If no extra
//          information is required by the <e WAVEFORMATEX.wFormatTag>, then
//          this member must be set to zero. Note that for WAVE_FORMAT_PCM
//          formats (and only WAVE_FORMAT_PCM formats), this member is ignored.
//
//          An example of a format that uses extra information is the
//          Microsoft Adaptive Delta Pulse Code Modulation (MS-ADPCM) format.
//          The <e WAVEFORMATEX.wFormatTag> for MS-ADPCM is WAVE_FORMAT_ADPCM.
//          The <e WAVEFORMATEX.cbSize> member will normally be set to 32.
//          The extra information stored for WAVE_FORMAT_ADPCM is coefficient
//          pairs required for encoding and decoding the waveform data.
//
//  @xref   <t WAVEFORMAT> <t PCMWAVEFORMAT> <t WAVEFILTER>
//
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   DWORD | acmGetVersion | This function returns the version number
//          of the Audio Compression Manager (ACM).
//
//  @rdesc  The version number is returned as a hexadecimal number of the form
//          0xAABBCCCC, where AA is the major version number, BB is the minor
//          version number, and CCCC is the build number.
//
//  @comm   An application must verify that the ACM version is at least
//          0x02000000 or greater before attempting to use any other ACM
//          functions. Versions earlier than version 2.00 of the ACM only support
//          <f acmGetVersion>. Also note that the build number (CCCC) is always
//          zero for the retail (non-debug) version of the ACM. The debug
//          version of the ACM will always return a non-zero value for the
//          build number.
//
//------------------------------------------------------------------------------
DWORD
acmGetVersion(void)
{
    FUNC("acmGetVersion");
    // BUGBUG real version?
    return (0x02001997);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IMetricsMaxSizeFormat(
    HACMDRIVER          had,        // NULL is OK.
    LPDWORD             pdwSize
    )
{
    MMRESULT            mmRet;
    PACMDRIVERID        padid;
    DWORD               cbFormatSizeLargest;
    UINT                u;

    FUNC("IMetricsMaxSizeFormat");

    cbFormatSizeLargest = sizeof(WAVEFORMATEX);

    if (NULL == had) {
        //
        // Find the largest size format of all drivers.
        //
        HACMDRIVERID    hadid;

        hadid = NULL;
        while (MMSYSERR_NOERROR == IDriverGetNext(&hadid, hadid, 0L)) {
            padid = (PACMDRIVERID) hadid;

            for (u = 0; u < padid->cFormatTags; u++) {
                if (padid->paFormatTagCache[u].cbFormatSize > cbFormatSizeLargest)
                    cbFormatSizeLargest = padid->paFormatTagCache[u].cbFormatSize;
            }
        }
    } else {
        //
        // Find the largest size format of this one driver.
        //
        padid = acm_GetPadidFromHad(had);
        VE_COND_HANDLE(!padid);

        for (u=0; u<padid->cFormatTags; u++) {
            if (padid->paFormatTagCache[u].cbFormatSize > cbFormatSizeLargest)
                cbFormatSizeLargest = padid->paFormatTagCache[u].cbFormatSize;
        }
    }

    *pdwSize = cbFormatSizeLargest;
    mmRet = (0 == cbFormatSizeLargest ? ACMERR_NOTPOSSIBLE : MMSYSERR_NOERROR);
EXIT:
    FUNC_EXIT();
    return (mmRet);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IMetricsMaxSizeFilter(
    HACMDRIVER          had,        // NULL is OK.
    LPDWORD             pdwSize
    )
{
    PACMDRIVERID        padid;
    DWORD               cbFilterSizeLargest;
    UINT                u;
    MMRESULT            mmRet;

    FUNC("IMetricsMaxSizeFilter");

    cbFilterSizeLargest = sizeof(WAVEFILTER);

    if (NULL == had) {
        HACMDRIVERID    hadid;

        hadid = NULL;
        while (MMSYSERR_NOERROR == IDriverGetNext(&hadid, hadid, 0L)) {
            padid = (PACMDRIVERID)hadid;

            for (u=0; u<padid->cFilterTags; u++) {
                if (padid->paFilterTagCache[u].cbFilterSize > cbFilterSizeLargest)
                    cbFilterSizeLargest = padid->paFilterTagCache[u].cbFilterSize;
            }
        }
    } else {
        padid = acm_GetPadidFromHad(had);
        VE_COND_HANDLE(!padid);

        for (u=0; u<padid->cFilterTags; u++) {
            if (padid->paFilterTagCache[u].cbFilterSize > cbFilterSizeLargest)
                cbFilterSizeLargest = padid->paFilterTagCache[u].cbFilterSize;
        }
    }

    *pdwSize = cbFilterSizeLargest;
    mmRet = (0 == cbFilterSizeLargest ? ACMERR_NOTPOSSIBLE : MMSYSERR_NOERROR);

EXIT:
    FUNC_EXIT();
    return(mmRet);
}


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmMetrics | This function returns various metrics for
//          the Audio Compression Manager (ACM) or related ACM objects.
//
//  @parm   HACMOBJ | hao | Specifies the ACM object to query for the metric
//          specified in <p uMetric>. This argument may be NULL for some
//          queries.
//
//  @parm   UINT | uMetric | Specifies the metric index to be returned in
//          <p pMetric>.
//
//          @flag ACM_METRIC_COUNT_DRIVERS | Specifies that the returned value is
//          the total number of enabled global ACM drivers (of all support types)
//          in the system. The <p hao> argument must be NULL for this metric
//          index. The <p pMetric> argument must point to buffer of size equal
//          to a DWORD.
//
//          @flag ACM_METRIC_COUNT_CODECS | Specifies that the returned value is
//          the number of global ACM compressor or decompressor drivers in
//          the system. The <p hao> argument must be NULL for this metric index.
//          The <p pMetric> argument must point to a buffer of a size equal to a
//          DWORD.
//
//          @flag ACM_METRIC_COUNT_CONVERTERS | Specifies that the returned value
//          is the number of global ACM converter drivers in the system. The
//          <p hao> argument must be NULL for this metric index. The <p pMetric>
//          argument must point to a buffer of a size equal to a DWORD.
//
//          @flag ACM_METRIC_COUNT_FILTERS | Specifies that the returned value
//          is the number of global ACM filter drivers in the system. The <p hao>
//          argument must be NULL for this metric index. The <p pMetric> argument
//          must point to buffer of size equal to a DWORD.
//
//          @flag ACM_METRIC_COUNT_DISABLED | Specifies that the returned value
//          is the total number of global disabled ACM drivers (of all support
//          types) in the system. The <p hao> argument must be NULL for this
//          metric index. The <p pMetric> argument must point to a buffer of a size
//          equal to a DWORD. The sum of the ACM_METRIC_COUNT_DRIVERS and
//          ACM_METRIC_COUNT_DISABLED metrics is the total number of globally
//          installed ACM drivers.
//
//          @flag ACM_METRIC_COUNT_HARDWARE | Specifies that the returned value
//          is the number of global ACM hardware drivers in the system. The <p hao>
//          argument must be NULL for this metric index. The <p pMetric> argument
//          must point to buffer of size equal to a DWORD.
//
//          @flag ACM_METRIC_COUNT_LOCAL_DRIVERS | Specifies that the returned
//          value is the total number of enabled local ACM drivers (of all
//          support types) for the calling task. The <p hao> argument must be NULL for
//          this metric index. The <p pMetric> argument must point to a buffer of
//          a size equal to a DWORD.
//          NOTE! : Windows CE does not support local drivers, so this
//          metric always returns zero.
//
//          @flag ACM_METRIC_COUNT_LOCAL_CODECS | Specifies that the returned
//          value is the number of local ACM compressor and/or decompressor
//          drivers for the calling task. The <p hao> argument must be NULL for
//          this metric index. The <p pMetric> argument must point to a buffer of
//          a size equal to a DWORD.
//          NOTE! : Windows CE does not support local drivers, so this
//          metric always returns zero.
//
//          @flag ACM_METRIC_COUNT_LOCAL_CONVERTERS | Specifies that the returned
//          value is the number of local ACM converter drivers for the calling
//          task. The <p hao> argument must be NULL for this metric index. The
//          <p pMetric> argument must point to a buffer of a size equal to a DWORD.
//          NOTE! : Windows CE does not support local drivers, so this
//          metric always returns zero.
//
//          @flag ACM_METRIC_COUNT_LOCAL_FILTERS | Specifies that the returned
//          value is the number of local ACM filter drivers for the calling
//          task. The <p hao> argument must be NULL for this metric index. The
//          <p pMetric> argument must point to a buffer of a size equal to a DWORD.
//          NOTE! : Windows CE does not support local drivers, so this
//          metric always returns zero.
//
//          @flag ACM_METRIC_COUNT_LOCAL_DISABLED | Specifies that the returned
//          value is the total number of local disabled ACM drivers, of all
//          support types, for the calling task. The <p hao> argument must be
//          NULL for this metric index. The <p pMetric> argument must point to a
//          buffer of a size equal to a DWORD. The sum of the
//          ACM_METRIC_COUNT_LOCAL_DRIVERS and ACM_METRIC_COUNT_LOCAL_DISABLED
//          metrics is the total number of locally installed ACM drivers.
//          NOTE! : Windows CE does not support local drivers, so this
//          metric always returns zero.
//
//          @flag ACM_METRIC_HARDWARE_WAVE_INPUT | Specifies that the returned
//          value is the waveform input device identifier associated with the
//          specified driver. The <p hao> argument must be a valid ACM driver
//          identifier (<t HACMDRIVERID>) that supports the
//          ACMDRIVERDETAILS_SUPPORTF_HARDWARE flag. If no waveform input device
//          is associated with the driver, then MMSYSERR_NOTSUPPORTED is returned.
//          The <p pMetric> argument must point to a buffer of a size equal to a
//          DWORD.
//
//          @flag ACM_METRIC_HARDWARE_WAVE_OUTPUT | Specifies that the returned
//          value is the waveform output device identifier associated with the
//          specified driver. The <p hao> argument must be a valid ACM driver
//          identifier (<t HACMDRIVERID>) that supports the
//          ACMDRIVERDETAILS_SUPPORTF_HARDWARE flag. If no waveform output device
//          is associated with the driver, then MMSYSERR_NOTSUPPORTED is returned.
//          The <p pMetric> argument must point to a buffer of a size equal to a
//          DWORD.
//
//          @flag ACM_METRIC_MAX_SIZE_FORMAT | Specifies that the returned value
//          is the size of the largest <t WAVEFORMATEX> structure. If <p hao>
//          is NULL, then the return value is the largest <t WAVEFORMATEX>
//          structure in the system. If <p hao> identifies an open instance
//          of an ACM driver (<t HACMDRIVER>) or an ACM driver identifier
//          (<t HACMDRIVERID>), then the largest <t WAVEFORMATEX>
//          structure for that driver is returned. The <p pMetric> argument must
//          point to a buffer of a size equal to a DWORD. This metric is not allowed
//          for an ACM stream handle (<t HACMSTREAM>).
//
//          @flag ACM_METRIC_MAX_SIZE_FILTER | Specifies that the returned value
//          is the size of the largest <t WAVEFILTER> structure. If <p hao>
//          is NULL, then the return value is the largest <t WAVEFILTER> structure
//          in the system. If <p hao> identifies an open instance of an ACM
//          driver (<t HACMDRIVER>) or an ACM driver identifier (<t HACMDRIVERID>),
//          then the largest <t WAVEFILTER> structure for that driver is returned.
//          The <p pMetric> argument must point to a buffer of a size equal to a
//          DWORD. This metric is not allowed for an ACM stream handle
//          (<t HACMSTREAM>).
//
//          @flag ACM_METRIC_DRIVER_SUPPORT | Specifies that the returned value
//          is the <e ACMDRIVERDETAILS.fdwSupport> flags for the specified driver.
//          The <p hao> argument must be a valid ACM driver identifier
//          (<t HACMDRIVERID>). The <p pMetric> argument must point to a buffer of
//          a size equal to a DWORD.
//
//          @flag ACM_METRIC_DRIVER_PRIORITY | Specifies that the returned value
//          is the current priority for the specified driver.
//          The <p hao> argument must be a valid ACM driver identifier
//          (<t HACMDRIVERID>). The <p pMetric> argument must point to a buffer of
//          a size equal to a DWORD.
//
//  @parm   LPVOID | pMetric | Specifies a pointer to the buffer that will
//          receive the metric details. The exact definition depends on the
//          <p uMetric> index.
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALPARAM | One or more arguments passed are invalid.
//
//          @flag MMSYSERR_NOTSUPPORTED | The <p uMetric> index is not supported.
//
//          @flag ACMERR_NOTPOSSIBLE | The <p uMetric> index cannot be returned
//          for the specified <p hao>.
//
//  @xref   <f acmDriverDetails> <f acmFormatTagDetails> <f acmFormatDetails>
//          <f acmFilterTagDetails> <f acmFilterDetails>
//
//------------------------------------------------------------------------------
MMRESULT
acmMetrics(
    HACMOBJ                 hao,
    UINT                    uMetric,
    LPVOID                  pMetricVoid
    )
{
    MMRESULT            mmRet = ACMERR_NOTPOSSIBLE;
    DWORD               fdwSupport;
    DWORD               fdwEnum;
    PACMDRIVERID        padid;
    BOOL                f;
    PDWORD              pMetric = (PDWORD)pMetricVoid;

    FUNC("acmMetrics");
    HEXPARAM(hao);
    HEXPARAM(uMetric);
    HEXPARAM(pMetric);

    if (NULL != hao)
        VE_HANDLE(hao, TYPE_HACMOBJ);

    VE_WPOINTER(pMetric, sizeof(DWORD));

    switch (uMetric) {

        case ACM_METRIC_COUNT_LOCAL_DRIVERS:
        case ACM_METRIC_COUNT_DRIVERS:

            if (NULL != hao)
                goto EXIT_ERROR;

            //
            //  include all global enabled drivers
            //
            fdwSupport = 0L;
            if (ACM_METRIC_COUNT_LOCAL_DRIVERS == uMetric)
                fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_LOCAL;

            fdwEnum    = 0L;
            mmRet = IDriverCount(pMetric, fdwSupport, fdwEnum);
            break;

        case ACM_METRIC_COUNT_LOCAL_CODECS:
        case ACM_METRIC_COUNT_CODECS:

            if (NULL != hao)
                goto EXIT_ERROR;

            fdwSupport = ACMDRIVERDETAILS_SUPPORTF_CODEC;
            if (ACM_METRIC_COUNT_LOCAL_CODECS == uMetric)
                fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_LOCAL;

            fdwEnum    = 0L;
            mmRet = IDriverCount(pMetric, fdwSupport, fdwEnum);
            break;

        case ACM_METRIC_COUNT_LOCAL_CONVERTERS:
        case ACM_METRIC_COUNT_CONVERTERS:

            if (NULL != hao)
                goto EXIT_ERROR;

            fdwSupport = ACMDRIVERDETAILS_SUPPORTF_CONVERTER;
            if (ACM_METRIC_COUNT_LOCAL_CONVERTERS == uMetric)
                fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_LOCAL;

            fdwEnum    = 0L;
            mmRet = IDriverCount(pMetric, fdwSupport, fdwEnum);
            break;

        case ACM_METRIC_COUNT_LOCAL_FILTERS:
        case ACM_METRIC_COUNT_FILTERS:

            if (NULL != hao)
                goto EXIT_ERROR;

            fdwSupport = ACMDRIVERDETAILS_SUPPORTF_FILTER;
            if (ACM_METRIC_COUNT_LOCAL_FILTERS == uMetric)
                fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_LOCAL;

            fdwEnum    = 0L;
            mmRet = IDriverCount(pMetric, fdwSupport, fdwEnum);
            break;

        case ACM_METRIC_COUNT_LOCAL_DISABLED:
        case ACM_METRIC_COUNT_DISABLED:

            if (NULL != hao)
                goto EXIT_ERROR;

            fdwSupport = ACMDRIVERDETAILS_SUPPORTF_DISABLED;
            if (ACM_METRIC_COUNT_LOCAL_DISABLED == uMetric)
                fdwSupport |= ACMDRIVERDETAILS_SUPPORTF_LOCAL;

            fdwEnum    = ACM_DRIVERENUMF_DISABLED;
            mmRet = IDriverCount(pMetric, fdwSupport, fdwEnum);
            break;


        case ACM_METRIC_COUNT_HARDWARE:

            if (NULL != hao)
                goto EXIT_ERROR;

            fdwSupport = ACMDRIVERDETAILS_SUPPORTF_HARDWARE;
            fdwEnum    = 0L;
            mmRet = IDriverCount(pMetric, fdwSupport, fdwEnum);
            break;


        case ACM_METRIC_HARDWARE_WAVE_INPUT:
        case ACM_METRIC_HARDWARE_WAVE_OUTPUT:

            *((LPDWORD)pMetric) = (DWORD)-1L;

            VE_HANDLE(hao, TYPE_HACMDRIVERID);

            f = (ACM_METRIC_HARDWARE_WAVE_INPUT == uMetric);
            mmRet = IDriverGetWaveIdentifier((HACMDRIVERID)hao, pMetric, f);
            break;


        case ACM_METRIC_MAX_SIZE_FORMAT:

            mmRet = IMetricsMaxSizeFormat((HACMDRIVER)hao, (LPDWORD)pMetric );
            break;


        case ACM_METRIC_MAX_SIZE_FILTER:

            mmRet = IMetricsMaxSizeFilter((HACMDRIVER)hao, (LPDWORD)pMetric );
            break;


        case ACM_METRIC_DRIVER_SUPPORT:

            *((LPDWORD)pMetric) = 0L;
            VE_HANDLE(hao, TYPE_HACMDRIVERID);

            mmRet = IDriverSupport((HACMDRIVERID)hao, pMetric, TRUE);
            break;

        case ACM_METRIC_DRIVER_PRIORITY:

            *((LPDWORD)pMetric) = 0L;

            VE_HANDLE(hao, TYPE_HACMDRIVERID);

            padid = (PACMDRIVERID)hao;

            if (padid)
            {
                *((LPDWORD) pMetric) = (UINT)padid->uPriority;
            }

            mmRet = MMSYSERR_NOERROR;
            break;

        default:
            mmRet = MMSYSERR_NOTSUPPORTED;
            break;
    }

    goto EXIT;

EXIT_ERROR:
    ERRMSG("ACM object handle must be NULL for specified metric.");
    *((LPDWORD)pMetric) = 0L;
    mmRet = MMSYSERR_INVALHANDLE;

EXIT:
    FUNC_EXIT();
    return (mmRet);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IFormatTagDetails(
    HACMDRIVERID            hadid,
    PACMFORMATTAGDETAILS_INT paftd,
    DWORD                   fdwDetails
    )
{
    PACMDRIVERID    padid = (PACMDRIVERID) hadid;
    UINT            u;
    DWORD           fdwQuery = (ACM_FORMATTAGDETAILSF_QUERYMASK & fdwDetails);
    MMRESULT        mmRet = ACMERR_NOTPOSSIBLE;

    FUNC("IFormatTagDetails");

    switch (fdwQuery) {

        case ACM_FORMATTAGDETAILSF_FORMATTAG:
            for (u=0; u<padid->cFormatTags; u++) {
                if (padid->paFormatTagCache[u].dwFormatTag == paftd->dwFormatTag) {
                    mmRet = MMSYSERR_NOERROR;
                    break;
                }
            }
            break;

        case ACM_FORMATTAGDETAILSF_LARGESTSIZE:
        case ACM_FORMATTAGDETAILSF_INDEX:
            mmRet = MMSYSERR_NOERROR;
            break;
    }

    //
    //
    //
    if (MMSYSERR_NOERROR == mmRet) {
        EnterHandle((HACMDRIVERID)padid);
        mmRet = (MMRESULT)IDriverMessageId((HACMDRIVERID)padid,
                                         ACMDM_FORMATTAG_DETAILS,
                                         (LPARAM)(LPVOID)paftd,
                                         fdwDetails);
        LeaveHandle((HACMDRIVERID)padid);
    }

    if (MMSYSERR_NOERROR != mmRet)
        goto EXIT;

    switch (paftd->dwFormatTag) {

        case WAVE_FORMAT_UNKNOWN:
            ERRMSG("Driver returned format tag 0!");
            GOTO_EXIT(mmRet = MMSYSERR_ERROR);

        case WAVE_FORMAT_PCM:
            if (TEXT('\0') != paftd->szFormatTag[0])
                WARNMSG("driver returned custom PCM format tag name! ignoring it!");

            LoadString(pag->hinst,
                       IDS_FORMAT_TAG_PCM,
                       paftd->szFormatTag,
                       SIZEOF(paftd->szFormatTag));
            break;

        case WAVE_FORMAT_DEVELOPMENT:
            WARNMSG("Driver returned DEVELOPMENT format tag--do not ship this way.");
            break;

    }

EXIT:
    FUNC_EXIT();
    return(mmRet);
}


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API_STRUCTURE
//
//  @struct ACMFORMATTAGDETAILS | The <t ACMFORMATTAGDETAILS> structure
//          details a wave format tag for an Audio Compression Manager (ACM)
//          driver.
//
//  @field  DWORD | cbStruct | Specifies the size in bytes of the
//          <t ACMFORMATTAGDETAILS> structure. This member must be initialized
//          before calling the <f acmFormatTagDetails> or <f acmFormatTagEnum>
//          functions. The size specified in this member must be large enough to
//          contain the base <t ACMFORMATTAGDETAILS> structure. When the
//          <f acmFormatTagDetails> function returns, this member contains the
//          actual size of the information returned. The returned information
//          will never exceed the requested size.
//
//  @field  DWORD | dwFormatTagIndex | Specifies the index of the format tag
//          for which details will be retrieved. The index ranges from zero to one
//          less than the number of format tags supported by an ACM driver. The
//          number of format tags supported by a driver is contained in the
//          <e ACMDRIVERDETAILS.cFormatTags> member of the <t ACMDRIVERDETAILS>
//          structure. The <e ACMFORMATTAGDETAILS.dwFormatTagIndex> member is
//          only used when querying format tag details on a driver by index;
//          otherwise, this member should be zero.
//
//  @field  DWORD | dwFormatTag | Specifies the wave format tag that the
//          <t ACMFORMATTAGDETAILS> structure describes. This member is used
//          as an input for the ACM_FORMATTAGDETAILSF_FORMATTAG and
//          ACM_FORMATTAGDETAILSF_LARGESTSIZE query flags. This member is always
//          returned if the <f acmFormatTagDetails>  function is successful. This member
//          should be set to WAVE_FORMAT_UNKNOWN for all other query flags.
//
//  @field  DWORD | cbFormatSize | Specifies the largest total size in bytes
//          of a wave format of the <e ACMFORMATTAGDETAILS.dwFormatTag> type.
//          For example, this member will be 16 for WAVE_FORMAT_PCM and 50 for
//          WAVE_FORMAT_ADPCM.
//
//  @field  DWORD | fdwSupport | Specifies driver-support flags specific to
//          the format tag. These flags are identical to the
//          <e ACMDRIVERDETAILS.fdwSupport> flags of the <t ACMDRIVERDETAILS>
//          structure. This argument may be some combination of the following
//          values and refer to what operations the driver supports with the
//          format tag:
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CODEC | Specifies that this driver
//          supports conversion between two different format tags where one of
//          the tags is the specified format tag. For example, if a driver
//          supports compression from WAVE_FORMAT_PCM to WAVE_FORMAT_ADPCM,
//          then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CONVERTER | Specifies that this
//          driver supports conversion between two different formats of the
//          specified format tag. For example, if a driver supports resampling
//          of WAVE_FORMAT_PCM, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_FILTER | Specifies that this driver
//          supports a filter (modification of the data without changing any
//          of the format attributes). For example, if a driver supports volume
//          or echo operations on the specified format tag, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_ASYNC | Specifies that this driver
//          supports asynchronous conversions with the specified format tag.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_HARDWARE | Specifies that this driver
//          supports hardware input and/or output of the specified format tag
//          through a waveform device. An application should use <f acmMetrics>
//          with the ACM_METRIC_HARDWARE_WAVE_INPUT and
//          ACM_METRIC_HARDWARE_WAVE_OUTPUT metric indexes to get the waveform
//          device identifiers associated with the supporting ACM driver.
//
//  @field  DWORD | cStandardFormats | Specifies the number of standard
//          formats of the <e ACMFORMATTAGDETAILS.dwFormatTag> type; that is, the
//          combination of all sample rates, bits per sample, channels, and so on.
//          This value can specify all formats supported by the driver, but not necessarily.
//
//  @field  char | szFormatTag[ACMFORMATTAGDETAILS_FORMATTAG_CHARS] |
//          Specifies a string that describes the <e ACMFORMATTAGDETAILS.dwFormatTag>
//          type. This string is always returned if the <f acmFormatTagDetails>
//          function is successful.
//
//  @xref   <f acmFormatTagDetails> <f acmFormatTagEnum>
//
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmFormatTagDetails | This function queries the Audio Compression
//          Manager (ACM) for details on a specific wave format tag.
//
//  @parm   HACMDRIVER | had | Optionally specifies an ACM driver to query
//          for wave format tag details. If this argument is NULL, then the
//          ACM uses the details from the first suitable ACM driver. Note that
//          an application must specify a valid <t HACMDRIVER> or <t HACMDRIVERID>
//          when using the ACM_FORMATTAGDETAILSF_INDEX query type. Driver
//          identifiers for disabled drivers are not allowed.
//
//  @parm   LPACMFORMATTAGDETAILS | paftd | Specifies a pointer to the
//          <t ACMFORMATTAGDETAILS> structure that is to receive the format
//          tag details.
//
//  @parm   DWORD | fdwDetails | Specifies flags for getting the details.
//
//          @flag ACM_FORMATTAGDETAILSF_INDEX | Indicates that a format tag index
//          was given in the <e ACMFORMATTAGDETAILS.dwFormatTagIndex> member of
//          the <t ACMFORMATTAGDETAILS> structure. The format tag and details
//          will be returned in the structure defined by <p paftd>. The index
//          ranges from zero to one less than the <e ACMDRIVERDETAILS.cFormatTags>
//          member returned in the <t ACMDRIVERDETAILS> structure for an ACM
//          driver. An application must specify a driver handle (<p had>) when
//          retrieving format tag details with this flag.
//
//          @flag ACM_FORMATTAGDETAILSF_FORMATTAG | Indicates that a format tag
//          was given in the <e ACMFORMATTAGDETAILS.dwFormatTag> member of
//          the <t ACMFORMATTAGDETAILS> structure. The format tag details will
//          be returned in the structure defined by <p paftd>. If an application
//          specifies an ACM driver handle (<p had>), then details on the format
//          tag will be returned for that driver. If an application specifies
//          NULL for <p had>, then the ACM finds the first acceptable driver
//          to return the details.
//
//          @flag ACM_FORMATTAGDETAILSF_LARGESTSIZE | Indicates that the details
//          on the format tag with the largest format size in bytes is to be
//          returned. The <e ACMFORMATTAGDETAILS.dwFormatTag> member must either
//          be WAVE_FORMAT_UNKNOWN or the format tag to find the largest size
//          for. If an application specifies an ACM driver handle (<p had>), then
//          details on the largest format tag will be returned for that driver.
//          If an application specifies NULL for <p had>, then the ACM finds an
//          acceptable driver with the largest format tag requested to return
//          the details.
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
//          @flag ACMERR_NOTPOSSIBLE | The details requested are not available.
//
//  @xref   <f acmDriverDetails> <f acmDriverOpen> <f acmFormatDetails>
//          <f acmFormatTagEnum> <f acmFilterTagDetails>
//
//------------------------------------------------------------------------------
MMRESULT
acmFormatTagDetails(
    HACMDRIVER              had,
    LPACMFORMATTAGDETAILS   paftdi,
    DWORD                   fdwDetails
    )
{
    PACMDRIVER              pad = NULL;
    HACMDRIVERID            hadid;
    PACMDRIVERID            padid = NULL;
    DWORD                   fdwQuery;
    MMRESULT                mmRet;
    UINT                    u;
    FUNC("acmFormatTagDetails");

    VE_FLAGS(fdwDetails, ACM_FORMATTAGDETAILSF_VALID);
    VE_WPOINTER(paftdi, sizeof(ACMFORMATTAGDETAILS));
    VE_COND_PARAM(sizeof(ACMFORMATTAGDETAILS) > paftdi->cbStruct);
    VE_COND_PARAM(0L != paftdi->fdwSupport);

    fdwQuery = (ACM_FORMATTAGDETAILSF_QUERYMASK & fdwDetails);

    switch (fdwQuery) {
        case ACM_FORMATTAGDETAILSF_INDEX:
            //
            //  we don't (currently) support index based enumeration across
            //  all drivers... may never support this. so validate the
            //  handle and fail if not valid (like NULL).
            //
            VE_HANDLE(had, TYPE_HACMOBJ);
// BUGBUG disable this check?!?
//            VE_COND_PARAM(WAVE_FORMAT_UNKNOWN != paftdi->dwFormatTag);
            break;

        case ACM_FORMATTAGDETAILSF_FORMATTAG:
            VE_COND_PARAM(WAVE_FORMAT_UNKNOWN == paftdi->dwFormatTag);
            break;

        case ACM_FORMATTAGDETAILSF_LARGESTSIZE:
            break;

        //
        //  we don't (currently) support the requested query type--so return
        //  not supported.
        //
        default:
            ERRMSG("unknown query type specified.");
            GOTO_EXIT(mmRet = MMSYSERR_NOTSUPPORTED);
    }

    //
    //
    //
    if (NULL != had) {
        VE_HANDLE(had, TYPE_HACMOBJ);

        pad = (PACMDRIVER)had;
        if (TYPE_HACMDRIVERID == pad->uHandleType) {
            padid = (PACMDRIVERID)pad;
            pad   = NULL;
            VE_COND(ACMDRIVERID_DRIVERF_DISABLED & padid->fdwDriver, MMSYSERR_NOTENABLED);
        } else {
            VE_HANDLE(had, TYPE_HACMDRIVER);
            padid = (PACMDRIVERID)pad->hadid;
        }
    }

    if (NULL == padid) {
        PACMDRIVERID    padidT;
        DWORD           cbFormatSizeLargest;

        padidT              = NULL;
        cbFormatSizeLargest = 0;
        hadid = NULL;

        ENTER_LIST_SHARED();

        while (MMSYSERR_NOERROR == IDriverGetNext(&hadid, hadid, 0L))
        {
            padidT = (PACMDRIVERID)hadid;

            switch (fdwQuery) {

                case ACM_FORMATTAGDETAILSF_FORMATTAG:
                    for (u=0; u<padidT->cFormatTags; u++) {
                        if (padidT->paFormatTagCache[u].dwFormatTag == paftdi->dwFormatTag) {
                            padid = padidT;
                            break;
                        }
                    }
                    break;

                case ACM_FORMATTAGDETAILSF_LARGESTSIZE:
                    for (u=0; u<padidT->cFormatTags; u++) {
                        if (WAVE_FORMAT_UNKNOWN != paftdi->dwFormatTag) {
                            if (padidT->paFormatTagCache[u].dwFormatTag != paftdi->dwFormatTag)
                                continue;
                        }
                        if (padidT->paFormatTagCache[u].cbFormatSize > cbFormatSizeLargest) {
                            cbFormatSizeLargest = padidT->paFormatTagCache[u].cbFormatSize;
                            padid = padidT;
                        }
                    }
                    break;

                default:
                    TESTMSG("unknown query type got through param validation?!?!");
            }
        }

        LEAVE_LIST_SHARED();
    }

    if (NULL != padid) {
        mmRet = IFormatTagDetails((HACMDRIVERID)padid, (PACMFORMATTAGDETAILS_INT)paftdi, fdwDetails);
    } else {
        //
        //  Caller didn't pass us a driver, nor could we find a driver.
        //  Unless caller was requesting a specific format tag other than
        //  PCM, let's go ahead and return PCM.
        //

        if ((ACM_FORMATTAGDETAILSF_FORMATTAG == fdwQuery) &&
            (WAVE_FORMAT_PCM != paftdi->dwFormatTag))
        {
            GOTO_EXIT(mmRet = ACMERR_NOTPOSSIBLE);
        }

        paftdi->dwFormatTagIndex = 0;
        paftdi->dwFormatTag      = WAVE_FORMAT_PCM;
        paftdi->cbFormatSize     = sizeof(PCMWAVEFORMAT);
        paftdi->fdwSupport       = 0;
        paftdi->cStandardFormats = CODEC_MAX_STANDARD_FORMATS_PCM;

        LoadString(pag->hinst,
                   IDS_FORMAT_TAG_PCM,
                   paftdi->szFormatTag,
                   SIZEOF(paftdi->szFormatTag));

        mmRet = MMSYSERR_NOERROR;
    }

EXIT:
    FUNC_EXIT();
    return(mmRet);
}


UINT
IFormatDetailsToString(
    LPACMFORMATDETAILS      pafd
    )
{
    TCHAR           ach[ACMFORMATDETAILS_FORMAT_CHARS];
    TCHAR           szChannels[24];
    UINT            u;
    LPWAVEFORMATEX  pwfx;
    UINT            uBits;
    UINT            uIds;
    TCHAR           gchIntlList[6];
    TCHAR           gchIntlThousand[6];
    int             iRet;

    FUNC("IFormatDetailsToString");

    pwfx = pafd->pwfx;

    uBits = pwfx->wBitsPerSample;

    iRet = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SLIST, gchIntlList, 6);
    if (iRet == 0) {
        //
        // Couldn't get list character, default to ','
        //
        StringCbPrintf(gchIntlList,sizeof(gchIntlList),TEXT(","));
    }

    iRet = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, gchIntlThousand, 6);
    if (iRet == 0) {
        //
        // Couldn't get thousands character, default to ','
        //
        StringCbPrintf(gchIntlThousand,sizeof(gchIntlThousand),TEXT(","));
    }


    if ((1 == pwfx->nChannels) || (2 == pwfx->nChannels))
    {
        if (0 == uBits)
            uIds = IDS_FORMAT_FORMAT_MONOSTEREO_0BIT;
        else
            uIds = IDS_FORMAT_FORMAT_MONOSTEREO;

        LoadString(pag->hinst, uIds, ach, SIZEOF(ach));

        u = (1 == pwfx->nChannels) ? IDS_FORMAT_CHANNELS_MONO : IDS_FORMAT_CHANNELS_STEREO;
        LoadString(pag->hinst, u, szChannels, SIZEOF(szChannels));

        if (0 == uBits)
        {
            u = StringCbPrintf(
                         pafd->szFormat,
                         sizeof(pafd->szFormat),
                         ach,
                         pwfx->nSamplesPerSec / 1000,
                         gchIntlThousand,
                         (UINT)(pwfx->nSamplesPerSec % 1000),
                         gchIntlList,
                         (LPSTR)szChannels);
        }
        else
        {
            u = StringCbPrintf(
                         pafd->szFormat,
                         sizeof(pafd->szFormat),
                         ach,
                         pwfx->nSamplesPerSec / 1000,
                         gchIntlThousand,
                         (UINT)(pwfx->nSamplesPerSec % 1000),
                         gchIntlList,
                         uBits,
                         gchIntlList,
                         (LPSTR)szChannels);
        }
    }
    else
    {
        if (0 == uBits)
            uIds = IDS_FORMAT_FORMAT_MULTICHANNEL_0BIT;
        else
            uIds = IDS_FORMAT_FORMAT_MULTICHANNEL;

        LoadString(pag->hinst, uIds, ach, SIZEOF(ach));

        if (0 == uBits)
        {
            u = StringCbPrintf(
                         pafd->szFormat,
                         sizeof(pafd->szFormat),
                         ach,
                         pwfx->nSamplesPerSec / 1000,
                         gchIntlThousand,
                         (UINT)(pwfx->nSamplesPerSec % 1000),
                         gchIntlList,
                         pwfx->nChannels);
        }
        else
        {
            u = StringCbPrintf(
                         pafd->szFormat,
                         sizeof(pafd->szFormat),
                         ach,
                         pwfx->nSamplesPerSec / 1000,
                         gchIntlThousand,
                         (UINT)(pwfx->nSamplesPerSec % 1000),
                         gchIntlList,
                         uBits,
                         gchIntlList,
                         pwfx->nChannels);
        }

    }

    FUNC_EXIT();
    return (u);
}


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API_STRUCTURE
//
//  @struct ACMFORMATDETAILS | The <t ACMFORMATDETAILS> structure details a
//          wave format for a specific format tag for an Audio Compression
//          Manager (ACM) driver.
//
//  @field  DWORD | cbStruct | Specifies the size, in bytes, of the
//          <t ACMFORMATDETAILS> structure. This member must be initialized
//          before calling the <f acmFormatDetails> or <f acmFormatEnum>
//          functions. The size specified in this member must be large enough to
//          contain the base <t ACMFORMATDETAILS> structure. When the
//          <f acmFormatDetails> function returns, this member contains the
//          actual size of the information returned. The returned information
//          will never exceed the requested size.
//
//  @field  DWORD | dwFormatIndex | Specifies the index of the format
//          to retrieve details for. The index ranges from zero to one
//          less than the number of standard formats supported by an ACM driver
//          for a format tag. The number of standard formats supported by a
//          driver for a format tag is contained in the
//          <e ACMFORMATTAGDETAILS.cStandardFormats> member of the
//          <t ACMFORMATTAGDETAILS> structure. The
//          <e ACMFORMATDETAILS.dwFormatIndex> member is only used when querying
//          standard format details on a driver by index; otherwise, this member
//          should be zero. Also note that this member will be set to zero
//          by the ACM when an application queries for details on a format. In
//          other words, this member is only used as an input argument and is
//          never returned by the ACM or an ACM driver.
//
//  @field  DWORD | dwFormatTag | Specifies the wave format tag that the
//          <t ACMFORMATDETAILS> structure describes. This member is used
//          as an input for the ACM_FORMATDETAILSF_INDEX query flag.  For
//          the ACM_FORMATDETAILSF_FORMAT query flag, this member
//          must be initialized to the same format tag as the
//          <e ACMFORMATDETAILS.pwfx> member specifies.  This member is always
//          returned if the <f acmFormatDetails> is successful. This member
//          should be set to WAVE_FORMAT_UNKNOWN for all other query flags.
//
//  @field  DWORD | fdwSupport | Specifies driver-support flags specific to
//          the specified format. These flags are identical to the
//          <e ACMDRIVERDETAILS.fdwSupport> flags of the <t ACMDRIVERDETAILS>
//          structure. This argument can be a combination of the following
//          values and indicates which operations the driver supports for the
//          format tag:
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CODEC | Specifies that this driver
//          supports conversion between two different format tags for the
//          specified format. For example, if a driver supports compression
//          from WAVE_FORMAT_PCM to WAVE_FORMAT_ADPCM with the specifed
//          format, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CONVERTER | Specifies that this
//          driver supports conversion between two different formats of the
//          same format tag while using the specified format. For example, if a
//          driver supports resampling of WAVE_FORMAT_PCM to the specified
//          format, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_FILTER | Specifies that this driver
//          supports a filter (which modifies data without changing any
//          format attributes) with the specified format. For example,
//          if a driver supports volume or echo operations on WAVE_FORMAT_PCM,
//          then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_ASYNC | Specifies that this driver
//          supports asynchronous conversions with the specified format.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_HARDWARE | Specifies that this driver
//          supports hardware input and/or output of the specified format
//          through a waveform device. An application should use <f acmMetrics>
//          with the ACM_METRIC_HARDWARE_WAVE_INPUT and
//          ACM_METRIC_HARDWARE_WAVE_OUTPUT metric indexes to get the waveform
//          device identifiers associated with the supporting ACM driver.
//
//  @field  LPWAVEFORMATEX | pwfx | Specifies a pointer to a <t WAVEFORMATEX>
//          data structure that will receive the format details. This structure requires no initialization
//          by the application unless the ACM_FORMATDETAILSF_FORMAT flag is specified
//          to <f acmFormatDetails>. In this case, the<e WAVEFORMATEX.wFormatTag> must be
//          equal to the <e ACMFORMATDETAILS.dwFormatTag> of the <t ACMFORMATDETAILS>
//          structure.
//
//  @field  DWORD | cbwfx | Specifies the size, in bytes, available for
//          the <e ACMFORMATDETAILS.pwfx> to receive the format details. The
//          <f acmMetrics> and <f acmFormatTagDetails> functions can be used to
//          determine the maximum size required for any format available for the
//          specified driver (or for all installed ACM drivers).
//
//  @field  char | szFormat[ACMFORMATDETAILS_FORMAT_CHARS] |
//          Specifies a string that describes the format for the
//          <e ACMFORMATDETAILS.dwFormatTag> type. This string is always returned
//          if the <f acmFormatDetails> function is successful.
//
//  @xref   <f acmFormatDetails> <f acmFormatEnum> <f acmFormatTagDetails>
//          <f acmFormatTagEnum>
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmFormatDetails | This function queries the Audio Compression
//          Manager (ACM) for details on format for a specific wave format tag.
//
//  @parm   HACMDRIVER | had | Optionally specifies an ACM driver to query
//          for wave format details for a format tag. If this argument is NULL,
//          then the ACM uses the details from the first suitable ACM driver.
//
//  @parm   LPACMFORMATDETAILS | pafd | Specifies a pointer to the
//          <t ACMFORMATDETAILS> structure that is to receive the format
//          details for the given format tag.
//
//  @parm   DWORD | fdwDetails | Specifies flags for getting the wave format tag details.
//
//          @flag ACM_FORMATDETAILSF_INDEX | Indicates that a format index for
//          the format tag was given in the <e ACMFORMATDETAILS.dwFormatIndex>
//          member of the <t ACMFORMATDETAILS> structure. The format details
//          will be returned in the structure defined by <p pafd>. The index
//          ranges from zero to one less than the
//          <e ACMFORMATTAGDETAILS.cStandardFormats> member returned in the
//          <t ACMFORMATTAGDETAILS> structure for a format tag. An application
//          must specify a driver handle (<p had>) when retrieving
//          format details with this flag. Refer to the description for the
//          <t ACMFORMATDETAILS> structure for information on what members
//          should be initialized before calling this function.
//
//          @flag ACM_FORMATDETAILSF_FORMAT | Indicates that a <t WAVEFORMATEX>
//          structure pointed to by the <e ACMFORMATDETAILS.pwfx> member of the
//          <t ACMFORMATDETAILS> structure was given and the remaining details
//          should be returned. The <e ACMFORMATDETAILS.dwFormatTag> member
//          of the <t ACMFORMATDETAILS> structure must be initialized to the same format
//          tag as the <e ACMFORMATDETAILS.pwfx> member specifies. This
//          query type can be used to get a string description of an arbitrary
//          format structure. If an application specifies an ACM driver handle
//          (<p had>), then details on the format will be returned for that
//          driver. If an application specifies NULL for <p had>, then the ACM
//          finds the first acceptable driver to return the details.
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
//          @flag ACMERR_NOTPOSSIBLE | The details requested are not available.
//
//  @xref   <f acmFormatTagDetails> <f acmDriverDetails> <f acmDriverOpen>
//
//------------------------------------------------------------------------------
MMRESULT
acmFormatDetails(
    HACMDRIVER              had,
    LPACMFORMATDETAILS      pafdi,
    DWORD                   fdwDetails
    )
{
    MMRESULT        mmRet;
    PACMDRIVER      pad;
    HACMDRIVERID    hadid;
    DWORD           dwQuery;
    BOOL            fNone;

    FUNC("acmFormatDetails");
    HEXPARAM(fdwDetails);

    VE_WPOINTER(pafdi, sizeof(ACMFORMATDETAILS));
    VE_COND_PARAM(sizeof(ACMFORMATDETAILS) > pafdi->cbStruct);

    VE_FLAGS(fdwDetails, ACM_FORMATDETAILSF_VALID);

    VE_COND_PARAM(sizeof(PCMWAVEFORMAT) > pafdi->cbwfx);
    VE_COND_PARAM(0L != pafdi->fdwSupport);

    dwQuery = ACM_FORMATDETAILSF_QUERYMASK & fdwDetails;

    switch (dwQuery) {
        case ACM_FORMATDETAILSF_FORMAT:
            VE_COND_PARAM(pafdi->dwFormatTag != pafdi->pwfx->wFormatTag);
            __fallthrough;

        case ACM_FORMATDETAILSF_INDEX:
            VE_COND_PARAM(WAVE_FORMAT_UNKNOWN == pafdi->dwFormatTag);
            //
            //  we don't (currently) support index based enumeration across
            //  all drivers... may never support this. so validate the
            //  handle and fail if not valid (like NULL).
            //
            if (ACM_FORMATDETAILSF_INDEX == dwQuery) {
                ACMFORMATTAGDETAILS aftd;

                VE_HANDLE(had, TYPE_HACMOBJ);

                memset(&aftd, 0, sizeof(aftd));
                aftd.cbStruct    = sizeof(aftd);
                aftd.dwFormatTag = pafdi->dwFormatTag;
                mmRet = acmFormatTagDetails(had, &aftd, ACM_FORMATTAGDETAILSF_FORMATTAG);
                VE_EXIT_ON_ERROR();

                VE_COND_PARAM(pafdi->dwFormatIndex >= aftd.cStandardFormats);
            }
            break;

        default:
            WARNMSG("unknown query type specified.");
            GOTO_EXIT(mmRet = MMSYSERR_NOTSUPPORTED);
    }

    //
    //  if we are passed a driver handle, then use it
    //
    if (NULL != had) {
        pafdi->szFormat[0] = TEXT('\0');

        pad = (PACMDRIVER)had;

        EnterHandle(had);
        if (TYPE_HACMDRIVERID == pad->uHandleType) {

            VE_HANDLE(had, TYPE_HACMDRIVERID);
            mmRet = (MMRESULT) IDriverMessageId((HACMDRIVERID)had,
                                             ACMDM_FORMAT_DETAILS,
                                             (LPARAM)pafdi,
                                             fdwDetails);
        } else {
            VE_HANDLE(had, TYPE_HACMDRIVER);
            mmRet = (MMRESULT) IDriverMessage(had,
                                           ACMDM_FORMAT_DETAILS,
                                           (LPARAM)pafdi,
                                           fdwDetails);
        }

        LeaveHandle(had);

        if (MMSYSERR_NOERROR == mmRet) {

            if (TEXT('\0') == pafdi->szFormat[0])
                IFormatDetailsToString((LPACMFORMATDETAILS) pafdi);

            //
            //  if caller is asking for details on a specific format, then
            //  always return index equal to zero (it means nothing)
            //
            if (ACM_FORMATDETAILSF_FORMAT == dwQuery)
                pafdi->dwFormatIndex = 0;
        }

        goto EXIT;
    }

    fNone = TRUE;
    hadid = NULL;
    mmRet   = MMSYSERR_NODRIVER;

    ENTER_LIST_SHARED();

    while (MMSYSERR_NOERROR == IDriverGetNext(&hadid, hadid, 0L)) {
        fNone = FALSE;

        pafdi->szFormat[0] = TEXT('\0');

        EnterHandle(hadid);
        mmRet = (MMRESULT)IDriverMessageId(hadid,
                                         ACMDM_FORMAT_DETAILS,
                                         (LPARAM)pafdi,
                                         fdwDetails);
        LeaveHandle(hadid);

        if (MMSYSERR_NOERROR == mmRet) {
            if (TEXT('\0') == pafdi->szFormat[0])
                IFormatDetailsToString((LPACMFORMATDETAILS) pafdi);

            //
            //  if caller is asking for details on a specific format, then
            //  always return index equal to zero (it means nothing)
            //
            if (ACM_FORMATDETAILSF_FORMAT == dwQuery)
                pafdi->dwFormatIndex = 0;
            break;
        }
    }

    LEAVE_LIST_SHARED();

    if( fNone && (dwQuery == ACM_FORMATDETAILSF_FORMAT) &&
                (pafdi->dwFormatTag == WAVE_FORMAT_PCM) ) {
        pafdi->dwFormatIndex = 0;
        pafdi->dwFormatTag   = WAVE_FORMAT_PCM;
        pafdi->fdwSupport    = 0;
        pafdi->cbwfx         = sizeof( PCMWAVEFORMAT );

        if ( FIELD_OFFSET(ACMFORMATDETAILS, szFormat) <
                    pafdi->cbStruct ) {
            pafdi->szFormat[0] = '\0';
            IFormatDetailsToString((LPACMFORMATDETAILS) pafdi);
        }
        GOTO_EXIT(mmRet = MMSYSERR_NOERROR);
    }

EXIT:

    FUNC_EXIT();
    return (mmRet);
}


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func BOOL ACMFORMATTAGENUMCB | acmFormatTagEnumCallback |
//          The <f acmFormatTagEnumCallback> function refers to the callback function used for
//          Audio Compression Manager (ACM) wave format tag enumeration. The
//          <f acmFormatTagEnumCallback> function is a placeholder for the application-supplied
//          function name.
//
//  @parm   HACMDRIVERID | hadid | Specifies an ACM driver identifier.
//
//  @parm  LPACMFORMATTAGDETAILS | paftd | Specifies a pointer to an
//          <t ACMFORMATTAGDETAILS> structure that contains the enumerated
//          format tag details.
//
//  @parm   DWORD | dwInstance | Specifies the application-defined value
//          specified in the <f acmFormatTagEnum> function.
//
//  @parm   DWORD | fdwSupport | Specifies driver-support flags specific to
//          the format tag. These flags are identical to the
//          <e ACMDRIVERDETAILS.fdwSupport> flags of the <t ACMDRIVERDETAILS>
//          structure. This argument can be a combination of the following
//          values and indicates which operations the driver supports with the
//          format tag:
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CODEC | Specifies that this driver
//          supports conversion between two different format tags where one of
//          the tags is the specified format tag. For example, if a driver
//          supports compression from WAVE_FORMAT_PCM to WAVE_FORMAT_ADPCM,
//          then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CONVERTER | Specifies that this
//          driver supports conversion between two different formats of the
//          specified format tag. For example, if a driver supports resampling
//          of WAVE_FORMAT_PCM, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_FILTER | Specifies that this driver
//          supports a filter (modification of the data without changing any
//          of the format attributes). For example, if a driver supports volume
//          or echo operations on the specified format tag, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_ASYNC | Specifies that this driver
//          supports asynchronous conversions with the specified format tag.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_HARDWARE | Specifies that this driver
//          supports hardware input and/or output of the specified format tag
//          through a waveform device. An application should use <f acmMetrics>
//          with the ACM_METRIC_HARDWARE_WAVE_INPUT and
//          ACM_METRIC_HARDWARE_WAVE_OUTPUT metric indexes to get the waveform
//          device identifiers associated with the supporting ACM driver.
//
//  @rdesc The callback function must return TRUE to continue enumeration;
//          to stop enumeration, it must return FALSE.
//
//  @comm The <f acmFormatTagEnum> function will return MMSYSERR_NOERROR
//          (zero) if no format tags are to be enumerated. Moreover, the callback
//          function will not be called.
//
//  @xref   <f acmFormatTagEnum> <f acmFormatTagDetails> <f acmDriverOpen>
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmFormatTagEnum | The <f acmFormatTagEnum> function
//          enumerates wave format tags available from an Audio Compression
//          Manager (ACM) driver. The <f acmFormatTagEnum> function continues
//          enumerating until there are no more suitable format tags or the
//          callback function returns FALSE.
//
//  @parm   HACMDRIVER | had | Optionally specifies an ACM driver to query
//          for wave format tag details. If this argument is NULL, then the
//          ACM uses the details from the first suitable ACM driver.
//
//  @parm   LPACMFORMATTAGDETAILS | paftd | Specifies a pointer to the
//          <t ACMFORMATTAGDETAILS> structure that is to receive the format
//          tag details passed to the <p fnCallback> function. This structure
//          must have the <e ACMFORMATTAGDETAILS.cbStruct> member of the
//          <t ACMFORMATTAGDETAILS> structure initialized.
//
//  @parm ACMFORMATTAGENUMCB | fnCallback | Specifies the address of the
//          application-defined callback function.
//
//  @parm   DWORD | dwInstance | Specifies a 32-bit, application-defined value
//          that is passed to the callback function along with ACM format tag
//          details.
//
//  @parm   DWORD | fdwEnum | This argument is not used and must be set to
//          zero.
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
//  @comm   The <f acmFormatTagEnum> function will return MMSYSERR_NOERROR
//          (zero) if no suitable ACM drivers are installed. Moreover, the
//          callback function will not be called.
//
//  @xref   <f acmFormatTagEnumCallback> <f acmFormatTagDetails>
//
//------------------------------------------------------------------------------
MMRESULT
acmFormatTagEnum(
    HACMDRIVER              had,
    LPACMFORMATTAGDETAILS   paftd,
    ACMFORMATTAGENUMCB      fnCallback,
    DWORD                   dwInstance,
    DWORD                   fdwEnum
    )
{
    PACMDRIVER          pad;
    PACMDRIVERID        padid;
    UINT                uIndex;
    UINT                uFormatTag;
    BOOL                f;
    HACMDRIVERID        hadid=0;
    PACMDRIVERID        padidCur;
    HACMDRIVERID        hadidCur;
    BOOL                fSent;
    BOOL                fNone;
    DWORD               cbaftd;
    DWORD               fdwSupport;
    MMRESULT            mmRet;
    PACMFORMATTAGDETAILS_INT paftdi = (PACMFORMATTAGDETAILS_INT) paftd;

    FUNC("acmFormatTagEnum");

    VE_CALLBACK(fnCallback);
    VE_FLAGS(fdwEnum, ACM_FORMATTAGENUMF_VALID);
    VE_WPOINTER(paftdi, sizeof(DWORD));
    VE_COND_PARAM(sizeof(ACMFORMATTAGDETAILS) > paftdi->cbStruct);
    VE_WPOINTER(paftdi, paftdi->cbStruct);
    VE_COND_PARAM(0L != paftdi->fdwSupport);

    cbaftd = min(paftdi->cbStruct, sizeof(ACMFORMATTAGDETAILS));

    if (NULL != had) {
        VE_HANDLE(had, TYPE_HACMDRIVER);

        //
        //  enum format tags for this driver only.
        //
        pad   = (PACMDRIVER)had;
        padid = (PACMDRIVERID)pad->hadid;

        //
        //  do NOT include the 'disabled' bit!
        //
        fdwSupport = padid->fdwSupport;

        //
        //  while there are Formats to enumerate and we have not been
        //  told to stop (client returns FALSE to stop enum)
        //
        mmRet = MMSYSERR_NOERROR;
        for (uIndex = 0; uIndex < padid->cFormatTags; uIndex++)
        {
            paftdi->cbStruct = cbaftd;
            paftdi->dwFormatTagIndex = uIndex;
            mmRet = IFormatTagDetails((HACMDRIVERID)padid, paftdi, ACM_FORMATTAGDETAILSF_INDEX);
            if (MMSYSERR_NOERROR != mmRet) {
                break;
            }

            f = (BOOL) wapi_DoFunctionCallback( fnCallback,
                                                4,
                                                (DWORD) hadid,
                                                (DWORD) paftd,
                                                dwInstance,
                                                fdwSupport,
                                                0);
// BUGBUG do function callback
//            f = (* fnCallback)(pad->hadid, paftdi, dwInstance, fdwSupport);
            if (FALSE == f)
                break;
        }

        goto EXIT;
    }

    hadidCur = NULL;
    fNone = TRUE;

    ENTER_LIST_SHARED();

    while (!IDriverGetNext(&hadidCur, hadidCur, 0L))
    {
        padidCur = (PACMDRIVERID)hadidCur;

        for (uIndex = 0; uIndex < padidCur->cFormatTags; uIndex++)
        {
            fNone = FALSE;
            uFormatTag = (UINT)(padidCur->paFormatTagCache[uIndex].dwFormatTag);
            fSent = FALSE;
            hadid = NULL;
            while (!fSent && !IDriverGetNext(&hadid, hadid, 0L)) {
                UINT    u;

                //
                //  same driver ?
                //
                if (hadid == hadidCur)
                    break;


                //
                //  for every previous driver
                //
                padid = (PACMDRIVERID)hadid;

                for (u = 0; u < padid->cFormatTags; u++) {
                    //
                    //  for every FormatTag in the driver
                    //
                    if (uFormatTag == padid->paFormatTagCache[u].dwFormatTag) {
                        //
                        //  we have a match, but this was already given.
                        //
                        fSent = TRUE;
                        break;
                    }
                }
            }

            if (!fSent)
            {
                //
                //  we have a format that has not been sent yet.
                //
                paftdi->dwFormatTagIndex = uIndex;
                paftdi->cbStruct = cbaftd;
                mmRet = IFormatTagDetails((HACMDRIVERID)padidCur,
                                        paftdi, ACM_FORMATTAGDETAILSF_INDEX);
                if (MMSYSERR_NOERROR != mmRet)
                {
                    LEAVE_LIST_SHARED();
                    goto EXIT;
                }

                //
                //  do NOT include the 'disabled' bit!
                //
                fdwSupport = padidCur->fdwSupport;

                f = (BOOL) wapi_DoFunctionCallback( fnCallback,
                                                    4,
                                                    (DWORD) hadidCur,
                                                    (DWORD) paftd,
                                                    dwInstance,
                                                    fdwSupport,
                                                    0);
// BUGBUG do function callback
//                f = (* fnCallback)(hadidCur, paftdi, dwInstance, fdwSupport);
                if (FALSE == f) {
                    LEAVE_LIST_SHARED();
                    GOTO_EXIT(mmRet = MMSYSERR_NOERROR);
                }
            }
        }
    }

    LEAVE_LIST_SHARED();

    if( fNone ) {
        /* No codecs enabled */
        /* Enum PCM as default */

        fdwSupport = 0L;

        paftdi->dwFormatTagIndex = 0;
        paftdi->dwFormatTag      = WAVE_FORMAT_PCM;
        paftdi->cbFormatSize     = sizeof(PCMWAVEFORMAT);
        paftdi->fdwSupport       = fdwSupport;
        paftdi->cStandardFormats = CODEC_MAX_STANDARD_FORMATS_PCM;

        //
        //  the ACM is responsible for the PCM format tag name
        //
        LoadString(pag->hinst,
                   IDS_FORMAT_TAG_PCM,
                   paftdi->szFormatTag,
                   SIZEOF(paftdi->szFormatTag));

        f = (BOOL) wapi_DoFunctionCallback( fnCallback,
                                            4,
                                            (DWORD) NULL,
                                            (DWORD) paftd,
                                            dwInstance,
                                            fdwSupport,
                                            0);
// BUGBUG do function callback
//        (* fnCallback)(NULL, paftd, dwInstance, fdwSupport);
    }

    mmRet = MMSYSERR_NOERROR;
EXIT:

    FUNC_EXIT();
    return(mmRet);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
ISupported(
    LPWAVEFORMATEX          pwfx,
    DWORD                   fdwEnum
    )
{
    DWORD               fdwOpen;
    MMRESULT            mmRet;
    BOOL                fRet;

    FUNC("ISupported");

    //
    //  if the 'hardware' bit is not set, then simply test for support
    //  through the mapper.  otherwise, test for direct support
    //
    if (0 == (ACM_FORMATENUMF_HARDWARE & fdwEnum)) {
        fdwOpen = WAVE_ALLOWSYNC | WAVE_FORMAT_QUERY;
    } else {
        fdwOpen = WAVE_ALLOWSYNC | WAVE_FORMAT_DIRECT_QUERY;
    }

    //
    //
    //
    if (0 != (ACM_FORMATENUMF_OUTPUT & fdwEnum))
    {
        mmRet = waveOutOpen(NULL, (UINT)WAVE_MAPPER,
                          pwfx,
                          0L, 0L, fdwOpen);
        if (MMSYSERR_NOERROR == mmRet) {
            fRet = TRUE;
            goto EXIT;
        }
    }

    if (0 != (ACM_FORMATENUMF_INPUT & fdwEnum))
    {
        mmRet = waveInOpen(NULL, (UINT)WAVE_MAPPER,
                         pwfx,
                         0L, 0L, fdwOpen);
        if (MMSYSERR_NOERROR == mmRet) {
            fRet = TRUE;
            goto EXIT;
        }
    }

    fRet = FALSE;
EXIT:
    FUNC_EXIT();
    return(fRet);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
ISuggestEnum(
    HACMDRIVER              had,
    PACMFORMATTAGDETAILS_INT   paftd,
    PACMFORMATDETAILS_INT   pafd,
    ACMFORMATENUMCB         fnCallback,
    DWORD                   dwInstance,
    LPWAVEFORMATEX          pwfxSrc,
    DWORD                   fdwEnum
    )
{
    MMRESULT            mmRet;
    BOOL                f;
    DWORD               fdwSuggest;
    DWORD               fdwSupport;
    HACMDRIVERID        hadid;

    FUNC("ISuggestEnum");

    hadid = ((PACMDRIVER)had)->hadid;

    fdwSupport = ((PACMDRIVERID)hadid)->fdwSupport;

    pafd->dwFormatTag       = paftd->dwFormatTag;
    pafd->fdwSupport        = 0L;
    pafd->pwfx->wFormatTag  = (WORD)(paftd->dwFormatTag);

    fdwSuggest = ACM_FORMATSUGGESTF_WFORMATTAG;
    if( fdwEnum & ACM_FORMATENUMF_NCHANNELS ) {
        fdwSuggest |= ACM_FORMATSUGGESTF_NCHANNELS;
    }
    if( fdwEnum & ACM_FORMATENUMF_NSAMPLESPERSEC ) {
        fdwSuggest |= ACM_FORMATSUGGESTF_NSAMPLESPERSEC;
    }
    if( fdwEnum & ACM_FORMATENUMF_WBITSPERSAMPLE ) {
        fdwSuggest |= ACM_FORMATSUGGESTF_WBITSPERSAMPLE;
    }

    mmRet = acmFormatSuggest(had, pwfxSrc, pafd->pwfx, pafd->cbwfx, fdwSuggest);
    if( mmRet != MMSYSERR_NOERROR )
        GOTO_EXIT(mmRet = MMSYSERR_NOERROR);

    mmRet = acmFormatDetails(had, (LPACMFORMATDETAILS) pafd, ACM_FORMATDETAILSF_FORMAT);
    if (MMSYSERR_NOERROR != mmRet)
        GOTO_EXIT(mmRet = MMSYSERR_NOERROR);

    if (0 != ((ACM_FORMATENUMF_INPUT | ACM_FORMATENUMF_OUTPUT) & fdwEnum))
        if (!ISupported(pafd->pwfx, fdwEnum))
            GOTO_EXIT(mmRet = MMSYSERR_NOERROR);

// BUGBUG do function callback
    f = (BOOL) wapi_DoFunctionCallback( fnCallback,
                                        4,
                                        (DWORD) hadid,
                                        (DWORD) pafd,
                                        dwInstance,
                                        fdwSupport,
                                        0);
//    f = (* fnCallback)(hadid, pafd, dwInstance, fdwSupport);
    if (!f)
        GOTO_EXIT(mmRet = MMSYSERR_ERROR);

    mmRet = MMSYSERR_NOERROR;
EXIT:
    FUNC_EXIT();
    return(mmRet);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IFormatEnum(
    HACMDRIVER              had,
    PACMFORMATTAGDETAILS_INT   paftd,
    PACMFORMATDETAILS_INT   pafd,
    ACMFORMATENUMCB         fnCallback,
    DWORD                   dwInstance,
    LPWAVEFORMATEX          pwfxSrc,
    DWORD                   fdwEnum
    )
{
    MMRESULT            mmRet;
    BOOL                f;
    DWORD               cbafd;
    LPWAVEFORMATEX      pwfx;
    DWORD               cbwfx;
    UINT                u;
    UINT                nChannels;
    DWORD               nSamplesPerSec;
    UINT                uBitsPerSample;
    LPWAVEFORMATEX      pwfxSuggest;
    DWORD               cbwfxSuggest;
    DWORD               fdwSupport;
    DWORD               fdwSuggest;
    HACMDRIVERID        hadid;

    FUNC("IFormatEnum");

    pwfxSuggest = NULL;
    if (0 != (ACM_FORMATENUMF_CONVERT & fdwEnum))
    {
        mmRet = IMetricsMaxSizeFormat(had, &cbwfxSuggest );
        if (MMSYSERR_NOERROR == mmRet)
        {
            pwfxSuggest = (LPWAVEFORMATEX) LocalAlloc(LMEM_FIXED, cbwfxSuggest);

            if (NULL != pwfxSuggest)
            {
                fdwSuggest = ACM_FORMATSUGGESTF_WFORMATTAG;
                pwfxSuggest->wFormatTag = LOWORD(paftd->dwFormatTag);

                if (0 != (ACM_FORMATENUMF_NCHANNELS & fdwEnum))
                {
                    fdwSuggest |= ACM_FORMATSUGGESTF_NCHANNELS;
                    pwfxSuggest->nChannels = pwfxSrc->nChannels;
                }
                if (0 != (ACM_FORMATENUMF_NSAMPLESPERSEC & fdwEnum))
                {
                    fdwSuggest |= ACM_FORMATSUGGESTF_NSAMPLESPERSEC;
                    pwfxSuggest->nSamplesPerSec = pwfxSrc->nSamplesPerSec;
                }
                if (0 != (ACM_FORMATENUMF_WBITSPERSAMPLE & fdwEnum))
                {
                    fdwSuggest |= ACM_FORMATSUGGESTF_WBITSPERSAMPLE;
                    pwfxSuggest->wBitsPerSample = pwfxSrc->wBitsPerSample;
                }

                //DPF(5, "calling acmFormatSuggest pwfxSuggest=%.08lXh--fdwSuggest=%.08lXh", pwfxSuggest, fdwSuggest);

                mmRet = acmFormatSuggest(had, pwfxSrc, pwfxSuggest, cbwfxSuggest, fdwSuggest);
                if (MMSYSERR_NOERROR != mmRet)
                {
                    TESTMSG("FREEING pwfxSuggest : no suggested format!");
                    LocalFree(pwfxSuggest);
                    pwfxSuggest = NULL;

                    //
                    //  if no 'suggestions', there better not be any
                    //  possible conversions that we would find below...
                    //
                    GOTO_EXIT(mmRet = MMSYSERR_NOERROR);
                }

                PRINTMSG(ZONE_MISC, (TEXT("******* suggestion--%u to %u"), pwfxSrc->wFormatTag, pwfxSuggest->wFormatTag));
                cbwfxSuggest = SIZEOF_WAVEFORMATEX(pwfxSuggest);
            }
        }
    }

    hadid = ((PACMDRIVER)had)->hadid;

    fdwSupport = ((PACMDRIVERID)hadid)->fdwSupport;

    //
    //  be a bit paranoid and save some stuff so we can always reinit
    //  the structure between calling the driver (i just don't trust
    //  driver writers)
    //
    cbafd = pafd->cbStruct;
    pwfx  = pafd->pwfx;
    cbwfx = pafd->cbwfx;
    nChannels = pwfxSrc->nChannels;
    nSamplesPerSec = pwfxSrc->nSamplesPerSec;
    uBitsPerSample = pwfxSrc->wBitsPerSample;

    //
    //
    //
    for (u = 0; u < paftd->cStandardFormats; u++)
    {
        pafd->cbStruct      = cbafd;
        pafd->dwFormatIndex = u;
        pafd->dwFormatTag   = paftd->dwFormatTag;
        pafd->fdwSupport    = 0;
        pafd->pwfx          = pwfx;
        pafd->cbwfx         = cbwfx;
        pafd->szFormat[0]   = '\0';

        mmRet = acmFormatDetails(had, (LPACMFORMATDETAILS) pafd, ACM_FORMATDETAILSF_INDEX);
        if (MMSYSERR_NOERROR != mmRet)
            continue;

        if ((fdwEnum & ACM_FORMATENUMF_NCHANNELS) && (pwfx->nChannels != nChannels))
            continue;

        if ((fdwEnum & ACM_FORMATENUMF_NSAMPLESPERSEC) && (pwfx->nSamplesPerSec != nSamplesPerSec))
            continue;

        if ((fdwEnum & ACM_FORMATENUMF_WBITSPERSAMPLE) && (pwfx->wBitsPerSample != uBitsPerSample))
            continue;

        if (0 != (fdwEnum & ACM_FORMATENUMF_CONVERT)) {
            mmRet = acmStreamOpen(NULL,
                                had,
                                pwfxSrc,
                                pwfx,
                                NULL,
                                0L,
                                0L,
                                ACM_STREAMOPENF_NONREALTIME |
                                ACM_STREAMOPENF_QUERY);

            if (MMSYSERR_NOERROR != mmRet)
                continue;

            if ((NULL != pwfxSuggest) &&
                (SIZEOF_WAVEFORMATEX(pwfx) == cbwfxSuggest))
            {
                if (0 == memcmp(pwfx, pwfxSuggest, (UINT)cbwfxSuggest))
                {
                    //DPF(5, "FREEING pwfxSuggest=%.08lXh--DUPLICATE!", pwfxSuggest);
                    LocalFree(pwfxSuggest);
                    pwfxSuggest = NULL;
                }
            }
        }

        if (0 != ((ACM_FORMATENUMF_INPUT | ACM_FORMATENUMF_OUTPUT) & fdwEnum))
        {
            if (!ISupported(pwfx, fdwEnum))
            {
                continue;
            }
        }

        // Copy data back to application buffers
        f = (BOOL) wapi_DoFunctionCallback( fnCallback,
                                            4,
                                            (DWORD) hadid,
                                            (DWORD) pafd,
                                            dwInstance,
                                            fdwSupport,
                                            0);

//        f = (* fnCallback)(hadid, pafd, dwInstance, fdwSupport);
        if (!f)
        {
            if (NULL != pwfxSuggest)
            {
                //DPF(5, "FREEING pwfxSuggest=%.08lXh--CALLBACK CANCELED!", pwfxSuggest);
                LocalFree(pwfxSuggest);
            }
            GOTO_EXIT(mmRet = MMSYSERR_ERROR);
        }
    }

    //
    //  if we have not passed back the 'suggested' format for the convert
    //  case, then do it now
    //
    //  this is a horribly gross fix, and i know it...
    //
    if (NULL != pwfxSuggest)
    {
        //DPF(5, "pwfxSuggest=%.08lXh--attempting callback (%u)", pwfxSuggest, pwfxSuggest->wFormatTag);

        pafd->cbStruct      = cbafd;
        pafd->dwFormatIndex = 0;
        pafd->dwFormatTag   = pwfxSuggest->wFormatTag;
        pafd->fdwSupport    = 0;
        pafd->pwfx          = pwfxSuggest;
        pafd->cbwfx         = cbwfxSuggest;
        pafd->szFormat[0]   = '\0';

        f   = TRUE;
        mmRet = acmFormatDetails(had, (LPACMFORMATDETAILS) pafd, ACM_FORMATDETAILSF_FORMAT);
        if (MMSYSERR_NOERROR == mmRet)
        {
            if (0 != ((ACM_FORMATENUMF_INPUT | ACM_FORMATENUMF_OUTPUT) & fdwEnum))
            {
                if (!ISupported(pwfxSuggest, fdwEnum))
                {
                    //DPF(5, "FREEING pwfxSuggest=%.08lXh--attempting callback NOT SUPPPORTED", pwfxSuggest);

                    LocalFree(pwfxSuggest);
                    pafd->cbwfx = cbwfx;
                    GOTO_EXIT(mmRet = MMSYSERR_NOERROR);
                }
            }

            f = (BOOL) wapi_DoFunctionCallback( fnCallback,
                                                4,
                                                (DWORD) hadid,
                                                (DWORD) pafd,
                                                dwInstance,
                                                fdwSupport,
                                                0);
// BUGBUG do function callback
//            f = (* fnCallback)(hadid, pafd, dwInstance, fdwSupport);
        }

        //
        //  reset these things or bad things will happen
        //
        pafd->pwfx  = pwfx;
        pafd->cbwfx = cbwfx;

        LocalFree(pwfxSuggest);

        VE_COND_ERR(!f);
    }

    mmRet = MMSYSERR_NOERROR;
EXIT:
    FUNC_EXIT();
    return(mmRet);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IHardwareEnum(
    PACMFORMATDETAILS_INT   pafd,
    ACMFORMATENUMCB         fnCallback,
    DWORD                   dwInstance,
    LPWAVEFORMATEX          pwfxSrc,
    DWORD                   fdwEnum
    )
{
    BOOL                f;
    DWORD               cbafd;
    LPWAVEFORMATEX      pwfx;
    DWORD               cbwfx;
    UINT                u1, u2;
    UINT                nChannels;
    DWORD               nSamplesPerSec;
    UINT                uBitsPerSample;
    MMRESULT            mmRet;

    FUNC("IHardwareEnum");
    //
    //  be a bit paranoid and save some stuff so we can always reinit
    //  the structure between calling the driver (i just don't trust
    //  driver writers)
    //
    cbafd = pafd->cbStruct;
    pwfx  = pafd->pwfx;
    cbwfx = pafd->cbwfx;
    nChannels = pwfxSrc->nChannels;
    nSamplesPerSec = pwfxSrc->nSamplesPerSec;
    uBitsPerSample = pwfxSrc->wBitsPerSample;

    //
    //
    //
    for (u1 = 0; u1 < CODEC_MAX_STANDARD_FORMATS_PCM; u1++)
    {
        pafd->cbStruct      = cbafd;
        pafd->dwFormatIndex = 0;
        pafd->dwFormatTag   = WAVE_FORMAT_PCM;
        pafd->fdwSupport    = 0L;
        pafd->pwfx          = pwfx;
        pafd->cbwfx         = cbwfx;

        //
        //  now fill in the format structure
        //
        pwfx->wFormatTag      = WAVE_FORMAT_PCM;

        u2 = u1 / (CODEC_MAX_BITSPERSAMPLE_PCM * CODEC_MAX_CHANNELS);
        pwfx->nSamplesPerSec  = auFormatIndexToSampleRate[u2];

        u2 = u1 % CODEC_MAX_CHANNELS;
        pwfx->nChannels       = u2 + 1;

        u2 = (u1 / CODEC_MAX_CHANNELS) % CODEC_MAX_CHANNELS;
        pwfx->wBitsPerSample  = auFormatIndexToBitsPerSample[u2];

        pwfx->nBlockAlign     = PCM_BLOCKALIGNMENT((LPPCMWAVEFORMAT)pwfx);
        pwfx->nAvgBytesPerSec = pwfx->nSamplesPerSec * pwfx->nBlockAlign;

        //
        //  note that the cbSize field is NOT valid for PCM formats
        //
        //  pwfx->cbSize      = 0;


        pafd->cbStruct    = min(pafd->cbStruct, sizeof(*pafd));
        IFormatDetailsToString((LPACMFORMATDETAILS) pafd);

        if( (fdwEnum & ACM_FORMATENUMF_NCHANNELS)
            && (pwfx->nChannels != nChannels) ) {
            continue;
        }
        if( (fdwEnum & ACM_FORMATENUMF_NSAMPLESPERSEC)
            && (pwfx->nSamplesPerSec != nSamplesPerSec) ) {
            continue;
        }
        if( (fdwEnum & ACM_FORMATENUMF_WBITSPERSAMPLE)
            && (pwfx->wBitsPerSample != uBitsPerSample) ) {
            continue;
        }

        if (0 != ((ACM_FORMATENUMF_INPUT | ACM_FORMATENUMF_OUTPUT) & fdwEnum))
        {
            if (!ISupported(pwfx, fdwEnum))
            {
                continue;
            }
        }

// BUGBUG do function callback
        f = (BOOL) wapi_DoFunctionCallback( fnCallback,
                                            4,
                                            (DWORD) NULL,
                                            (DWORD) pafd,
                                            dwInstance,
                                            0,
                                            0);
//        f = (* fnCallback)(NULL, pafd, dwInstance, 0L);
        VE_COND_ERR(!f);
    }

    mmRet = MMSYSERR_NOERROR;
EXIT:
    FUNC_EXIT();
    return(mmRet);
}


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   BOOL ACMFORMATENUMCB | acmFormatEnumCallback |
//          The <f acmFormatEnumCallback> function refers to the callback
//          function used for Audio Compression Manager (ACM) wave format
//          detail enumeration. The <f acmFormatEnumCallback> is a placeholder
//          for the application-supplied function name.
//
//  @parm   HACMDRIVERID | hadid | Specifies an ACM driver identifier.
//
//  @parm   LPACMFORMATDETAILS | pafd | Specifies a pointer to an
//          <t ACMFORMATDETAILS> structure that contains the enumerated
//          format details for a format tag.
//
//  @parm   DWORD | dwInstance | Specifies the application-defined value
//          specified in the <f acmFormatEnum> function.
//
//  @parm   DWORD | fdwSupport | Specifies driver-support flags specific to
//          the driver identifier <p hadid> for the specified format. These flags
//          are identical to the <e ACMDRIVERDETAILS.fdwSupport> flags of the
//          <t ACMDRIVERDETAILS> structure, but are specific to the format that
//          is being enumerated. This argument can be a combination of the
//          following values and indicates which operations the driver supports
//          for the format tag.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CODEC | Specifies that this driver
//          supports conversion between two different format tags for the
//          specified format. For example, if a driver supports compression
//          from WAVE_FORMAT_PCM to WAVE_FORMAT_ADPCM with the specifed
//          format, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CONVERTER | Specifies that this
//          driver supports conversion between two different formats of the
//          same format tag while using the specified format. For example, if a
//          driver supports resampling of WAVE_FORMAT_PCM to the specified
//          format, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_FILTER | Specifies that this driver
//          supports a filter (modification of the data without changing any
//          of the format attributes) with the specified format. For example,
//          if a driver supports volume or echo operations on WAVE_FORMAT_PCM,
//          then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_ASYNC | Specifies that this driver
//          supports asynchronous conversions with the specified format.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_HARDWARE | Specifies that this driver
//          supports hardware input and/or output of the specified format tag
//          through a waveform device. An application should use <f acmMetrics>
//          with the ACM_METRIC_HARDWARE_WAVE_INPUT and
//          ACM_METRIC_HARDWARE_WAVE_OUTPUT metric indexes to get the waveform
//          device identifiers associated with the supporting ACM driver.
//
//  @rdesc  The callback function must return TRUE to continue enumeration;
//          to stop enumeration, it must return FALSE.
//
//  @comm   The <f acmFormatEnum> function will return MMSYSERR_NOERROR
//          (zero) if no formats are to be enumerated. Moreover, the callback
//          function will not be called.
//
//  @xref   <f acmFormatEnum> <f acmFormatTagDetails> <f acmDriverOpen>
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmFormatEnum | The <f acmFormatEnum> function
//          enumerates wave formats available for a given format tag from
//          an Audio Compression Manager (ACM) driver. The <f acmFormatEnum>
//          function continues enumerating until there are no more suitable
//          formats for the format tag or the callback function returns FALSE.
//
//  @parm   HACMDRIVER | had | Optionally specifies an ACM driver to query
//          for wave format details. If this argument is NULL, then the
//          ACM uses the details from the first suitable ACM driver.
//
//  @parm   LPACMFORMATDETAILS | pafd | Specifies a pointer to the
//          <t ACMFORMATDETAILS> structure that is to receive the format details
//          passed to the <p fnCallback> function. This structure must have the
//          <e ACMFORMATDETAILS.cbStruct>, <e ACMFORMATDETAILS.pwfx>, and
//          <e ACMFORMATDETAILS.cbwfx> members of the <t ACMFORMATDETAILS>
//          structure initialized. The <e ACMFORMATDETAILS.dwFormatTag> member
//          must also be initialized to either WAVE_FORMAT_UNKNOWN or a
//          valid format tag.
//
//  @parm   ACMFORMATENUMCB | fnCallback | Specifies the address of the
//          application-defined callback function.
//
//  @parm   DWORD | dwInstance | Specifies a 32-bit, application-defined value
//          that is passed to the callback function along with ACM format details.
//
//  @parm   DWORD | fdwEnum | Specifies flags for enumerating the formats for
//          a given format tag.
//
//          @flag ACM_FORMATENUMF_WFORMATTAG | Specifies that the
//          <e WAVEFORMATEX.wFormatTag> member of the <t WAVEFORMATEX> structure
//          referred to by the <e ACMFORMATDETAILS.pwfx> member of the
//          <t ACMFORMATDETAILS> structure is valid. The enumerator will only
//          enumerate a format that conforms to this attribute. Note that the
//          <e ACMFORMATDETAILS.dwFormatTag> member of the <t ACMFORMATDETAILS>
//          structure must be equal to the <e WAVEFORMATEX.wFormatTag> member.
//
//          @flag ACM_FORMATENUMF_NCHANNELS | Specifies that the
//          <e WAVEFORMATEX.nChannels> member of the <t WAVEFORMATEX>
//          structure referred to by the <e ACMFORMATDETAILS.pwfx> member of the
//          <t ACMFORMATDETAILS> structure is valid. The enumerator will only
//          enumerate a format that conforms to this attribute.
//
//          @flag ACM_FORMATENUMF_NSAMPLESPERSEC | Specifies that the
//          <e WAVEFORMATEX.nSamplesPerSec> member of the <t WAVEFORMATEX>
//          structure referred to by the <e ACMFORMATDETAILS.pwfx> member of the
//          <t ACMFORMATDETAILS> structure is valid. The enumerator will only
//          enumerate a format that conforms to this attribute.
//
//          @flag ACM_FORMATENUMF_WBITSPERSAMPLE | Specifies that the
//          <e WAVEFORMATEX.wBitsPerSample> member of the <t WAVEFORMATEX>
//          structure referred to by the <e ACMFORMATDETAILS.pwfx> member of the
//          <t ACMFORMATDETAILS> structure is valid. The enumerator will only
//          enumerate a format that conforms to this attribute.
//
//          @flag ACM_FORMATENUMF_CONVERT | Specifies that the <t WAVEFORMATEX>
//          structure referenced by the <e ACMFORMATDETAILS.pwfx> member of the
//          <t ACMFORMATDETAILS> structure is valid. The enumerator will only
//          enumerate destination formats that can be converted from the given
//          <e ACMFORMATDETAILS.pwfx> format.
//
//          @flag ACM_FORMATENUMF_SUGGEST | Specifies that the <t WAVEFORMATEX>
//          structure referred to by the <e ACMFORMATDETAILS.pwfx> member of the
//          <t ACMFORMATDETAILS> structure is valid. The enumerator will
//          enumerate all suggested destination formats for the given
//          <e ACMFORMATDETAILS.pwfx> format. This can be used instead of
//          <f acmFormatSuggest> to allow an application to choose the best
//          suggested format for conversion. Note that the
//          <e ACMFORMATDETAILS.dwFormatIndex> member will always be set to
//          zero on return.
//
//          @flag ACM_FORMATENUMF_HARDWARE | Specifies that the enumerator should
//          only enumerate formats that are supported as native input or output
//          formats on one or more of the installed wave devices. This provides
//          a way for an application to choose only formats native to an
//          installed wave device. This flag must be used with one or both
//          of the ACM_FORMATENUMF_INPUT and ACM_FORMATENUMF_OUTPUT flags.
//          Note that specifying both ACM_FORMATENUMF_INPUT and
//          ACM_FORMATENUMF_OUTPUT will enumerate only formats that can be
//          opened for input or output.
//          This is true regardless of of whether the ACM_FORMATENUMF_HARDWARE
//          flag is specified.
//
//          @flag ACM_FORMATENUMF_INPUT | Specifies that the enumerator should
//          only enumerate formats that are supported for input (recording).
//
//          @flag ACM_FORMATENUMF_OUTPUT | Specifies that the enumerator should
//          only enumerate formats that are supported for output (playback).
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
//          @flag ACMERR_NOTPOSSIBLE | The details for the format cannot be
//          returned.
//
//  @comm   The <f acmFormatEnum> function will return MMSYSERR_NOERROR
//          (zero) if no suitable ACM drivers are installed. Moreover, the
//          callback function will not be called.
//
//  @xref   <f acmFormatEnumCallback> <f acmFormatDetails> <f acmFormatSuggest>
//          <f acmFormatTagDetails> <f acmFilterEnum>
//
//------------------------------------------------------------------------------
MMRESULT
acmFormatEnum(
    HACMDRIVER              had,
    LPACMFORMATDETAILS      pafd,
    ACMFORMATENUMCB         fnCallback,
    DWORD                   dwInstance,
    DWORD                   fdwEnum
    )
{
    MMRESULT                mmRet;
    PACMDRIVERID            padid;
    HACMDRIVERID            hadid;
    UINT                    u;
    UINT                    uIndex;
    UINT                    uFormatSize;
    UINT                    uFormatTag;
    LPWAVEFORMATEX          pwfxSrc = NULL;
    BOOL                    fNoDrivers;
    DWORD                   cbwfxRqd;
    BOOL                    fFormatTag;
    BOOL                    fConvert;
    BOOL                    fSuggest;
    ACMFORMATTAGDETAILS_INT aftd;
    PACMFORMATDETAILS_INT   pafdi = (PACMFORMATDETAILS_INT) pafd;


    FUNC("acmFormatEnum");

    //
    // Does the app think the struct is at least as big as I think it is?
    //
    VE_COND_PARAM(sizeof(ACMFORMATDETAILS) > pafdi->cbStruct);

    if (NULL != had)
        VE_HANDLE(had, TYPE_HACMDRIVER);
    VE_CALLBACK(fnCallback);
    VE_FLAGS(fdwEnum, ACM_FORMATENUMF_VALID);

    if (0 != (ACM_FORMATENUMF_HARDWARE & fdwEnum)) {
        VE_COND_FLAG(!((ACM_FORMATENUMF_INPUT|ACM_FORMATENUMF_OUTPUT) & fdwEnum));
    }
    //
    // What's the biggest format size I need to alloc for?
    //
    mmRet = IMetricsMaxSizeFormat(had, &cbwfxRqd );
    VE_EXIT_ON_ERROR();

    VE_COND_PARAM(pafdi->cbwfx < cbwfxRqd);
    VE_WPOINTER(pafdi->pwfx, pafdi->cbwfx);
    VE_COND_PARAM(0L != pafdi->fdwSupport);

    //
    //  Get the restrictions on the enum.
    //
    uFormatTag = WAVE_FORMAT_UNKNOWN;
    fFormatTag = (0 != (fdwEnum & ACM_FORMATENUMF_WFORMATTAG));

    if (fFormatTag) {
        uFormatTag = (UINT)pafdi->pwfx->wFormatTag;

        VE_COND_PARAM(WAVE_FORMAT_UNKNOWN == uFormatTag);
        VE_COND_PARAM(pafdi->dwFormatTag != uFormatTag);
    }

    fConvert = (0 != (fdwEnum & ACM_FORMATENUMF_CONVERT));
    fSuggest = (0 != (fdwEnum & ACM_FORMATENUMF_SUGGEST));

    //
    // find the size of the source format for restrictions
    //
    if (fConvert || fSuggest) {
        uFormatSize = SIZEOF_WAVEFORMATEX(pafdi->pwfx);
        VE_COND_PARAM(WAVE_FORMAT_UNKNOWN == pafdi->pwfx->wFormatTag);
    } else {
        // if we are not using the convert or suggest restrictions
        // then we do not need the full format
        uFormatSize = sizeof(PCMWAVEFORMAT);
    }

    // Alloc a copy of the source format for restrictions
    pwfxSrc = (LPWAVEFORMATEX)LocalAlloc(LMEM_FIXED, uFormatSize);
    VE_COND_NOMEM(NULL == pwfxSrc);

    // Init the copy
    memcpy(pwfxSrc, pafdi->pwfx, uFormatSize);

    mmRet = MMSYSERR_NOERROR;

    if (NULL != had) {
        //
        //  IF a driver is specified, then enum from that driver only.
        //
        fNoDrivers = FALSE;

        padid = (PACMDRIVERID)((PACMDRIVER)had)->hadid;

        //
        //  step through all format tags that the caller is interested in
        //  and enumerate the formats...
        //
        for (u = 0; u < padid->cFormatTags; u++) {

            if (fFormatTag) {
                if (uFormatTag != padid->paFormatTagCache[u].dwFormatTag)
                    continue;
            }

            aftd.cbStruct = sizeof(aftd);
            aftd.dwFormatTagIndex   = u;
            mmRet = IFormatTagDetails((HACMDRIVERID)padid,
                                    &aftd, ACM_FORMATTAGDETAILSF_INDEX);

            if (MMSYSERR_NOERROR == mmRet) {
                if (fSuggest)
                    mmRet = ISuggestEnum(had, &aftd, pafdi, fnCallback,
                                       dwInstance, pwfxSrc,fdwEnum);
                else
                    mmRet = IFormatEnum(had, &aftd, pafdi, fnCallback,
                                      dwInstance, pwfxSrc, fdwEnum);
            }

            if ((mmRet == MMSYSERR_ERROR) || fFormatTag) {
                // Returned generic error to stop the enumeration.
                mmRet = MMSYSERR_NOERROR;
                break;
            }
        }
    } else if (fFormatTag) {
        PACMDRIVERID    padidBestCount;
        UINT            uBestCountFormat;
        DWORD           cBestCount;

        hadid = NULL;
        fNoDrivers = (!fConvert && !fSuggest);
        padidBestCount = NULL;
        cBestCount = 0;

        ENTER_LIST_SHARED();

        while (!IDriverGetNext(&hadid, hadid, 0L)) {
            fNoDrivers = FALSE;
            padid = (PACMDRIVERID) hadid;
            //
            //  find the format tag that caller is interested in and
            //  enumerate the formats...
            //
            for (u = 0; u < padid->cFormatTags; u++) {
                if (uFormatTag != padid->paFormatTagCache[u].dwFormatTag)
                    continue;

                aftd.cbStruct = sizeof(aftd);
                aftd.dwFormatTagIndex   = u;
                mmRet = IFormatTagDetails((HACMDRIVERID)padid,
                                        &aftd, ACM_FORMATTAGDETAILSF_INDEX);

                if (MMSYSERR_NOERROR == mmRet) {

                    if ( !padidBestCount || (aftd.cStandardFormats > cBestCount ) ) {
                        padidBestCount      = padid;
                        cBestCount          = aftd.cStandardFormats;
                        uBestCountFormat    = u;
                    }
                }

                break;

            }
        }

        if (NULL != padidBestCount)
        {
            HACMDRIVER  had1;

            aftd.cbStruct = sizeof(aftd);
            aftd.dwFormatTagIndex   = uBestCountFormat;
            mmRet = IFormatTagDetails((HACMDRIVERID)padidBestCount,
                                    &aftd, ACM_FORMATTAGDETAILSF_INDEX);

            if (MMSYSERR_NOERROR == mmRet) {
                mmRet = acmDriverOpen(&had1, (HACMDRIVERID)padidBestCount, 0L);
            }

            if (MMSYSERR_NOERROR == mmRet)
            {
                if (fSuggest) {
                    mmRet = ISuggestEnum(had1, &aftd, pafdi, fnCallback,
                                       dwInstance, pwfxSrc, fdwEnum);
                } else {
                    mmRet = IFormatEnum(had1, &aftd, pafdi, fnCallback,
                                      dwInstance, pwfxSrc, fdwEnum);
                }

                acmDriverClose(had1, 0L);

                if (MMSYSERR_ERROR == mmRet) {
                    // Returned generic error to stop the enumeration.
                    mmRet = MMSYSERR_NOERROR;
                }
            }
        }

        if (fNoDrivers && (WAVE_FORMAT_PCM != uFormatTag)) {
            fNoDrivers = FALSE;
        }

        LEAVE_LIST_SHARED();

    } else {
        //
        // Enum formats across all drivers.
        //
        fNoDrivers = (!fConvert && !fSuggest);
        hadid = NULL;

        ENTER_LIST_SHARED();

        while (!IDriverGetNext(&hadid, hadid, 0L)) {
            HACMDRIVER  had1;

            fNoDrivers = FALSE;
            padid = (PACMDRIVERID)hadid;

            if (fConvert || fSuggest) {
                uFormatTag = pwfxSrc->wFormatTag;

                //
                //  for every FormatTag in the driver
                //
                for (u = 0; u < padid->cFormatTags; u++) {
                    if (uFormatTag == padid->paFormatTagCache[u].dwFormatTag) {
                        //
                        //  flag that this driver supports that tag
                        //
                        uFormatTag = WAVE_FORMAT_UNKNOWN;
                        break;
                    }
                }

                if (WAVE_FORMAT_UNKNOWN != uFormatTag) {
                    //
                    //  the current driver does not support the format
                    //  tag, so skip to the next driver...
                    //
                    continue;
                }
            }

            mmRet = acmDriverOpen(&had1, hadid, 0L);
            if (MMSYSERR_NOERROR != mmRet)
            {
                continue;
            }

            for (uIndex = 0; uIndex < padid->cFormatTags; uIndex++)
            {
                aftd.cbStruct = sizeof(aftd);
                aftd.dwFormatTagIndex   = uIndex;
                mmRet = IFormatTagDetails((HACMDRIVERID)padid,
                                        &aftd, ACM_FORMATTAGDETAILSF_INDEX);


                if (MMSYSERR_NOERROR == mmRet)
                {
                    //
                    //  we have a format that has not been sent yet.
                    //
                    if (fSuggest)
                        mmRet = ISuggestEnum(had, &aftd, pafdi, fnCallback,
                                           dwInstance, pwfxSrc, fdwEnum);
                    else
                        mmRet = IFormatEnum(had1, &aftd, pafdi, fnCallback,
                                          dwInstance, pwfxSrc, fdwEnum);
                }

                if (MMSYSERR_ERROR == mmRet)
                {
                    // Returned generic error to stop the enumeration.
                    break;
                }
            }

            acmDriverClose(had1, 0L);

            if (MMSYSERR_ERROR == mmRet)
            {
                mmRet = MMSYSERR_NOERROR;
                break;
            }
        }

        LEAVE_LIST_SHARED();

    }

    if (fNoDrivers) {
        IHardwareEnum(pafdi, fnCallback, dwInstance, pwfxSrc, fdwEnum);
    }

EXIT:
    if (pwfxSrc != NULL)
    {
        // Free the copy of the source format
        LocalFree(pwfxSrc);
    }

    FUNC_EXIT();
    return(mmRet);
}

