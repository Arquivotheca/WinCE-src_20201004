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
//    acmdrvr.c
//
//    Copyright (c) 1991-2000 Microsoft Corporation
//
//    This module provides ACM driver add/remove/enumeration
//
//------------------------------------------------------------------------------
#include "acmi.h"

//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API_OVERVIEWS
//
//  @topic  Windows CE implementation of the Win32 Audio Compression Manager |
//          There are a few ACM functions that have some minor limitations in
//          the Windows CE implementation of the Audio Compression Manager.
//
//          The most important difference is the loading mechanism for ACM
//          drivers. All ACM drivers (CODEC's, converters, and filters) are
//          GLOBAL. Individual apps may not install drivers as LOCAL. All
//          drivers are loaded by DEVICE.EXE on startup, thus <f acmDriverAdd>
//          and <f acmDriverRemove> can only be used to install a notification
//          window to receive broadcast ACM messages.
//
//          The second difference is in the chooser dialogs that are displayed
//          by the ACM. Windows CE does not support hooking the window
//          procedure of these dialogs nor custom dialog templates. It also
//          does not use custom friendly names for formats.
//
//  @xref   <f acmDriverAdd> <f acmDriverRemove>
//          <f acmFormatChoose> <f acmFilterChoose> <f acmMetrics>
//
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmDriverID | Returns the handle to an Audio Compression
//          Manager (ACM) driver identifier associated with an open ACM driver
//          instance or stream handle. <t HACMOBJ> is the handle to an ACM
//          object, such as an open <t HACMDRIVER> or <t HACMSTREAM>.
//
//  @parm   HACMOBJ | hao | Specifies the open driver instance or stream
//          handle.
//
//  @parm   LPHACMDRIVERID | phadid | Specifies a pointer to an <t HACMDRIVERID>
//          handle. This location is filled with a handle identifying the
//          installed driver that is associated with the <p hao>.
//
//  @parm   DWORD | fdwDriverID | This argument is not used and must be set to
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
//  @xref   <f acmDriverDetails> <f acmDriverOpen>
//
//------------------------------------------------------------------------------
MMRESULT
acmDriverID(
    HACMOBJ                 hao,
    LPHACMDRIVERID          phadid,
    DWORD                   fdwDriverID
    )
{
    MMRESULT mmRet = MMSYSERR_NOERROR;

    FUNC("acmDriverID");

    VE_HANDLE(hao, TYPE_HACMOBJ);
    VE_WPOINTER(phadid, sizeof(HACMDRIVERID));
    VE_FLAGS(fdwDriverID, ACM_DRIVERIDF_VALID);

    switch (((ACMOBJ *)hao)->uHandleType) {

        case TYPE_HACMDRIVERID:
            VE_HANDLE(hao, TYPE_HACMDRIVERID);
            *phadid = (HACMDRIVERID) hao;
            break;

        case TYPE_HACMDRIVER:
            VE_HANDLE(hao, TYPE_HACMDRIVER);
            *phadid = ((PACMDRIVER)hao)->hadid;
            break;

        case TYPE_HACMSTREAM:
            VE_HANDLE(hao, TYPE_HACMSTREAM);
            *phadid = ((PACMDRIVER) ((PACMSTREAM) hao)->had)->hadid;
            break;

        default:
            mmRet = MMSYSERR_INVALPARAM;
            goto EXIT;
    }

EXIT:
    FUNC_EXIT();
    return(mmRet);
}



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   LRESULT CALLBACK | acmDriverProc | The <f acmDriverProc> function
//          is a placeholder for an application-defined function name, and
//          refers to the callback function used with the ACM.
//
//  @parm   DWORD | dwID | Specifies an identifier of the installable ACM
//          driver.
//
//  @parm   HDRIVER | hdrvr | Identifies the installable ACM driver. This
//          argument is a unique handle the ACM assigns to the driver.
//
//  @parm   UINT | uMsg | Specifies an ACM driver message.
//
//  @parm   LPARAM | lParam1 | Specifies the first message parameter.
//
//  @parm   LPARAM | lParam2 | Specifies the second message parameter.
//
//  @xref   <f acmDriverAdd> <f acmDriverRemove> <f acmDriverDetails>
//          <f acmDriverOpen> <f DriverProc>
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmDriverAdd | Adds a window to the list of windows
//          to receive ACM notification messages.
//
//  @parm   LPHACMDRIVERID | phadid | Specifies a pointer to an <t HACMDRIVERID>
//          handle. This location is filled with a handle identifying the
//          installed driver. Use the handle to identify the driver when
//          calling other ACM functions.
//
//  @parm   HINSTANCE | hinstModule | This parameter is ignored for Windows CE.
//
//  @parm   LPARAM | lParam | Notification window handle.
//
//  @parm   DWORD | dwPriority | This parameter is only used with
//          the ACM_DRIVERADDF_NOTIFYHWND flag to specify the window message
//          to send for notification broadcasts.
//
//  @parm   DWORD | fdwAdd | Specifies flags for adding ACM drivers.
//
//          @flag ACM_DRIVERADDF_NOTIFYHWND | Specifies that <p lParam> is a
//          notification window handle to receive messages when changes to
//          global driver priorities and states are made. The window message
//          to receive is defined by the application and must be passed in
//          the <p dwPriority> argument. The <p wParam> and <p lParam> arguments
//          passed with the window message are reserved for future use and
//          should be ignored.
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag MMSYSERR_INVALPARAM | One or more arguments passed is invalid.
//
//          @flag MMSYSERR_NOMEM | Unable to allocate resources.
//
//  @comm   For Windows CE, all ACM drivers are GLOBAL and cannot be added
//          with the <f acmDriverAdd> function. ACM Drivers must be loaded at
//          boot time by DEVICE.EXE. See the Windows CE DDK for more
//          information on ACM driver generation/installation/
//
//  @xref   <f acmDriverProc> <f acmDriverRemove> <f acmDriverDetails>
//          <f acmDriverOpen>
//
//------------------------------------------------------------------------------
MMRESULT
acmDriverAdd(
    LPHACMDRIVERID          phadid,
    HINSTANCE               hinstModule,
    LPARAM                  lParam,
    DWORD                   dwPriority,
    DWORD                   fdwAdd
    )
{
    return(MMSYSERR_NOTSUPPORTED);
}



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmDriverRemove | Removes a window handle
//          from the list of ACM notification window handles.
//
//  @parm   HACMDRIVERID | hadid | Handle to the driver identifier to be
//          removed.
//
//  @parm   DWORD | fdwRemove | This argument is not used and must be set to
//          zero.
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag ACMERR_BUSY | The driver is in use and cannot be removed.
//
//  @xref   <f acmDriverAdd>
//
//------------------------------------------------------------------------------
MMRESULT
acmDriverRemove(
    HACMDRIVERID            hadid,
    DWORD                   fdwRemove
    )
{
    return(MMSYSERR_NOTSUPPORTED);
}



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   BOOL ACMDRIVERENUMCB | acmDriverEnumCallback | The
//          <f acmDriverEnumCallback> function is a placeholder for an
//          application-defined function name, and refers to the callback function
//          used with <f acmDriverEnum>.
//
//  @parm   HACMDRIVERID | hadid | Specifies an ACM driver identifier.
//
//  @parm   DWORD | dwInstance | Specifies the application-defined value
//          specified in the <f acmDriverEnum> function.
//
//  @parm   DWORD | fdwSupport | Specifies driver-support flags specific to
//          the driver identifier <p hadid>. These flags are identical to the
//          <e ACMDRIVERDETAILS.fdwSupport> flags of the <t ACMDRIVERDETAILS>
//          structure. This argument can be a combination of the following
//          values:
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CODEC | Specifies that this driver
//          supports conversion between two different format tags. For example,
//          if a driver supports compression from WAVE_FORMAT_PCM to
//          WAVE_FORMAT_ADPCM, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CONVERTER | Specifies that this
//          driver supports conversion between two different formats of the
//          same format tag. For example, if a driver supports resampling of
//          WAVE_FORMAT_PCM, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_FILTER | Specifies that this driver
//          supports a filter (modification of the data without changing any
//          of the format attributes). For example, if a driver supports volume
//          or echo operations on WAVE_FORMAT_PCM, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_ASYNC | Specifies that this driver
//          supports asynchronous conversions.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_DISABLED | Specifies that this
//          driver has been disabled. An application must specify the
//          ACM_DRIVERENUMF_DISABLED to the <f acmDriverEnum> function to
//          include disabled drivers in the enumeration.
//
//  @rdesc  The callback function must return TRUE to continue enumeration;
//          to stop enumeration, it must return FALSE.
//
//  @comm   The <f acmDriverEnum> function will return MMSYSERR_NOERROR (zero)
//          if no ACM drivers are installed. Moreover, the callback function will
//          not be called.
//
//  @xref   <f acmDriverEnum> <f acmDriverDetails> <f acmDriverOpen>
//
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmDriverEnum | The <f acmDriverEnum> function enumerates
//          the available Audio Compression Manager (ACM) drivers, continuing
//          until there are no more ACM drivers or the callback function returns FALSE.
//
//  @parm   ACMDRIVERENUMCB | fnCallback | Specifies the procedure-instance
//          address of the application-defined callback function.
//
//  @parm   DWORD | dwInstance | Specifies a 32-bit application-defined value
//          that is passed to the callback function along with ACM driver
//          information.
//
//  @parm   DWORD | fdwEnum | Specifies flags for enumerating ACM drivers.
//
//          @flag ACM_DRIVERENUMF_DISABLED | Specifies that disabled ACM drivers
//          should be included in the enumeration. If a driver is
//          disabled, the <p fdwSupport> argument to the callback function will
//          have the ACMDRIVERDETAILS_SUPPORTF_DISABLED flag set.
//
//  @rdesc  Returns zero if the function was successful. Otherwise, it returns
//          a non-zero error number. Possible error returns are:
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag MMSYSERR_INVALPARAM | One or more arguments passed are invalid.
//
//  @comm   The <f acmDriverEnum> function will return MMSYSERR_NOERROR (zero)
//          if no ACM drivers are installed. Moreover, the callback function will
//          not be called.
//
//  @xref   <f acmDriverEnumCallback> <f acmDriverDetails> <f acmDriverOpen>
//
//------------------------------------------------------------------------------
MMRESULT
acmDriverEnum(
    ACMDRIVERENUMCB         fnCallback,
    DWORD                   dwInstance,
    DWORD                   fdwEnum
    )
{
    MMRESULT        mmRet;
    HACMDRIVERID    hadid;
    BOOL            fRet;
    DWORD           fdwSupport;

    FUNC("acmDriverEnum");
    HEXPARAM(fnCallback);
    HEXPARAM(dwInstance);
    HEXPARAM(fdwEnum);

    VE_CALLBACK(fnCallback);
    VE_FLAGS(fdwEnum, ACM_DRIVERENUMF_VALID);

    hadid = NULL;

    ENTER_LIST_SHARED();

    TESTMSG("about to enter IDriverGetNext");
    HEXVALUE(pag);
    HEXVALUE(&hadid);
    HEXVALUE(hadid);
    HEXVALUE(fdwEnum);
    while (!IDriverGetNext(&hadid, hadid, fdwEnum)) {

        mmRet = IDriverSupport(hadid, &fdwSupport, TRUE);
        if (MMSYSERR_NOERROR != mmRet) {
            continue;
        }

        //
        //  do the callback--if the client returns FALSE we need to
        //  terminate the enumeration process...
        // Note: we pass back the "true" handle, not our local pseudo-handle
        //
        fRet = (BOOL) wapi_DoFunctionCallback(
            fnCallback, 3, ((DWORD)hadid), dwInstance,fdwSupport, 0, 0);

        if (FALSE == fRet)
            break;
    }

    LEAVE_LIST_SHARED();

    mmRet = MMSYSERR_NOERROR;
EXIT:
    FUNC_EXIT();
    return(mmRet);
}



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API_STRUCTURE
//
//  @struct ACMDRIVERDETAILS | The <t ACMDRIVERDETAILS> structure describes
//          various details of an Audio Compression Manager (ACM) driver.
//
//  @field  DWORD | cbStruct | Specifies the size, in bytes,  of the valid
//          information contained in the <t ACMDRIVERDETAILS> structure.
//          An application should initialize this member to the size, in bytes, of
//          the desired information. The size specified in this member must be
//          large enough to contain the <e ACMDRIVERDETAILS.cbStruct> member of
//          the <t ACMDRIVERDETAILS> structure. When the <f acmDriverDetails>
//          function returns, this member contains the actual size of the
//          information returned. The returned information will never exceed
//          the requested size.
//
//  @field  FOURCC | fccType | Specifies the type of the driver. For ACM drivers, set
//          this member  to <p audc>, which represents ACMDRIVERDETAILS_FCCTYPE_AUDIOCODEC.
//
//  @field  FOURCC | fccComp | Specifies the sub-type of the driver. This
//          member is currently set to ACMDRIVERDETAILS_FCCCOMP_UNDEFINED (zero).
//
//  @field  WORD | wMid | Specifies a manufacturer ID for the ACM driver.
//
//  @field  WORD | wPid | Specifies a product ID for the ACM driver.
//
//  @field  DWORD | vdwACM | Specifies the version of the ACM for which
//          this driver was compiled. The version number is a hexadecimal number
//          in the format 0xAABBCCCC, where AA is the major version number,
//          BB is the minor version number, and CCCC is the build number.
//          Note that the version parts (major, minor, and build) should be
//          displayed as decimal numbers.
//
//  @field  DWORD | vdwDriver | Specifies the version of the driver.
//          The version number is a hexadecimal number in the format 0xAABBCCCC, where
//          AA is the major version number, BB is the minor version number,
//          and CCCC is the build number. Note that the version parts (major,
//          minor, and build) should be displayed as decimal numbers.
//
//  @field  DWORD | fdwSupport | Specifies support flags for the driver.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CODEC | Specifies that this driver
//          supports conversion between two different format tags. For example,
//          if a driver supports compression from WAVE_FORMAT_PCM to
//          WAVE_FORMAT_ADPCM, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_CONVERTER | Specifies that this
//          driver supports conversion between two different formats of the
//          same format tag. For example, if a driver supports resampling of
//          WAVE_FORMAT_PCM, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_FILTER | Specifies that this driver
//          supports a filter (modification of the data without changing any
//          of the format attributes). For example, if a driver supports volume
//          or echo operations on WAVE_FORMAT_PCM, then this flag is set.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_ASYNC | Specifies that this driver
//          supports asynchronous conversions.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_HARDWARE | Specifies that this driver
//          supports hardware input and/or output through a waveform device. An
//          application should use <f acmMetrics> with the
//          ACM_METRIC_HARDWARE_WAVE_INPUT and ACM_METRIC_HARDWARE_WAVE_OUTPUT
//          metric indexes to get the waveform device identifiers associated with
//          the supporting ACM driver.
//
//          @flag ACMDRIVERDETAILS_SUPPORTF_DISABLED | Specifies that this driver
//          has been disabled. This flag is set by the ACM for a driver when
//          it has been disabled for any of a number of reasons. Disabled
//          drivers cannot be opened and can only be used under very limited
//          circumstances.
//
//  @field  DWORD | cFormatTags | Specifies the number of unique format tags
//          supported by this driver.
//
//  @field  DWORD | cFilterTags | Specifies the number of unique filter tags
//          supported by this driver.
//
//  @field  HICON | hicon | Specifies an optional handle to a custom icon for
//          this driver. An application can use this icon for referencing the
//          driver visually. This member can be NULL.
//
//  @field  char | szShortName[ACMDRIVERDETAILS_SHORTNAME_CHARS] | Specifies
//          a NULL-terminated string that describes the name of the driver. This
//          string is intended to be displayed in small spaces.
//
//  @field  char | szLongName[ACMDRIVERDETAILS_LONGNAME_CHARS] | Specifies a
//          NULL-terminated string that describes the full name of the driver.
//          This string is intended to be displayed in large (descriptive)
//          spaces.
//
//  @field  char | szCopyright[ACMDRIVERDETAILS_COPYRIGHT_CHARS] | Specifies
//          a NULL-terminated string that provides copyright information for the
//          driver.
//
//  @field  char | szLicensing[ACMDRIVERDETAILS_LICENSING_CHARS] | Specifies a
//          NULL-terminated string that provides special licensing information
//          for the driver.
//
//  @field  char | szFeatures[ACMDRIVERDETAILS_FEATURES_CHARS] | Specifies a
//          NULL-terminated string that provides special feature information for
//          the driver.
//
//  @xref   <f acmDriverDetails>
//
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmDriverDetails | This function queries a specified
//          Audio Compression Manager (ACM) driver to determine its capabilities.
//
//  @parm   HACMDRIVERID | hadid | Handle to the driver identifier of an
//          installed ACM driver. Disabled drivers can be queried for details.
//
//  @parm   LPACMDRIVERDETAILS | padd | Pointer to an <t ACMDRIVERDETAILS>
//          structure that will receive the driver details. The
//          <e ACMDRIVERDETAILS.cbStruct> member must be initialized to the
//          size, in bytes,  of the structure.
//
//  @parm   DWORD | fdwDetails | This argument is not used and must be set to
//          zero.
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
//  @xref   <f acmDriverAdd> <f acmDriverEnum> <f acmDriverID>
//          <f acmDriverOpen>
//
//------------------------------------------------------------------------------
MMRESULT
acmDriverDetails(
    HACMDRIVERID            hadid,
    LPACMDRIVERDETAILS      padd,
    DWORD                   fdwDetails
    )
{
    MMRESULT mmRet = MMSYSERR_NOERROR;

    FUNC("acmDriverDetails");

    //
    //  note that we allow both HACMDRIVERID's and HACMDRIVER's
    //
    VE_HANDLE(hadid, TYPE_HACMOBJ);

    if (TYPE_HACMDRIVER == ((PACMDRIVERID)hadid)->uHandleType) {
        hadid = ((PACMDRIVER)hadid)->hadid;
    }
    VE_HANDLE(hadid, TYPE_HACMDRIVERID);

    // check that ACMDRIVERDETAILS argument is a valid pointer
    // and that the cbStruct member is >= to the sizeof(ACMDRIVERDETAILS).
    VE_WPOINTER(padd, padd->cbStruct);
    VE_COND_PARAM(sizeof(ACMDRIVERDETAILS) > padd->cbStruct);

    VE_FLAGS(fdwDetails, ACM_DRIVERDETAILSF_VALID);

    //
    // The parameters have been validated and mapped, so do the call.
    //
    mmRet = IDriverDetails(hadid, padd, fdwDetails);

EXIT:
    FUNC_EXIT();
    return (mmRet);
}


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmDriverPriority | This function modifies the priority
//          and state of an Audio Compression Manager (ACM) driver.
//
//  @parm   HACMDRIVERID | hadid | Handle to the driver identifier of an
//          installed ACM driver. This argument must be NULL when specifying
//          the ACM_DRIVERPRIORITYF_BEGIN and ACM_DRIVERPRIORITYF_END flags.
//
//  @parm   DWORD | dwPriority | Specifies the new priority for a global
//          ACM driver identifier. A zero value specifies that the priority of
//          the driver identifier should remain unchanged. A value of one (1)
//          specifies that the driver should be placed as the highest search
//          priority driver. A value of (DWORD)-1 specifies that the driver
//          should be placed as the lowest search priority driver.
//
//  @parm   DWORD | fdwPriority | Specifies flags for setting priorities of
//          ACM drivers.
//
//          @flag ACM_DRIVERPRIORITYF_ENABLE | Specifies that the ACM driver
//          should be enabled if it is currently disabled. Enabling an already
//          enabled driver does nothing.
//
//          @flag ACM_DRIVERPRIORITYF_DISABLE | Specifies that the ACM driver
//          should be disabled if it is currently enabled. Disabling an already
//          disabled driver does nothing.
//
//          @flag ACM_DRIVERPRIORITYF_BEGIN | Specifies that the calling task
//          wants to defer change notification broadcasts. An application must
//          take care to reenable notification broadcasts as soon as possible
//          with the ACM_DRIVERPRIORITYF_END flag. Note that <p hadid> must be
//          NULL, <p dwPriority> must be zero, and only the
//          ACM_DRIVERPRIORITYF_BEGIN flag can be set.
//
//          @flag ACM_DRIVERPRIORITYF_END | Specifies that the calling task
//          wants to reenable change notification broadcasts. An application
//          must call <f acmDriverPriority> with ACM_DRIVERPRIORITYF_END for
//          each successful call with the ACM_DRIVERPRIORITYF_BEGIN flag. Note
//          that <p hadid> must be NULL, <p dwPriority> must be zero, and only
//          the ACM_DRIVERPRIORITYF_END flag can be set.
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
//          @flag MMSYSERR_ALLOCATED | Returned if the deferred broadcast lock
//          is owned by a different task.
//
//          @flag MMSYSERR_NOTSUPPORTED | The requested operation is not
//          supported for the specified driver. For example, local and notify
//          driver identifiers do not support priorities (but can be enabled
//          and disabled). This error will therefore be returned if an
//          application specifies a non-zero <p dwPriority> for a local and
//          notify driver identifiers.
//
//  @comm   All driver identifiers can be enabled and disabled; this includes
//          global and notification driver identifiers.
//
//          If more than one global driver identifier needs to be enabled,
//          disabled or shifted in priority, then an application should defer
//          change notification broadcasts using the ACM_DRIVERPRIORITYF_BEGIN
//          flag. A single change notification will be broadcast when the
//          ACM_DRIVERPRIORITYF_END flag is specified.
//
//          An application can use the <f acmMetrics> function with the
//          ACM_METRIC_DRIVER_PRIORITY metric index to retrieve the current
//          priority of a global driver. Also note that drivers are always
//          enumerated from highest to lowest priority by the <f acmDriverEnum>
//          function.
//
//          All enabled driver identifiers will receive change notifications.
//          An application can register a notification message using the
//          <f acmDriverAdd> function in conjunction with the
//          ACM_DRIVERADDF_NOTIFYHWND flag.
//
//          Note that priorities are simply used for the search order
//          when an application does not specify a driver. Boosting the
//          priority of a driver will have no effect on the performance of
//          a driver.
//
//  @xref   <f acmDriverAdd> <f acmDriverEnum> <f acmDriverDetails>
//          <f acmMetrics>
//
//------------------------------------------------------------------------------
MMRESULT
acmDriverPriority(
    HACMDRIVERID            hadid,
    DWORD                   dwPriority,
    DWORD                   fdwPriority
    )
{
    return(MMSYSERR_NOTSUPPORTED);
}


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmDriverOpen | This function opens the specified Audio
//          Compression Manager (ACM) driver and returns a driver-instance handle
//          that can be used to communicate with the driver.
//
//  @parm   LPHACMDRIVER | phad | Specifies a pointer to a <t HACMDRIVER>
//          handle that will receive the new driver instance handle that can
//          be used to communicate with the driver.
//
//  @parm   HACMDRIVERID | hadid | Handle to the driver identifier of an
//          installed and enabled ACM driver.
//
//  @parm   DWORD | fdwOpen | This argument is not used and must be set to
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
//          @flag MMSYSERR_NOTENABLED | The driver is not enabled.
//
//          @flag MMSYSERR_NOMEM | Unable to allocate resources.
//
//  @xref   <f acmDriverAdd> <f acmDriverEnum> <f acmDriverID>
//          <f acmDriverClose>
//
//------------------------------------------------------------------------------
MMRESULT
acmDriverOpen(
    LPHACMDRIVER            phad,
    HACMDRIVERID            hadid,
    DWORD                   fdwOpen
    )
{
    MMRESULT        mmRet = MMSYSERR_NOERROR;

    FUNC("acmDriverOpen");

    VE_WPOINTER(phad, sizeof(HACMDRIVER));
    VE_HANDLE(hadid, TYPE_HACMDRIVERID);
    VE_FLAGS(fdwOpen, ACM_DRIVEROPENF_VALID);

    *phad = NULL;

    LOCK_HANDLE(hadid);
    mmRet = IDriverOpen(phad, hadid, fdwOpen);
    UNLOCK_HANDLE(hadid);

EXIT:
    FUNC_EXIT();
    return(mmRet);
}


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmDriverClose | Closes a previously opened Audio
//          Compression Manager (ACM) driver instance. If the function is
//          successful, the handle is invalidated.
//
//  @parm   HACMDRIVER | had | Identifies the open driver instance to be
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
//          @flag ACMERR_BUSY | The driver is in use and cannot be closed.
//
//  @xref   <f acmDriverOpen>
//
//------------------------------------------------------------------------------
MMRESULT
acmDriverClose(
    HACMDRIVER              had,
    DWORD                   fdwClose
    )
{
    MMRESULT mmRet = MMSYSERR_NOERROR;
    HACMDRIVERID hadid;

    FUNC("acmDriverClose");

    VE_HANDLE(had, TYPE_HACMDRIVER);
    VE_FLAGS(fdwClose, ACM_DRIVERCLOSEF_VALID);

    hadid = ((PACMDRIVER)had)->hadid;
    EnterHandle(hadid);
    mmRet = IDriverClose(had, fdwClose);
    LeaveHandle(hadid);

EXIT:
    FUNC_EXIT();
    return(mmRet);
}


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   LRESULT | acmDriverMessage | This function sends a user-defined
//          message to a given Audio Compression Manager (ACM) driver instance.
//
//  @parm   HACMDRIVER | had | Specifies the ACM driver instance to which the
//          message will be sent.
//
//  @parm   UINT | uMsg | Specifies the message that the ACM driver must
//          process. This message must be in the <m ACMDM_USER> message range
//          (above or equal to <m ACMDM_USER> and less than
//          <m ACMDM_RESERVED_LOW>). The exception to this restriction is
//          the <m ACMDM_DRIVER_ABOUT> message.
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
//  @comm   The <f acmDriverMessage> function is provided to allow ACM driver-
//          specific messages to be sent to an ACM driver. The messages that
//          can be sent through this function must be above or equal to the
//          <m ACMDM_USER> message and less than <m ACMDM_RESERVED_LOW>. The
//          exceptions to this restriction are the <m ACMDM_DRIVER_ABOUT>,
//          <m DRV_QUERYCONFIGURE> and <m DRV_CONFIGURE> messages.
//
//          To display a custom About dialog box from an ACM driver,an application
//          must send the <m ACMDM_DRIVER_ABOUT> message to the
//          driver. The <p lParam1> argument should be the handle of the
//          owner window for the custom about box; <p lParam2> must be set to
//          zero. If the driver does not support a custom about box, then
//          MMSYSERR_NOTSUPPORTED will be returned and it is up to the calling
//          application to display its own dialog box. An application can
//          query a driver for custom about box support without the dialog
//          box being displayed by setting <p lParam1> to -1L. If the driver
//          supports a custom about box, then MMSYSERR_NOERROR will be
//          returned. Otherwise, the return value is MMSYSERR_NOTSUPPORTED.
//
//          User-defined messages must only be sent to an ACM driver that
//          specifically supports the messages. The caller should verify that
//          the ACM driver is in fact the correct driver by getting the
//          driver details and checking the <e ACMDRIVERDETAILS.wMid>,
//          <e ACMDRIVERDETAILS.wPid>, and <e ACMDRIVERDETAILS.vdwDriver> members.
//
//          Never send user-defined messages to an unknown ACM driver.
//
//  @xref   <f acmDriverOpen> <f acmDriverDetails>
//
// ***************************************************************************/
LRESULT
acmDriverMessage(
    HACMDRIVER              had,
    UINT                    uMsg,
    LPARAM                  lParam1,
    LPARAM                  lParam2
    )
{
    MMRESULT            mmRet;
    BOOL                fAllowDriverId;

    FUNC("acmDriverMessage");

    //
    //  assume no driver id allowed
    //
    fAllowDriverId = FALSE;

    //
    //  do not allow non-user range messages through!
    //
    //  we have to allow ACMDM_DRIVER_ABOUT through because we define no
    //  other interface to bring up the about box for a driver. so special
    //  case this message and validate the arguments for it...
    //
    //  we also have to allow DRV_QUERYCONFIGURE and DRV_CONFIGURE through.
    //
// BUGBUG what goes here?
    if ((uMsg < ACMDM_USER) || (uMsg >= ACMDM_RESERVED_LOW)) {
        switch (uMsg) {

            case DRV_QUERYCONFIGURE:
                VE_COND_PARAM((0L != lParam1) || (0L != lParam2));
                fAllowDriverId = TRUE;
                break;

            case DRV_CONFIGURE:
                VE_COND((0L != lParam1) && !IsWindow((HWND)(UINT)lParam1), DRVCNF_CANCEL);

                VE_COND(0L != lParam2, DRVCNF_CANCEL);

                if (!acm_IsValidHandle(had, TYPE_HACMOBJ)) {
                    GOTO_EXIT(mmRet = DRVCNF_CANCEL);
                }

                EnterHandle(had);
                mmRet = IDriverConfigure((HACMDRIVERID)had, (HWND)(UINT)lParam1);
                LeaveHandle(had);
                goto EXIT;

            case ACMDM_DRIVER_ABOUT:
                VE_COND_HANDLE( (-1L != lParam1) &&
                                ( 0L != lParam1) &&
                                !IsWindow((HWND)(UINT)lParam1));

                VE_COND_PARAM(0L != lParam2);
                fAllowDriverId = TRUE;
                break;

            default:
                TESTMSG("acmDriverMessage: non-user range messages are not allowed.");
                GOTO_EXIT(mmRet = MMSYSERR_INVALPARAM)
        }
    }


    //
    //  validate handle as an HACMOBJ. this API can take an HACMDRIVERID
    //  as well as an HACMDRIVER. an HACMDRIVERID can only be used with
    //  the following messages:
    //
    //      DRV_QUERYCONFIGURE
    //      DRV_CONFIGURE
    //      ACMDM_DRIVER_ABOUT
    //
    VE_HANDLE(had, TYPE_HACMOBJ);

    if (TYPE_HACMDRIVER == ((PACMDRIVER)had)->uHandleType) {
        EnterHandle(had);
        mmRet = IDriverMessage(had, uMsg, lParam1, lParam2);
        LeaveHandle(had);
        goto EXIT;
    }

    if (!fAllowDriverId)
        VE_HANDLE(had, TYPE_HACMDRIVER);

    VE_HANDLE(had, TYPE_HACMDRIVERID);

    EnterHandle(had);
    mmRet = (MMRESULT) IDriverMessageId((HACMDRIVERID)had, uMsg, lParam1, lParam2);
    LeaveHandle(had);

EXIT:
    FUNC_EXIT();
    return ((LRESULT) mmRet);
}

typedef DWORD (* FCB_t)(DWORD, ...);

DWORD
wapi_DoFunctionCallback(
    PVOID  pfnCallback,
    DWORD  dwNumParams,
    DWORD  dwParam1,
    DWORD  dwParam2,
    DWORD  dwParam3,
    DWORD  dwParam4,
    DWORD  dwParam5
    )
{
    DWORD dwRet = FALSE;
    FCB_t pFn;

    FUNC("wapi_DoFunctionCallback");

    pFn = (FCB_t) pfnCallback;

    //
    // This is ugly, but I'll look at beautifying later...
    //
    __try {
        switch (dwNumParams) {
            case 3:
                dwRet = pFn(dwParam1, dwParam2, dwParam3);
                break;

            case 4:
                dwRet = pFn(dwParam1, dwParam2, dwParam3, dwParam4);
                break;

            case 5:
                dwRet = pFn(dwParam1, dwParam2, dwParam3, dwParam4, dwParam5);
                break;

            default:
                ERRMSG("WAVEAPI.C (function callback): What the ?!?\r\n");
                dwRet = FALSE;
                break;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        ERRMSG("Renegade WAVEAPI.DLL function callback! Recovering...");
    }

    FUNC_EXIT();
    return(dwRet);
}


