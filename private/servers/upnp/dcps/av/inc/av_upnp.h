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

#ifndef __AV_UPNP_H
#define __AV_UPNP_H

#include <atlbase.h>

extern CComModule _Module;

#include <atlcom.h>
#include <upnp.h>
#include <upnphost.h>

#include "av_dcp.h"

#include <string.hxx>
#include <hash_map.hxx>
#include <hash_set.hxx>
#include <sync.hxx>
#include <auto_xxx.hxx>

// Debug zone bits
#define BIT_AV_ERROR            0
#define BIT_AV_WARN             1
#define BIT_AV_TRACE            2

// Debug zones
#define ZONE_AV_ERROR           DEBUGZONE(BIT_AV_ERROR)
#define ZONE_AV_WARN            DEBUGZONE(BIT_AV_WARN)
#define ZONE_AV_TRACE           DEBUGZONE(BIT_AV_TRACE)

#define ZONE_AV_ERROR_NAME      L"AV UPnP error"
#define ZONE_AV_WARN_NAME       L"AV UPnP warning"
#define ZONE_AV_TRACE_NAME      L"AV UPnP trace"

// Default zones
#define DEFAULT_AV_ZONES        (1 << BIT_AV_ERROR) | (1 << BIT_AV_WARN)

namespace av_upnp
{

enum UPnPAVError
{
    SUCCESS_AV                                              = 0,

    // Internal-AV-toolkit-only Errors
    ERROR_AV_POINTER                                        = -1,  // unexpected NULL pointer function argument
    ERROR_AV_OOM                                            = -2,  // ran out of memory during function's execution
    ERROR_AV_INVALID_INSTANCE                               = -3,  // nonexistant (when it must exist) or existing (when it must not exist) Instance ID/pointer
    ERROR_AV_INVALID_STATEVAR                               = -4,  // the given state variable name is invalid
    ERROR_AV_ALREADY_INITED                                 = -5,  // this service class has already had its Init() called
    
    // Internal-AV-toolkit ctrl-point-only Errors
    ERROR_AV_NONEXISTANT_SERVICE                            = -6,  // a required service does not exist on the device
    ERROR_AV_NO_MORE_ITEMS                                  = -7,  // returned enumerators to indicate end of data set
    ERROR_AV_UPNP_ERROR                                     = -8,  // UPnP control point APIs returned a system error; the specific error is available using GetLastError
    ERROR_AV_INVALID_OUT_ARGUMENTS                          = -9,  // action returned wrong number and/or type of [out] arguments

    // Standard UPnP Errors
    ERROR_AV_UPNP_INVALID_ACTION                            = FAULT_INVALID_ACTION,
    ERROR_AV_UPNP_ACTION_FAILED                             = FAULT_DEVICE_INTERNAL_ERROR,

    // ConnectionManager Errors
    ERROR_AV_UPNP_CM_INCOMPATIBLE_PROTOCOL                  = 701,
    ERROR_AV_UPNP_CM_INCOMPATIBLE_DIRECTION                 = 702,
    ERROR_AV_UPNP_CM_INSUFFICIENT_NET_RESOURCES             = 703,
    ERROR_AV_UPNP_CM_LOCAL_RESTRICTIONS                     = 704,
    ERROR_AV_UPNP_CM_ACCESS_DENIED                          = 705,
    ERROR_AV_UPNP_CM_INVALID_CONNECTION_REFERENCE           = 706,
    ERROR_AV_UPNP_CM_NOT_IN_NETWORK                         = 707,

    // AVTransport Errors
    ERROR_AV_UPNP_AVT_INVALID_TRANSITION                    = 701,
    ERROR_AV_UPNP_AVT_NO_CONTENTS                           = 702,
    ERROR_AV_UPNP_AVT_READ_ERROR                            = 703,
    ERROR_AV_UPNP_AVT_UNSUPPORTED_PLAY_FORMAT               = 704,
    ERROR_AV_UPNP_AVT_TRANSPORT_LOCKED                      = 705,
    ERROR_AV_UPNP_AVT_WRITE_ERROR                           = 706,
    ERROR_AV_UPNP_AVT_PROTECTED_MEDIA                       = 707,
    ERROR_AV_UPNP_AVT_UNSUPPORTED_REC_FORMAT                = 708,
    ERROR_AV_UPNP_AVT_FULL_MEDIA                            = 709,
    ERROR_AV_UPNP_AVT_UNSUPPORTED_SEEK_MODE                 = 710,
    ERROR_AV_UPNP_AVT_ILLEGAL_SEEK_TARGET                   = 711,
    ERROR_AV_UPNP_AVT_UNSUPPORTED_PLAY_MODE                 = 712,
    ERROR_AV_UPNP_AVT_UNSUPPORTED_REC_QUALITY               = 713,
    ERROR_AV_UPNP_AVT_ILLEGAL_MIME                          = 714,
    ERROR_AV_UPNP_AVT_CONTENT_BUSY                          = 715,
    ERROR_AV_UPNP_AVT_RESOURCE_NOT_FOUND                    = 716,
    ERROR_AV_UPNP_AVT_UNSUPPORTED_PLAY_SPEED                = 717,
    ERROR_AV_UPNP_AVT_INVALID_INSTANCE_ID                   = 718,

    // RenderingControl Errors
    ERROR_AV_UPNP_RC_INVALID_PRESET_NAME                    = 701,
    ERROR_AV_UPNP_RC_INVALID_INSTANCE_ID                    = 702,

    // ContentDirectory Errors
    ERROR_AV_UPNP_CD_NO_SUCH_OBJECT                         = 701,
    ERROR_AV_UPNP_CD_INVALID_CURRENTTAGVALUE                = 702,
    ERROR_AV_UPNP_CD_INVALID_NEWTAGVALUE                    = 703,
    ERROR_AV_UPNP_CD_REQUIRED_TAG_DELETE                    = 704,
    ERROR_AV_UPNP_CD_READONLY_TAG_UPDATE                    = 705,
    ERROR_AV_UPNP_CD_PARAMETER_NUM_MISMATCH                 = 706,
    ERROR_AV_UPNP_CD_BAD_SEARCH_CRITERIA                    = 708,
    ERROR_AV_UPNP_CD_BAD_SORT_CRITERIA                      = 709,
    ERROR_AV_UPNP_CD_NO_SUCH_CONTAINER                      = 710,
    ERROR_AV_UPNP_CD_RESTRICTED_OBJECT                      = 711,
    ERROR_AV_UPNP_CD_BAD_METADATA                           = 712,
    ERROR_AV_UPNP_CD_RESTRICTED_PARENT_OBJECT               = 713,
    ERROR_AV_UPNP_CD_NO_SUCH_SOURCE_RESOURCE                = 714,
    ERROR_AV_UPNP_CD_SOURCE_RESOURCE_ACCESS_DENIED          = 715,
    ERROR_AV_UPNP_CD_TRANSFER_BUSY                          = 716,
    ERROR_AV_UPNP_CD_NO_SUCH_FILE_TRANSFER                  = 717,
    ERROR_AV_UPNP_CD_NO_SUCH_DESTINATION_RESOURCE           = 718,
    ERROR_AV_UPNP_CD_DESTINATION_RESOURCE_ACCESS_DENIED     = 719,
    ERROR_AV_UPNP_CD_REQUEST_FAILED                         = 720
};



enum DIRECTION
{
    OUTPUT,
    INPUT
};


//
// Define string classes in the av_upnp namespace
//
typedef ce::_string_t<wchar_t>                                      wstring;
typedef ce::_string_t<wchar_t, 16, ce::i_char_traits<wchar_t> >     wistring;


//
// Data structures used in parameter passing
//

// forward declaration for struct ConnectionInfo
class IRenderingControl;
class IAVTransport;

struct ConnectionInfo
{
    IRenderingControl*  pRenderingControl;
    IAVTransport*       pAVTransport;
    wstring             strRemoteProtocolInfo;
    wstring             strPeerConnectionManager;
    long                nPeerConnectionID;
    DIRECTION           Direction;
    wstring             strStatus;
};

struct MediaInfo
{
    unsigned long nNrTracks;
    wstring strMediaDuration;
    wstring strCurrentURI;
    wstring strCurrentURIMetaData;
    wstring strNextURI;
    wstring strNextURIMetaData;
    wstring strPlayMedium;
    wstring strRecordMedium;
    wstring strWriteStatus;
};

struct TransportInfo
{
    wstring strCurrentTransportState;
    wstring strCurrentTransportStatus;
    wstring strCurrentSpeed;
};

struct PositionInfo
{
    unsigned long nTrack;
    wstring strTrackDuration;
    wstring strTrackMetaData;
    wstring strTrackURI;
    wstring strRelTime;
    wstring strAbsTime;
    long nRelCount;
    long nAbsCount;
};

struct DeviceCapabilities
{
    wstring strPossiblePlayMedia;
    wstring strPossibleRecMedia;
    wstring strPossibleRecQualityModes;
};

struct TransportSettings
{
    wstring strPlayMode;
    wstring strRecQualityMode;
};

struct TransportActions
{
    bool bPlay;
    bool bStop;
    bool bPause;
    bool bSeek;
    bool bNext;
    bool bPrevious;
    bool bRecord;
};



// Class naming convention:
// upnp_av::I[Xxx]                 - interface
// upnp_av::[Functionality]Support - toolkit class providing functionality "Functionality", inherited from by toolkit classes
// upnp_av::[Xxx]Impl              - toolkit class which user may use as a base class but can not be instantiated
// upnp_av::[Xxx]ServiceImpl       - toolkit COM class implementing UPnP service
// upnp_av::[Xxx]                  - toolkit class user may instantiate but not use as base class


//
// Interfaces
//


//
// IEventSink
//
class IEventSink
{
public:
    virtual DWORD OnStateChanged(
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszValue) = 0;
        
    virtual DWORD OnStateChanged(
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  long nValue) = 0;

    virtual DWORD OnStateChanged(
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszChannel,
        /*[in]*/  long nValue) = 0;
};


//
// IEventSource
//
class IEventSource
{
public:
    virtual DWORD Advise(  /*[in]*/ IEventSink *pSubscriber) = 0;
    virtual DWORD Unadvise(/*[in]*/ IEventSink *pSubscriber) = 0;
};


//
// ServiceInstanceLifetime
//
// Only provides a reference count, relies on derived class to implement
// OnFinalRelease to handle object cleanup.
//
class ServiceInstanceLifetime
{
public:
    ServiceInstanceLifetime() : m_lRef(0) { }

    unsigned long AddRef()
    {
        return InterlockedIncrement(&m_lRef);
    }

    unsigned long Release()
    {
        unsigned long ul = InterlockedDecrement(&m_lRef);
        if (ul == 0)
        {
            OnFinalRelease();
        }

        return ul;
    }

protected:
    virtual void OnFinalRelease() = 0;
    long m_lRef;
};



//
// IAVTransport - interface of AVTransport virtual service
//
class IAVTransport : public ServiceInstanceLifetime, 
                     public IEventSource
{
public:
    virtual DWORD SetAVTransportURI(
        /* [in] */ LPCWSTR pszCurrentURI,
        /* [in] */ LPCWSTR pszCurrentURIMetaData) = 0;

    virtual DWORD SetNextAVTransportURI(
        /* [in] */ LPCWSTR pszNextURI,
        /* [in] */ LPCWSTR pszNextURIMetaData) = 0;

    virtual DWORD GetMediaInfo(
        /* [in, out] */ MediaInfo* pMediaInfo) = 0;

    virtual DWORD GetTransportInfo(
        /* [in, out] */ TransportInfo* pTransportInfo) = 0;

    virtual DWORD GetPositionInfo(
        /* [in, out] */ PositionInfo* pPositionInfo) = 0;

    virtual DWORD GetDeviceCapabilities(
        /* [in, out] */ DeviceCapabilities* pDeviceCapabilities) = 0;

    virtual DWORD GetTransportSettings(
        /* [in, out] */ TransportSettings* pTransportSettings) = 0;

    virtual DWORD Stop() = 0;

    virtual DWORD Play(
        /* [in] */ LPCWSTR pszSpeed) = 0;

    virtual DWORD Pause() = 0;

    virtual DWORD Record() = 0;

    virtual DWORD Seek(
        /* [in] */ LPCWSTR pszUnit,
        /* [in] */ LPCWSTR pszTarget) = 0;

    virtual DWORD Next() = 0;

    virtual DWORD Previous() = 0;

    virtual DWORD SetPlayMode(
        /* [in] */ LPCWSTR pszNewPlayMode) = 0;

    virtual DWORD SetRecordQualityMode(
        /* [in] */ LPCWSTR pszNewRecordQualityMode) = 0;

    virtual DWORD GetCurrentTransportActions(
        /* [in, out] */ TransportActions* pActions) = 0;

    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult) = 0;
};


//
// IRenderingControl - interface of RenderingControl virtual service
//
class IRenderingControl : public ServiceInstanceLifetime, 
                          public IEventSource
{
public:
    virtual DWORD ListPresets(
        /* [in, out] */ wstring* pstrPresetNameList) = 0;

    virtual DWORD SelectPreset(
        /* [in] */ LPCWSTR pszPresetName) = 0;

    virtual DWORD GetBrightness(
        /* [in, out] */ unsigned short* pBrightness) = 0;

    virtual DWORD SetBrightness(
        /* [in] */ unsigned short Brightness) = 0;

    virtual DWORD GetContrast(
        /* [in, out] */ unsigned short* pContrast) = 0;

    virtual DWORD SetContrast(
        /* [in] */ unsigned short Contrast) = 0;

    virtual DWORD GetSharpness(
        /* [in, out] */ unsigned short* pSharpness) = 0;

    virtual DWORD SetSharpness(
        /* [in] */ unsigned short Sharpness) = 0;

    virtual DWORD GetRedVideoGain(
        /* [in, out] */ unsigned short* pRedVideoGain) = 0;

    virtual DWORD SetRedVideoGain(
        /* [in] */ unsigned short RedVideoGain) = 0;

    virtual DWORD GetGreenVideoGain(
        /* [in, out] */ unsigned short* pGreenVideoGain) = 0;

    virtual DWORD SetGreenVideoGain(
        /* [in] */ unsigned short GreenVideoGain) = 0;

    virtual DWORD GetBlueVideoGain(
        /* [in, out] */ unsigned short* pBlueVideoGain) = 0;

    virtual DWORD SetBlueVideoGain(
        /* [in] */ unsigned short BlueVideoGain) = 0;

    virtual DWORD GetRedVideoBlackLevel(
        /* [in, out] */ unsigned short* pRedVideoBlackLevel) = 0;

    virtual DWORD SetRedVideoBlackLevel(
        /* [in] */ unsigned short RedVideoBlackLevel) = 0;

    virtual DWORD GetGreenVideoBlackLevel(
        /* [in, out] */ unsigned short* pGreenVideoBlackLevel) = 0;

    virtual DWORD SetGreenVideoBlackLevel(
        /* [in] */ unsigned short GreenVideoBlackLevel) = 0;

    virtual DWORD GetBlueVideoBlackLevel(
        /* [in, out] */ unsigned short* pBlueVideoBlackLevel) = 0;

    virtual DWORD SetBlueVideoBlackLevel(
        /* [in] */ unsigned short BlueVideoBlackLevel) = 0;

    virtual DWORD GetColorTemperature(
        /* [in, out] */ unsigned short* pColorTemperature) = 0;

    virtual DWORD SetColorTemperature(
        /* [in] */ unsigned short ColorTemperature) = 0;

    virtual DWORD GetHorizontalKeystone(
        /* [in, out] */ short* pHorizontalKeystone) = 0;

    virtual DWORD SetHorizontalKeystone(
        /* [in] */ short HorizontalKeystone) = 0;

    virtual DWORD GetVerticalKeystone(
        /* [in, out] */ short* pVerticalKeystone) = 0;

    virtual DWORD SetVerticalKeystone(
        /* [in] */ short VerticalKeystone) = 0;

    virtual DWORD GetMute(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ bool* pMute) = 0;

    virtual DWORD SetMute(
        /* [in] */ LPCWSTR pszChannel,
        /* [in] */ bool Mute) = 0;

    virtual DWORD GetVolume(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ unsigned short* pVolume) = 0;

    virtual DWORD SetVolume(
        /* [in] */ LPCWSTR pszChannel,
        /* [in] */ unsigned short Volume) = 0;

    virtual DWORD GetVolumeDB(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ short* pVolumeDB) = 0;

    virtual DWORD SetVolumeDB(
        /* [in] */ LPCWSTR pszChannel,
        /* [in] */ short VolumeDB) = 0;

    virtual DWORD GetVolumeDBRange(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ short* pMinValue,
        /* [in, out] */ short* pMaxValue) = 0;

    virtual DWORD GetLoudness(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ bool* pLoudness) = 0;

    virtual DWORD SetLoudness(
        /* [in] */ LPCWSTR pszChannel,
        /* [in] */ bool Loudness) = 0;


    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult) = 0;
};

//
// IConnectionManager - interface of ConnectionManager service
//
class IConnectionManager : public IEventSource
{
public:
    virtual DWORD GetProtocolInfo(
            /* [in, out] */ wstring* pstrSourceProtocolInfo,
            /* [in, out] */ wstring* pstrSinkProtocolInfo) = 0;

    virtual DWORD PrepareForConnection(
            /* [in] */ LPCWSTR pszRemoteProtocolInfo,
            /* [in] */ LPCWSTR pszPeerConnectionManager,
            /* [in] */ long PeerConnectionID,
            /* [in] */ DIRECTION Direction,
            /* [in, out] */ long* pConnectionID,
            /* [in, out] */ IAVTransport** ppAVTransport,
            /* [in, out] */ IRenderingControl** ppRenderingControl) = 0;

    virtual DWORD ConnectionComplete(
            /* [in] */ long ConnectionID) = 0;

    virtual DWORD GetFirstConnectionID(
            /* [in, out] */ long* pConnectionID) = 0;

    virtual DWORD GetNextConnectionID(
            /* [in, out] */ long* pConnectionID) = 0;

    virtual DWORD GetCurrentConnectionInfo(
            /* [in] */ long ConnectionID,
            /* [in, out] */ ConnectionInfo* pConnectionInfo) = 0;
            
    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult) = 0;
};


//
// IContentDirectory - interface of ContentDirectory service
//
class IContentDirectory : public IEventSource
{
public:
    virtual DWORD GetSearchCapabilities(
        /* [in, out] */ wstring* pstrSearchCaps) = 0;

    virtual DWORD GetSortCapabilities(
        /* [in, out] */ wstring* pstrSortCaps) = 0;

    virtual DWORD GetSystemUpdateID(
        /* [in, out] */ unsigned long* pId) = 0;

    virtual DWORD BrowseMetadata(
        /* [in] */ LPCWSTR pszObjectID,
        /* [in] */ LPCWSTR pszFilter,
        /* [in, out] */ wstring* pstrResult,
        /* [in, out] */ unsigned long* pUpdateID) = 0;

    virtual DWORD BrowseChildren(
        /* [in] */ LPCWSTR pszObjectID,
        /* [in] */ LPCWSTR pszFilter,
        /* [in] */ unsigned long StartingIndex,
        /* [in] */ unsigned long RequestedCount,
        /* [in] */ LPCWSTR pszSortCriteria,
        /* [in, out] */ wstring* pstrResult,
        /* [in, out] */ unsigned long* pNumberReturned,
        /* [in, out] */ unsigned long* pTotalMatches,
        /* [in, out] */ unsigned long* pUpdateID) = 0;
        
    virtual DWORD Search(
        /* [in] */ LPCWSTR pszContainerID,
        /* [in] */ LPCWSTR pszSearchCriteria,
        /* [in] */ LPCWSTR pszFilter,
        /* [in] */ unsigned long StartingIndex,
        /* [in] */ unsigned long RequestedCount,
        /* [in] */ LPCWSTR pszSortCriteria,
        /* [in, out] */ wstring* pstrResult,
        /* [in, out] */ unsigned long* pNumberReturned,
        /* [in, out] */ unsigned long* pTotalMatches,
        /* [in, out] */ unsigned long* pUpdateID) = 0;

    virtual DWORD CreateObject(
        /* [in] */ LPCWSTR pszContainerID,
        /* [in] */ LPCWSTR pszElements,
        /* [in, out] */ wstring* pstrObjectID,
        /* [in, out] */ wstring* pstrResult) = 0;

    virtual DWORD DestroyObject(
        /* [in] */ LPCWSTR pszObjectID) = 0;

    virtual DWORD UpdateObject(
        /* [in] */ LPCWSTR pszObjectID,
        /* [in] */ LPCWSTR pszCurrentTagValue,
        /* [in] */ LPCWSTR pszNewTagValue) = 0;

    virtual DWORD ImportResource(
        /* [in] */ LPCWSTR pszSourceURI,
        /* [in] */ LPCWSTR pszDestinationURI,
        /* [in, out] */ unsigned long* pTransferID) = 0;

    virtual DWORD ExportResource(
        /* [in] */ LPCWSTR pszSourceURI,
        /* [in] */ LPCWSTR pszDestinationURI,
        /* [in, out] */ unsigned long* pTransferID) = 0;

    virtual DWORD StopTransferResource(
        /* [in] */ unsigned long TransferID) = 0;

    virtual DWORD GetTransferProgress(
        /* [in] */ unsigned long TransferID,
        /* [in, out] */ wstring* pstrTransferStatus,
        /* [in, out] */ wstring* pstrTransferLength,
        /* [in, out] */ wstring* pstrTransferTotal) = 0;

    virtual DWORD DeleteResource(
        /* [in] */ LPCWSTR pszResourceURI) = 0;

    virtual DWORD CreateReference(
        /* [in] */ LPCWSTR pszContainerID,
        /* [in] */ LPCWSTR pszObjectID,
        /* [in, out] */ wstring* pstrNewID) = 0;
        
    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult) = 0;
};


}

#include "av_upnp_internal.h"

namespace av_upnp
{

//
// Utilities
//

extern LPCWSTR AVDCPListDelimiter; // = L",";

DWORD EscapeAVDCPListDelimiters(wstring* pstr);


//
// UPnPErrorReporting
//
class UPnPErrorReporting
{
public:
    //
    // Add/Remove Error Number -> Description mappings.
    //
    DWORD AddErrorDescription(int UPnPErrorNum, const wstring &UPnPErrorDescription);
    DWORD RemoveErrorDescription(int UPnPErrorNum);

    //
    // Add/Remove internal toolkit error number -> upnp error number mappings.
    //
    DWORD AddToolkitError(int UPnPToolkitErrorNum, int UPnPErrorNum);
    DWORD RemoveToolkitError(int UPnPToolkitErrorNum);

    //
    // NOTE: This function modifies the thread's error information object using SetErrorInfo()
    //
#ifdef DEBUG
    HRESULT ReportActionError(int UPnPErrorNum, LPCWSTR pszAction);
#else    
    HRESULT ReportActionError(int UPnPErrorNum);
#endif

private:
    typedef ce::hash_map<int, wstring>  ErrDescripMap;      // map from UPnP error num -> error description
    typedef ce::hash_map<int, int>      ToolkitErrMap;      // map from internal toolkit error num -> UPnP error num to return

    ce::critical_section    m_csDataMembers;
    ErrDescripMap           m_mapErrDescrips;
    ToolkitErrMap           m_mapToolkitErrs;
};

#ifdef DEBUG
#   define ReportError(x)  ReportActionError(x, TEXT(__FUNCTION__))
#else
#   define ReportError(x)  ReportActionError(x)
#endif


//
// Functionality Support for toolkit classes
//

//
// Support for IEventSink for non-virtual service classes (ConnectionManagerServiceImpl and ContentDirectoryServiceImpl)
//
class IEventSinkSupport : public IEventSink,
                          virtual protected details::IUPnPEventSinkSupport
{
// IEventSink
public:
    virtual DWORD OnStateChanged(
        /*[in]*/ LPCWSTR pszStateVariableName,
        /*[in*/  LPCWSTR pszValue);
        
    virtual DWORD OnStateChanged(
        /*[in]*/ LPCWSTR pszStateVariableName,
        /*[in*/  long nValue);

    virtual DWORD OnStateChanged(
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszChannel,
        /*[in]*/  long nValue);

protected:
    typedef ce::hash_map<wstring, DISPID> DISPIDMap; // map from state variable name -> DISPID

    ce::critical_section m_csMapDISPIDs;
    DISPIDMap            m_mapDISPIDs;
};


//
// ModeratedEventSupport
//
class ModeratedEventSupport : public IUPnPEventSource,
                              virtual protected details::IUPnPEventSinkSupport,
                              public details::ITimedEventCallee
{
public:
    // nMaxEventRate is the minimum time to sleep between events; must be less than 0x7FFFFFFF
    ModeratedEventSupport(DWORD nMaxEventRate);
    virtual ~ModeratedEventSupport();

    void AddRefModeratedEvent();
    void ReleaseModeratedEvent();

// TimedEventCallee
public:
    virtual void TimedEventCall() = 0;

// IUPnPEventSource
public:
    STDMETHOD(Advise)(
        /*[in]*/ IUPnPEventSink* punkSubscriber);
    STDMETHOD(Unadvise)(
        /*[in]*/ IUPnPEventSink* punkSubscriber);

private:
    const DWORD m_nMaxEventRate;
    bool        m_bTimerInitialized;
};


//
// VirtualServiceSupport
//
template <typename T>
class VirtualServiceSupport : public ModeratedEventSupport
{
public:
    // Via nFirstInstanceID, optionally set the ID for the first instance registered if
    // RegisterServiceInstance(T*, long*) is used to register this first instance
    VirtualServiceSupport(DISPID dispidLastChange, long nFirstInstanceID = 0);
    ~VirtualServiceSupport();

    // Register a service instance, InstanceID will be the instance's ID upon return
    DWORD RegisterInstance(
            /* [in] */  T* pIServiceInstance,
            /* [in, out] */ long* pInstanceID);

    // Register a service instance, using InstanceID as the instance's ID
    DWORD RegisterInstance(
            /* [in] */  T* pIServiceInstance,
            /* [in] */  long   InstanceID);

    DWORD UnregisterInstance(
            /* [in] */  long   InstanceID);
            
    bool DefaultInstanceExists();

// TimedEventCallee
public:
    void TimedEventCall();
    
public:
    HRESULT GetLastChange(BSTR* pLastChange);
    
protected:
    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult);

private:
    //
    // Append state variable data for all registered services to *pLastChange.
    // If m_bGetLastChangeUpdatesOnly append data for all variables,
    // else: only variables which have changed since last call to GetLastChanges
    //
    DWORD GetLastChange(wstring* pLastChange);
    DWORD CreateUniqueID(long* pIstanceID);

protected:
    struct Instance
    {
        Instance(T* pServiceInstance = NULL)
            : pService(pServiceInstance)
        {}

        T*                          pService;
        details::VirtualEventSink   virtualEventSink;
    };

    typedef ce::hash_map<long, Instance> InstanceMap; // map from Instance ID -> Instance

    ce::critical_section    m_csMapInstances;
    ce::critical_section    m_csGetLastChange;
    InstanceMap             m_mapInstances;
    DISPID                  m_dispidLastChange[1];
    bool                    m_bGetLastChangeUpdatesOnly;
    long                    m_nLastCreatedID;
    UPnPErrorReporting      m_ErrReport;
};

//
//
// Device classes and implementations
//


//
// Interface class basic implementations
//

class IAVTransportImpl : public IAVTransport
{
// IAVTransport
public:
    virtual DWORD SetAVTransportURI(
        /*[in]*/ LPCWSTR pszCurrentURI,
        /*[in]*/ LPCWSTR pszCurrentURIMetaData) = 0;

    virtual DWORD SetNextAVTransportURI(
        /*[in]*/ LPCWSTR pszNextURI,
        /*[in]*/ LPCWSTR pszNextURIMetaData);

    virtual DWORD GetMediaInfo(/*[in, out]*/ MediaInfo* pMediaInfo) = 0;

    virtual DWORD GetTransportInfo(
        /*[in, out]*/ TransportInfo* pTransportInfo) = 0;

    virtual DWORD GetPositionInfo(
        /*[in, out]*/ PositionInfo* pPositionInfo) = 0;

    virtual DWORD GetDeviceCapabilities(
        /*[in, out]*/ DeviceCapabilities* pDeviceCapabilities) = 0;

    virtual DWORD GetTransportSettings(
        /*[in, out]*/ TransportSettings* pTransportSettings) = 0;

    virtual DWORD Stop() = 0;

    virtual DWORD Play(/*[in]*/ LPCWSTR pszSpeed) = 0;

    virtual DWORD Pause();

    virtual DWORD Record();

    virtual DWORD Seek(
        /*[in]*/ LPCWSTR pszUnit,
        /*[in]*/ LPCWSTR pszTarget) = 0;

    virtual DWORD Next() = 0;

    virtual DWORD Previous() = 0;

    virtual DWORD SetPlayMode(/*[in]*/ LPCWSTR pszNewPlayMode);

    virtual DWORD SetRecordQualityMode(
        /*[in]*/ LPCWSTR pszNewRecordQualityMode);

    virtual DWORD GetCurrentTransportActions(
        /*[in, out]*/ TransportActions* pActions);

    // InvokeVendorAction may be overriden by the toolkit user to extend this service's actions
    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult);
};


class IRenderingControlImpl : public IRenderingControl
{
// IRenderingControl
public:
    virtual DWORD ListPresets(
        /* [in, out] */ wstring* pstrPresetNameList) = 0;

    virtual DWORD SelectPreset(
        /* [in] */ LPCWSTR pszPresetName) = 0;


    virtual DWORD GetBrightness(
        /* [in, out] */ unsigned short* pBrightness);

    virtual DWORD SetBrightness(
        /* [in] */ unsigned short Brightness);

    virtual DWORD GetContrast(
        /* [in, out] */ unsigned short* pContrast);

    virtual DWORD SetContrast(
        /* [in] */ unsigned short Contrast);

    virtual DWORD GetSharpness(
        /* [in, out] */ unsigned short* pSharpness);

    virtual DWORD SetSharpness(
        /* [in] */ unsigned short Sharpness);

    virtual DWORD GetRedVideoGain(
        /* [in, out] */ unsigned short* pRedVideoGain);

    virtual DWORD SetRedVideoGain(
        /* [in] */ unsigned short RedVideoGain);

    virtual DWORD GetGreenVideoGain(
        /* [in, out] */ unsigned short* pGreenVideoGain);

    virtual DWORD SetGreenVideoGain(
        /* [in] */ unsigned short GreenVideoGain);

    virtual DWORD GetBlueVideoGain(
        /* [in, out] */ unsigned short* pBlueVideoGain);

    virtual DWORD SetBlueVideoGain(
        /* [in] */ unsigned short BlueVideoGain);

    virtual DWORD GetRedVideoBlackLevel(
        /* [in, out] */ unsigned short* pRedVideoBlackLevel);

    virtual DWORD SetRedVideoBlackLevel(
        /* [in] */ unsigned short RedVideoBlackLevel);

    virtual DWORD GetGreenVideoBlackLevel(
        /* [in, out] */ unsigned short* pGreenVideoBlackLevel);

    virtual DWORD SetGreenVideoBlackLevel(
        /* [in] */ unsigned short GreenVideoBlackLevel);

    virtual DWORD GetBlueVideoBlackLevel(
        /* [in, out] */ unsigned short* pBlueVideoBlackLevel);

    virtual DWORD SetBlueVideoBlackLevel(
        /* [in] */ unsigned short BlueVideoBlackLevel);

    virtual DWORD GetColorTemperature(
        /* [in, out] */ unsigned short* pColorTemperature);

    virtual DWORD SetColorTemperature(
        /* [in] */ unsigned short ColorTemperature);

    virtual DWORD GetHorizontalKeystone(
        /* [in, out] */ short* pHorizontalKeystone);

    virtual DWORD SetHorizontalKeystone(
        /* [in] */ short HorizontalKeystone);

    virtual DWORD GetVerticalKeystone(
        /* [in, out] */ short* pVerticalKeystone);

    virtual DWORD SetVerticalKeystone(
        /* [in] */ short VerticalKeystone);

    virtual DWORD GetMute(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ bool* pMute);

    virtual DWORD SetMute(
        /* [in] */ LPCWSTR pszChannel,
        /* [in] */ bool Mute);

    virtual DWORD GetVolume(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ unsigned short* pVolume);

    virtual DWORD SetVolume(
        /* [in] */ LPCWSTR pszChannel,
        /* [in] */ unsigned short Volume);

    virtual DWORD GetVolumeDB(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ short* pVolumeDB);

    virtual DWORD SetVolumeDB(
        /* [in] */ LPCWSTR pszChannel,
        /* [in] */ short VolumeDB);

    virtual DWORD GetVolumeDBRange(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ short* pMinValue,
        /* [in, out] */ short* pMaxValue);

    virtual DWORD GetLoudness(
        /* [in] */ LPCWSTR pszChannel,
        /* [in, out] */ bool* pLoudness);

    virtual DWORD SetLoudness(
        /* [in] */ LPCWSTR pszChannel,
        /* [in] */ bool Loudness);


    // InvokeVendorAction may be overriden by the toolkit user to extend this service's actions
    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult);
};


class IConnectionManagerImpl : public IConnectionManager
{
public:
    IConnectionManagerImpl();
    virtual ~IConnectionManagerImpl();

    virtual DWORD AddSinkProtocol(LPCWSTR pszProtocolInfo);
    virtual DWORD RemoveSinkProtocol(LPCWSTR pszProtocolInfo);
    
    virtual DWORD AddSourceProtocol(LPCWSTR pszProtocolInfo);
    virtual DWORD RemoveSourceProtocol(LPCWSTR pszProtocolInfo);

// IConnectionManager
public:
    virtual DWORD PrepareForConnection(
            /* [in] */ LPCWSTR pszRemoteProtocolInfo,
            /* [in] */ LPCWSTR pszPeerConnectionManager,
            /* [in] */ long PeerConnectionID,
            /* [in] */ DIRECTION Direction,
            /* [in, out] */ long* pConnectionID,
            /* [in, out] */ IAVTransport** ppAVTransport,
            /* [in, out] */ IRenderingControl** ppRenderingControl);
    
    virtual DWORD ConnectionComplete(
            /* [in] */ long ConnectionID);

    virtual DWORD GetFirstConnectionID(
            /* [in, out] */ long* pConnectionID);

    virtual DWORD GetNextConnectionID(
            /* [in, out] */ long* pConnectionID);


    virtual DWORD GetProtocolInfo(
        /* [in, out] */ wstring* pstrSourceProtocolInfo,
        /* [in, out] */ wstring* pstrSinkProtocolInfo);

    virtual DWORD GetCurrentConnectionInfo(
            /* [in] */ long ConnectionID,
            /* [in, out] */ ConnectionInfo* pConnectionInfo);

// IEventSource
public:
    virtual DWORD Advise(  /*[in]*/ IEventSink* pSubscriber);
    virtual DWORD Unadvise(/*[in]*/ IEventSink* pSubscriber);
    
protected:
    virtual DWORD CreateConnection(
        /* [in] */ LPCWSTR pszRemoteProtocolInfo,
        /* [in] */ DIRECTION Direction,
        /* [in, out] */ long ConnectionID,
        /* [in, out] */ IAVTransport** ppAVTransport,
        /* [in, out] */ IRenderingControl** ppRenderingControl) = 0;

    virtual DWORD EndConnection(
        /* [in] */ long ConnectionID) = 0;
        
    // InvokeVendorAction may be overriden by the toolkit user to extend this service's actions
    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult);

protected:
    typedef ce::hash_set<wstring>               ProtocolInfoSet;

    ce::critical_section         m_csDataMembers;

    IEventSink*                  m_pSubscriber;

    ProtocolInfoSet              m_setSourceProtocols;
    ProtocolInfoSet              m_setSinkProtocols;

};


class IContentDirectoryImpl : public IContentDirectory
{
// IContentDirectory
public:
    virtual DWORD GetSystemUpdateID(
        /* [in, out] */ unsigned long* pId) = 0;

    virtual DWORD BrowseMetadata(
        /* [in] */ LPCWSTR pszObjectID,
        /* [in] */ LPCWSTR pszFilter,
        /* [in, out] */ wstring* pstrResult,
        /* [in, out] */ unsigned long* pUpdateID) = 0;

    virtual DWORD BrowseChildren(
        /* [in] */ LPCWSTR pszObjectID,
        /* [in] */ LPCWSTR pszFilter,
        /* [in] */ unsigned long StartingIndex,
        /* [in] */ unsigned long RequestedCount,
        /* [in] */ LPCWSTR pszSortCriteria,
        /* [in, out] */ wstring* pstrResult,
        /* [in, out] */ unsigned long* pNumberReturned,
        /* [in, out] */ unsigned long* pTotalMatches,
        /* [in, out] */ unsigned long* pUpdateID) = 0;


    virtual DWORD GetSearchCapabilities(
        /* [in, out] */ wstring* pstrSearchCaps);

    virtual DWORD GetSortCapabilities(
        /* [in, out] */ wstring* pstrSortCaps);


    virtual DWORD Search(
        /* [in] */ LPCWSTR pszContainerID,
        /* [in] */ LPCWSTR pszSearchCriteria,
        /* [in] */ LPCWSTR pszFilter,
        /* [in] */ unsigned long StartingIndex,
        /* [in] */ unsigned long RequestedCount,
        /* [in] */ LPCWSTR pszSortCriteria,
        /* [in, out] */ wstring* pstrResult,
        /* [in, out] */ unsigned long* pNumberReturned,
        /* [in, out] */ unsigned long* pTotalMatches,
        /* [in, out] */ unsigned long* pUpdateID);

    virtual DWORD CreateObject(
        /* [in] */ LPCWSTR pszContainerID,
        /* [in] */ LPCWSTR pszElements,
        /* [in, out] */ wstring* pstrObjectID,
        /* [in, out] */ wstring* pstrResult);

    virtual DWORD DestroyObject(
        /* [in] */ LPCWSTR pszObjectID);

    virtual DWORD UpdateObject(
        /* [in] */ LPCWSTR pszObjectID,
        /* [in] */ LPCWSTR pszCurrentTagValue,
        /* [in] */ LPCWSTR pszNewTagValue);

    virtual DWORD ImportResource(
        /* [in] */ LPCWSTR pszSourceURI,
        /* [in] */ LPCWSTR pszDestinationURI,
        /* [in, out] */ unsigned long* pTransferID);

    virtual DWORD ExportResource(
        /* [in] */ LPCWSTR pszSourceURI,
        /* [in] */ LPCWSTR pszDestinationURI,
        /* [in, out] */ unsigned long* pTransferID);

    virtual DWORD StopTransferResource(
        /* [in] */ unsigned long TransferID);

    virtual DWORD GetTransferProgress(
        /* [in] */ unsigned long TransferID,
        /* [in, out] */ wstring* pstrTransferStatus,
        /* [in, out] */ wstring* pstrTransferLength,
        /* [in, out] */ wstring* pstrTransferTotal);

    virtual DWORD DeleteResource(
        /* [in] */ LPCWSTR pszResourceURI);

    virtual DWORD CreateReference(
        /* [in] */ LPCWSTR pszContainerID,
        /* [in] */ LPCWSTR pszObjectID,
        /* [in, out] */ wstring* pstrNewID);
        
protected:
    // InvokeVendorAction may be overriden by the toolkit user to extend this service's actions
    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult);        
};


//
// Default implementation of AV services
//

class AVTransportServiceImpl :
    public IUPnPService_AVTransport1,
    public VirtualServiceSupport<IAVTransport>
{
public:
    AVTransportServiceImpl();
    virtual ~AVTransportServiceImpl();

// IUPnPService_AVTransport1
public:
    STDMETHOD(get_TransportState)(BSTR* pTransportState);
    STDMETHOD(get_TransportStatus)(BSTR* pTransportStatus);
    STDMETHOD(get_PlaybackStorageMedium)(BSTR* pPlaybackStorageMedium);
    STDMETHOD(get_RecordStorageMedium)(BSTR* pRecordStorageMedium);
    STDMETHOD(get_PossiblePlaybackStorageMedia)(BSTR* pPossiblePlaybackStorageMedia);
    STDMETHOD(get_PossibleRecordStorageMedia)(BSTR* pPossibleRecordStorageMedia);
    STDMETHOD(get_CurrentPlayMode)(BSTR* pCurrentPlayMode);
    STDMETHOD(get_TransportPlaySpeed)(BSTR* pTransportPlaySpeed);
    STDMETHOD(get_RecordMediumWriteStatus)(BSTR* pRecordMediumWriteStatus);
    STDMETHOD(get_CurrentRecordQualityMode)(BSTR* pCurrentRecordQualityMode);
    STDMETHOD(get_PossibleRecordQualityModes)(BSTR* pPossibleRecordQualityModes);
    STDMETHOD(get_NumberOfTracks)(unsigned long* pNumberOfTracks);
    STDMETHOD(get_CurrentTrack)(unsigned long* pCurrentTrack);
    STDMETHOD(get_CurrentTrackDuration)(BSTR* pCurrentTrackDuration);
    STDMETHOD(get_CurrentMediaDuration)(BSTR* pCurrentMediaDuration);
    STDMETHOD(get_CurrentTrackMetaData)(BSTR* pCurrentTrackMetaData);
    STDMETHOD(get_CurrentTrackURI)(BSTR* pCurrentTrackURI);
    STDMETHOD(get_AVTransportURI)(BSTR* pAVTransportURI);
    STDMETHOD(get_AVTransportURIMetaData)(BSTR* pAVTransportURIMetaData);
    STDMETHOD(get_NextAVTransportURI)(BSTR* pNextAVTransportURI);
    STDMETHOD(get_NextAVTransportURIMetaData)(BSTR* pNextAVTransportURIMetaData);
    STDMETHOD(get_RelativeTimePosition)(BSTR* pRelativeTimePosition);
    STDMETHOD(get_AbsoluteTimePosition)(BSTR* pAbsoluteTimePosition);
    STDMETHOD(get_RelativeCounterPosition)(long* pRelativeCounterPosition);
    STDMETHOD(get_AbsoluteCounterPosition)(long* pAbsoluteCounterPosition);
    STDMETHOD(get_CurrentTransportActions)(BSTR* pCurrentTransportActions);
    STDMETHOD(get_LastChange)(BSTR* pLastChange);

    STDMETHOD(get_A_ARG_TYPE_SeekMode)(BSTR* pA_ARG_TYPE_SeekMode);
    STDMETHOD(get_A_ARG_TYPE_SeekTarget)(BSTR* pA_ARG_TYPE_SeekTarget);
    STDMETHOD(get_A_ARG_TYPE_InstanceID)(unsigned long* pA_ARG_TYPE_InstanceID);

    STDMETHOD(SetAVTransportURI)(unsigned long InstanceID,
                                 BSTR           CurrentURI,
                                 BSTR           CurrentURIMetaData);
    STDMETHOD(SetNextAVTransportURI)(unsigned long InstanceID,
                                     BSTR          NextURI,
                                     BSTR          NextURIMetaData);
    STDMETHOD(GetMediaInfo)(unsigned long InstanceID,
                            unsigned long* pNrTracks,
                            BSTR*          pMediaDuration,
                            BSTR*          pCurrentURI,
                            BSTR*          pCurrentURIMetaData,
                            BSTR*          pNextURI,
                            BSTR*          pNextURIMetaData,
                            BSTR*          pPlayMedium,
                            BSTR*          pRecordMedium,
                            BSTR*          pWriteStatus);
    STDMETHOD(GetTransportInfo)(unsigned long InstanceID,
                                BSTR* pCurrentTransportState,
                                BSTR* pCurrentTransportStatus,
                                BSTR* pCurrentSpeed);
    STDMETHOD(GetPositionInfo)(unsigned long InstanceID,
                               unsigned long* pTrack,
                               BSTR*          pTrackDuration,
                               BSTR*          pTrackMetaData,
                               BSTR*          pTrackURI,
                               BSTR*          pRelTime,
                               BSTR*          pAbsTime,
                               long*          pRelCount,
                               long*          pAbsCount);
    STDMETHOD(GetDeviceCapabilities)(unsigned long InstanceID,
                                     BSTR* pPlayMedia,
                                     BSTR* pRecMedia,
                                     BSTR* pRecQualityModes);
    STDMETHOD(GetTransportSettings)(unsigned long InstanceID,
                                    BSTR* pPlayMode,
                                    BSTR* pRecQualityMode);
    STDMETHOD(Stop)(unsigned long InstanceID);
    STDMETHOD(Play)(unsigned long InstanceID,
                    BSTR          Speed);
    STDMETHOD(Pause)(unsigned long InstanceID);
    STDMETHOD(Record)(unsigned long InstanceID);
    STDMETHOD(Seek)(unsigned long InstanceID,
                    BSTR          Unit,
                    BSTR          Target);
    STDMETHOD(Next)(unsigned long InstanceID);
    STDMETHOD(Previous)(unsigned long InstanceID);
    STDMETHOD(SetPlayMode)(unsigned long InstanceID,
                           BSTR          NewPlayMode);
    STDMETHOD(SetRecordQualityMode)(unsigned long InstanceID,
                                    BSTR          NewRecordQualityMode);
    STDMETHOD(GetCurrentTransportActions)(unsigned long InstanceID,
                                          BSTR* pActions);

private:
    HRESULT InitErrDescrips();
    HRESULT InitToolkitErrs();
};


class RenderingControlServiceImpl :
    public IUPnPService_RenderingControl1,
    public VirtualServiceSupport<IRenderingControl>
{
public:
    RenderingControlServiceImpl();
    virtual ~RenderingControlServiceImpl();

// IUPnPService_RenderingControl1
public:
    STDMETHOD(get_PresetNameList)(BSTR* pPresetNameList);
    STDMETHOD(get_LastChange)(BSTR* pLastChange);
    STDMETHOD(get_Brightness)(unsigned short* pBrightness);
    STDMETHOD(get_Contrast)(unsigned short* pContrast);
    STDMETHOD(get_Sharpness)(unsigned short* pSharpness);
    STDMETHOD(get_RedVideoGain)(unsigned short* pRedVideoGain);
    STDMETHOD(get_GreenVideoGain)(unsigned short* pGreenVideoGain);
    STDMETHOD(get_BlueVideoGain)(unsigned short* pBlueVideoGain);
    STDMETHOD(get_RedVideoBlackLevel)(unsigned short* pRedVideoBlackLevel);
    STDMETHOD(get_GreenVideoBlackLevel)(unsigned short* pGreenVideoBlackLevel);
    STDMETHOD(get_BlueVideoBlackLevel)(unsigned short* pBlueVideoBlackLevel);
    STDMETHOD(get_ColorTemperature)(unsigned short* pColorTemperature);
    STDMETHOD(get_HorizontalKeystone)(short* pHorizontalKeystone);
    STDMETHOD(get_VerticalKeystone)(short* pVerticalKeystone);
    STDMETHOD(get_Mute)(VARIANT_BOOL* pMute);
    STDMETHOD(get_Volume)(unsigned short* pVolume);
    STDMETHOD(get_VolumeDB)(short* pVolumeDB);
    STDMETHOD(get_Loudness)(VARIANT_BOOL* pLoudness);

    STDMETHOD(get_A_ARG_TYPE_Channel)(BSTR* pA_ARG_TYPE_Channel);
    STDMETHOD(get_A_ARG_TYPE_InstanceID)(unsigned long * pA_ARG_TYPE_InstanceID);
    STDMETHOD(get_A_ARG_TYPE_PresetName)(BSTR* pA_ARG_TYPE_PresetName);

    STDMETHOD(ListPresets)(unsigned long InstanceID, 
                           BSTR* pCurrentPresetNameList);
    STDMETHOD(SelectPreset)(unsigned long InstanceID,
                            BSTR          PresetName);
    STDMETHOD(GetBrightness)(unsigned long InstanceID,
                             unsigned short* pCurrentBrightness);
    STDMETHOD(SetBrightness)(unsigned long   InstanceID,
                             unsigned short  DesiredBrightness);
    STDMETHOD(GetContrast)(unsigned long InstanceID,
                           unsigned short* pCurrentContrast);
    STDMETHOD(SetContrast)(unsigned long  InstanceID,
                           unsigned short DesiredContrast);
    STDMETHOD(GetSharpness)(unsigned long InstanceID,
                            unsigned short* pCurrentSharpness);
    STDMETHOD(SetSharpness)(unsigned long  InstanceID,
                            unsigned short DesiredSharpness);
    STDMETHOD(GetRedVideoGain)(unsigned long InstanceID,
                               unsigned short* pCurrentRedVideoGain);
    STDMETHOD(SetRedVideoGain)(unsigned long  InstanceID,
                               unsigned short DesiredRedVideoGain);
    STDMETHOD(GetGreenVideoGain)(unsigned long InstanceID,
                                 unsigned short* pCurrentGreenVideoGain);
    STDMETHOD(SetGreenVideoGain)(unsigned long  InstanceID,
                                 unsigned short DesiredGreenVideoGain);
    STDMETHOD(GetBlueVideoGain)(unsigned long InstanceID,
                                unsigned short* pCurrentBlueVideoGain);
    STDMETHOD(SetBlueVideoGain)(unsigned long  InstanceID,
                                unsigned short DesiredBlueVideoGain);
    STDMETHOD(GetRedVideoBlackLevel)(unsigned long InstanceID,
                                     unsigned short* pCurrentRedVideoBlackLevel);
    STDMETHOD(SetRedVideoBlackLevel)(unsigned long InstanceID,
                                     unsigned short DesiredRedVideoBlackLevel);
    STDMETHOD(GetGreenVideoBlackLevel)(unsigned long InstanceID,
                                       unsigned short* pCurrentGreenVideoBlackLevel);
    STDMETHOD(SetGreenVideoBlackLevel)(unsigned long  InstanceID,
                                       unsigned short DesiredGreenVideoBlackLevel);
    STDMETHOD(GetBlueVideoBlackLevel)(unsigned long InstanceID,
                                      unsigned short* pCurrentBlueVideoBlackLevel);
    STDMETHOD(SetBlueVideoBlackLevel)(unsigned long  InstanceID,
                                      unsigned short DesiredBlueVideoBlackLevel);
    STDMETHOD(GetColorTemperature)(unsigned long InstanceID,
                                   unsigned short* pCurrentColorTemperature);
    STDMETHOD(SetColorTemperature)(unsigned long  InstanceID,
                                   unsigned short DesiredColorTemperature);
    STDMETHOD(GetHorizontalKeystone)(unsigned long InstanceID,
                                     short* pCurrentHorizontalKeystone);
    STDMETHOD(SetHorizontalKeystone)(unsigned long InstanceID,
                                     short         DesiredHorizontalKeystone);
    STDMETHOD(GetVerticalKeystone)(unsigned long InstanceID,
                                   short* pCurrentVerticalKeystone);
    STDMETHOD(SetVerticalKeystone)(unsigned long InstanceID,
                                   short DesiredVerticalKeystone);
    STDMETHOD(GetMute)(unsigned long InstanceID,
                       BSTR          Channel,
                       VARIANT_BOOL* pCurrentMute);
    STDMETHOD(SetMute)(unsigned long InstanceID,
                       BSTR          Channel,
                       VARIANT_BOOL  DesiredMute);
    STDMETHOD(GetVolume)(unsigned long InstanceID,
                         BSTR          Channel,
                         unsigned short* pCurrentVolume);
    STDMETHOD(SetVolume)(unsigned long  InstanceID,
                         BSTR           Channel,
                         unsigned short DesiredVolume);
    STDMETHOD(GetVolumeDB)(unsigned long InstanceID,
                           BSTR          Channel,
                           short* pCurrentVolume);
    STDMETHOD(SetVolumeDB)(unsigned long InstanceID,
                           BSTR          Channel,
                           short         DesiredVolume);
    STDMETHOD(GetVolumeDBRange)(unsigned long InstanceID,
                                BSTR          Channel,
                                short* pMinValue,
                                short* pMaxValue);
    STDMETHOD(GetLoudness)(unsigned long InstanceID,
                           BSTR          Channel,
                           VARIANT_BOOL* pCurrentLoudness);
    STDMETHOD(SetLoudness)(unsigned long InstanceID,
                           BSTR          Channel,
                           VARIANT_BOOL  DesiredLoudness);

private:
    HRESULT InitErrDescrips();
    HRESULT InitToolkitErrs();
};


class ConnectionManagerServiceImpl :
    public IUPnPService_ConnectionManager1,
    public IEventSinkSupport,
    public IUPnPEventSource
{
public:
    ConnectionManagerServiceImpl();
    virtual ~ConnectionManagerServiceImpl();

    // Init() must be called before using this class
    DWORD Init(/* [in] */ IConnectionManager*           pIConnectionManager,
               /* [in] */ AVTransportServiceImpl*       pAVTransportService,
               /* [in] */ RenderingControlServiceImpl*  pRenderingControlService);

// IUPnPEventSource
public:
    STDMETHOD(Advise)(
        /*[in]*/ IUPnPEventSink* punkSubscriber);
    STDMETHOD(Unadvise)(
        /*[in]*/ IUPnPEventSink* punkSubscriber);


// IUPnPService_ConnectionManager1
public:
    STDMETHOD(get_SourceProtocolInfo)(BSTR* pSourceProtocolInfo);
    STDMETHOD(get_SinkProtocolInfo)(BSTR* pSinkProtocolInfo);
    STDMETHOD(get_CurrentConnectionIDs)(BSTR* pCurrentConnectionIDs);

    STDMETHOD(get_A_ARG_TYPE_ConnectionStatus)(BSTR* pA_ARG_TYPE_ConnectionStatus);
    STDMETHOD(get_A_ARG_TYPE_ConnectionManager)(BSTR* pA_ARG_TYPE_ConnectionManager);
    STDMETHOD(get_A_ARG_TYPE_Direction)(BSTR* pA_ARG_TYPE_Direction);
    STDMETHOD(get_A_ARG_TYPE_ProtocolInfo)(BSTR* pA_ARG_TYPE_ProtocolInfo);
    STDMETHOD(get_A_ARG_TYPE_ConnectionID)(long* pA_ARG_TYPE_ConnectionID);
    STDMETHOD(get_A_ARG_TYPE_AVTransportID)(long* pA_ARG_TYPE_AVTransportID);
    STDMETHOD(get_A_ARG_TYPE_RcsID)(long* pA_ARG_TYPE_RcsID);

    STDMETHOD(GetProtocolInfo)(BSTR* pSource,
                               BSTR* pSink);
    STDMETHOD(PrepareForConnection)(BSTR RemoteProtocolInfo,
                                    BSTR PeerConnectionManager,
                                    long PeerConnectionID,
                                    BSTR Direction,
                                    long* pConnectionID,
                                    long* pAVTransportID,
                                    long* pRcsID);
    STDMETHOD(ConnectionComplete)(long ConnectionID);
    STDMETHOD(GetCurrentConnectionIDs)(BSTR* pConnectionIDs);
    STDMETHOD(GetCurrentConnectionInfo)(long ConnectionID,
                                        long* pRcsID,
                                        long* pAVTransportID,
                                        BSTR* pProtocolInfo,
                                        BSTR* pPeerConnectionManager,
                                        long* pPeerConnectionID,
                                        BSTR* pDirection,
                                        BSTR* pStatus);

protected:
    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult);
        
protected:
    struct InstanceIDs
    {
        long AVTID;
        long RCID;
    };


    AVTransportServiceImpl*         m_pAVTS;
    RenderingControlServiceImpl*    m_pRCS;

    IConnectionManager*             m_pIConnectionManager;

    typedef ce::hash_map<long, InstanceIDs> IDMap; // map from ConnectionID -> InstanceIDs
    IDMap                           m_mapIDs;

    UPnPErrorReporting              m_ErrReport;

private:
    HRESULT InitErrDescrips();
    HRESULT InitToolkitErrs();
    void    InitDISPIDs();

    HRESULT generic_get_ProtocolInfo(BSTR* pSource, BSTR* pSink);
};


class ContentDirectoryServiceImpl :
    public IUPnPService_ContentDirectory1,
    public ModeratedEventSupport,
    public IEventSinkSupport
{
public:
    ContentDirectoryServiceImpl();
    virtual ~ContentDirectoryServiceImpl();

    // Init() must be called before using this class
    DWORD Init(/* [in] */ IContentDirectory* pContentDirectory);
    
// IEventSink
public:
    virtual DWORD OnStateChanged(
        /*[in]*/ LPCWSTR pszStateVariableName,
        /*[in*/  LPCWSTR pszValue);
        
    virtual DWORD OnStateChanged(
        /*[in]*/ LPCWSTR pszStateVariableName,
        /*[in*/  long nValue);

    virtual DWORD OnStateChanged(
        /*[in]*/  LPCWSTR pszStateVariableName,
        /*[in]*/  LPCWSTR pszChannel,
        /*[in]*/  long nValue);

// TimedEventCallee
public:
    virtual void TimedEventCall();

// IUPnPService_ContentDirectory1
public:
    STDMETHOD(get_TransferIDs)(BSTR* pTransferIDs);

    STDMETHOD(get_A_ARG_TYPE_ObjectID)(BSTR* pA_ARG_TYPE_ObjectID);
    STDMETHOD(get_A_ARG_TYPE_Result)(BSTR* pA_ARG_TYPE_Result);
    STDMETHOD(get_A_ARG_TYPE_SearchCriteria)(BSTR* pA_ARG_TYPE_SearchCriteria);
    STDMETHOD(get_A_ARG_TYPE_BrowseFlag)(BSTR* pA_ARG_TYPE_BrowseFlag);
    STDMETHOD(get_A_ARG_TYPE_Filter)(BSTR* pA_ARG_TYPE_Filter);
    STDMETHOD(get_A_ARG_TYPE_SortCriteria)(BSTR* pA_ARG_TYPE_SortCriteria);
    STDMETHOD(get_A_ARG_TYPE_Index)(unsigned long* pA_ARG_TYPE_Index);
    STDMETHOD(get_A_ARG_TYPE_Count)(unsigned long* pA_ARG_TYPE_Count);
    STDMETHOD(get_A_ARG_TYPE_UpdateID)(unsigned long* pA_ARG_TYPE_UpdateID);
    STDMETHOD(get_A_ARG_TYPE_TransferID)(unsigned long* pA_ARG_TYPE_TransferID);
    STDMETHOD(get_A_ARG_TYPE_TransferStatus)(BSTR* pA_ARG_TYPE_TransferStatus);
    STDMETHOD(get_A_ARG_TYPE_TransferLength)(BSTR* pA_ARG_TYPE_TransferLength);
    STDMETHOD(get_A_ARG_TYPE_TransferTotal)(BSTR* pA_ARG_TYPE_TransferTotal);
    STDMETHOD(get_A_ARG_TYPE_TagValueList)(BSTR* pA_ARG_TYPE_TagValueList);
    STDMETHOD(get_A_ARG_TYPE_URI)(BSTR* pA_ARG_TYPE_URI);

    STDMETHOD(get_SearchCapabilities)(BSTR* pSearchCapabilities);
    STDMETHOD(get_SortCapabilities)(BSTR* pSortCapabilities);
    STDMETHOD(get_SystemUpdateID)(unsigned long* pSystemUpdateID);
    STDMETHOD(get_ContainerUpdateIDs)(BSTR* pContainerUpdateIDs);

    STDMETHOD(GetSearchCapabilities)(BSTR* pSearchCaps);
    STDMETHOD(GetSortCapabilities)(BSTR* pSortCaps);
    STDMETHOD(GetSystemUpdateID)(unsigned long* pId);
    STDMETHOD(Browse)(BSTR          ObjectID,
                      BSTR          BrowseFlag,
                      BSTR          Filter,
                      unsigned long StartingIndex,
                      unsigned long RequestedCount,
                      BSTR          SortCriteria,
                      BSTR*          pResult,
                      unsigned long* pNumberReturned,
                      unsigned long* pTotalMatches,
                      unsigned long* pUpdateID);
    STDMETHOD(Search)(BSTR          ContainerID,
                      BSTR          SearchCriteria,
                      BSTR          Filter,
                      unsigned long StartingIndex,
                      unsigned long RequestedCount,
                      BSTR          SortCriteria,
                      BSTR*          pResult,
                      unsigned long* pNumberReturned,
                      unsigned long* pTotalMatches,
                      unsigned long* pUpdateID);
    STDMETHOD(CreateObject)(BSTR ContainerID,
                            BSTR Elements,
                            BSTR* pObjectID,
                            BSTR* pResult);
    STDMETHOD(DestroyObject)(BSTR ObjectID);
    STDMETHOD(UpdateObject)(BSTR ObjectID,
                            BSTR CurrentTagValue,
                            BSTR NewTagValue);
    STDMETHOD(ImportResource)(BSTR SourceURI,
                              BSTR DestinationURI,
                              unsigned long* pTransferID);
    STDMETHOD(ExportResource)(BSTR SourceURI,
                              BSTR DestinationURI,
                              unsigned long* pTransferID);
    STDMETHOD(StopTransferResource)(unsigned long TransferID);
    STDMETHOD(GetTransferProgress)(unsigned long TransferID,
                                   BSTR* pTransferStatus,
                                   BSTR* pTransferLength,
                                   BSTR* pTransferTotal);
    STDMETHOD(DeleteResource)(BSTR ResourceURI);
    STDMETHOD(CreateReference)(BSTR ContainerID,
                               BSTR ObjectID,
                               BSTR* pNewID);

protected:
    virtual DWORD InvokeVendorAction(
        /* [in] */ LPCWSTR pszActionName,
        /* [in] */ DISPPARAMS* pdispparams, 
        /* [in, out] */ VARIANT* pvarResult);

protected:
    IContentDirectory*      m_pContentDirectory;
    UPnPErrorReporting      m_ErrReport;

private:
    HRESULT InitErrDescrips();
    HRESULT InitToolkitErrs();
    void    InitDISPIDs();

    typedef ce::hash_map<wstring, wstring> ContainerUpdateIDsMap;
        
    ce::critical_section    m_csEvents;
    wstring                 m_strTransferIDs;
    bool                    m_bSystemUpdateIDChanged;
    ContainerUpdateIDsMap   m_mapContainerUpdateIDs;
};


//
// IDispatchVendorActionsImpl
//
template <class T, const IID* piid, const GUID* plibid, WORD wMajor = 1, WORD wMinor = 0, class tihclass = CComTypeInfoHolder>
class ATL_NO_VTABLE IDispatchVendorActionsImpl : public IDispatchImpl<T, piid, plibid, wMajor, wMinor, tihclass>
{
    typedef IDispatchImpl<T, piid, plibid, wMajor, wMinor, tihclass> _Mybase;
    
    static const DISPID nVendorActionDispidBase = 0x10000000;
    
public:
    // GetIDsOfNames
    STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR* rgszNames, UINT cNames,
                             LCID lcid, DISPID* rgdispid)
    {
        HRESULT hr = _Mybase::GetIDsOfNames(riid, rgszNames, cNames, lcid, rgdispid);
        
        if(DISP_E_MEMBERNOTFOUND == hr || DISP_E_UNKNOWNNAME == hr)
        {
            unsigned i;
            
            // Only support query for action name, not named parameters
            assert(cNames == 1);
            
            // Look for action with given name
            for(i = 0; i < m_VendorActions.size(); ++i)
            {
                if(m_VendorActions[i] == *rgszNames)
                {
                    rgdispid[0] = i + nVendorActionDispidBase;
                    return S_OK;
                }
            }
            
            //
            // Add the action to the array of vendor actions.
            // Note that UPnP host will only call GetIDsOfNames for
            // actions that are in the service description so there is 
            // no danger of growing m_VendorActions array indefinitly.
            //
            if(m_VendorActions.push_back(*rgszNames))
            {
                rgdispid[0] = i + nVendorActionDispidBase;
                return S_OK;
            }
            else
                return E_OUTOFMEMORY;
        }
        
        return hr;
    }
    
    // Invoke
    STDMETHOD(Invoke)(DISPID dispidMember, REFIID riid,
                      LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult,
                      EXCEPINFO* pexcepinfo, UINT* puArgErr)
    {
        if(dispidMember >= nVendorActionDispidBase)
        {
            assert((DISPID)m_VendorActions.size() > dispidMember - nVendorActionDispidBase);
            
            // Invoke vendor action by name
            DWORD dw = InvokeVendorAction(m_VendorActions[dispidMember - nVendorActionDispidBase], pdispparams, pvarResult);
            
            if(SUCCESS_AV != dw)
            {
                CComPtr<IErrorInfo> pErrorInfo;
                HRESULT             hr = m_ErrReport.ReportError(dw);
                
                // Fill pexcepinfo if the action failed
                GetErrorInfo(0, &pErrorInfo);
                
                if(pErrorInfo)
                {
                    pErrorInfo->GetDescription(&pexcepinfo->bstrDescription);
                    pErrorInfo->GetSource(&pexcepinfo->bstrSource);
                }
            }
            
            return S_OK;
        }
        else
            return _Mybase::Invoke(dispidMember, riid, lcid,
                                   wFlags, pdispparams, pvarResult, pexcepinfo, puArgErr);
    }
    
protected:
    ce::vector<ce::wistring> m_VendorActions;
};


//
// COM classes implementing UPnP services
//

class ATL_NO_VTABLE AVTransportService :
    public CComObjectRootEx<CComMultiThreadModel>,
    public IDispatchVendorActionsImpl<AVTransportServiceImpl, 
                         &IID_IUPnPService_AVTransport1, 
                         &LIBID_UPNPAVTOOLKITlib>
{
    BEGIN_COM_MAP(AVTransportService)
        COM_INTERFACE_ENTRY(IUPnPService_AVTransport1)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY(IUPnPEventSource)
    END_COM_MAP()
};

class ATL_NO_VTABLE RenderingControlService :
    public CComObjectRootEx<CComMultiThreadModel>,
    public IDispatchVendorActionsImpl<RenderingControlServiceImpl,
                         &IID_IUPnPService_RenderingControl1,
                         &LIBID_UPNPAVTOOLKITlib>
{
    BEGIN_COM_MAP(RenderingControlService)
        COM_INTERFACE_ENTRY(IUPnPService_RenderingControl1)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY(IUPnPEventSource)
    END_COM_MAP()
};

class ATL_NO_VTABLE ConnectionManagerService :
    public CComObjectRootEx<CComMultiThreadModel>,
    public IDispatchVendorActionsImpl<ConnectionManagerServiceImpl,
                         &IID_IUPnPService_ConnectionManager1, 
                         &LIBID_UPNPAVTOOLKITlib>
{   
    BEGIN_COM_MAP(ConnectionManagerService)
        COM_INTERFACE_ENTRY(IUPnPService_ConnectionManager1)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY(IUPnPEventSource)
    END_COM_MAP()
};



class ATL_NO_VTABLE ContentDirectoryService :
    public CComObjectRootEx<CComMultiThreadModel>,
    public IDispatchVendorActionsImpl<ContentDirectoryServiceImpl,
                         &IID_IUPnPService_ContentDirectory1, 
                         &LIBID_UPNPAVTOOLKITlib>
{
    BEGIN_COM_MAP(ContentDirectoryService)
        COM_INTERFACE_ENTRY(IUPnPService_ContentDirectory1)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY(IUPnPEventSource)
    END_COM_MAP()
};


// Forward declarations for MediaRendererDevice and MediaServerDevice
namespace details
{
    class ConnectionManagerCtrl;
    class ContentDirectoryCtrl;
};


//
// MediaRendererDevice
//
class MediaRendererDevice
{
public:
    MediaRendererDevice(IUPnPDevice* pDevice);
    ~MediaRendererDevice();

    DWORD GetConnectionManager(
            IConnectionManager** ppConnectionManager,
            wstring*             pstrConnectionmanager);

private:
    ce::critical_section            m_cs;
    CComPtr<IUPnPDevice>            m_pDevice;
    details::ConnectionManagerCtrl* m_pConnectionManager;
    wstring                         m_strUDN;
};


//
// MediaServerDevice
//
class MediaServerDevice
{
public:
    MediaServerDevice(IUPnPDevice* pDevice);
    ~MediaServerDevice();

    DWORD GetConnectionManager(
            IConnectionManager** ppIConnectionManager,
            wstring*             pstrIConnectionManager);

    DWORD GetContentDirectory(
            IContentDirectory** ppContentDirectory);

private:
    ce::critical_section            m_cs;
    CComPtr<IUPnPDevice>            m_pDevice;
    details::ConnectionManagerCtrl* m_pConnectionManager;
    details::ContentDirectoryCtrl*  m_pContentDirectory;
    wstring                         m_strUDN;
};

namespace DIDL_Lite
{

namespace details
{
    class properties;
    class objects;
};

//
// object
//
class object
{
public:
    object();
    ~object();
    
    //
    // true if the object is of a container class (object.container.*), 
    // false if the object is of an item class (object.item.*)
    //
    bool            bContainer;

    //
    // required object properties
    //
    wstring         strID;
    wstring         strParentID;
    wstring         strTitle;
    wstring         strClass;
    bool            bRestricted;
    
    //
    // access to optional object properties
    //
    bool GetProperty(const WCHAR *pszName, wstring* pValue, unsigned long nIndex = 0);
    bool GetProperty(const WCHAR *pszName, WCHAR *pValue, unsigned int cchValue, unsigned long nIndex = 0);
    bool GetProperty(const WCHAR *pszName, unsigned long* pValue, unsigned long nIndex = 0);
    bool GetProperty(const WCHAR *pszName, signed long* pValue, unsigned long nIndex = 0);
    bool GetProperty(const WCHAR *pszName, bool* pValue, unsigned long nIndex = 0);

    // Used by details::objects to set the ref to the properties
    void SetPropertiesRef(details::properties *pPropertiesRef) { m_pPropertiesRef = pPropertiesRef; }
    
private:
    details::properties *m_pPropertiesRef;
};


//
// parser
//
class parser
{
public:
    parser();
    ~parser();
    
    bool GetFirstObject(LPCWSTR pszXml, object* pObj);
    bool GetNextObject(object* pObj);
    
    bool AddNamespaceMapping(LPCWSTR pszNamespace, LPCWSTR pszPrefix);

private:
    details::objects* m_pObjects;
};

}; // namespace DIDL

//
// String constants
//

const LPCWSTR
    NotImpl                 = L"NOT_IMPLEMENTED",
    OK                      = L"OK";

namespace TransportState
{
    const LPCWSTR 
        Stopped             = L"STOPPED",
        Playing             = L"PLAYING",
        Transitioning       = L"TRANSITIONING",
        PausedPlayback      = L"PAUSED_PLAYBACK",
        PausedRecording     = L"PAUSED_RECORDING",
        Recording           = L"RECORDING",
        NoMedia             = L"NO_MEDIA_PRESENT";
};


namespace TransportAction 
{
    const LPCWSTR
        Play                = L"Play",
        Stop                = L"Stop",
        Pause               = L"Pause", 
        Seek                = L"Seek", 
        Next                = L"Next", 
        Previous            = L"Previous",
        Record              = L"Record";
};


namespace PlayMode
{
    const LPCWSTR
        Normal              = L"NORMAL",
        Shuffle             = L"SHUFFLE",
        RepeatOne           = L"REPEAT_ONE",
        RepeatAll           = L"REPEAT_ALL",
        Random              = L"RANDOM",
        Direct1             = L"DIRECT_1",
        Intro               = L"INTRO";
};


namespace Medium
{
    const LPCWSTR
        Unknown             = L"UNKNOWN",
        Dv                  = L"DV",
        Mini                = L"MINI-DV",
        VHS                 = L"VHS",
        W_VHS               = L"W-VHS",
        S_VHS               = L"S-VHS",
        D_VHS               = L"D-VHS",
        VHSC                = L"VHSC",
        Video8              = L"VIDEO8",
        Hi8                 = L"HI8",
        CD_ROM              = L"CD-ROM",
        CD_DA               = L"CD-DA",
        CD_R                = L"CD-R",
        CD_RW               = L"CD-RW",
        Video               = L"VIDEO-CD",
        Sacd                = L"SACD",
        MD_Audio            = L"MD-AUDIO",
        MD_Picture          = L"MD-PICTURE",
        DVD_ROM             = L"DVD-ROM",
        DVD_Video           = L"DVD-VIDEO",
        DVD_R               = L"DVD-R",
        DVD_Plus_RW         = L"DVD+RW",
        DVD_Minus_RW        = L"DVD-RW",
        DVD_RAM             = L"DVD-RAM",
        DVD_Audio           = L"DVD-AUDIO",
        DAT                 = L"DAT",
        LD                  = L"LD",
        Hdd                 = L"HDD",
        Micro               = L"MICRO-MV",
        Network             = L"NETWORK",
        None                = L"NONE";
};

namespace SeekMode
{
    const LPCWSTR
        TrackNr             = L"TRACK_NR",
        AbsTime             = L"ABS_TIME",
        RelTime             = L"REL_TIME",
        AbsCount            = L"ABS_COUNT",
        RelCount            = L"REL_COUNT",
        ChannelFreq         = L"CHANNEL_FREQ",
        TapeIndex           = L"TAPE_INDEX",
        Frame               = L"FRAME";
};


namespace AVTransportState
{
    const LPCWSTR 
        // TransportInfo
        TransportState          = L"TransportState",
        TransportStatus         = L"TransportStatus",
        PlaySpeed               = L"TransportPlaySpeed",

        // PositionInfo
        CurrentTrack            = L"CurrentTrack",
        CurrentTrackDuration    = L"CurrentTrackDuration",
        CurrentTrackMetaData    = L"CurrentTrackMetaData",
        CurrentTrackURI         = L"CurrentTrackURI",
        RelTime                 = L"RelativeTimePosition",
        AbsTime                 = L"AbsoluteTimePosition",
        RelCounter              = L"RelativeCounterPosition",
        AbsCounter              = L"AbsoluteCounterPosition",

        // DeviceCapabilities
        PossiblePlayMedia       = L"PossiblePlaybackStorageMedia",
        PossibleRecMedia        = L"PossibleRecordStorageMedia",
        PossibleRecQualityModes = L"PossibleRecordQualityModes",

        // TransportSettings
        PlayMode                = L"CurrentPlayMode",
        RecQualityMode          = L"CurrentRecordQualityMode",
        
        // CurrentTransportActions
        CurrentTransportActions = L"CurrentTransportActions",

        // MediaInfo
        NrTracks                = L"NumberOfTracks",
        MediaDuration           = L"CurrentMediaDuration",
        CurrentURI              = L"AVTransportURI",
        CurrentURIMetaData      = L"AVTransportURIMetaData",
        NextURI                 = L"NextAVTransportURI",
        NextURIMetaData         = L"NextAVTransportURIMetaData",
        PlaybackMedia           = L"PlaybackStorageMedium",
        RecordMedia             = L"RecordStorageMedium",
        WriteStatus             = L"RecordMediumWriteStatus";
};

namespace RenderingControlState
{
    const LPCWSTR
        PresetNameList          = L"PresetNameList",
        Brightness              = L"Brightness",
        Contrast                = L"Contrast",
        Sharpness               = L"Sharpness",
        RedVideoGain            = L"RedVideoGain",
        GreenVideoGain          = L"GreenVideoGain",
        BlueVideoGain           = L"BlueVideoGain",
        RedVideoBlackLevel      = L"RedVideoBlackLevel",
        GreenVideoBlackLevel    = L"GreenVideoBlackLevel",
        BlueVideoBlackLevel     = L"BlueVideoBlackLevel",
        ColorTemperature        = L"ColorTemperature",
        HorizontalKeystone      = L"HorizontalKeystone",
        VerticalKeystone        = L"VerticalKeystone",
        Mute                    = L"Mute",
        Volume                  = L"Volume",
        VolumeDB                = L"VolumeDB",
        Loudness                = L"Loudness";
};

namespace ContentDirectoryState
{
    const LPCWSTR
        TransferIDs             = L"TransferIDs",
        SystemUpdateID          = L"SystemUpdateID",
        ContainerUpdateIDs      = L"ContainerUpdateIDs";
};

namespace ConnectionManagerState
{
    const LPCWSTR
        SourceProtocolInfo      = L"SourceProtocolInfo",
        SinkProtocolInfo        = L"SinkProtocolInfo",
        CurrentConnectionIDs    = L"CurrentConnectionIDs";
};

}; // namespace av_upnp

#include "av_upnp_impl.h"


#ifndef NO_UPNPAV_AV_ALIAS
namespace av = av_upnp;
#endif

#endif // __AV_UPNP_H
