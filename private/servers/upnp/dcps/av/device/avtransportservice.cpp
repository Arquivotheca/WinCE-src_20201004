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

#include "av_dcp.h"
#include "av_upnp.h"
#include "av_upnp_device_internal.h"

using namespace av_upnp;

/////////////////////////////////////////////////////////////////////////////
// AVTransportServiceImpl

AVTransportServiceImpl::AVTransportServiceImpl()
    : VirtualServiceSupport<IAVTransport>(DISPID_LASTCHANGE_AVT)      
{
    InitErrDescrips();
    InitToolkitErrs();
}


AVTransportServiceImpl::~AVTransportServiceImpl()
{
}



//
// IUPnPService_AVTransport1
//

STDMETHODIMP AVTransportServiceImpl::get_LastChange(BSTR* pLastChange)
{
    return VirtualServiceSupport<IAVTransport>::GetLastChange(pLastChange);
}



// non-"get_LastChange()" get_foo() methods are implemented only to satisfy upnp's devicehost, they are not used

STDMETHODIMP AVTransportServiceImpl::get_TransportState(BSTR* pTransportState)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_TransportStatus(BSTR* pTransportStatus)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_PlaybackStorageMedium(BSTR* pPlaybackStorageMedium)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_RecordStorageMedium(BSTR* pRecordStorageMedium)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_PossiblePlaybackStorageMedia(BSTR* pPossiblePlaybackStorageMedia)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_PossibleRecordStorageMedia(BSTR* pPossibleRecordStorageMedia)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_CurrentPlayMode(BSTR* pCurrentPlayMode)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_TransportPlaySpeed(BSTR* pTransportPlaySpeed)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_RecordMediumWriteStatus(BSTR* pRecordMediumWriteStatus)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_CurrentRecordQualityMode(BSTR* pCurrentRecordQualityMode)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_PossibleRecordQualityModes(BSTR* pPossibleRecordQualityModes)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_NumberOfTracks(unsigned long* pNumberOfTracks)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_CurrentTrack(unsigned long* pCurrentTrack)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_CurrentTrackDuration(BSTR* pCurrentTrackDuration)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_CurrentMediaDuration(BSTR* pCurrentMediaDuration)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_CurrentTrackMetaData(BSTR* pCurrentTrackMetaData)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_CurrentTrackURI(BSTR* pCurrentTrackURI)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_AVTransportURI(BSTR* pAVTransportURI)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_AVTransportURIMetaData(BSTR* pAVTransportURIMetaData)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_NextAVTransportURI(BSTR* pNextAVTransportURI)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_NextAVTransportURIMetaData(BSTR* pNextAVTransportURIMetaData)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_RelativeTimePosition(BSTR* pRelativeTimePosition)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_AbsoluteTimePosition(BSTR* pAbsoluteTimePosition)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_RelativeCounterPosition(long* pRelativeCounterPosition)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_AbsoluteCounterPosition(long* pAbsoluteCounterPosition)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_CurrentTransportActions(BSTR* pCurrentTransportActions)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_A_ARG_TYPE_SeekMode(BSTR* pA_ARG_TYPE_SeekMode)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_A_ARG_TYPE_SeekTarget(BSTR* pA_ARG_TYPE_SeekTarget)
{
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::get_A_ARG_TYPE_InstanceID(unsigned long* pA_ARG_TYPE_InstanceID)
{
    return S_OK;
}



STDMETHODIMP AVTransportServiceImpl::SetAVTransportURI(unsigned long InstanceID,
                                                       BSTR CurrentURI,
                                                       BSTR CurrentURIMetaData)
{
    MAKE_INSTANCE_CALL_2(SetAVTransportURI, CurrentURI, CurrentURIMetaData);
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::SetNextAVTransportURI(unsigned long InstanceID,
                                                           BSTR NextURI,
                                                           BSTR NextURIMetaData)
{
    MAKE_INSTANCE_CALL_2(SetNextAVTransportURI, NextURI, NextURIMetaData);
    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::GetMediaInfo(unsigned long InstanceID,
                                                  unsigned long* pNrTracks,
                                                  BSTR* pMediaDuration,
                                                  BSTR* pCurrentURI,
                                                  BSTR* pCurrentURIMetaData,
                                                  BSTR* pNextURI,
                                                  BSTR* pNextURIMetaData,
                                                  BSTR* pPlayMedium,
                                                  BSTR* pRecordMedium,
                                                  BSTR* pWriteStatus)
{
    if(!pNrTracks || !pMediaDuration || !pCurrentURI || !pCurrentURIMetaData || !pNextURI
       || !pNextURIMetaData || !pPlayMedium || !pRecordMedium || !pWriteStatus)
    {
        return E_POINTER;
    }


    MediaInfo mediaInfo;

    MAKE_INSTANCE_CALL_1(GetMediaInfo, &mediaInfo);


    // Set out args

    *pNrTracks = mediaInfo.nNrTracks;

    if(   !(*pMediaDuration      = SysAllocString(mediaInfo.strMediaDuration))
       || !(*pCurrentURI         = SysAllocString(mediaInfo.strCurrentURI))
       || !(*pCurrentURIMetaData = SysAllocString(mediaInfo.strCurrentURIMetaData))
       || !(*pNextURI            = SysAllocString(mediaInfo.strNextURI))
       || !(*pNextURIMetaData    = SysAllocString(mediaInfo.strNextURIMetaData))
       || !(*pPlayMedium         = SysAllocString(mediaInfo.strPlayMedium))
       || !(*pRecordMedium       = SysAllocString(mediaInfo.strRecordMedium))
       || !(*pWriteStatus        = SysAllocString(mediaInfo.strWriteStatus))   )
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::GetTransportInfo(unsigned long InstanceID,
                                                      BSTR* pCurrentTransportState,
                                                      BSTR* pCurrentTransportStatus,
                                                      BSTR* pCurrentSpeed)
{
    if(!pCurrentTransportState || !pCurrentTransportStatus || !pCurrentSpeed)
    {
        return E_POINTER;
    }


    TransportInfo transportInfo;

    MAKE_INSTANCE_CALL_1(GetTransportInfo, &transportInfo);


    // Set out args
    if(   !(*pCurrentTransportState  = SysAllocString(transportInfo.strCurrentTransportState))
       || !(*pCurrentTransportStatus = SysAllocString(transportInfo.strCurrentTransportStatus))
       || !(*pCurrentSpeed           = SysAllocString(transportInfo.strCurrentSpeed))   )
    {
       return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::GetPositionInfo(unsigned long InstanceID,
                                                     unsigned long* pTrack,
                                                     BSTR* pTrackDuration,
                                                     BSTR* pTrackMetaData,
                                                     BSTR* pTrackURI,
                                                     BSTR* pRelTime,
                                                     BSTR* pAbsTime,
                                                     long* pRelCount,
                                                     long* pAbsCount)
{
    if(!pTrack || !pTrackDuration || !pTrackMetaData || !pTrackURI || !pRelTime || !pAbsTime || !pRelCount || !pAbsCount)
    {
        return E_POINTER;
    }


    PositionInfo PI;

    MAKE_INSTANCE_CALL_1(GetPositionInfo, &PI);


    // Set out args

    *pTrack = PI.nTrack;

    if(   !(*pTrackDuration = SysAllocString(PI.strTrackDuration))
       || !(*pTrackMetaData = SysAllocString(PI.strTrackMetaData))
       || !(*pTrackURI = SysAllocString(PI.strTrackURI))
       || !(*pRelTime = SysAllocString(PI.strRelTime))
       || !(*pAbsTime = SysAllocString(PI.strAbsTime)))
    {
       return m_ErrReport.ReportError(ERROR_AV_OOM);
    }

    *pRelCount = PI.nRelCount;

    *pAbsCount = PI.nAbsCount;


    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::GetDeviceCapabilities(unsigned long InstanceID,
                                                           BSTR* pPlayMedia,
                                                           BSTR* pRecMedia,
                                                           BSTR* pRecQualityModes)
{
    if(!pPlayMedia || !pRecMedia || !pRecQualityModes)
    {
        return E_POINTER;
    }


    DeviceCapabilities deviceCaps;

    MAKE_INSTANCE_CALL_1(GetDeviceCapabilities, &deviceCaps);


    // Set out args
    if(   !(*pPlayMedia       = SysAllocString(deviceCaps.strPossiblePlayMedia))
       || !(*pRecMedia        = SysAllocString(deviceCaps.strPossibleRecMedia))
       || !(*pRecQualityModes = SysAllocString(deviceCaps.strPossibleRecQualityModes))   )
    {
       return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::GetTransportSettings(unsigned long InstanceID,
                                                          BSTR* pPlayMode,
                                                          BSTR* pRecQualityMode)
{
    if(!pPlayMode || !pRecQualityMode)
    {
        return E_POINTER;
    }


    TransportSettings transportSettings;

    MAKE_INSTANCE_CALL_1(GetTransportSettings, &transportSettings);


    // Set out args
    if(   !(*pPlayMode       = SysAllocString(transportSettings.strPlayMode))
       || !(*pRecQualityMode = SysAllocString(transportSettings.strRecQualityMode))   )
    {
       return m_ErrReport.ReportError(ERROR_AV_OOM);
    }
    

    return S_OK;// was E_NOTIMPL;
}


STDMETHODIMP AVTransportServiceImpl::Stop(unsigned long InstanceID)
{
    MAKE_INSTANCE_CALL_0(Stop);

    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::Play(unsigned long InstanceID,
                                          BSTR Speed)
{
    // Convert arguments for call

    wstring strSpeed;

    if(!strSpeed.assign(Speed, SysStringLen(Speed)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    MAKE_INSTANCE_CALL_1(Play, strSpeed);


    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::Pause(unsigned long InstanceID)
{
    MAKE_INSTANCE_CALL_0(Pause);

    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::Record(unsigned long InstanceID)
{
    MAKE_INSTANCE_CALL_0(Record);

    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::Seek(unsigned long InstanceID,
                                          BSTR Unit,
                                          BSTR Target)
{
    // Convert arguments for call

    wstring strUnit, strTarget;

    if(   !strUnit.assign(  Unit,   SysStringLen(Unit))
       || !strTarget.assign(Target, SysStringLen(Target))   )
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    MAKE_INSTANCE_CALL_2(Seek, strUnit, strTarget);


    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::Next(unsigned long InstanceID)
{
    MAKE_INSTANCE_CALL_0(Next);

    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::Previous(unsigned long InstanceID)
{
    MAKE_INSTANCE_CALL_0(Previous);

    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::SetPlayMode(unsigned long InstanceID,
                                                 BSTR NewPlayMode)
{
    // Convert arguments for call

    wstring strNewPlayMode;

    if(!strNewPlayMode.assign(NewPlayMode, SysStringLen(NewPlayMode)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    MAKE_INSTANCE_CALL_1(SetPlayMode, strNewPlayMode);


    return S_OK;
}


STDMETHODIMP AVTransportServiceImpl::SetRecordQualityMode(unsigned long InstanceID,
                                                          BSTR NewRecordQualityMode)
{
    // Convert arguments for call

    wstring strNewRecordQualityMode;

    if(!strNewRecordQualityMode.assign(NewRecordQualityMode, SysStringLen(NewRecordQualityMode)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    MAKE_INSTANCE_CALL_1(SetRecordQualityMode, strNewRecordQualityMode);


    return S_OK;
}


// Appends the string DATA_MEMBER_STRING to the outside-of-this-macro wstring strActions
// if transportActions.DATA_MEMBER is true. Prepends a te AV DCP delimiter
// if the bool fFirstItem is true and then sets it to false.
#define APPEND_ACTION(DATA_MEMBER, DATA_MEMBER_STRING)          \
if(transportActions.DATA_MEMBER)                                \
{                                                               \
    if(!strActions.empty())                                     \
    {                                                           \
        if(!strActions.append(AVDCPListDelimiter))              \
        {                                                       \
            return m_ErrReport.ReportError(ERROR_AV_OOM);       \
        }                                                       \
    }                                                           \
                                                                \
    if(!strActions.append(DATA_MEMBER_STRING))                  \
    {                                                           \
        return m_ErrReport.ReportError(ERROR_AV_OOM);           \
    }                                                           \
}

STDMETHODIMP AVTransportServiceImpl::GetCurrentTransportActions(unsigned long InstanceID,
                                                                BSTR* pActions)
{
    if(!pActions)
    {
        return E_POINTER;
    }


    TransportActions transportActions;

    MAKE_INSTANCE_CALL_1(GetCurrentTransportActions, &transportActions);


    // Set out args
    wstring strActions;

    // Create the list of current transport actions
    APPEND_ACTION(bPlay,     TransportAction::Play)
    APPEND_ACTION(bStop,     TransportAction::Stop)
    APPEND_ACTION(bPause,    TransportAction::Pause)
    APPEND_ACTION(bSeek,     TransportAction::Seek)
    APPEND_ACTION(bSeek,     L"X_DLNA_SeekTime")
    APPEND_ACTION(bNext,     TransportAction::Next)
    APPEND_ACTION(bPrevious, TransportAction::Previous)
    APPEND_ACTION(bRecord,   TransportAction::Record)


    // Convert this list of transport actions from wstring to a BSTR
    if(!(*pActions = SysAllocString(strActions)))
    {
        return m_ErrReport.ReportError(ERROR_AV_OOM);
    }


    return S_OK;
}



//
// Private
//

HRESULT AVTransportServiceImpl::InitToolkitErrs()
{
    const int vErrMap[][2] =
    {
        {ERROR_AV_POINTER,          ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_OOM,              ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_INVALID_INSTANCE, ERROR_AV_UPNP_AVT_INVALID_INSTANCE_ID},
        {ERROR_AV_INVALID_STATEVAR, ERROR_AV_UPNP_ACTION_FAILED},
        {ERROR_AV_ALREADY_INITED,   ERROR_AV_UPNP_ACTION_FAILED},
        {0,                         0} // 0 is used to denote the end of this array
    };


    for(unsigned int i=0; 0 != vErrMap[i][0]; ++i)
    {
        if(ERROR_AV_OOM == m_ErrReport.AddToolkitError(vErrMap[i][0], vErrMap[i][1]))
        {
            return E_OUTOFMEMORY;
        }
    }

    return S_OK;
}


HRESULT AVTransportServiceImpl::InitErrDescrips()
{
    const int vErrNums[] =
    {
        // UPnP
        ERROR_AV_UPNP_INVALID_ACTION,
        ERROR_AV_UPNP_ACTION_FAILED,
        // AVTransport
        ERROR_AV_UPNP_AVT_INVALID_TRANSITION,
        ERROR_AV_UPNP_AVT_NO_CONTENTS,
        ERROR_AV_UPNP_AVT_READ_ERROR,
        ERROR_AV_UPNP_AVT_UNSUPPORTED_PLAY_FORMAT,
        ERROR_AV_UPNP_AVT_TRANSPORT_LOCKED,
        ERROR_AV_UPNP_AVT_WRITE_ERROR,
        ERROR_AV_UPNP_AVT_PROTECTED_MEDIA,
        ERROR_AV_UPNP_AVT_UNSUPPORTED_REC_FORMAT,
        ERROR_AV_UPNP_AVT_FULL_MEDIA,
        ERROR_AV_UPNP_AVT_UNSUPPORTED_SEEK_MODE,
        ERROR_AV_UPNP_AVT_ILLEGAL_SEEK_TARGET,
        ERROR_AV_UPNP_AVT_UNSUPPORTED_PLAY_MODE,
        ERROR_AV_UPNP_AVT_UNSUPPORTED_REC_QUALITY,
        ERROR_AV_UPNP_AVT_ILLEGAL_MIME,
        ERROR_AV_UPNP_AVT_CONTENT_BUSY,
        ERROR_AV_UPNP_AVT_RESOURCE_NOT_FOUND,
        ERROR_AV_UPNP_AVT_UNSUPPORTED_PLAY_SPEED,
        ERROR_AV_UPNP_AVT_INVALID_INSTANCE_ID,
        0 // 0 is used to denote the end of this array
    };
    const wstring vErrDescrips[] =
    {
        // UPnP
        L"Invalid Action",
        L"Action Failed",
        // AVTransport
        L"Transition not available",
        L"No contents",
        L"Read error",
        L"Format not supported for playback",
        L"Transport is locked",
        L"Write error",
        L"Media is protected or not writable",
        L"Format not supported for recording",
        L"Media is full",
        L"Seek mode not supported",
        L"Illegal seek target",
        L"Play mode not supported",
        L"Record quality not supported",
        L"Illegal MIME-type",
        L"Content 'BUSY'",
        L"Resource not found",
        L"Play speed not supported",
        L"Invalid InstanceID"
    };


    for(unsigned int i=0; 0 != vErrNums[i]; ++i)
    {
        if(ERROR_AV_OOM == m_ErrReport.AddErrorDescription(vErrNums[i], vErrDescrips[i]))
        {
            return E_OUTOFMEMORY;
        }
    }

    return S_OK;
}
