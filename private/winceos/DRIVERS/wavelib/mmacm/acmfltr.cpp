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
//   acmfltr.c
//
//   Copyright (c) 1991-2000 Microsoft Corporation
//
//   This module provides the wave filter enumeration and string API's.
//
//------------------------------------------------------------------------------
#include "acmi.h"


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT
IFilterTagDetails(
    HACMDRIVERID            hadid,
    LPACMFILTERTAGDETAILS   paftd,
    DWORD                   fdwDetails
    )
{
    PACMDRIVERID            padid = (PACMDRIVERID) hadid;
    UINT                    u;
    DWORD                   fdwQuery = (ACM_FILTERTAGDETAILSF_QUERYMASK & fdwDetails);
    MMRESULT                mmRet = ACMERR_NOTPOSSIBLE;

    FUNC("IFilterTagDetails");

    switch (fdwQuery) {

        case ACM_FILTERTAGDETAILSF_FILTERTAG:

            for (u = 0; u < padid->cFilterTags; u++) {
                if (padid->paFilterTagCache[u].dwFilterTag == paftd->dwFilterTag) {
                    mmRet = MMSYSERR_NOERROR;
                    break;
                }
            }
            break;

        case ACM_FILTERTAGDETAILSF_LARGESTSIZE:
        case ACM_FILTERTAGDETAILSF_INDEX:
            mmRet = MMSYSERR_NOERROR;
            break;
    }

    if (MMSYSERR_NOERROR == mmRet) {
        EnterHandle((HACMDRIVERID)padid);
        mmRet = (MMRESULT)IDriverMessageId((HACMDRIVERID)padid,
                                         ACMDM_FILTERTAG_DETAILS,
                                         (LPARAM)(LPVOID)paftd,
                                         fdwDetails);
        LeaveHandle((HACMDRIVERID)padid);
    }


    if (MMSYSERR_NOERROR != mmRet) {
        goto EXIT;
    }

    switch (paftd->dwFilterTag) {

        case WAVE_FILTER_UNKNOWN:
            ERRMSG("Driver returned Filter tag 0!");
            mmRet = MMSYSERR_ERROR;
            goto EXIT;

        case WAVE_FILTER_DEVELOPMENT:
            WARNMSG("Driver returned DEVELOPMENT Filter tag--do not ship this way.");
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
//  @struct WAVEFILTER | The <t WAVEFILTER> structure defines a filter
//          for waveform data. Only filter information common to all
//          waveform data filters is included in this structure. For filters
//          that require additional information, this structure is included
//          as the first member in another structure, along with the additional
//          information.
//
//  @field  DWORD | cbStruct | Specifies the size, in bytes, of the
//          <t WAVEFILTER> structure. The size specified in this member must be
//          large enough to contain the base <t WAVEFILTER> structure.
//
//  @field  DWORD | dwFilterTag | Specifies the waveform filter type. Filter
//          tags are registered with Microsoft for various filter algorithms.
//          A complete list of filter tags can be found in the MMREG.H header
//          file available from Microsoft. For more information on filter
//          tags, contact Microsoft for availability of the Multimedia Developer
//          Registration Kit:
//
//              Microsoft Corporation
//              Advanced Consumer Technology
//              Product Marketing
//              One Microsoft Way
//              Redmond, WA 98052-6399
//
//  @field  DWORD | fdwFilter | Specifies flags for the <e WAVEFILTER.dwFilterTag>.
//          The flags defined for this member are universal to all filters.
//          Currently, no flags are defined.
//
//  @field  DWORD | dwReserved[5] | This member is reserved for system use and should not
//          be examined or modified by an application.
//
//  @xref   <t WAVEFORMAT> <t WAVEFORMATEX> <t PCMWAVEFORMAT>
//
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API_STRUCTURE
//
//  @struct ACMFILTERTAGDETAILS | The <t ACMFILTERTAGDETAILS> structure
//          details a wave filter tag for an Audio Compression Manager (ACM)
//          filter driver.
//
//  @field  DWORD | cbStruct | Specifies the size in bytes of the
//          <t ACMFILTERTAGDETAILS> structure. This member must be initialized
//          before calling the <f acmFilterTagDetails> or <f acmFilterTagEnum>
//          functions. The size specified in this member must be large enough to
//          contain the base <t ACMFILTERTAGDETAILS> structure. When the
//          <f acmFilterTagDetails> function returns, this member contains the
//          actual size of the information returned. The returned information
//          will never exceed the requested size.
//
//  @field  DWORD | dwFilterTagIndex | Specifies the index of the filter tag
//          to retrieve details for. The index ranges from zero to one
//          less than the number of filter tags supported by an ACM driver. The
//          number of filter tags supported by a driver is contained in the
//          <e ACMDRIVERDETAILS.cFilterTags> member of the <t ACMDRIVERDETAILS>
//          structure. The <e ACMFILTERTAGDETAILS.dwFilterTagIndex> member is
//          only used when querying filter tag details on a driver by index;
//          otherwise, this member should be zero.
//
//  @field  DWORD | dwFilterTag | Specifies the wave filter tag that the
//          <t ACMFILTERTAGDETAILS> structure describes. This member is used
//          as an input for the ACM_FILTERTAGDETAILSF_FILTERTAG and
//          ACM_FILTERTAGDETAILSF_LARGESTSIZE query flags. This member is always
//          returned if the <f acmFilterTagDetails> is successful. This member
//          should be set to WAVE_FILTER_UNKNOWN for all other query flags.
//
//  @field  DWORD | cbFilterSize | Specifies the largest total size in bytes
//          of a wave filter of the <e ACMFILTERTAGDETAILS.dwFilterTag> type.
//          For example, this member will be 40 for WAVE_FILTER_ECHO and 36 for
//          WAVE_FILTER_VOLUME.
//
//  @field  DWORD | fdwSupport | Specifies driver-support flags specific to
//          the filter tag. These flags are identical to the <e ACMDRIVERDETAILS.fdwSupport>
//          flags of the <t ACMDRIVERDETAILS> structure. This argument can be a
//          combination of the following values and identifies which operations the
//          driver supports with the filter tag:
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CODEC | Specifies that this driver
//          supports conversion between two different format tags while using
//          the specified filter tag. For example, if a driver supports
//          compression from WAVE_FORMAT_PCM to WAVE_FORMAT_ADPCM with the
//          specifed filter tag, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CONVERTER | Specifies that this
//          driver supports conversion between two different formats of the
//          same format tag while using the specified filter tag. For example,
//          if a driver supports resampling of WAVE_FORMAT_PCM with the specified
//          filter tag, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_FILTER | Specifies that this driver
//          supports a filter (modification of the data without changing any
//          of the format attributes). For example, if a driver supports volume
//          or echo operations on WAVE_FORMAT_PCM, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_ASYNC | Specifies that this driver
//          supports asynchronous conversions with the specified format tag.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_HARDWARE | Specifies that this driver
//          supports hardware input and/or output with the specified filter tag
//          through a waveform device. An application should use <f acmMetrics>
//          with the ACM_METRIC_HARDWARE_WAVE_INPUT and
//          ACM_METRIC_HARDWARE_WAVE_OUTPUT metric indexes to get the waveform
//          device identifiers associated with the supporting ACM driver.
//
//  @field  DWORD | cStandardFilters | Specifies the number of standard filters of the
//          <e ACMFILTERTAGDETAILS.dwFilterTag> type. (that is, the combination of all
//          filter characteristics). This value cannot specify all filters supported by the driver.
//
//  @field  char | szFilterTag[ACMFILTERTAGDETAILS_FILTERTAG_CHARS] |
//          Specifies a string that describes the <e ACMFILTERTAGDETAILS.dwFilterTag>
//          type. This string is always returned if the <f acmFilterTagDetails>
//          function is successful.
//
//  @xref   <f acmFilterTagDetails> <f acmFilterTagEnum>
//
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmFilterTagDetails | This function queries the
//          Audio Compression Manager (ACM) for details on a specific wave
//          filter tag.
//
//  @parm   HACMDRIVER | had | Optionally specifies an ACM driver to query
//          for wave filter tag details. If this argument is NULL, then the
//          ACM uses the details from the first suitable ACM driver. Note that
//          an application must specify a valid <t HACMDRIVER> or <t HACMDRIVERID>
//          when using the ACM_FILTERTAGDETAILSF_INDEX query type. Driver
//          identifiers for disabled drivers are not allowed.
//
//  @parm   LPACMFILTERTAGDETAILS | paftd | Specifies a pointer to the
//          <t ACMFILTERTAGDETAILS> structure that is to receive the filter
//          tag details.
//
//  @parm   DWORD | fdwDetails | Specifies flags for getting the details.
//
//          @flag ACM_FILTERTAGDETAILSF_INDEX | Indicates that a filter tag index
//          was given in the <e ACMFILTERTAGDETAILS.dwFilterTagIndex> member of
//          the <t ACMFILTERTAGDETAILS> structure. The filter tag and details
//          will be returned in the structure defined by <p paftd>. The index
//          ranges from zero to one less than the <e ACMDRIVERDETAILS.cFilterTags>
//          member returned in the <t ACMDRIVERDETAILS> structure for an ACM
//          driver. An application must specify a driver handle (<p had>) when
//          retrieving filter tag details with this flag.
//
//          @flag ACM_FILTERTAGDETAILSF_FILTERTAG | Indicates that a filter tag
//          was given in the <e ACMFILTERTAGDETAILS.dwFilterTag> member of
//          the <t ACMFILTERTAGDETAILS> structure. The filter tag details will
//          be returned in the structure defined by <p paftd>. If an application
//          specifies an ACM driver handle (<p had>), then details on the filter
//          tag will be returned for that driver. If an application specifies
//          NULL for <p had>, then the ACM finds the first acceptable driver
//          to return the details.
//
//          @flag ACM_FILTERTAGDETAILSF_LARGESTSIZE | Indicates that the details
//          on the filter tag with the largest filter size in bytes is to be
//          returned. The <e ACMFILTERTAGDETAILS.dwFilterTag> member must either
//          be WAVE_FILTER_UNKNOWN or the filter tag to find the largest size
//          for. If an application specifies an ACM driver handle (<p had>), then
//          details on the largest filter tag will be returned for that driver.
//          If an application specifies NULL for <p had>, then the ACM finds an
//          acceptable driver with the largest filter tag requested to return
//          the details.
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALPARAM | One or more arguments passed is invalid.
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag ACMERR_NOTPOSSIBLE | The details requested are not available.
//
//  @xref   <f acmDriverDetails> <f acmDriverOpen> <f acmFilterDetails>
//          <f acmFilterTagEnum> <f acmFormatTagDetails>
//
//------------------------------------------------------------------------------
MMRESULT
acmFilterTagDetails(
    HACMDRIVER              had,
    LPACMFILTERTAGDETAILS   paftd,
    DWORD                   fdwDetails
    )
{
    PACMDRIVER              pad = NULL;
    HACMDRIVERID            hadid;
    PACMDRIVERID            padid = NULL;
    DWORD                   fdwQuery;
    MMRESULT                mmRet;
    UINT                    u;

    FUNC("acmFilterTagDetails");

    VE_FLAGS(fdwDetails, ACM_FILTERTAGDETAILSF_VALID);
    VE_COND_PARAM(sizeof(ACMFILTERTAGDETAILS) > paftd->cbStruct);
    VE_COND_PARAM(0L != paftd->fdwSupport);

    fdwQuery = (ACM_FILTERTAGDETAILSF_QUERYMASK & fdwDetails);

    switch (fdwQuery) {

        case ACM_FILTERTAGDETAILSF_INDEX:
            //
            //  we don't (currently) support index based enumeration across
            //  all drivers... may never support this. so validate the
            //  handle and fail if not valid (like NULL).
            //
            VE_HANDLE(had, TYPE_HACMOBJ);
            VE_COND_PARAM(WAVE_FILTER_UNKNOWN != paftd->dwFilterTag);
            break;

        case ACM_FILTERTAGDETAILSF_FILTERTAG:
            VE_COND_PARAM(WAVE_FILTER_UNKNOWN == paftd->dwFilterTag);
            break;

        case ACM_FILTERTAGDETAILSF_LARGESTSIZE:
            break;

        //
        //  we don't (currently) support the requested query type--so return
        //  not supported.
        //
        default:
            ERRMSG("Unknown query type specified.");
            GOTO_EXIT(mmRet = MMSYSERR_NOTSUPPORTED);
    }

    if (NULL != had) {
        //
        // Non-null HAD.
        //
        VE_HANDLE(had, TYPE_HACMOBJ);

        pad = (PACMDRIVER)had;
        if (TYPE_HACMDRIVERID == pad->uHandleType) {
            padid = (PACMDRIVERID) pad;
            pad   = NULL;
            VE_COND(ACMDRIVERID_DRIVERF_DISABLED & padid->fdwDriver, MMSYSERR_NOTENABLED);
        } else {
            VE_HANDLE(had, TYPE_HACMDRIVER);
            padid = (PACMDRIVERID) pad->hadid;
        }
    }

    if (NULL == padid) {
        PACMDRIVERID    padidT;
        DWORD           cbFilterSizeLargest;

        padidT              = NULL;
        cbFilterSizeLargest = 0;
        hadid = NULL;

        ENTER_LIST_SHARED();

        while (MMSYSERR_NOERROR == IDriverGetNext(&hadid, hadid, 0L)) {
            padidT = (PACMDRIVERID)hadid;

            switch (fdwQuery) {
                case ACM_FILTERTAGDETAILSF_FILTERTAG:
                    for (u=0; u<padidT->cFilterTags; u++) {
                        if (padidT->paFilterTagCache[u].dwFilterTag == paftd->dwFilterTag) {
                            padid = padidT;
                            break;
                        }
                    }
                    break;

                case ACM_FILTERTAGDETAILSF_LARGESTSIZE:
                    for (u=0; u<padidT->cFilterTags; u++) {
                        if (WAVE_FORMAT_UNKNOWN != paftd->dwFilterTag) {
                            if (padidT->paFilterTagCache[u].dwFilterTag != paftd->dwFilterTag) {
                                continue;
                            }
                        }
                        if (padidT->paFilterTagCache[u].cbFilterSize > cbFilterSizeLargest) {
                            cbFilterSizeLargest = padidT->paFilterTagCache[u].cbFilterSize;
                            padid = padidT;
                        }
                    }
                    break;

                default:
                    TESTMSG("Unknown query type got through param validation?!?!");
            }
        }

        LEAVE_LIST_SHARED();
    }

    if (NULL != padid) {
        mmRet = IFilterTagDetails((HACMDRIVERID) padid, paftd, fdwDetails);
    } else {
        mmRet = ACMERR_NOTPOSSIBLE;
    }

 EXIT:
    FUNC_EXIT();
    return(mmRet);
}




//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API_STRUCTURE
//
//  @struct ACMFILTERDETAILS | The <t ACMFILTERDETAILS> structure details a
//          wave filter for a specific filter tag for an Audio Compression
//          Manager (ACM) driver.
//
//  @field  DWORD | cbStruct | Specifies the size in bytes of the
//          <t ACMFILTERDETAILS> structure. This member must be initialized
//          before calling the <f acmFilterDetails> or <f acmFilterEnum>
//          functions. The size specified in this member must be large enough to
//          contain the base <t ACMFILTERDETAILS> structure. When the
//          <f acmFilterDetails> function returns, this member contains the
//          actual size of the information returned. The returned information
//          will never exceed the requested size.
//
//  @field  DWORD | dwFilterIndex | Specifies the index of the filter about which
//          details will be retrieved. The index ranges from zero to one
//          less than the number of standard filters supported by an ACM driver
//          for a filter tag. The number of standard filters supported by a
//          driver for a filter tag is contained in the
//          <e ACMFILTERTAGDETAILS.cStandardFilters> member of the
//          <t ACMFILTERTAGDETAILS> structure. The
//          <e ACMFILTERDETAILS.dwFilterIndex> member is only used when querying
//          standard filter details on a driver by index; otherwise, this member
//          should be zero. Also note that this member will be set to zero
//          by the ACM when an application queries for details on a filter; in
//          other words, this member is only used as an input argument and is
//          never returned by the ACM or an ACM driver.
//
//  @field  DWORD | dwFilterTag | Specifies the wave filter tag that the
//          <t ACMFILTERDETAILS> structure describes. This member is used
//          as an input for the ACM_FILTERDETAILSF_INDEX query flag.  For
//          the ACM_FILTERDETAILSF_FORMAT query flag, this member
//          must be initialized to the same filter tag as the
//          <e ACMFILTERDETAILS.pwfltr> member specifies.
//          This member is always returned if the <f acmFilterDetails> function is
//          successful. This member should be set to WAVE_FILTER_UNKNOWN for all
//          other query flags.
//
//  @field  DWORD | fdwSupport | Specifies driver-support flags specific to
//          the specified filter. These flags are identical to the
//          <e ACMDRIVERDETAILS.fdwSupport> flags of the <t ACMDRIVERDETAILS>
//          structure, but are specific to the filter that is being queried.
//          This argument can be a combination of the following values and
//          identifies which operations the driver supports for the filter tag:
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CODEC | Specifies that this driver
//          supports conversion between two different format tags while using the
//          specified filter. For example, if a driver supports compression from
//          WAVE_FORMAT_PCM to WAVE_FORMAT_ADPCM with the specifed
//          filter, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CONVERTER | Specifies that this
//          driver supports conversion between two different formats of the
//          same format tag while using the specified filter. For example, if a
//          driver supports resampling of WAVE_FORMAT_PCM with the specified
//          filter, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_FILTER | Specifies that this driver
//          supports a filter (modification of the data without changing any
//          of the format attributes). For example, if a driver supports volume
//          or echo operations on WAVE_FORMAT_PCM, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_ASYNC | Specifies that this driver
//          supports asynchronous conversions with the specified filter tag.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_HARDWARE | Specifies that this driver
//          supports hardware input and/or output with the specified filter
//          through a waveform device. An application should use <f acmMetrics>
//          with the ACM_METRIC_HARDWARE_WAVE_INPUT and
//          ACM_METRIC_HARDWARE_WAVE_OUTPUT metric indexes to get the waveform
//          device identifiers associated with the supporting ACM driver.
//
//  @field  LPWAVEFILTER | pwfltr | Specifies a pointer to a <t WAVEFILTER>
//          data structure that will receive the filter details. This structure
//          requires no initialization by the application unless the
//          ACM_FILTERDETAILSF_FILTER flag is specified to <f acmFilterDetails>.
//          In this case, the <e WAVEFILTER.dwFilterTag> must be equal to
//          the <e ACMFILTERDETAILS.dwFilterTag> of the <t ACMFILTERDETAILS>
//          structure.
//
//  @field  DWORD | cbwfltr | Specifies the size, in bytes, available for
//          the <e ACMFILTERDETAILS.pwfltr> to receive the filter details. The
//          <f acmMetrics> and <f acmFilterTagDetails> functions can be used to
//          determine the maximum size required for any filter available for the
//          specified driver (or for all installed ACM drivers).
//
//  @field  char | szFilter[ACMFILTERDETAILS_FILTER_CHARS] |
//          Specifies a string that describes the filter for the
//          <e ACMFILTERDETAILS.dwFilterTag> type. This string is always returned
//          if the <f acmFilterDetails> function is successful.
//
//  @xref   <f acmFilterDetails> <f acmFilterEnum> <f acmFilterTagDetails>
//          <f acmFilterTagEnum>
//
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmFilterDetails | This function queries the Audio Compression
//          Manager (ACM) for details about a filter with a specific wave filter tag.
//
//  @parm   HACMDRIVER | had | Optionally specifies an ACM driver to query
//          for wave filter details for a filter tag. If this argument is NULL,
//          then the ACM uses the details from the first suitable ACM driver.
//
//  @parm   LPACMFILTERDETAILS | pafd | Specifies a pointer to the
//          <t ACMFILTERDETAILS> structure that is to receive the filter
//          details for the given filter tag.
//
//  @parm   DWORD | fdwDetails | Specifies flags for getting the details.
//
//          @flag ACM_FILTERDETAILSF_INDEX | Indicates that a filter index for
//          the filter tag was given in the <e ACMFILTERDETAILS.dwFilterIndex>
//          member of the <t ACMFILTERDETAILS> structure. The filter details
//          will be returned in the structure defined by <p pafd>. The index
//          ranges from zero to one less than the
//          <e ACMFILTERTAGDETAILS.cStandardFilters> member returned in the
//          <t ACMFILTERTAGDETAILS> structure for a filter tag. An application
//          must specify a driver handle (<p had>) when retrieving
//          filter details with this flag. Refer to the description for the
//          <t ACMFILTERDETAILS> structure for information on what members
//          should be initialized before calling this function.
//
//          @flag ACM_FILTERDETAILSF_FILTER | Indicates that a <t WAVEFILTER>
//          structure pointed to by <e ACMFILTERDETAILS.pwfltr> of the
//          <t ACMFILTERDETAILS> structure was given and the remaining details
//          should be returned. The <e ACMFILTERDETAILS.dwFilterTag> member
//          of the <t ACMFILTERDETAILS> structure must be initialized to the same filter
//          tag as the <e ACMFILTERDETAILS.pwfltr> member specifies. This
//          query type can be used to get a string description of an arbitrary
//          filter structure. If an application specifies an ACM driver handle
//          (<p had>), then details on the filter will be returned for that
//          driver. If an application specifies NULL for <p had>, then the ACM
//          finds the first acceptable driver to return the details.
//
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALPARAM | One or more arguments passed is invalid.
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag ACMERR_NOTPOSSIBLE | The details requested are not available.
//
//  @xref   <f acmFilterTagDetails> <f acmDriverDetails> <f acmDriverOpen>
//
//------------------------------------------------------------------------------
MMRESULT
acmFilterDetails(
    HACMDRIVER              had,
    LPACMFILTERDETAILS      pafd,
    DWORD                   fdwDetails
    )
{
    MMRESULT        mmRet;
    PACMDRIVER      pad;
    HACMDRIVERID    hadid;
    PACMDRIVERID    padid;
    DWORD           dwQuery;

    FUNC("acmFilterDetails");

    VE_FLAGS(fdwDetails, ACM_FILTERDETAILSF_VALID);
    VE_COND_PARAM(sizeof(ACMFILTERDETAILS) > pafd->cbStruct);

    VE_COND_PARAM(sizeof(WAVEFILTER) > pafd->cbwfltr);
    VE_WPOINTER(pafd->pwfltr, pafd->cbwfltr);
    VE_COND_PARAM(0L != pafd->fdwSupport);

    dwQuery = ACM_FILTERDETAILSF_QUERYMASK & fdwDetails;

    switch (dwQuery) {

        case ACM_FILTERDETAILSF_FILTER:
            VE_COND_PARAM(pafd->dwFilterTag != pafd->pwfltr->dwFilterTag);
            __fallthrough;

        case ACM_FILTERDETAILSF_INDEX:
            VE_COND_PARAM(WAVE_FILTER_UNKNOWN == pafd->dwFilterTag);
            //
            //  we don't (currently) support index based enumeration across
            //  all drivers... may never support this. so validate the
            //  handle and fail if not valid (like NULL).
            //
            if (ACM_FILTERDETAILSF_INDEX == dwQuery) {
                ACMFILTERTAGDETAILS aftd;

                VE_HANDLE(had, TYPE_HACMOBJ);

                memset(&aftd, 0, sizeof(aftd));
                aftd.cbStruct    = sizeof(aftd);
                aftd.dwFilterTag = pafd->dwFilterTag;

                mmRet = acmFilterTagDetails(had, &aftd, ACM_FILTERTAGDETAILSF_FILTERTAG);
                VE_EXIT_ON_ERROR();

                VE_COND_PARAM(pafd->dwFilterIndex >= aftd.cStandardFilters);
            }
            break;

        default:
            ERRMSG("Unknown query type specified.");
            GOTO_EXIT(mmRet = MMSYSERR_NOTSUPPORTED);
    }


    //
    //  if we are passed a driver handle, then use it
    //
    if (NULL != had) {
        pad = (PACMDRIVER)had;
        if (TYPE_HACMDRIVERID == pad->uHandleType) {

            VE_HANDLE(had, TYPE_HACMDRIVERID);
            //
            //  !!! yes, this is right !!!
            //
            padid = (PACMDRIVERID)had;

            if (0 == (ACMDRIVERDETAILS_SUPPORTF_FILTER & padid->fdwSupport)) {
                ERRMSG("Driver does not support filter operations.");
                GOTO_EXIT(mmRet = MMSYSERR_INVALPARAM);
            }

            EnterHandle(had);
            mmRet = (MMRESULT) IDriverMessageId((HACMDRIVERID)had,
                                             ACMDM_FILTER_DETAILS,
                                             (LPARAM)pafd,
                                             fdwDetails);
            LeaveHandle(had);

        } else {
            VE_HANDLE(had, TYPE_HACMDRIVER);

            pad   = (PACMDRIVER) had;
            padid = (PACMDRIVERID) pad->hadid;

            if (0 == (ACMDRIVERDETAILS_SUPPORTF_FILTER & padid->fdwSupport)) {
                ERRMSG("Driver does not support filter operations.");
                GOTO_EXIT(mmRet = MMSYSERR_INVALPARAM);
            }

            EnterHandle(had);
            mmRet = (MMRESULT) IDriverMessage(had,
                                           ACMDM_FILTER_DETAILS,
                                           (LPARAM)pafd,
                                           fdwDetails);
            LeaveHandle(had);
        }

        if (MMSYSERR_NOERROR == mmRet) {
            //
            //  if caller is asking for details on a specific filter, then
            //  always return index equal to zero (it means nothing)
            //
            if (ACM_FILTERDETAILSF_FILTER == dwQuery) {
                pafd->dwFilterIndex = 0;
            }
        }

        goto EXIT;
    }


    //
    //
    //
    hadid = NULL;
    mmRet   = MMSYSERR_NODRIVER;

    ENTER_LIST_SHARED();

    while (MMSYSERR_NOERROR == IDriverGetNext(&hadid, hadid, 0L)) {
        padid = (PACMDRIVERID)hadid;

        if (0 == (ACMDRIVERDETAILS_SUPPORTF_FILTER & padid->fdwSupport))
            continue;

        //
        //
        //
        EnterHandle(hadid);
        mmRet = (MMRESULT) IDriverMessageId(hadid,
                                         ACMDM_FILTER_DETAILS,
                                         (LPARAM)pafd,
                                         fdwDetails);
        LeaveHandle(hadid);

        if (MMSYSERR_NOERROR == mmRet) {
            //
            //  if caller is asking for details on a specific filter, then
            //  always return index equal to zero (it means nothing)
            //
            if (ACM_FILTERDETAILSF_FILTER == dwQuery)
                pafd->dwFilterIndex = 0;
            break;
        }
    }

    LEAVE_LIST_SHARED();

EXIT:
    FUNC_EXIT();
    return(mmRet);
}




//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   BOOL ACMFILTERTAGENUMCB | acmFilterTagEnumCallback |
//          The <f acmFilterTagEnumCallback> function  is a placeholder for an
//          application-supplied function name and refers to the callback function
//          used for Audio Compression Manager (ACM) wave filter tag enumeration.
//
//  @parm   HACMDRIVERID | hadid | Specifies an ACM driver identifier.
//
//  @parm   LPACMFILTERTAGDETAILS | paftd | Specifies a pointer to an
//          <t ACMFILTERTAGDETAILS> structure that contains the enumerated
//          filter tag details.
//
//  @parm   DWORD | dwInstance | Specifies the application-defined value
//          specified in the <f acmFilterTagEnum> function.
//
//  @parm   DWORD | fdwSupport | Specifies driver-support flags specific to
//          the driver identifier <p hadid>. These flags are identical to the
//          <e ACMDRIVERDETAILS.fdwSupport> flags of the <t ACMDRIVERDETAILS>
//          structure. This argument can be a combination of the following
//          values and identifies which operations the driver supports with the
//          filter tag
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CODEC | Specifies that this driver
//          supports conversion between two different format tags while using
//          the specified filter tag. For example, if a driver supports
//          compression from WAVE_FORMAT_PCM to WAVE_FORMAT_ADPCM with the
//          specifed filter tag, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CONVERTER | Specifies that this
//          driver supports conversion between two different formats of the
//          same format tag while using the specified filter tag. For example,
//          if a driver supports resampling of WAVE_FORMAT_PCM with the specified
//          filter tag, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_FILTER | Specifies that this driver
//          supports a filter (modification of the data without changing any
//          of the format attributes). For example, if a driver supports volume
//          or echo operations on WAVE_FORMAT_PCM, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_ASYNC | Specifies that this driver
//          supports asynchronous conversions with the specified filter tag.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_HARDWARE | Specifies that this driver
//          supports hardware input and/or output with the specified filter tag
//          through a waveform device. An application should use <f acmMetrics>
//          with the ACM_METRIC_HARDWARE_WAVE_INPUT and
//          ACM_METRIC_HARDWARE_WAVE_OUTPUT metric indexes to get the waveform
//          device identifiers associated with the supporting ACM driver.
//
//  @rdesc  The callback function must return TRUE to continue enumeration;
//          to stop enumeration, it must return FALSE.
//
//  @comm   The <f acmFilterTagEnum> function will return MMSYSERR_NOERROR
//          (zero) if no filter tags are to be enumerated. Moreover, the callback
//          function will not be called.
//
//  @xref   <f acmFilterTagEnum> <f acmFilterTagDetails> <f acmDriverOpen>
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmFilterTagEnum | The <f acmFilterTagEnum> function
//          enumerates wave filter tags available from an Audio Compression
//          Manager (ACM) driver. The <f acmFilterTagEnum> function continues
//          enumerating until there are no more suitable filter tags or the
//          callback function returns FALSE.
//
//  @parm   HACMDRIVER | had | Optionally specifies an ACM driver to query
//          for wave filter tag details. If this argument is NULL, then the
//          ACM uses the details from the first suitable ACM driver.
//
//  @parm   LPACMFILTERTAGDETAILS | paftd | Specifies a pointer to the
//          <t ACMFILTERTAGDETAILS> structure that is to receive the filter
//          tag details passed to the <p fnCallback> function. This structure
//          must have the <e ACMFILTERTAGDETAILS.cbStruct> member of the
//          <t ACMFILTERTAGDETAILS> structure initialized.
//
//  @parm   ACMFILTERTAGENUMCB | fnCallback | Specifies the address of the
//          application-defined callback function.
//
//  @parm   DWORD | dwInstance | Specifies a 32-bit, application-defined value
//          that is passed to the callback function along with ACM filter tag
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
//  @comm   The <f acmFilterTagEnum> function will return MMSYSERR_NOERROR
//          (zero) if no suitable ACM drivers are installed. Moreover, the
//          callback function will not be called.
//
//  @xref   <f acmFilterTagEnumCallback> <f acmFilterTagDetails>
//
//------------------------------------------------------------------------------
MMRESULT
acmFilterTagEnum(
    HACMDRIVER              had,
    LPACMFILTERTAGDETAILS   paftd,
    ACMFILTERTAGENUMCB      fnCallback,
    DWORD                   dwInstance,
    DWORD                   fdwEnum
    )
{
    PACMDRIVER          pad;
    PACMDRIVERID        padid;
    UINT                uIndex;
    UINT                uFilterTag;
    BOOL                f;
    HACMDRIVERID        hadid;
    PACMDRIVERID        padidCur;
    HACMDRIVERID        hadidCur;
    BOOL                fSent;
    DWORD               cbaftd;
    DWORD               fdwSupport;
    MMRESULT            mmRet;

    FUNC("acmFilterTagEnum");

    VE_CALLBACK(fnCallback);
    VE_FLAGS(fdwEnum, ACM_FILTERTAGENUMF_VALID);
    VE_COND_PARAM(sizeof(ACMFILTERTAGDETAILS) > paftd->cbStruct);
// BUGBUG is this needed?
//    VE_COND_PARAM(0L != paftd->fdwSupport);

    cbaftd = min(paftd->cbStruct, sizeof(ACMFILTERTAGDETAILS));

    if (NULL != had) {
        VE_HANDLE(had, TYPE_HACMDRIVER);

        //
        //  enum filter tags for this driver only.
        //
        pad   = (PACMDRIVER) had;
        padid = (PACMDRIVERID) pad->hadid;

        //
        //  do NOT include the 'disabled' bit!
        //
        fdwSupport = padid->fdwSupport;

        //
        //  while there are Filters to enumerate and we have not been
        //  told to stop (client returns FALSE to stop enum)
        //
        mmRet = MMSYSERR_NOERROR;
        for (uIndex = 0; uIndex < padid->cFilterTags; uIndex++) {
            paftd->cbStruct = cbaftd;
            paftd->dwFilterTagIndex = uIndex;
            mmRet = IFilterTagDetails((HACMDRIVERID)padid, paftd, ACM_FILTERTAGDETAILSF_INDEX);
            if (MMSYSERR_NOERROR != mmRet) {
                break;
            }

            f = (BOOL) wapi_DoFunctionCallback( fnCallback,
                                                4,
                                                (DWORD) pad->hadid,
                                                (DWORD) paftd,
                                                dwInstance,
                                                fdwSupport,
                                                0);
    // BUGBUG do function callback
//            f = (* fnCallback)(pad->hadid, paftd, dwInstance, fdwSupport);
            if (FALSE == f)
                break;
        }

        goto EXIT;
    }


    hadidCur = NULL;

    ENTER_LIST_SHARED();

    while (!IDriverGetNext(&hadidCur, hadidCur, 0L)) {
        padidCur = (PACMDRIVERID)hadidCur;

        for (uIndex = 0; uIndex < padidCur->cFilterTags; uIndex++) {
            uFilterTag = (UINT)(padidCur->paFilterTagCache[uIndex].dwFilterTag);
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

                for (u = 0; u < padid->cFilterTags; u++) {
                    //
                    //  for every FilterTag in the driver
                    //
                    if (uFilterTag == padid->paFilterTagCache[u].dwFilterTag) {
                        //
                        //  we have a match, but this was already given.
                        //
                        fSent = TRUE;
                        break;
                    }
                }
            }

            if (!fSent) {
                //
                //  we have a filter that has not been sent yet.
                //
                paftd->dwFilterTagIndex = uIndex;
                paftd->cbStruct = cbaftd;
                mmRet = IFilterTagDetails((HACMDRIVERID)padidCur,
                                        paftd, ACM_FILTERTAGDETAILSF_INDEX);
                if (MMSYSERR_NOERROR != mmRet) {
                    LEAVE_LIST_SHARED();
                    goto EXIT;
                }

                //
                //  do NOT include the 'disabled' bit!
                //
                fdwSupport = padidCur->fdwSupport;

                f = (BOOL) wapi_DoFunctionCallback(
                                                   fnCallback,
                                                   4,
                                                   (DWORD) hadidCur,
                                                   (DWORD) paftd,
                                                   dwInstance,
                                                   fdwSupport,
                                                   0);

                // f = (* fnCallback)(hadidCur, paftd, dwInstance, fdwSupport);
                if (FALSE == f) {
                    LEAVE_LIST_SHARED();
                    GOTO_EXIT(mmRet = MMSYSERR_NOERROR);
                }
            }
        }
    }

    LEAVE_LIST_SHARED();

    mmRet = MMSYSERR_NOERROR;
EXIT:
    FUNC_EXIT();
    return(mmRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT FNLOCAL
IFilterEnum(
    HACMDRIVERID            hadid,
    LPACMFILTERTAGDETAILS   paftd,
    LPACMFILTERDETAILS      pafd,
    ACMFILTERENUMCB         fnCallback,
    DWORD                   dwInstance
    )
{
    MMRESULT            mmRet;
    BOOL                f;
    DWORD               cbafd;
    LPWAVEFILTER        pwfltr;
    DWORD               cbwfltr;
    UINT                u;
    PACMDRIVERID        padid;
    DWORD               fdwSupport;

    FUNC("IFilterEnum");
    //
    //  be a bit paranoid and save some stuff so we can always reinit
    //  the structure between calling the driver (i just don't trust
    //  driver writers)
    //
    cbafd   = pafd->cbStruct;
    pwfltr  = pafd->pwfltr;
    cbwfltr = pafd->cbwfltr;

    padid = (PACMDRIVERID) hadid;

    //
    //  do NOT include whether the 'disabled' bit!
    //
    fdwSupport = padid->fdwSupport;

    for (u = 0; u < paftd->cStandardFilters; u++) {
        pafd->cbStruct      = cbafd;
        pafd->dwFilterIndex = u;
        pafd->dwFilterTag   = paftd->dwFilterTag;
        pafd->fdwSupport    = 0;
        pafd->pwfltr        = pwfltr;
        pafd->cbwfltr       = cbwfltr;

        if (FIELD_OFFSET(ACMFILTERDETAILS, szFilter) < cbafd)
            pafd->szFilter[0] = '\0';

        mmRet = acmFilterDetails((HACMDRIVER)hadid,
                                pafd,
                                ACM_FILTERDETAILSF_INDEX);
        if (MMSYSERR_NOERROR != mmRet)
            continue;

        f = (BOOL) wapi_DoFunctionCallback(fnCallback,
                                           4,
                                           (DWORD) hadid,
                                           (DWORD) pafd,
                                           dwInstance,
                                           fdwSupport,
                                           0);

// BUGBUG do function callback
//        f = (* fnCallback)(hadid, pafd, dwInstance, fdwSupport);
        if (FALSE == f)
            GOTO_EXIT(mmRet = MMSYSERR_ERROR);
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
//  @func   BOOL ACMFILTERENUMCB | acmFilterEnumCallback |
//          <f acmFilterEnumCallback>  is a placeholder for an application-
//          supplied function name and refers to the callback function used for
//          Audio Compression Manager (ACM) wave filter detail enumeration.
//
//  @parm   HACMDRIVERID | hadid | Specifies an ACM driver identifier.
//
//  @parm   LPACMFILTERDETAILS | pafd | Specifies a pointer to an
//          <t ACMFILTERDETAILS> structure that contains the enumerated
//          filter details for a filter tag.
//
//  @parm   DWORD | dwInstance | Specifies the application-defined value
//          specified in the <f acmFilterEnum> function.
//
//  @parm   DWORD | fdwSupport | Specifies driver-support flags specific to
//          the driver identifier <p hadid> for the specified filter. These flags
//          are identical to the <e ACMDRIVERDETAILS.fdwSupport> flags of the
//          <t ACMDRIVERDETAILS> structure, but are specific to the filter that
//          is being enumerated. This argument can be a combination of the
//          following values and identifies which operations the driver supports
//          for the filter tag.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CODEC | Specifies that this driver
//          supports conversion between two different format tags while using
//          the specified filter. For example, if a driver supports compression
//          from WAVE_FORMAT_PCM to WAVE_FORMAT_ADPCM with the specifed
//          filter, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CONVERTER | Specifies that this
//          driver supports conversion between two different formats of the
//          same format tag while using the specified filter. For example, if a
//          driver supports resampling of WAVE_FORMAT_PCM with the specified
//          filter, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_FILTER | Specifies that this driver
//          supports a filter (modification of the data without changing any
//          of the format attributes). For example, if a driver supports volume
//          or echo operations on WAVE_FORMAT_PCM, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_ASYNC | Specifies that this driver
//          supports asynchronous conversions with the specified filter tag.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_HARDWARE | Specifies that this driver
//          supports hardware input and/or output with the specified filter
//          through a waveform device. An application should use <f acmMetrics>
//          with the ACM_METRIC_HARDWARE_WAVE_INPUT and
//          ACM_METRIC_HARDWARE_WAVE_OUTPUT metric indexes to get the waveform
//          device identifiers associated with the supporting ACM driver.
//
//  @rdesc  The callback function must return TRUE to continue enumeration;
//          to stop enumeration, it must return FALSE.
//
//  @comm   The <f acmFilterEnum> function will return MMSYSERR_NOERROR
//          (zero) if no filters are to be enumerated. Moreover, the callback
//          function will not be called.
//
//  @xref   <f acmFilterEnum> <f acmFilterTagDetails> <f acmDriverOpen>
//
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmFilterEnum | The <f acmFilterEnum> function
//          enumerates wave filters available for a given filter tag from
//          an Audio Compression Manager (ACM) driver. The <f acmFilterEnum>
//          function continues enumerating until there are no more suitable
//          filters for the filter tag or the callback function returns FALSE.
//
//  @parm   HACMDRIVER | had | Optionally specifies an ACM driver to query
//          for wave filter details. If this argument is NULL, then the
//          ACM uses the details from the first suitable ACM driver.
//
//  @parm   LPACMFILTERDETAILS | pafd | Specifies a pointer to the
//          <t ACMFILTERDETAILS> structure that is to receive the filter details
//          passed to the <p fnCallback> function. This structure must have the
//          <e ACMFILTERDETAILS.cbStruct>, <e ACMFILTERDETAILS.pwfltr>, and
//          <e ACMFILTERDETAILS.cbwfltr> members of the <t ACMFILTERDETAILS>
//          structure initialized. The <e ACMFILTERDETAILS.dwFilterTag> member
//          must also be initialized to either WAVE_FILTER_UNKNOWN or a
//          valid filter tag.
//
//  @parm   ACMFILTERENUMCB | fnCallback | Specifies the address of the
//          application-defined callback function.
//
//  @parm   DWORD | dwInstance | Specifies a 32-bit, application-defined value
//          that is passed to the callback function along with ACM filter details.
//
//  @parm   DWORD | fdwEnum | Specifies flags for enumerating the filters for
//          a given filter tag.
//
//          @flag ACM_FILTERENUMF_DWFILTERTAG | Specifies that the
//          <e WAVEFILTER.dwFilterTag> member of the <t WAVEFILTER> structure
//          referred to by the <e ACMFILTERDETAILS.pwfltr> member of the
//          <t ACMFILTERDETAILS> structure is valid. The enumerator will only
//          enumerate a filter that conforms to this attribute. Note that the
//          <e ACMFILTERDETAILS.dwFilterTag> member of the <t ACMFILTERDETAILS>
//          structure must be equal to the <e WAVEFILTER.dwFilterTag> member.
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
//          @flag ACMERR_NOTPOSSIBLE | The details for the filter cannot be
//          returned.
//
//  @comm   The <f acmFilterEnum> function will return MMSYSERR_NOERROR
//          (zero) if no suitable ACM drivers are installed. Moreover, the
//          callback function will not be called.
//
//  @xref   <f acmFilterEnumCallback> <f acmFilterDetails> <f acmFilterSuggest>
//          <f acmFilterTagDetails> <f acmFormatEnum>
//
//------------------------------------------------------------------------------
MMRESULT
acmFilterEnum(
    HACMDRIVER          had,
    LPACMFILTERDETAILS  pafd,
    ACMFILTERENUMCB     fnCallback,
    DWORD               dwInstance,
    DWORD               fdwEnum
    )
{
    MMRESULT            mmRet;
    PACMDRIVER          pad;
    PACMDRIVERID        padid;
    HACMDRIVERID        hadid;
    PACMDRIVERID        padidCur;
    PACMDRIVERID        padidBestCount;
    DWORD               cBestCount;
    HACMDRIVERID        hadidCur;
    ACMFILTERTAGDETAILS aftd;
    UINT                u;
    UINT                uBestCount;
    UINT                uIndex;
    UINT                uFilterTag;
    BOOL                fDone;
    BOOL                fStop;

    FUNC("acmFilterEnum");

    VE_COND_PARAM(sizeof(ACMFILTERDETAILS) > pafd->cbStruct);

    if (NULL != had)
        VE_HANDLE(had, TYPE_HACMDRIVER);

    VE_CALLBACK(fnCallback);
    VE_FLAGS(fdwEnum, ACM_FILTERENUMF_VALID);
    VE_COND_PARAM(sizeof(WAVEFILTER) > pafd->cbwfltr);
    VE_WPOINTER(pafd->pwfltr, pafd->cbwfltr);
// BUGBUG is this needed?
//    VE_COND_PARAM(0L != pafd->fdwSupport);

    //
    //  Get the restrictions on the enum.
    //
    if( fdwEnum & ACM_FILTERENUMF_DWFILTERTAG ) {
        uFilterTag = (UINT)(pafd->pwfltr->dwFilterTag);
        VE_COND_PARAM(WAVE_FILTER_UNKNOWN == uFilterTag);
    } else {
        uFilterTag = WAVE_FILTER_UNKNOWN;
    }

    mmRet = MMSYSERR_NOERROR;

    if (NULL != had) {
        pad   = (PACMDRIVER)   had;
        padid = (PACMDRIVERID) pad->hadid;

        //
        //  step through all Filter tags that the caller is interested in
        //  and enumerate the Filters...
        //
        for (u = 0; u < padid->cFilterTags; u++)
        {
            if( fdwEnum & ACM_FILTERENUMF_DWFILTERTAG ) {
                if (uFilterTag != padid->paFilterTagCache[u].dwFilterTag)
                    continue;
            }

            aftd.cbStruct = sizeof(aftd);
            aftd.dwFilterTagIndex = u;
            mmRet = IFilterTagDetails((HACMDRIVERID)padid,
                                     &aftd, ACM_FILTERTAGDETAILSF_INDEX );

            if (MMSYSERR_NOERROR == mmRet) {
                mmRet = IFilterEnum( pad->hadid,
                                     &aftd,
                                     pafd,
                                     fnCallback,
                                     dwInstance
                                     );

            }

            if( mmRet == MMSYSERR_ERROR ) {
                // Returned generic error to stop the enumeration.
                mmRet = MMSYSERR_NOERROR;
                break;
            }
            if( fdwEnum & ACM_FILTERENUMF_DWFILTERTAG ) {
                break;
            }
        }
    } else if( fdwEnum & ACM_FILTERENUMF_DWFILTERTAG ) {
        hadid = NULL;
        fDone = FALSE;
        padidBestCount = NULL;
        cBestCount     = 0;

        ENTER_LIST_SHARED();

        while( !IDriverGetNext(&hadid, hadid, 0L) ) {
            padid = (PACMDRIVERID)hadid;

            //
            //  find the Filter tag that the caller is interested in and
            //  enumerate the Filters...
            //
            for (u = 0; u < padid->cFilterTags; u++)
            {
                if (uFilterTag != padid->paFilterTagCache[u].dwFilterTag)
                    continue;

                aftd.cbStruct = sizeof(aftd);
                aftd.dwFilterTagIndex = u;
                mmRet = IFilterTagDetails((HACMDRIVERID)padid,
                                         &aftd, ACM_FILTERTAGDETAILSF_INDEX );

                if( !padidBestCount ||
                    (aftd.cStandardFilters > cBestCount) ) {
                    padidBestCount = padid;
                    uBestCount = u;
                    cBestCount = aftd.cStandardFilters;
                }
                break;
            }
        }

        if( padidBestCount ) {

            aftd.cbStruct = sizeof(aftd);
            aftd.dwFilterTagIndex = uBestCount;
            mmRet = IFilterTagDetails((HACMDRIVERID)padidBestCount,
                                     &aftd, ACM_FILTERTAGDETAILSF_INDEX );

            if (MMSYSERR_NOERROR == mmRet) {
                mmRet = IFilterEnum( (HACMDRIVERID)padidBestCount,
                                     &aftd,
                                     pafd,
                                     fnCallback,
                                     dwInstance
                                     );
            }

            if( mmRet == MMSYSERR_ERROR ) {
                // Returned generic error to stop the enumeration.
                mmRet = MMSYSERR_NOERROR;
            }
            fDone = TRUE;
        }

        LEAVE_LIST_SHARED();

    } else {
        // Enum Filters across all drivers.

        fStop = FALSE;
        hadidCur = NULL;

        ENTER_LIST_SHARED();

        while( !fStop && !IDriverGetNext(&hadidCur, hadidCur, 0L)) {
            padidCur = (PACMDRIVERID)hadidCur;

            for (uIndex = 0; (uIndex < padidCur->cFilterTags)
                              && !fStop; uIndex++) {
                uFilterTag =
                        (UINT)(padidCur->paFilterTagCache[uIndex].dwFilterTag);
                fDone = FALSE;
                hadid = NULL;
                while (!fDone && !IDriverGetNext(&hadid, hadid, 0L)) {

                    //
                    //  same driver ?
                    //
                    if (hadid == hadidCur)
                        break;


                    //
                    //  for every previous driver
                    //
                    padid = (PACMDRIVERID)hadid;

                    for (u = 0; u < padid->cFilterTags; u++) {
                        //
                        //  for every FilterTag in the driver
                        //
                        if (uFilterTag ==
                                padid->paFilterTagCache[u].dwFilterTag) {
                            //
                            //  we have a match, but this was already given.
                            //
                            fDone = TRUE;
                            break;
                        }
                    }
                }

                if (!fDone) {
                    //
                    //  we have a Filter that has not been sent yet.
                    //
                    aftd.cbStruct = sizeof(aftd);
                    aftd.dwFilterTagIndex = uIndex;
                    mmRet = IFilterTagDetails(hadid, &aftd,
                                             ACM_FILTERTAGDETAILSF_INDEX );

                    if (MMSYSERR_NOERROR == mmRet)
                    {
                        mmRet = IFilterEnum( hadid,
                                             &aftd,
                                             pafd,
                                             fnCallback,
                                             dwInstance
                                             );
                    }

                    if( mmRet == MMSYSERR_ERROR ) {
                        // Returned generic error to stop the enumeration.
                        mmRet = MMSYSERR_NOERROR;
                        fStop = TRUE;
                        break;
                    }
                }
            }
        }

        LEAVE_LIST_SHARED();
    }

EXIT:
    FUNC_EXIT();
    return(mmRet);
}

