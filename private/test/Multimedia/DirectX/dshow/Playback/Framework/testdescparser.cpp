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
////////////////////////////////////////////////////////////////////////////////

#include "logging.h"
#include "globals.h"
#include "MMTestParser.h"
#include "TestDescParser.h"
#include "TestDesc.h"
#include "StringConversions.h"
#include "Operation.h"
#include "TuxGraphTest.h"
#include "Verification.h"
#include "DxXML.h"

// extern global varialbes from test DLLs
extern GraphFunctionTableEntry g_lpLocalFunctionTable[];
extern int g_nLocalTestFunctions;

// static function
HRESULT 
ParsePlaybackMediaList( CXMLElement *pElement, PBMediaList* pMediaList );
HRESULT 
ParsePlaybackMediaListFile( TCHAR* szMediaListFile, PBMediaList* pMediaList );

HRESULT 
ParseMediaMeta( CXMLElement *pEleMedia, PlaybackMedia* pMedia );

HRESULT 
ParseMediaMetaData( CXMLElement *pEleMedia, PlaybackMedia* pMedia );

HRESULT 
ParseASXMetaData( CXMLElement *pEleMedia, PlaybackMedia* pMedia );

HRESULT 
ParseASXEntryData( CXMLElement *pEleMedia, PlaybackMedia* pMedia );


const char* STR_MediaList = "MediaList";
const char* STR_NameAttr = "Name";
const char* STR_TestDesc = "Test";

const char* const STR_Tolerance = "Tolerance";
const char* const STR_GraphDesc = "GraphDesc";
const char* const STR_Url = "Url";
const char* const STR_Desc = "Desc";
const char* const STR_FileName = "FileName";
const char* const STR_Description = "Description";
const char* const STR_IsVideo = "IsVideo";
const char* const STR_BaseUrls = "BaseUrl";
const char* const STR_PlaybackDuration = "PlaybackDuration";
const char* const STR_AudioInfo = "AudioInfo";
const char* const STR_VideoInfo = "VideoInfo";
const char* const STR_NumFrames = "Frames";
const char* const STR_Index = "Index";
const char* const STR_GDIMode = "GDI";
const char* const STR_DDrawMode = "DDRAW";
const char* const STR_Container = "Container";

const char* const STR_PS="PS";
const char* const STR_RGBOVR="RGBOVR";
const char* const STR_YUVOVR="YUVOVR";
const char* const STR_RGBOFF="RGBOFF";
const char* const STR_YUVOFF="YUVOFF";
const char* const STR_RGBFLP="RGBFLP";
const char* const STR_YUVFLP="YUVFLP";
const char* const STR_ALL="ALL";
const char* const STR_YUV="YUV";
const char* const STR_RGB="RGB";

const char* const STR_MetaData ="MetaData";
const char* const STR_ASXMetaData ="ASXMetaData";

//media meta data for IAMMediaContent  and IAMMediaContentExtest
const char* const STR_AuthorName ="MDAuthorName";
const char* const STR_Title ="MDTitle";
const char* const STR_Rating ="MDRating";
const char* const STR_MetaDescription ="MDDescription";
const char* const STR_Copyright ="MDCopyright";
const char* const STR_MetaBaseURL ="MDBaseURL";
const char* const STR_LogoURL  ="MDLogoURL";
const char* const STR_LogoIconURL  ="MDLogoIconURL";
const char* const STR_WatermarkURL ="MDWatermarkURL";
const char* const STR_MoreInfoURL ="MDMoreInfoURL";
const char* const STR_MoreInfoBannerImage ="MDMoreInfoBannerImage";
const char* const STR_MoreInfoBannerURL ="MDMoreInfoBannerURL";
const char* const STR_MoreInfoText ="MDMoreInfoText";

//ASF meta data for IAMPlayList test
const char* const STR_ASX_ItemCount ="ASXMDItemCount";
const char* const STR_ASX_RepeatCount ="ASXMDRepeatCount";
const char* const STR_ASX_RepeatStart ="ASXMDRepeatStart";
const char* const STR_ASX_RepeatEnd ="ASXMDRepeatEnd";
const char* const STR_ASX_Flags ="ASXMDFlags";
const char* const STR_ASX_Entry ="ASXMDEntry";
// for one Entry's data,used in IAMPlayList test
const char* const STR_ASX_EntrySourceCount ="ASXMDEntrySourceCount";
const char* const STR_ASX_EntrySourceURLs ="ASXMDEntrySourceURLs";
const char* const STR_ASX_EntryFlags ="ASXMDEntryFlags";


// for one Entry's data,used in MediacontentEx test
const char* const STR_ASX_EntryIndex ="ASXMDEntryIndex";
const char* const STR_ASX_EntryParamCount ="ASXMDEntryParamCount";
const char* const STR_ASX_ParamNames ="ASXMDParamNames";
const char* const STR_ASX_ParamNameValues ="ASXMDParamNameValues";

// Basic media properties
const char* const STR_MEDIA_PROP = "MediaProperties";
const char* const STR_TIME_EARLIEST = "Earliest";
const char* const STR_TIME_LATEST = "Latest";
const char* const STR_TIME_DURATION = "Duration";
const char* const STR_TIME_PREROLL = "Preroll";
const char* const STR_SEEK_CAPS = "SeekCaps";

// Performance information for video
const char* const STR_VIDEO_AVG_FRAME_RATE = "AvgFrameRate";
const char* const STR_VIDEO_AVG_SYNC_OFFSET = "AvgSyncOffset";
const char* const STR_VIDEO_DEV_SYNC_OFFSET = "DevSyncOffset";
const char* const STR_VIDEO_FRAME_DRAWN = "FramesDrawn";
const char* const STR_VIDEO_FRAME_DROPPED = "FramesDropped";
const char* const STR_VIDEO_JITTER = "Jitter";

const char SURFACE_MODE_STR_DELIMITER = ',';
const char RENDERER_MODE_STR_DELIMITER = ',';
const char STR_RECT_DELIMITER = ',';
const char STR_VERIFY_SAMPLE_DELIVERED_DATA_DELIMITER = ',';
const char STR_VERIFY_PLAYBACK_DURATION_DATA_DELIMITER = ',';
const char POSITION_LIST_DELIMITER = ',';

HRESULT 
ToState( const char* szState, FILTER_STATE* pState )
{
    HRESULT hr = S_OK;
    // Convert from string to state
    if (!strcmp(szState, STR_Stopped))
        *pState = State_Stopped;
    else if (!strcmp(szState, STR_Paused))
        *pState = State_Paused;
    else if (!strcmp(szState, STR_Running))
        *pState = State_Running;
    else {
        hr = E_FAIL;
    }

    return hr;
}


HRESULT 
ToStateChangeSequence( const char* szSequence, StateChangeSequence* pSequence )
{
    HRESULT hr = S_OK;
    // Convert from string to state
    if (!strcmp(szSequence, STR_StopPause))
        *pSequence = StopPause;
    else if (!strcmp(szSequence, STR_StopPlay))
        *pSequence = StopPlay;
    else if (!strcmp(szSequence, STR_PauseStop))
        *pSequence = PauseStop;
    else if (!strcmp(szSequence, STR_PausePlay))
        *pSequence = PausePlay;
    else if (!strcmp(szSequence, STR_PlayPause))
        *pSequence = PlayPause;
    else if (!strcmp(szSequence, STR_PlayStop))
        *pSequence = PlayStop;
    else if (!strcmp(szSequence, STR_PausePlayStop))
        *pSequence = PausePlayStop;
    else if (!strcmp(szSequence, STR_RandomSequence))
        *pSequence = RandomSequence;
    else {
        hr = E_FAIL;
    }
    return hr;
}

HRESULT 
ParseCommonTestInfo( CXMLElement *hElement, TestDesc* pTestDesc )
{
    HRESULT hr = S_OK;
    if ( !hElement || !pTestDesc ) return E_FAIL;

    PCSTR pElemName = hElement->GetName();
    if ( !pElemName ) 
        return E_FAIL;
    if ( !strcmp(STR_TestId, pElemName) )
    {
        hr = ParseDWORD( hElement, (DWORD*)&pTestDesc->testId );
        if ( FAILED(hr) )
            LOG( TEXT("%S: ERROR %d@%S - failed to parse test ID."), 
                        __FUNCTION__, __LINE__, __FILE__ );
    }
    else if ( !strcmp(STR_Media, pElemName) )
    {
        hr = ParseStringList( hElement, &pTestDesc->mediaList, MEDIALIST_DELIMITER );
        if ( FAILED(hr) )
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the media."), 
                        __FUNCTION__, __LINE__, __FILE__ );
    }
    else if ( !strcmp(STR_DownloadLocation, pElemName) )
    {
        hr = ParseString( hElement, pTestDesc->szDownloadLocation, countof(pTestDesc->szDownloadLocation));
        if ( FAILED(hr) )
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the download location."), 
                            __FUNCTION__, __LINE__, __FILE__ );
    }
    else if ( !strcmp( "DRM", pElemName) )
    {
        const char* szDRM = hElement->GetValue();
        if ( !strcmp(STR_True, szDRM) )
            pTestDesc->DRMContent( TRUE );
    }
    else if ( !strcmp( "Chamber", pElemName) )
    {
        const char* szChamber = hElement->GetValue();
        if ( !strcmp( "Secure", szChamber) )
            pTestDesc->SecureChamber( TRUE );
    }
    else if ( !strcmp( "FGM", pElemName) )
    {
        const char* szRemoteGB = hElement->GetValue();
        if ( !strcmp( "Restricted", szRemoteGB) )
            pTestDesc->RemoteGB( TRUE );
    }
    else if ( !strcmp( "ExpectedHR", pElemName) )
    {
        DWORD dwHR;
        hr = ParseDWORD( hElement, &dwHR );
        if ( FAILED(hr) )
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the expected HR."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        pTestDesc->TestExpectedHR( dwHR );
    }
    else if ( !strcmp(STR_Verification, pElemName) )
    {
        hr = ParseVerification(hElement, &pTestDesc->verificationList);
        if ( FAILED(hr) )
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the verification list."), 
                            __FUNCTION__, __LINE__, __FILE__ );
    }

    return hr;
}

HRESULT 
ParseState( CXMLElement *hState, FILTER_STATE* pState )
{
    HRESULT hr = S_OK;

    if (!pState)
        return E_POINTER;

    const char* szState = NULL;
    szState = hState->GetValue();
    if ( !szState )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse state."), __FUNCTION__, __LINE__, __FILE__ );
        hr = E_FAIL;
    }
    else {
        hr = ToState( szState, pState );
    }

    return hr;
}

HRESULT 
ParseStateList( CXMLElement *hStateList, StateList* pStateList )
{
    HRESULT hr = S_OK;
    FILTER_STATE state = State_Stopped;

    const char* szStateStringList = hStateList->GetValue();
    if ( !szStateStringList )
        return S_OK;

    char szString[MAX_PATH];

    int pos = 0;
    while( pos != -1 )
    {
        hr = ParseStringListString( szStateStringList, STR_STATE_DELIMITER, szString, countof(szString), pos );
        if ( FAILED(hr) )
            break;

        hr = ToState( szString, &state );
        if ( SUCCEEDED(hr) )
            pStateList->push_back( state );
        else
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to parse state %s."), __FUNCTION__, __LINE__, __FILE__, szString );
        }
    }
    
    return hr;
}



HRESULT 
ParseRect( CXMLElement *hRect, RECT* pRect )
{
    HRESULT hr = S_OK;

    if (!pRect)
        return E_POINTER;

    const char* szRect = hRect->GetValue();
    if ( !szRect )
        return E_FAIL;

    char szValue[8];
    int pos = 0;

    // Parse the left of the rect
    ParseStringListString( szRect, STR_RECT_DELIMITER, szValue, countof(szValue), pos );
    if ( pos == -1 )
        return E_FAIL;
    pRect->left = atol( szValue );

    // Parse the top of the rect
    ParseStringListString( szRect, STR_RECT_DELIMITER, szValue, countof(szValue), pos );
    if ( pos == -1 )
        return E_FAIL;
    pRect->top = atol( szValue );

    // Parse the width of the rect
    ParseStringListString( szRect, STR_RECT_DELIMITER, szValue, countof(szValue), pos );
    if ( pos == -1 )
        return E_FAIL;
    pRect->right = atol( szValue );

    // Parse the height of the rect
    ParseStringListString( szRect, STR_RECT_DELIMITER, szValue, countof(szValue), pos );
    pRect->bottom = atol( szValue );

    return hr;
}

HRESULT 
ParseRendererMode( const char* szRendererMode, VideoRendererMode *pVRMode )
{
    HRESULT hr = S_OK;

    char szMode[32] = "";    
    *pVRMode = 0;
    
    int pos = 0;
    while( pos != -1 )
    {
        hr = ParseStringListString( szRendererMode, 
                                    RENDERER_MODE_STR_DELIMITER, 
                                    szMode, 
                                    countof(szMode), 
                                    pos );
        if ( FAILED(hr) )
            return hr;

        if ( !strcmp(szMode, STR_GDIMode) )
            *pVRMode |= VIDEO_RENDERER_MODE_GDI;
        else if ( !strcmp(szMode, STR_DDrawMode) )
            *pVRMode |= VIDEO_RENDERER_MODE_DDRAW;
        else {
            return E_FAIL;
        }
    }

    return hr;
}

HRESULT 
ParseSurfaceType( const char* szSurfaceType, DWORD *pSurfaceType )
{
    HRESULT hr = S_OK;

    char szType[32] = "";    
    *pSurfaceType = 0;

    int pos = 0;
    while( pos != -1 )
    {
        hr = ParseStringListString( szSurfaceType, 
                                    SURFACE_MODE_STR_DELIMITER, 
                                    szType, 
                                    countof(szType), 
                                    pos );
        if ( FAILED(hr) )
            return hr;

        if ( !strcmp(szType, STR_PS) )
            *pSurfaceType |= AMDDS_PS;
        else if ( !strcmp(szType, STR_RGBOVR) )
            *pSurfaceType |= AMDDS_RGBOVR;
        else if ( !strcmp(szType, STR_YUVOVR) )
            *pSurfaceType |= AMDDS_YUVOVR;
        else if ( !strcmp(szType, STR_RGBOFF) )
            *pSurfaceType |= AMDDS_RGBOFF;
        else if ( !strcmp(szType, STR_YUVOFF) )
            *pSurfaceType |= AMDDS_YUVOFF;
        else if ( !strcmp(szType, STR_RGBFLP) )
            *pSurfaceType |= AMDDS_RGBFLP;
        else if ( !strcmp(szType, STR_YUVFLP) )
            *pSurfaceType |= AMDDS_YUVFLP;
        else if ( !strcmp(szType, STR_ALL) )
            *pSurfaceType |= AMDDS_ALL;
        else if ( !strcmp(szType, STR_YUV) )
            *pSurfaceType |= AMDDS_YUV;
        else if ( !strcmp(szType, STR_RGB) )
            *pSurfaceType |= AMDDS_RGB;
        else {
            LOG( TEXT("%S: ERROR %d@%S - unknown surface type %S."), __FUNCTION__, __LINE__, __FILE__, szType );
            return E_FAIL;
        }
    }
    
    return hr;
}


HRESULT 
ParseGraphDesc( CXMLElement *hElement, GraphDesc* pGraphDesc )
{
    HRESULT hr = S_OK;

    // Parse the string list - reading in the filters, 
    // BUGBUG: don't handle branched graphs
    hr = ParseStringList( hElement, &pGraphDesc->filterList, ':' );

    return hr;
}


HRESULT 
ParseFilterId( CXMLElement *hElement, FilterId* pFilterId )
{
    return S_OK;
}

HRESULT 
ParseFilterDesc( CXMLElement *hElement, FilterDesc* pFilterDesc )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;

    hXmlChild = hElement->GetFirstChildElement();
    if ( !hElement )
        return E_FAIL;


    hr = ParseFilterId( hXmlChild, &pFilterDesc->m_id );
    if ( FAILED(hr) )
        return hr;

    return hr;
}

HRESULT 
ParseFilterDescList( CXMLElement *hElement, FilterDescList* pFilterDescList )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;

    hXmlChild = hElement->GetFirstChildElement();
    if ( !hElement )
        return S_OK;

    int i = 0;
    while ( true )
    {
        hXmlChild = hXmlChild->GetNextSiblingElement();
        if ( !hElement )
            return S_OK;

        FilterDesc* pFilterDesc = new FilterDesc();
        if ( !pFilterDesc )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to allocate FilterDesc object when parsing."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_OUTOFMEMORY;
            break;
        }

        hr = ParseFilterDesc( hXmlChild, pFilterDesc );
        if ( FAILED(hr) )
            return hr;

        pFilterDescList->push_back( pFilterDesc );
    }
    return hr;
}



HRESULT 
ParseTimeFormatPara( CXMLElement *hElement, CMediaProp* pMediaProp, GUID format )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;

    hXmlChild = hElement->GetFirstChildElement();
    if ( !hXmlChild || !pMediaProp )
        return E_FAIL;

    while ( SUCCEEDED(hr) && hXmlChild )
    {
        if ( !strcmp( STR_TIME_EARLIEST, hXmlChild->GetName()) )
        {
            if ( TIME_FORMAT_MEDIA_TIME == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llMediaTimeEarliest );
            else if ( TIME_FORMAT_FRAME == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llFrameEarliest );
            else if ( TIME_FORMAT_FIELD == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llFieldEarliest );
            else if ( TIME_FORMAT_SAMPLE == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llSampleEarliest );
            else if ( TIME_FORMAT_BYTE == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llByteEarliest );
        }
        else if ( !strcmp(STR_TIME_LATEST, hXmlChild->GetName()) )
        {
            if ( TIME_FORMAT_MEDIA_TIME == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llMediaTimeLatest );
            else if ( TIME_FORMAT_FRAME == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llFrameLatest );
            else if ( TIME_FORMAT_FIELD == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llFieldLatest );
            else if ( TIME_FORMAT_SAMPLE == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llSampleLatest );
            else if ( TIME_FORMAT_BYTE == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llByteLatest );
        }
        else if ( !strcmp(STR_TIME_DURATION, hXmlChild->GetName()) )
        {
            if ( TIME_FORMAT_MEDIA_TIME == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llMediaTimeDuration );
            else if ( TIME_FORMAT_FRAME == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llFrameDuration );
            else if ( TIME_FORMAT_FIELD == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llFieldDuration );
            else if ( TIME_FORMAT_SAMPLE == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llSampleDuration );
            else if ( TIME_FORMAT_BYTE == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llByteDuration );
        }
        else if ( !strcmp(STR_TIME_PREROLL, hXmlChild->GetName()) )
        {
            if ( TIME_FORMAT_MEDIA_TIME == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llMediaTimePreroll );
            else if ( TIME_FORMAT_FRAME == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llFramePreroll );
            else if ( TIME_FORMAT_FIELD == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llFieldPreroll );
            else if ( TIME_FORMAT_SAMPLE == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llSamplePreroll );
            else if ( TIME_FORMAT_BYTE == format )
                hr = ParseLONGLONG( hXmlChild, &pMediaProp->m_llBytePreroll );
        }

        if ( FAILED(hr) )
            LOG( TEXT("%S: ERROR %d@%S - failed to parse the time format."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        // Get next sibling element
        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();        
    }

    return hr;
}


HRESULT 
ParseMediaProps( CXMLElement *hElement, CMediaProp* pMediaProp )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;

    hXmlChild = hElement->GetFirstChildElement();
    if ( !hXmlChild )
        return E_FAIL;

    while ( SUCCEEDED(hr) && hXmlChild )
    {    
        if ( !strcmp(STR_SEEK_CAPS, hXmlChild->GetName()) )
        {
            hr = ParseDWORD( hXmlChild, &pMediaProp->m_dwSeekCaps );
            if ( FAILED(hr) )
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the SeekCaps tag."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }        
        else if ( !strcmp(STR_TIME_MEDIA_TIME, hXmlChild->GetName()) )
        {
            hr = ParseTimeFormatPara( hXmlChild, pMediaProp, TIME_FORMAT_MEDIA_TIME );
            if ( FAILED(hr) )
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the TimeMediaTime tag."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }
        else if ( !strcmp(STR_TIME_FIELD, hXmlChild->GetName()) )
        {
            hr = ParseTimeFormatPara( hXmlChild, pMediaProp, TIME_FORMAT_FIELD );
            if ( FAILED(hr) )
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the TimeField tag."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }
        else if ( !strcmp(STR_TIME_SAMPLE, hXmlChild->GetName()) )
        {
            hr = ParseTimeFormatPara( hXmlChild, pMediaProp, TIME_FORMAT_SAMPLE );
            if ( FAILED(hr) )
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the TimeSample tag."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }
        else if ( !strcmp(STR_TIME_BYTE, hXmlChild->GetName()) )
        {
            hr = ParseTimeFormatPara( hXmlChild, pMediaProp, TIME_FORMAT_BYTE );
            if ( FAILED(hr) )
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the TimeByte tag."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }
        else if ( !strcmp(STR_TIME_FRAME, hXmlChild->GetName()) )
        {
            hr = ParseTimeFormatPara( hXmlChild, pMediaProp, TIME_FORMAT_FRAME );
            if ( FAILED(hr) )
                LOG( TEXT("%S: ERROR %d@%S - failed to parse the TimeFrame tag."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        }
        // Get next sibling element
        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();        
    }
    return hr;
}

HRESULT 
ParseTimeFormat( CXMLElement *hTimeFormat, TimeFormatList *pTimeFormatList )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;
    int listmarker = 0;

    char szTimeFormat[32];
    const char* szFormatList = NULL;
    szFormatList = hTimeFormat->GetValue();
    
    if ( !szFormatList || !pTimeFormatList )
        return E_POINTER;

    // Read the format list
    while( listmarker != -1 )
    {
        // Read the next format
        hr = ParseStringListString( szFormatList, 
                                    TIMEFORMAT_LIST_DELIMITER, 
                                    szTimeFormat, 
                                    countof(szTimeFormat), 
                                    listmarker );
        if ( FAILED(hr) )
            break;

        // TIME_FORMAT_MEDIA_TIME is tested by default and present in the list already.        
        if ( 0 == strcmp( "Frame", szTimeFormat ) )
        {
            CTimeFormat format( TIME_FORMAT_FRAME, 5, 30 );    // default tolerane is 30 frames
            pTimeFormatList->push_back( format );
        }
        else if ( 0 == strcmp( "Sample", szTimeFormat ) )
        {
            CTimeFormat format( TIME_FORMAT_SAMPLE, 1, 10 );    // default tolerance is 10 sample
            pTimeFormatList->push_back( format );
        }
        else if ( 0 == strcmp( "Field", szTimeFormat ) )
        {
            CTimeFormat format( TIME_FORMAT_FIELD );
            pTimeFormatList->push_back( format );
        }
        else if ( 0 == strcmp( "Byte", szTimeFormat ) )
        {
            CTimeFormat format( TIME_FORMAT_BYTE, 65000, 65000 );    // tolerance is 65000 bytes
            pTimeFormatList->push_back( format );    
        }
    }
    
    return hr;
}


HRESULT 
ParseFilterType( char* szFilterMajorClass, 
                 char* szFilterSubClass, 
                 DWORD* pFilterType )
{
    // BUGBUG:
    *pFilterType = ToSubClass( VideoDecoder );
    return S_OK;
}

HRESULT 
ParsePinDirection( char *pinDirStr, PIN_DIRECTION* pPinDir )
{
    // BUGBUG:
    *pPinDir = PINDIR_INPUT;
    return S_OK;
}

HRESULT 
ParseMediaTypeMajor( char *mediaTypeStr, GUID* pMajorType )
{
    // BUGBUG:
    *pMajorType = MEDIATYPE_Video;
    return S_OK;
}

HRESULT 
ParseGraphEvent( char* graphEventStr, 
                 GraphEvent* pGraphEvent )
{
    // BUGBUG:
    *pGraphEvent = END_FLUSH;
    return S_OK;
}

HRESULT 
ParseVerifyDWORD( CXMLElement *hVerification, 
                  VerificationType type, 
                  VerificationList* pVerificationList )
{
    HRESULT hr = S_OK;

    DWORD dwValue= 0;
    hr = ParseDWORD( hVerification, &dwValue );
    if ( SUCCEEDED(hr) )
    {
        DWORD* pDWord = new DWORD;
        if ( !pDWord )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to allocate tempory DWORD when parsing verification."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_OUTOFMEMORY;
        }
        else 
        {
            *pDWord = dwValue;
            pVerificationList->insert( VerificationPair(type, pDWord) );
        }
    }
    else {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse verification DWORD value."), 
                        __FUNCTION__, __LINE__, __FILE__ );
    }
    return hr;
}

HRESULT 
ParseVerifyVideoRECT( CXMLElement *hVerification, 
                      VerificationType type, 
                      VerificationList* pVerificationList )
{
    HRESULT hr = S_OK;

    bool bUseIndivisualAPI = false;
    TCHAR szVerifyData[MAX_PATH];
    hr = ParseString( hVerification, szVerifyData, countof(szVerifyData) );

    if ( SUCCEEDED(hr) )
    {
        if ( !_tcscmp( TEXT("UseIndivisualAPI"), szVerifyData) ) {
            bUseIndivisualAPI = true;
        }

        bool *pbData = new bool;
        if ( !pbData )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to allocate tempory boolean when parsing verification."), 
                        __FUNCTION__, __LINE__, __FILE__);
            hr = E_OUTOFMEMORY;
        }
        else 
        {
            *pbData = bUseIndivisualAPI;
            pVerificationList->insert( VerificationPair(type, pbData) );
        }
    }
    else {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse verification string."), 
                    __FUNCTION__, __LINE__, __FILE__ );
    }
    return hr;
}


HRESULT 
ParseVerifyRECT( CXMLElement *hVerification, 
                 VerificationType type, 
                 VerificationList* pVerificationList )
{
    HRESULT hr = S_OK;

    RECT rect = {};
    hr = ParseRect( hVerification, &rect );
    if ( SUCCEEDED(hr) )
    {
        RECT* pRect = new RECT;
        if ( !pRect )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to allocate tempory RECT when parsing verification."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_OUTOFMEMORY;
        }
        else 
        {
            *pRect = rect;
            pVerificationList->insert( VerificationPair(type, pRect) );
        }
    }
    else {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse verification RECT value."), 
                    __FUNCTION__, __LINE__, __FILE__ );
    }
    return hr;
}

HRESULT 
ParseVerifyGraphDesc( CXMLElement *hVerification, 
                      VerificationType type, 
                      VerificationList* pVerificationList )
{
    HRESULT hr = S_OK;

    GraphDesc* pGraphDesc = new GraphDesc();
    if ( !pGraphDesc )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to allocate TestDesc object when parsing."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        hr = E_OUTOFMEMORY;
    }

    if ( SUCCEEDED(hr) )
    {
        hr = ParseGraphDesc( hVerification, pGraphDesc );
        if ( SUCCEEDED(hr) )
        {
            pVerificationList->insert( VerificationPair(type, (void*)pGraphDesc) );
        }
        else {
            delete pGraphDesc;
            LOG( TEXT("%S: ERROR %d@%S - failed to parse verification graph description."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        }
    }

    return hr;
}

HRESULT 
ParseVerifyRendererMode( CXMLElement *hVerification, 
                         VerificationType type, 
                         VerificationList* pVerificationList )
{
    HRESULT hr = S_OK;

    VideoRendererMode mode = 0;
    hr = ParseRendererMode( hVerification->GetValue(), &mode );
    if ( SUCCEEDED(hr) )
    {
        VideoRendererMode* pMode = new VideoRendererMode;
        if ( !pMode )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to allocate space for VideoRendererMode when parsing verification."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_OUTOFMEMORY;
        }
        else 
        {
            *pMode = mode;
            pVerificationList->insert(VerificationPair(type, pMode));
        }
    }
    else {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse video renderer mode verification %S."), 
                    __FUNCTION__, __LINE__, __FILE__, hVerification->GetValue() );
    }
    return hr;
}

HRESULT 
ParseVerifyRendererSurfaceType( CXMLElement *hVerification, 
                                VerificationType type, 
                                VerificationList* pVerificationList )
{
    HRESULT hr = S_OK;

    DWORD surfaceType = 0;
    hr = ParseSurfaceType( hVerification->GetValue(), &surfaceType );
    if ( SUCCEEDED(hr) )
    {
        DWORD* pSurfaceType = new DWORD;
        if ( !pSurfaceType )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to allocate space for surface type when parsing verification."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_OUTOFMEMORY;
        }
        else 
        {
            *pSurfaceType = surfaceType;
            pVerificationList->insert(VerificationPair(type, pSurfaceType));
        }
    }
    else {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse video renderer surface type verification %S."), 
                    __FUNCTION__, __LINE__, __FILE__, hVerification->GetValue() );
    }
    return hr;
}

HRESULT 
ParseVerifySampleDelivered( CXMLElement *hVerification, 
                            VerificationType type, 
                            VerificationList* pVerificationList )
{
    HRESULT hr = S_OK;

    // Temp to store parsed data
    SampleDeliveryVerifierData data;
    
    // Extract the string
    const char* szVerifySampleDeliveredData = hVerification->GetValue();
    if ( !szVerifySampleDeliveredData )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to extract VerifySampleDelivered data."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    // Now parse the string list into the component elements
    int pos = 0;
    char szString[128] = "";
    // Parse the filter sub type
    hr = ParseStringListString( szVerifySampleDeliveredData, 
                                STR_VERIFY_SAMPLE_DELIVERED_DATA_DELIMITER, 
                                szString, 
                                countof(szString), 
                                pos );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered data."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }
    hr = ParseFilterType( NULL, szString, &data.filterType );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered filter type data."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    // Parse the pin direction
    hr = ParseStringListString( szVerifySampleDeliveredData, 
                                STR_VERIFY_SAMPLE_DELIVERED_DATA_DELIMITER, 
                                szString, 
                                countof(szString), 
                                pos );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered data."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }
    hr = ParsePinDirection( szString, &data.pindir );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered pindir data."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    // Parse the media type
    hr = ParseStringListString( szVerifySampleDeliveredData, 
                                STR_VERIFY_SAMPLE_DELIVERED_DATA_DELIMITER, 
                                szString, 
                                countof(szString), 
                                pos    );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered data."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }
    hr = ParseMediaTypeMajor( szString, &data.mediaMajorType );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered major type data."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    // Parse the tolerance
    hr = ParseStringListString( szVerifySampleDeliveredData, 
                                STR_VERIFY_SAMPLE_DELIVERED_DATA_DELIMITER, 
                                szString, 
                                countof(szString), 
                                pos );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered data."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }
    hr = ParseDWORD( szString, &data.tolerance );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered tolerance data."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    // Parse the event
    hr = ParseStringListString( szVerifySampleDeliveredData, 
                                STR_VERIFY_SAMPLE_DELIVERED_DATA_DELIMITER, 
                                szString, 
                                countof(szString), 
                                pos );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered data."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }
    hr = ParseGraphEvent( szString, &data.event );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse VerifySampleDelivered event data."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    // Add the verification data to the verifier list
    SampleDeliveryVerifierData* pData = new SampleDeliveryVerifierData;
    if ( !pData )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to allocate space for SampleDeliveryVerifierData."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        hr = E_OUTOFMEMORY;
    }
    else 
    {
        *pData = data;
        pVerificationList->insert(VerificationPair(type, pData));
    }

    return hr;
}

HRESULT 
ParseVerifyPlaybackDuration( CXMLElement *hVerification, 
                             VerificationType type, 
                             VerificationList* pVerificationList )
{
    HRESULT hr = S_OK;

    // Temp to store parsed data
    PlaybackDurationTolerance data;
    
    // Extract the string
    const char* szVerifyPlaybackDurationData = hVerification->GetValue();
    if ( !szVerifyPlaybackDurationData )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to extract VerifyPlaybackDuration data."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    // Now parse the string list into the component elements
    int pos = 0;
    char szString[128] = "";
    
    // Parse the pct duration tolerance
    hr = ParseStringListString( szVerifyPlaybackDurationData, 
                                STR_VERIFY_PLAYBACK_DURATION_DATA_DELIMITER, 
                                szString, 
                                countof(szString), 
                                pos );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse VerifyPlaybackDuration data."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    hr = ParseDWORD( szString, &data.pctDurationTolerance );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse pct duration tolerance."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    // Parse the abs duration tolerance
    hr = ParseStringListString( szVerifyPlaybackDurationData, 
                                STR_VERIFY_PLAYBACK_DURATION_DATA_DELIMITER, 
                                szString, 
                                countof(szString), 
                                pos );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse VerifyPlaybackDuration data."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    hr = ParseDWORD( szString, &data.absDurationTolerance );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse abs duration tolerance."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    // Add the verification data to the verifier list
    PlaybackDurationTolerance* pData = new PlaybackDurationTolerance;
    if ( !pData )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to allocate space for PlaybackDurationTolerance."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        hr = E_OUTOFMEMORY;
    }
    else 
    {
        *pData = data;
        pVerificationList->insert(VerificationPair(type, pData));
    }

    return hr;
}


HRESULT 
ParseStartStopPosition( CXMLElement *hXmlChild, 
                        LONGLONG &start, 
                        LONGLONG &stop )
{
    HRESULT hr = S_OK;
    const char* szStartStopPosition = hXmlChild->GetValue();
    if ( !szStartStopPosition )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to extract Start /Stop position data."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    // Now parse the string list into the component elements
    int pos = 0;
    DWORD iPosition = 0;
    char szString[128] = "";
    
    // Parse the start position
    hr = ParseStringListString( szStartStopPosition, 
                                POSITION_LIST_DELIMITER, 
                                szString, 
                                countof(szString), 
                                pos );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse Start /Stop position data."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    hr = ParseDWORD( szString, &iPosition );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse start position."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    start = iPosition;

    // Parse the stop position
    hr = ParseStringListString( szStartStopPosition, 
                                POSITION_LIST_DELIMITER, 
                                szString, 
                                countof(szString), 
                                pos );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse Start /Stop position data."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    hr = ParseDWORD( szString, &iPosition );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse stop position."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    stop = iPosition;

    return hr;    
}


HRESULT 
ParseStateChangeSequenceString( const char* szStateChangeString, 
                                StateChangeData* pStateChange )
{
    HRESULT hr = S_OK;

    char szString[128];
    int pos = 0;

    // Parse the initial state
    ParseStringListString( szStateChangeString, 
                           STR_STATE_CHANGE_SEQ_DELIMITER, 
                           szString, 
                           countof(szString), 
                           pos );
    if ( pos == -1 )
        return E_FAIL;
    hr = ToState( szString, &pStateChange->state );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse state %s."), 
                    __FUNCTION__, __LINE__, __FILE__, szString );
        return hr;
    }

    // Parse the sequence
    ParseStringListString( szStateChangeString, 
                           STR_STATE_CHANGE_SEQ_DELIMITER, 
                           szString, 
                           countof(szString), 
                           pos );
    if ( pos == -1 )
        return E_FAIL;

    hr = ToStateChangeSequence( szString, &pStateChange->sequence );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse state change sequence %S."), 
                        __FUNCTION__, __LINE__, __FILE__, szString );
        return hr;
    }

    // Parse the number of state changes
    ParseStringListString( szStateChangeString, 
                           STR_STATE_CHANGE_SEQ_DELIMITER, 
                           szString, 
                           countof(szString), 
                           pos );
    if ( pos == -1 )
        return E_FAIL;

    hr = ParseDWORD( szString, &pStateChange->nStateChanges );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse nStateChanges."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    // Parse the time between states
    ParseStringListString( szStateChangeString, 
                           STR_STATE_CHANGE_SEQ_DELIMITER, 
                           szString, 
                           countof(szString), 
                           pos );

    // This should be the last part of the string
    if ( pos != -1 )
        return E_FAIL;

    hr = ParseDWORD( szString, &pStateChange->dwTimeBetweenStates );
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to parse dwTimeBetweenStates."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        return hr;
    }

    return hr;
}


static HRESULT 
AddVerification( VerificationType type, 
                 VerificationList* pVerificationList )
{
    HRESULT hr = S_OK;
    pVerificationList->insert( VerificationPair(type, NULL) );
    return hr;
}


ParseTestFunction 
GetParseFunction( TCHAR* szTestName )
{
    // Run through table and determine funciton or NULL
    for(int i = 0; i < g_nLocalTestFunctions; i++)
    {
        if ( !_tcscmp(g_lpLocalFunctionTable[i].szTestName, szTestName) ) 
        {
            return g_lpLocalFunctionTable[i].parsefn;
            break;
        }
    }

    return NULL;
}


HRESULT 
ParseTestDesc( CXMLElement *hTestDesc, TestDesc** ppTestDesc )
{
    HRESULT hr = S_OK;

    // We don't have a test desc structure yet to store the name and desc - need temp storage
    TCHAR szTestName[TEST_NAME_LENGTH] = TEXT("");
    TCHAR szTestDesc[TEST_DESC_LENGTH] = TEXT("");

    CXMLAttribute *hAttrib = hTestDesc->GetFirstAttribute();
    if ( hAttrib ) 
    {
        // Get name of the test node
        const char *szName = NULL;
        if ( !strcmp(STR_NameAttr, hAttrib->GetName()) )
        {
            szName = hAttrib->GetValue();

            // Convert name to TCHAR
            CharToTChar( szName, szTestName, countof(szTestName) );
            // Move to the next attribute
            hAttrib = hAttrib->GetNextAttribute();
        }
        else {
            LOG( TEXT("%S: ERROR %d@%S - failed to find test name attribute."), __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
        }
    }
    else {
        LOG( TEXT("%S: ERROR %d@%S - test node has no test name attribute."), __FUNCTION__, __LINE__, __FILE__ );
        hr = E_FAIL;
    }

    if ( SUCCEEDED(hr) && hAttrib )
    {
        const char* szDesc = NULL;
        if ( !strcmp(STR_Desc, hAttrib->GetName()) )
        {
            szDesc = hAttrib->GetValue();

            // Convert name to TCHAR
            CharToTChar(szDesc, szTestDesc, countof(szTestDesc));

            // Move to the next attribute
            hAttrib = hAttrib->GetNextAttribute();
        }
    }

    // From test name, determine function to be called
    ParseTestFunction parsefn = GetParseFunction( szTestName );
    if ( !parsefn )
    {
        LOG( TEXT("%S: ERROR %d@%S - unknown test name %s - failed to determine test function."), 
                    __FUNCTION__, __LINE__, __FILE__, szTestName );
        hr = E_FAIL;
    }
    else {
        hr = parsefn( hTestDesc, ppTestDesc );
        if ( FAILED(hr) )
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to parse test %s."), __FUNCTION__, __LINE__, __FILE__, szTestName );
        }
        else {
            // Set the test name
            _tcscpy_s( (*ppTestDesc)->szTestName,TEST_NAME_LENGTH, szTestName );

            // Set the test desc
            _tcscpy_s( (*ppTestDesc)->szTestDesc, TEST_NAME_LENGTH, szTestDesc );
        }
    }

    return hr;
}

HRESULT 
ParseTestDescList( CXMLElement *hElement, TestDescList* pTestDescList, TestGroupList* pTestGroupList )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;
    CXMLElement *hXmlGroup = 0;
    CXMLAttribute *hAttrib = 0;

    if ( !hElement || !pTestDescList || !pTestGroupList ) 
        return S_FALSE;


    hXmlChild = hElement->GetFirstChildElement();
    if ( !hXmlChild )
        return S_OK;

    int nTests = 0;

    TestGroup* pTestGroup = NULL;
    while ( hXmlChild )
    {
        // Check if this is a group
        if ( !strcmp(STR_TestGroup, hXmlChild->GetName()) )
        {
            // Get the test group's name
            const char* szGroupName = NULL;
            INT testIdBase = -1;

            // Get the test group XML element
            hXmlGroup = hXmlChild;

            // Get the name attribute
            hAttrib = hXmlGroup->GetFirstAttribute();
            if ( hAttrib && !strcmp(STR_NameAttr, hAttrib->GetName()) )
            {
                // Get the test group's name
                szGroupName = hAttrib->GetValue();
            }
            else {
                LOG( TEXT("%S: ERROR %d@%S - failed to find TestGroup name."), __FUNCTION__, __LINE__, __FILE__ );
                hr = E_FAIL;
                break;
            }

            // Get the TestIdBase attribute if present
            hAttrib = hAttrib->GetNextAttribute();
            if ( hAttrib )
            {
                const char* szTestIdBase = hAttrib->GetValue();
                testIdBase = atol( szTestIdBase );
            }

            // Allocate a test group object
            pTestGroup = new TestGroup();
            if ( !pTestGroup )
            {
                hr = E_OUTOFMEMORY;
                LOG( TEXT("%S: ERROR %d@%S - failed to allocate TestGroup object."), __FUNCTION__, __LINE__, __FILE__ );
                break;
            }

            // Copy the group name
            CharToTChar( szGroupName, pTestGroup->szGroupName, countof(pTestGroup->szGroupName) );
            pTestGroup->testIdBase = testIdBase;

            // Get the xml test objects to add to this group
            hXmlChild = hXmlGroup->GetFirstChildElement();

            // Add the test group to the test group list
            pTestGroupList->push_back( pTestGroup );
        }

        // If there is a test in this group or list, then parse it
        if ( hXmlChild && !strcmp(STR_TestDesc, hXmlChild->GetName()) )
        {
            TestDesc* pTestDesc = NULL;
            hr = ParseTestDesc( hXmlChild, &pTestDesc );
            if ( pTestDesc )
            {
                // set the media list
                // if ( GraphTest::m_pTestConfig )
                //     pTestDesc->SetAllMediaList( GraphTest::m_pTestConfig->GetMediaList() );

                // Add it to the test list
                pTestDescList->push_back( pTestDesc );

                // Add it to the test group
                if ( pTestGroup )
                    pTestGroup->testDescList.push_back( pTestDesc );
            }

            if ( FAILED(hr) )
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to parse Test."), __FUNCTION__, __LINE__, __FILE__);
                break;
            }
            else nTests++;
        }

        // If there is a test in this group or list, then try going to the next one
        if ( hXmlChild )
            hXmlChild = hXmlChild->GetNextSiblingElement();

        // If we went to the end of the list and there is a group, pop back to the next group
        if ( !hXmlChild && hXmlGroup )
        {
            hXmlChild = hXmlGroup->GetNextSiblingElement();
            hXmlGroup = 0;
        }
    }

    // Return an error if we found no tests
    if ( SUCCEEDED(hr) && !nTests )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to find Test."), __FUNCTION__, __LINE__, __FILE__ );
        hr = E_FAIL;
    }

    return hr;
}


HRESULT 
ParseVerification( CXMLElement *hVerification, 
                   VerificationList* pVerificationList )
{
    HRESULT hr = S_OK;
    CXMLElement *hXmlChild = 0;
    CVerificationMgr *verifMgr = NULL;

    hXmlChild = hVerification->GetFirstChildElement();
    // It's ok if we don't have a child. No verification is specified.
    if ( !hXmlChild )
        goto cleanup;


    verifMgr = CVerificationMgr::Instance();
    if ( !verifMgr )
    {
        hr = S_FALSE;
        goto cleanup;
    }

    if ( !verifMgr->IsInitialized() )
    {
        if ( !verifMgr->InitializeVerifications() ) 
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to initialize the verification map."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_FAIL;
            goto cleanup;
        }
    }

    int nVerifications = verifMgr->GetNumberOfVerifications();

    // For each verification type specified, parse the type and value
    while( hXmlChild && SUCCEEDED_F(hr) )
    {
        const char* szVerificationType = hXmlChild->GetName();
        int i = 0;
        VerificationInfo* pInfo = verifMgr->GetVerificationInfo( szVerificationType );
        
        // No verification or invalid verification specified.
        if ( !pInfo ) 
        {
            LOG( TEXT("%S: WARNING %d@%S - Can't find the verification info for %s."), 
                        __FUNCTION__, __LINE__, __FILE__, szVerificationType );
            break;
        }

        if ( pInfo->fpVerificationParser )
        {
            hr = pInfo->fpVerificationParser( hXmlChild, 
                                              pInfo->enVerificationType, 
                                              pVerificationList );
        }
        else 
        {
            // Otherwise just add the verification type to the list
            hr = AddVerification( pInfo->enVerificationType, pVerificationList );
        }

        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();
        else  // If we failed to parse the verification, skip the remaining verifications
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to parse verification type %S."), 
                        __FUNCTION__, __LINE__, __FILE__, szVerificationType );
            break;
        }
    }

cleanup:
    if(verifMgr)
        verifMgr->Release();

    return hr;
}


HRESULT 
ParseTestConfig( CXMLElement *hElement, TestConfig* pTestConfig )
{
    HRESULT hr = S_OK;
    
    // Parse the attributes
    CXMLAttribute *hAttrib = hElement->GetFirstAttribute();
    while ( hAttrib ) 
    {
        // We are only looking for the GenerateId attribute
        if ( !strcmp(STR_GenerateId, hAttrib->GetName()) )
        {
            const char* szGenerateId = hAttrib->GetValue();
            if ( !strcmp(STR_True, szGenerateId) )
                pTestConfig->bGenerateId = true;
            break;
        }
        // Move to the next attribute
        hAttrib = hAttrib->GetNextAttribute();
    }

    // Parse the children
    CXMLElement *hXmlChild = hElement->GetFirstChildElement();

    while ( SUCCEEDED(hr) && hXmlChild )
    {
        if ( !hXmlChild )
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to find MediaList."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            return E_FAIL;
        }

        if ( !strcmp(STR_MediaList, hXmlChild->GetName()) )
        {
            hr = ParsePlaybackMediaList( hXmlChild, &pTestConfig->mediaList );
            if ( SUCCEEDED(hr) )
                hXmlChild = hXmlChild->GetNextSiblingElement();
        }

        if ( SUCCEEDED(hr) )
        {
            if ( hXmlChild )
            {
                if ( !strcmp(STR_TestList, hXmlChild->GetName()) )
                {
                    // Get the test and group list pointers from the test config structure
                    hr = ParseTestDescList( hXmlChild, &pTestConfig->testDescList, &pTestConfig->testGroupList );
                    if ( FAILED(hr) )
                        LOG( TEXT("%S: ERROR %d@%S - failed to parse TestList."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
                }
            }
            else 
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to find TestList."), 
                            __FUNCTION__, __LINE__, __FILE__ );
                hr = E_FAIL;
            }
        }

        if ( SUCCEEDED(hr) )
            hXmlChild = hXmlChild->GetNextSiblingElement();        
    }

    return hr;
}


TestConfig * 
ParseConfigFile( TCHAR* szXmlConfig )
{
    TestConfig* pTestConfig = NULL;
    HRESULT hr = NOERROR;
    errno_t  Error;
    // If the caller has not specified a config file or pTestDesc is NULL
    if ( !szXmlConfig )
        return NULL;
    
    // Open the config file
    HANDLE hFile = CreateFile( szXmlConfig, 
                               GENERIC_READ, 
                               0, 
                               NULL, 
                               OPEN_EXISTING, 
                               0, 
                               0 );
    if ( hFile == INVALID_HANDLE_VALUE )
    {
        //if we didn't actually get the complete path of the file, let's try if we can find the file in the current folder
        if( szXmlConfig[0] != '\\' ) {
        
            LOG( TEXT("%S: WARNING %d@%S - failed to open file %s"), 
                            __FUNCTION__, __LINE__, __FILE__, szXmlConfig );

            // Try to see if maybe we can find the file in our current directory.
            TCHAR szDir[MAX_PATH];
            DWORD dwActual, i =0;
            dwActual= GetModuleFileName( NULL, szDir, MAX_PATH );
            if( dwActual ) 
            {
                i = dwActual -1;
                while( i >=0 && szDir[i]!='\\')
                    i--;
                i++;
            }        
            szDir[i] = '\0';

            LOG( TEXT("%S: Trying to see if we can find the file in the current directory: %s"), 
                        __FUNCTION__, szDir );        

            //make sure we never try to write more data than we have buffer
            if( (wcsnlen(szXmlConfig, MAX_PATH) + wcsnlen(szDir, MAX_PATH) + 1) <= MAX_PATH ) 
            {
                
                Error = wcsncat_s ( szDir,countof(szDir), szXmlConfig,  _TRUNCATE ); 
                if(Error != 0)
                {
                    LOG(TEXT("FAIL : PlaybackMedia::SetDownloadLocation- _tcscpy_s Error"));
                }
                
                hFile = CreateFile( szDir, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0 );
                if ( hFile == INVALID_HANDLE_VALUE )
                {
                    LOG( TEXT("%S: ERROR %d@%S - failed to open file %s."), 
                                __FUNCTION__, __LINE__, __FILE__, szDir );
                    return NULL;
                }
            }
            else 
            {
                LOG( TEXT("%S: ERROR %d@%S - File name (%s) or directory name(%s) is too big %s."), 
                            __FUNCTION__, __LINE__, __FILE__, szXmlConfig, szDir );
                return NULL;
            }
        }
        else 
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to open file %s."), 
                        __FUNCTION__, __LINE__, __FILE__, szXmlConfig );            
            return NULL;
        }
    }

    // Read the size of the file
    DWORD size = 0;
    size = GetFileSize( hFile, 0 );
    if ( size == -1 )
    {
        LOG( TEXT("%S: ERROR %d@%s - failed to read the file size."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        CloseHandle( hFile );
        return NULL;
    }

    // Allocate space for the whole file and read it in completely
    char* xmlConfig = new char[size + 1];
    if ( !xmlConfig )
    {
        LOG( TEXT("%S: ERROR %d@%s - failed to allocate memory for the config file."), 
                    __FUNCTION__, __LINE__, __FILE__ );
        CloseHandle( hFile );
        return NULL;
    }

    DWORD nBytesRead = 0;
    BOOL bRead = ReadFile( hFile, xmlConfig, size, &nBytesRead, NULL );
    if ( !bRead || (nBytesRead != size) )
    {
        LOG( TEXT("%S: ERROR %d@%s - failed to read the complete file."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        CloseHandle( hFile );
        delete [] xmlConfig;
        return NULL;
    }
    
    // Insert a string terminator after reading in the file - which is now an XML string
    xmlConfig[size] = 0;

    // Parse the XML string
    CXMLElement *hXmlRoot = NULL;
    BOOL rtn = TRUE;

    rtn = XMLParse( xmlConfig, hXmlRoot );
    if ( !rtn || !hXmlRoot)
    {
        LOG( TEXT("%S: XMLParse failed to parse the config file."), __FUNCTION__ );
        hr = E_FAIL;
    }
    else 
    {
        // Parse the DMO configurations from the XML string
        pTestConfig = new TestConfig();
        if ( pTestConfig )
        {
            hr = ParseTestConfig( hXmlRoot, pTestConfig );
            if ( FAILED(hr) )
                LOG( TEXT("%S: ParseTestDesc failed."), __FUNCTION__);
        }
        else 
        {
            LOG( TEXT("%S: ERROR %d@%S - failed to allocate TestConfig object when parsing."), 
                        __FUNCTION__, __LINE__, __FILE__ );
            hr = E_OUTOFMEMORY;
        }
    }

    // Free resources and return
    if ( hXmlRoot )
        XMLFree( hXmlRoot );

    if ( xmlConfig )
        delete [] xmlConfig;

    if ( hFile != INVALID_HANDLE_VALUE )
        CloseHandle( hFile );

    if ( FAILED(hr) )
    {
        if ( pTestConfig )
            delete pTestConfig;
        pTestConfig = NULL;
    }

    return pTestConfig;
}


HRESULT 
ParsePlaybackMediaList( CXMLElement *pElement, PBMediaList* pMediaList )
{
    HRESULT hr = S_OK;
    
    if ( !pElement || !pMediaList ) return E_POINTER;

    // Check the attributes of the media list element for a container file for the media list
    CXMLAttribute *pAttrib = pElement->GetFirstAttribute();
    while ( pAttrib ) 
    {
        // We are only looking for the Container attribute
        if ( !strcmp( STR_Container, pAttrib->GetName() ) )
        {
            const char* szContainer = pAttrib->GetValue();
            // If the file name is invalid, return an error
            if ( !szContainer || !strcmp("", szContainer) )
                return E_FAIL;
            else 
            {
                // Parse the media list file
                TCHAR tszContainer[MAX_PATH];
                CharToTChar( szContainer, tszContainer, countof(tszContainer) );
                return ParsePlaybackMediaListFile( tszContainer, pMediaList );
            }
        }

        // Move to the next attribute
        pAttrib = pAttrib->GetNextAttribute();
    }

    CXMLElement *pChild = pElement->GetFirstChildElement();
    if ( !pChild )
        return S_OK;

    while ( true )
    {
        if ( !strcmp( STR_Media, pChild->GetName() ) )
        {
            PlaybackMedia* pMedia = new PlaybackMedia();
            if ( !pMedia )
            {    
                LOG( TEXT("%S: ERROR %d@%S - failed to allocate media object when parsing."), 
                                __FUNCTION__, __LINE__, __FILE__);
                hr = E_OUTOFMEMORY;
                break;
            }

            hr = ParseMedia( pChild, pMedia );
            hr = ParseMediaMeta(pChild, pMedia);
            
            pMediaList->push_back( pMedia );
            if ( FAILED(hr) )
                break;

        }
        else 
        {
            hr = E_FAIL;
            break;
        }

        pChild = pChild->GetNextSiblingElement();
        if ( pChild == 0 )
            return S_OK;
    }
    return hr;
}


HRESULT 
ParsePlaybackMediaListFile( TCHAR* szMediaListFile, PBMediaList* pMediaList )
{
    HRESULT hr = S_OK;
    
    // If the caller has not specified a config file or pTestDesc is NULL
    if ( !szMediaListFile || !pMediaList )
        return E_POINTER;
    
    // Open the config file
    HANDLE hFile = CreateFile( szMediaListFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0 );
    if ( hFile == INVALID_HANDLE_VALUE )
    {
        LOG( TEXT("%S: ERROR %d@%S - failed to open file %s."), 
                        __FUNCTION__, __LINE__, __FILE__, szMediaListFile );
        return E_INVALIDARG;
    }

    // Read the size of the file
    DWORD size = 0;
    size = GetFileSize( hFile, 0 );
    if ( -1 == size )
    {
        LOG( TEXT("%S: ERROR %d@%s - failed to read the file size."), 
                            __FUNCTION__, __LINE__, __FILE__ );
        CloseHandle( hFile );
        return E_FAIL;
    }

    // Allocate space for the whole file and read it in completely
    char* xmlMediaList = new char[size + 1];
    if ( !xmlMediaList )
    {
        LOG( TEXT("%S: ERROR %d@%s - failed to allocate memory for the media list file."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        CloseHandle( hFile );
        return E_OUTOFMEMORY;
    }

    DWORD nBytesRead = 0;
    BOOL bRead = ReadFile( hFile, xmlMediaList, size, &nBytesRead, NULL );
    if ( !bRead || (nBytesRead != size) )
    {
        LOG( TEXT("%S: ERROR %d@%s - failed to read the complete file."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        CloseHandle( hFile );
        delete [] xmlMediaList;
        return E_FAIL;
    }
    
    // Insert a string terminator after reading in the file - which is now an XML string
    xmlMediaList[size] = 0;

    // Parse the XML string
    CXMLElement *pXmlRoot = NULL;
 
    if ( !XMLParse( xmlMediaList, pXmlRoot ) || !pXmlRoot )
    {
        LOG( TEXT("%S ERROR %d@%S: XMLParse failed to parse the config file."), 
                        __FUNCTION__, __LINE__, __FILE__ );
        hr = E_FAIL;
    }
    else 
    {
        // Parse the media list from the xml string
        hr = ParsePlaybackMediaList( pXmlRoot, pMediaList );
        if ( FAILED(hr) )
            LOG( TEXT("%S: ParseMediaList failed."), __FUNCTION__ );
    }

    // Free resources and return
    if ( pXmlRoot )
        XMLFree( pXmlRoot );

    if ( xmlMediaList )
        delete [] xmlMediaList;

    if ( hFile != INVALID_HANDLE_VALUE )
        CloseHandle( hFile );

    return hr;
}


HRESULT
ParseMediaMeta( CXMLElement *pElement, PlaybackMedia* pMedia )
{
    HRESULT hr = S_OK;

    if ( !pElement || !pMedia ) return E_POINTER;

    CXMLElement *pChild = pElement->GetFirstChildElement();
    if ( !pChild )
        return E_FAIL;

    while ( SUCCEEDED(hr) && pChild )
    {
        PCSTR elementName = pChild->GetName();
        if ( !strcmp( STR_MetaData, elementName ) )
        {                     
            hr = ParseMediaMetaData(pChild, pMedia);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse Meta data in XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );            
            }
        }

        else if ( !strcmp( STR_ASXMetaData, elementName ) )
        {                     
            hr = ParseASXMetaData(pChild, pMedia);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse Meta data in XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );
            }
        }
    
        if ( SUCCEEDED(hr) )
            pChild = pChild->GetNextSiblingElement();
        }
    return hr;
}

HRESULT 
ParseMediaMetaData( CXMLElement *pElement, PlaybackMedia* pMedia )
{
    HRESULT hr = S_OK;

    if ( !pElement || !pMedia ) return E_POINTER;

    CXMLElement *pChild = pElement->GetFirstChildElement();
    if ( !pChild )
        return E_FAIL;
    
    while ( SUCCEEDED(hr) && pChild )
    {
        PCSTR elementName = pChild->GetName();
        if ( !strcmp( STR_AuthorName, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szAutherName,countof(pMedia->szAutherName) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse auther name from XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );            
            }
            else
                pMedia->bAutherName = TRUE;
        }        
        else if ( !strcmp( STR_Title, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szTitle, countof(pMedia->szTitle) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse media Title."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bTitle = TRUE;
        }
        else if ( !strcmp( STR_Rating, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szRating, countof(pMedia->szRating) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse media Rating."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bRating = TRUE;
        }
        else if ( !strcmp( STR_MetaDescription, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szDescription, countof(pMedia->szDescription) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse media Description."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bDescription = TRUE;
        }
        else if ( !strcmp( STR_Copyright, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szCopyright, countof(pMedia->szCopyright) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse media Copyright."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bCopyright = TRUE;
        }
        else if ( !strcmp( STR_MetaBaseURL, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szBaseURL, countof(pMedia->szBaseURL) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse media BaseURL."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bBaseURL = TRUE;
        }
        else if ( !strcmp( STR_LogoURL, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szLogoURL, countof(pMedia->szLogoURL) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse media LogoURL."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bLogoURL = TRUE;
        }
        else if ( !strcmp( STR_LogoIconURL, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szLogoIconURL, countof(pMedia->szLogoIconURL) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse media LogoIconURL."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bLogoIconURL = TRUE;
        }
        else if ( !strcmp( STR_WatermarkURL, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szWatermarkURL, countof(pMedia->szWatermarkURL) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse media WatermarkURL."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bWatermarkURL = TRUE;
        }
        else if ( !strcmp( STR_MoreInfoURL, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szMoreInfoURL, countof(pMedia->szMoreInfoURL) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse media MoreInfoURL."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bMoreInfoURL = TRUE;
        }
        else if ( !strcmp( STR_MoreInfoBannerImage, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szMoreInfoBannerImage, countof(pMedia->szMoreInfoBannerImage) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse media MoreInfoBannerImage."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bMoreInfoBannerImage = TRUE;
        }
        else if ( !strcmp( STR_MoreInfoBannerURL, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szMoreInfoBannerURL, countof(pMedia->szMoreInfoBannerURL) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse media MoreInfoBannerURL."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bMoreInfoBannerURL = TRUE;
        }
        else if ( !strcmp( STR_MoreInfoText, elementName ) )
        {
            hr = ParseString( pChild, pMedia->szMoreInfoText, countof(pMedia->szMoreInfoText) );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse media MoreInfoText."), 
                                    __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bMoreInfoText = TRUE;
        }
        if ( SUCCEEDED(hr) )
            pChild = pChild->GetNextSiblingElement();
    }
    return hr;
}


HRESULT 
ParseASXMetaData( CXMLElement *pElement, PlaybackMedia* pMedia )
{
    HRESULT hr = S_OK;

    if ( !pElement || !pMedia ) return E_POINTER;

    CXMLElement *pChild = pElement->GetFirstChildElement();
    if ( !pChild )
        return E_FAIL;

    while ( SUCCEEDED(hr) && pChild )
    {
        PCSTR elementName = pChild->GetName();
        if ( !strcmp( STR_ASX_ItemCount, elementName ) )
        {
            hr = ParseDWORD( pChild, &pMedia->nItemCount);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse ASXMDItemCount  from XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );                
            }
            else
                pMedia->bItemCount= TRUE;
        }        
        
        else if ( !strcmp( STR_ASX_RepeatCount, elementName ) )
        {
            hr = ParseDWORD( pChild, &pMedia->nRepeatCount);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse ASXMDRepeatCount  from XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );            
            }
            else
                pMedia->bRepeatCount= TRUE;
        }              
    
        else if ( !strcmp( STR_ASX_RepeatStart, elementName ) )
        {
            hr = ParseDWORD( pChild, &pMedia->nRepeatStart);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse ASXMDRepeatStart  from XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );            
            }
            else
                pMedia->bRepeatStart= TRUE;
        }      

        else if ( !strcmp( STR_ASX_RepeatEnd, elementName ) )
        {
            hr = ParseDWORD( pChild, &pMedia->nRepeatEnd);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse ASXMDRepeatEnd  from XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );
            }else
                pMedia->bRepeatEnd= TRUE;
        } 
     
        else if ( !strcmp( STR_ASX_Flags, elementName ) )
        {
            hr = ParseDWORD( pChild, &pMedia->nFlags);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse ASXMDFlags  from XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );
            }
            else
                pMedia->bFlags= TRUE;
        } 
    
        else if ( !strcmp( STR_ASX_Entry, elementName ) )
        {
            hr = ParseASXEntryData(pChild, pMedia);    
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse this ASXMDEntry from XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );
            }
        }   
        if ( SUCCEEDED(hr) )
            pChild = pChild->GetNextSiblingElement();
    }
    return hr;
}

HRESULT 
ParseASXEntryData( CXMLElement *pElement, PlaybackMedia* pMedia )
{
    HRESULT hr = S_OK;

    if ( !pElement || !pMedia ) return E_POINTER;

    ASXEntryData *tmpEntry = new ASXEntryData;
    ASXEntrySource *tmpSource = new ASXEntrySource;

    //if parse failed, delete this new object before leave this function.
    BOOL bHaveParam = FALSE;
    BOOL bHaveSource = FALSE;    

    CXMLElement *pChild = pElement->GetFirstChildElement();
    if ( !pChild )
        return E_FAIL;

    while ( SUCCEEDED(hr) && pChild )
    {
        PCSTR elementName = pChild->GetName();
        if ( !strcmp( STR_ASX_EntryIndex, elementName ) )
        {
            hr = ParseDWORD( pChild, &tmpEntry->ASXEntryIndex);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse this entry's ASXMDEntryIndex  from XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                bHaveParam = FALSE;
                break;
            }
            bHaveParam = TRUE;    
        }               
        else if ( !strcmp( STR_ASX_EntryParamCount, elementName ) )
        {
            hr = ParseDWORD( pChild, &tmpEntry->ASXEntryParamCount);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse this entry's ASXMDParamCount  from XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                bHaveParam = FALSE;
                break;
            }
            bHaveParam = TRUE;    
        }   
        else if ( !strcmp( STR_ASX_ParamNames, elementName ) )
        {
            hr = ParseStringList( pChild, &tmpEntry->ASXEntryParamNames, MEDIALIST_DELIMITER );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse  ASXMDParamNames for ASFMDEntry"), 
                                    __FUNCTION__, __LINE__, __FILE__ );
                bHaveParam = FALSE;
                break;
            }
            bHaveParam = TRUE;    
        }
        else if ( !strcmp( STR_ASX_ParamNameValues, elementName ) )
        {
            hr = ParseStringList( pChild, &tmpEntry->ASXEntryParamNameValues, MEDIALIST_DELIMITER );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse  ASXMDParamNameValues for ASXMDEntry"), 
                                    __FUNCTION__, __LINE__, __FILE__ );
                bHaveParam = FALSE;
                break;
            }
            bHaveParam = TRUE;    
        }
        else if ( !strcmp( STR_ASX_EntrySourceURLs, elementName ) )
        {
            hr = ParseStringList( pChild,  &tmpSource->ASXEntrySourceURLs, MEDIALIST_DELIMITER );
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse  ASXMDSourceURLs for ASXMDEntry"), 
                                    __FUNCTION__, __LINE__, __FILE__ );
                bHaveSource = FALSE;
                break;
            }
            bHaveSource = TRUE;
        }
        else if ( !strcmp( STR_ASX_EntrySourceCount, elementName ) )
        {
            hr = ParseDWORD( pChild, &tmpSource->ASXEntrySourceCount);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse this entry's ASXMDSourceCount  from XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                bHaveSource = FALSE;
                break;
            }
            bHaveSource = TRUE;

        }
        else if ( !strcmp( STR_ASX_EntryFlags, elementName ) )
        {
            hr = ParseDWORD( pChild, &tmpSource->ASXEntryFlags);
            if ( FAILED(hr) )
            {
                LOG( TEXT("%S: ERROR %d@%S - failed to parse this entry's ASXMDEntryFlags from XML."), 
                                __FUNCTION__, __LINE__, __FILE__ );
                bHaveSource = FALSE;
                break;
            }
            bHaveSource = TRUE;    
        }   
        if ( SUCCEEDED(hr) )
        {
            pChild = pChild->GetNextSiblingElement();
        }
    }

    if(bHaveParam)
    {
        pMedia->EntryList.push_back(tmpEntry);
    }    
    else    
    {
        delete tmpEntry;
    }

    if(bHaveSource)
    {
        pMedia->EntrySources.push_back(tmpSource);
    }    
    else    
    {
        delete tmpSource;
    }
    
    return hr;

}

#if 0
HRESULT ParseConnectionDesc(HELEMENT hElement, ConnectionDesc* pConnectionDesc)
{
    HRESULT hr = S_OK;
    HELEMENT hXmlChild = 0;

    hXmlChild = XMLGetFirstChild(hElement);
    if (hElement == 0)
        return E_FAIL;

    hr = ParseFilterDesc(hXmlChild, &pConnectionDesc->fromFilter);
    if ( FAILED(hr) )
        return hr;

    hXmlChild = XMLGetNextSibling(hXmlChild);
    if (hElement == 0)
        return E_FAIL;


    hr = ParseFilterDesc(hXmlChild, &pConnectionDesc->toFilter);
    if ( FAILED(hr) )
        return hr;

    return hr;
}

HRESULT ParseGraphDesc(HELEMENT hElement, GraphDesc* pGraphDesc)
{
    HRESULT hr = S_OK;
    HELEMENT hXmlChild = 0;

    hXmlChild = XMLGetFirstChild(hElement);
    if (hElement == 0)
        return E_FAIL;


    hr = ParseConnectionDescList(hXmlChild, &pGraphDesc->connectionDescList);
    if ( FAILED(hr) )
        return hr;

    hXmlChild = XMLGetNextSibling(hXmlChild);
    if (hElement == 0)
        return E_FAIL;


    hr = ParseFilterDescList(hXmlChild, &pGraphDesc->filterDescList);
    if ( FAILED(hr) )
        return hr;

    return hr;
}

HRESULT ParseConnectionDescList(HELEMENT hElement, ConnectionDescList* pConnectionDescList)
{
    HRESULT hr = S_OK;
    HELEMENT hXmlChild = 0;

    hXmlChild = XMLGetFirstChild(hElement);
    if (hXmlChild == 0)
        return S_OK;

    ConnectionDesc* pConnectionDesc = new ConnectionDesc();
    if (!pConnectionDesc)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to allocate ConnectionDesc object when parsing."), __FUNCTION__, __LINE__, __FILE__);
        hr = E_OUTOFMEMORY;
        return hr;
    }


    hr = ParseConnectionDesc(hXmlChild, pConnectionDesc);
    if ( FAILED(hr) )
    {
        delete pConnectionDesc;
        return hr;
    }

    pConnectionDescList->push_back(pConnectionDesc);

    while ( true )
    {
        hXmlChild = XMLGetNextSibling(hXmlChild);
        if (hXmlChild == 0)
            return S_OK;

        pConnectionDesc = new ConnectionDesc();
        if (!pConnectionDesc)
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to allocate SetPosRateTestDesc object when parsing."), __FUNCTION__, __LINE__, __FILE__);
            hr = E_OUTOFMEMORY;
            break;
        }


        hr = ParseConnectionDesc(hXmlChild, pConnectionDesc);
        if ( FAILED(hr) )
        {
            delete pConnectionDesc;
            return hr;
        }

        pConnectionDescList->push_back(pConnectionDesc);
    }

    return hr;
}
#endif

