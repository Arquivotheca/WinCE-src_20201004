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

#include "globals.h"
#include "logging.h"
#include "TestGraph.h"
#include "PlaybackMedia.h"
#include "utility.h"
#include "TestDesc.h"

// ctor: Time Format Helper classes
CTimeFormat::CTimeFormat( GUID formatID, 
                          LONGLONG llSetGetTolerance,
                          LONGLONG llStopTolerance )
: m_Guid( formatID ), m_llSetGetTolerance( llSetGetTolerance ), m_llStopTolerance( llStopTolerance )
{
    ZeroMemory( m_szGuid, 32 );
    if ( TIME_FORMAT_MEDIA_TIME == formatID )
        _tcscpy_s( m_szGuid,countof(m_szGuid),TEXT("TIME_FORMAT_MEDIA_TIME") );
    else if ( TIME_FORMAT_FRAME == formatID )
        _tcscpy_s( m_szGuid,countof(m_szGuid), TEXT("TIME_FORMAT_FRAME") );
    else if ( TIME_FORMAT_SAMPLE == formatID )
        _tcscpy_s( m_szGuid,countof(m_szGuid), TEXT("TIME_FORMAT_SAMPLE") );
    else if ( TIME_FORMAT_FIELD == formatID )
        _tcscpy_s( m_szGuid,countof(m_szGuid), TEXT("TIME_FORMAT_FIELD") );
    else if ( TIME_FORMAT_BYTE == formatID )
        _tcscpy_s( m_szGuid,countof(m_szGuid), TEXT("TIME_FORMAT_BYTE") );
    else
        _tcscpy_s( m_szGuid,countof(m_szGuid),TEXT("TIME_FORMAT_NONE") );
}

// copy ctor
CTimeFormat::CTimeFormat( const CTimeFormat &data )
{
    this->m_llSetGetTolerance = data.m_llSetGetTolerance;
    this->m_llStopTolerance   = data.m_llStopTolerance;
    this->m_Guid = data.m_Guid;
    ZeroMemory( m_szGuid, 32 );
    _tcscpy_s( m_szGuid, countof(m_szGuid), data.m_szGuid );
}

// assignment ctor
CTimeFormat &CTimeFormat::operator=( const CTimeFormat &data )
{
    this->m_llSetGetTolerance = data.m_llSetGetTolerance;
    this->m_llStopTolerance   = data.m_llStopTolerance;
    this->m_Guid = data.m_Guid;
    ZeroMemory( m_szGuid, 32 );
    _tcscpy_s( m_szGuid, countof(m_szGuid), data.m_szGuid );
    return *this;
}

bool CTimeFormat::IsEqual( GUID guid )
{
    return ( m_Guid == guid || IsEqualGUID( guid, m_Guid ) ); 
}


// ctor/dtor
CMediaProp::CMediaProp()
{
    m_bSeekBackwards = false;
    m_bSeekForwards = false;
    m_llDurations = 0;
    m_bPrerollSupported = false;
    m_llPreroll = 0;

    // Seeking Capabilities
    m_dwSeekCaps = 0;

    // MEDIA_TIME
    m_llMediaTimeEarliest = 0;
    m_llMediaTimeLatest = 0;
    m_llMediaTimeDuration = 0;
    m_bMediaTimePreroll = false;
    m_llMediaTimePreroll = 0;

    // FIELD
    m_llFieldEarliest = 0;
    m_llFieldLatest = 0;
    m_llFieldDuration = 0;
    m_bFieldPreroll = false;
    m_llFieldPreroll = 0;

    // SAMPLE
    m_llSampleEarliest = 0;
    m_llSampleLatest = 0;
    m_llSampleDuration = 0;
    m_bSamplePreroll = false;
    m_llSamplePreroll = 0;

    // BYTE
    m_llByteEarliest = 0;
    m_llByteLatest = 0;
    m_llByteDuration = 0;
    m_bBytePreroll = false;
    m_llBytePreroll = 0;

    // Frame
    m_llFrameEarliest = 0;
    m_llFrameLatest = 0;
    m_llFrameDuration = 0;
    m_bFramePreroll = false;
    m_llFramePreroll = 0;
}

CMediaProp::~CMediaProp()
{
}

CMediaProp & CMediaProp::operator=(const CMediaProp &prop )
{
    // Seeking Capabilities
    this->m_dwSeekCaps = prop.m_dwSeekCaps;

    // MEDIA_TIME
    this->m_llMediaTimeEarliest = prop.m_llMediaTimeEarliest;
    this->m_llMediaTimeLatest = prop.m_llMediaTimeLatest;
    this->m_llMediaTimeDuration = prop.m_llMediaTimeDuration;
    this->m_bMediaTimePreroll = prop.m_bMediaTimePreroll;
    this->m_llMediaTimePreroll = prop.m_llMediaTimePreroll;

    // FIELD
    this->m_llFieldEarliest = prop.m_llFieldEarliest;
    this->m_llFieldLatest = prop.m_llFieldLatest;
    this->m_llFieldDuration = prop.m_llFieldDuration;
    this->m_bFieldPreroll = prop.m_bFieldPreroll;
    this->m_llFieldPreroll = prop.m_llFieldPreroll;

    // SAMPLE
    this->m_llSampleEarliest = prop.m_llSampleEarliest;
    this->m_llSampleLatest = prop.m_llSampleLatest;
    this->m_llSampleDuration = prop.m_llSampleDuration;
    this->m_bSamplePreroll = prop.m_bSamplePreroll;
    this->m_llSamplePreroll = prop.m_llSamplePreroll;

    // BYTE
    this->m_llByteEarliest = prop.m_llByteEarliest;
    this->m_llByteLatest = prop.m_llByteLatest;
    this->m_llByteDuration = prop.m_llByteDuration;
    this->m_bBytePreroll = prop.m_bBytePreroll;
    this->m_llBytePreroll = prop.m_llBytePreroll;

    // Frame
    this->m_llFrameEarliest = prop.m_llFrameEarliest;
    this->m_llFrameLatest = prop.m_llFrameLatest;
    this->m_llFrameDuration = prop.m_llFrameDuration;
    this->m_bFramePreroll = prop.m_bFramePreroll;
    this->m_llFramePreroll = prop.m_llFramePreroll;
    return *this;
}

PlaybackAudioInfo::PlaybackAudioInfo() : AudioInfo(), CMediaProp()
{
    bCompressed = false;
    dwBitRate = 0;
    memset(&decSpecificInfo, 0, sizeof(decSpecificInfo));
}

PlaybackAudioInfo::~PlaybackAudioInfo()
{
}

HRESULT 
PlaybackAudioInfo::SaveToXml(HANDLE hXmlFile, int depth)
{
    tstring xml = TEXT("");
    tstring prefix = TEXT("");
    static const int iXmlStringSize =256;
    TCHAR szXmlString[iXmlStringSize];
    int nChars =0;
    for(int i = 0; i < depth; i++)
        prefix += TEXT("\t");
     
    // Open the XML elements
    xml += prefix;
    xml += _T("<Audio>\r\n");

    // Increase depth
    prefix += TEXT("\t");

    // Type
    nChars  = _stprintf_s (szXmlString,countof(szXmlString),_T("<Type>%s</Type>\r\n"), m_szType);
    if(nChars < 0)
    {
       LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;
    
    // Compressed?
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<Compressed>%s</Compressed>\r\n"), (bCompressed) ? TEXT("TRUE") : TEXT("FALSE"));
    if(nChars < 0)
    {
       LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // Bitrate
    if (dwBitRate)
    {
        nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<BitRate>%d</BitRate>\r\n"), dwBitRate);
    }
    else 
    {
        nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<BitRate>Unknown/Variable</BitRate>\r\n"));
        if(nChars < 0)
        {
            LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
        }
    }
    xml += prefix + szXmlString;

    // Sample Size
    if (m_dwSampleSize)
    {
        nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<SampleSize>%d</SampleSize>\r\n"), m_dwSampleSize);
        if(nChars < 0)
        {
            LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
        }
    }
    else 
    {
        nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<SampleSize>Unknown/Variable</SampleSize>\r\n"));
        if(nChars < 0)
        {
            LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
        }
    }
    xml += prefix + szXmlString;

    // nChannels
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<nChannels>%d</nChannels>\r\n"), m_dwChannels);
    if(nChars < 0)
    {
        LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // Sampling rate
    /*
    _stprintf(szXmlString, _T("<nSamplesPerSec>%d</nSamplesPerSec>\r\n"), nSamplesPerSec);
    xml += prefix + szXmlString;
    */
        
    // Reduce depth
    prefix.erase(prefix.end() - 1);
    xml += prefix;

    // Close the element
    xml += _T("</Audio>\r\n");

    // Write the xml string to the file
    DWORD nBytesWritten = 0;
    WriteFile(hXmlFile, (const void*)xml.c_str(), xml.length()*sizeof(TCHAR), &nBytesWritten, NULL);

    return S_OK;
}

PlaybackVideoInfo::PlaybackVideoInfo() : VideoInfo(), CMediaProp()
{
    bCompressed = false;
    dwBitRate = 0;
    memset(&dwAvgTimePerFrame, 0, sizeof(REFERENCE_TIME));
    memset(&decSpecificInfo, 0, sizeof(decSpecificInfo));
}

PlaybackVideoInfo::~PlaybackVideoInfo()
{
}

HRESULT 
PlaybackVideoInfo::SaveToXml( HANDLE hXmlFile, int depth )
{
    tstring xml = TEXT("");
    tstring prefix = TEXT("");
    static const int iXmlStringSize =256;
    TCHAR szXmlString[iXmlStringSize];
    int nChars =0;
    for(int i = 0; i < depth; i++)
        prefix += TEXT("\t");
     
    // Open the XML elements
    xml += prefix;
    xml += _T("<Video>\r\n");

    // Increase depth
    prefix += TEXT("\t");

    // Type
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<Type>%s</Type>\r\n"), m_szType );
    if(nChars < 0)
    {
        LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // Compressed?
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<Compressed>%s</Compressed>\r\n"), (bCompressed) ? TEXT("TRUE") : TEXT("FALSE"));
    if(nChars < 0)
    {
        LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // Bitrate
    if (m_dwBitRate)
    {
        nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<BitRate>%d</BitRate>\r\n"), m_dwBitRate);
    }
    else 
    {
        nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<BitRate>Unknown/Variable</BitRate>\r\n"));
        if(nChars < 0)
        {
            LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
        }
    }
    xml += prefix + szXmlString;

    // Sample Size
    if (m_dwFrameSize)
    {
        nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<SampleSize>%d</SampleSize>\r\n"), m_dwFrameSize);
        if(nChars < 0)
        {
            LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
        }
    }
    else 
    {
        nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<SampleSize>Unknown/Variable</SampleSize>\r\n"));
        if(nChars < 0)
        {
            LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
        }
    }
    xml += prefix + szXmlString;

    // Frame rate
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<dwAvgTimePerFrame>%d</dwAvgTimePerFrame>\r\n"), dwAvgTimePerFrame);
    if(nChars < 0)
    {
        LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // Width
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<Width>%d</Width>\r\n"), m_nWidth);
    if(nChars < 0)
    {
        LOG(TEXT("FAIL : PlaybackAudioInfo::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // Height
    _stprintf_s(szXmlString, countof(szXmlString), _T("<Height>%d</Height>\r\n"), m_nHeight);
    xml += prefix + szXmlString;

    // Interlaced?
    _stprintf_s(szXmlString, countof(szXmlString), _T("<Interlaced>%s</Interlaced>\r\n"), (m_bInterlaced) ? TEXT("TRUE") : TEXT("FALSE"));
    xml += prefix + szXmlString;

    // AspectRatio?
    _stprintf_s(szXmlString, countof(szXmlString), _T("<AspectRatio>%f:%f</AspectRatio>\r\n"), m_fXAspectRatio, m_fYAspectRatio );
    xml += prefix + szXmlString;

    // Reduce depth
    prefix.erase(prefix.end() - 1);
    xml += prefix;

    // Close the element
    xml += _T("</Video>\r\n");

    // Write the xml string to the file
    DWORD nBytesWritten = 0;
    WriteFile(hXmlFile, (const void*)xml.c_str(), xml.length()*sizeof(TCHAR), &nBytesWritten, NULL);

    return S_OK;
}

SystemInfo::SystemInfo()
{
    memset(szType, 0, sizeof(szType));
    lSampleSize = 0;
    dwBitRate = 0;
}

SystemInfo::~SystemInfo()
{
}

HRESULT SystemInfo::SaveToXml(HANDLE hXmlFile, int depth)
{
    tstring xml = TEXT("");
    tstring prefix = TEXT("");
    static const int iXmlStringSize = 256;
    TCHAR szXmlString[iXmlStringSize];
    int nChars =0;
    for(int i = 0; i < depth; i++)
        prefix += TEXT("\t");
     
    // Open the XML elements
    xml += prefix;
    xml += _T("<System>\r\n");

    // Increase depth
    prefix += TEXT("\t");

    // Type
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<Type>%s</Type>\r\n"), szType);
    if(nChars < 0)
    {
       LOG(TEXT("FAIL : SystemInfo::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // Close the element
    xml += _T("</System>\r\n");

    // Write the xml string to the file
    DWORD nBytesWritten = 0;
    WriteFile(hXmlFile, (const void*)xml.c_str(), xml.length()*sizeof(TCHAR), &nBytesWritten, NULL);

    return S_OK;
}


PlaybackMedia::PlaybackMedia() : Media()
{
    knownFilterList.clear();
    duration = 0;
    preroll = 0;
    nSystemStreams = 0;
    nVideoStreams = 0;
    nAudioStreams = 0;

    // for IAMMediaContent tests        
    bAutherName = FALSE;
    bTitle = FALSE; 
    bRating = FALSE; 
    bDescription = FALSE; 
    bCopyright = FALSE; 
    bBaseURL = FALSE; 
    bLogoURL = FALSE;  
    bLogoIconURL = FALSE;  
    bWatermarkURL = FALSE; 
    bMoreInfoURL = FALSE; 
    bMoreInfoBannerImage = FALSE; 
    bMoreInfoBannerURL = FALSE; 
    bMoreInfoText = FALSE; 
    bItemCount= FALSE; 
    bRepeatCount= FALSE; 
    bRepeatEnd= FALSE; 
    bRepeatStart= FALSE; 
    bFlags = FALSE;

    // for IAMPlayList tests    
    DWORD nItemCount = NULL;
    DWORD nRepeatCount= NULL;
    DWORD nRepeatStart= NULL;
    DWORD nRepeatEnd= NULL;
    
    memset(&systemInfo, 0, sizeof(SystemInfo));
}

PlaybackMedia::PlaybackMedia(const TCHAR* szUrl) : Media( szUrl )
{
}

PlaybackMedia::~PlaybackMedia()
{
    // Delete any downloaded file
    if (m_bDownloaded)
    {
        if (!DeleteFile(m_szCurrentUrl))
            LOG( TEXT("%S: ERROR %d@%S - failed to delete downloaded file %S."), 
                        __FUNCTION__, __LINE__, __FILE__, m_szCurrentUrl);
        m_bDownloaded = false;
    }
}



HRESULT 
PlaybackMedia::SetDownloadLocation(const TCHAR* szDownloadLocation)
{
    HRESULT hr = S_OK;

    // If the protocol for accesing this media has not been set, return error
    if (!m_bProtocolSet)
        return E_FAIL;
    errno_t  Error;
    // Form the local file url
    TCHAR szLocalFile[TEST_MAX_PATH] = TEXT("");
    // For safety, null-terminate the last character
    szLocalFile[TEST_MAX_PATH - 1] = TEXT('\0');
    Error = _tcscpy_s(szLocalFile, countof(szLocalFile), szDownloadLocation);
    if(Error != 0)
    {
       LOG(TEXT("FAIL : PlaybackMedia::SetDownloadLocation- _tcscpy_s Error"));
    }

    int length = _tcslen(m_szFileName);
    Error = _tcsncat_s(szLocalFile, countof(szLocalFile), m_szFileName, _TRUNCATE);
    if(Error != 0)
    {
       LOG(TEXT("FAIL : PlaybackMedia::SetDownloadLocation- _tcsncat_s Error"));
    }

    // Download the file from the currently set url to the download location
    LOG(TEXT("Downloading media: %s to %s"), m_szCurrentUrl, szLocalFile);
    HANDLE hFile = INVALID_HANDLE_VALUE;
    hr = UrlDownloadFile(m_szCurrentUrl, szLocalFile, &hFile);
    if (SUCCEEDED_F(hr))
    {
        LOG(TEXT("Downloaded!"));
        
        // Close the handle to the file
        CloseHandle(hFile);

        // Copy the local file url to szCurrentUrl
       Error = _tcscpy_s(m_szCurrentUrl, countof(m_szCurrentUrl), szLocalFile);
       if(Error != 0)
       {
        LOG(TEXT("FAIL : PlaybackMedia::SetDownloadLocation- _tcsncat_s Error"));
       }

        // Set the flag
        m_bDownloaded = true;
    }
    else {
        LOG(TEXT("Error while downloading!"));
    }
    return hr;
}



int PlaybackMedia::GetNumAudioStreams()
{
    return nAudioStreams;
}

int PlaybackMedia::GetNumVideoStreams()
{
    return nVideoStreams;
}

LONGLONG PlaybackMedia::GetDuration()
{
    return -1;
}


void PlaybackMedia::Reset()
{
    // Delete any downloaded file
    if (m_bDownloaded)
    {
        if (!DeleteFile(m_szCurrentUrl))
            LOG(TEXT("%S: ERROR %d@%S - failed to delete downloaded file %s."), 
                        __FUNCTION__, __LINE__, __FILE__, m_szCurrentUrl);
        else m_bDownloaded = false;
    }

    m_bProtocolSet = false;

    if( EntrySources.size() > 0)
    {
        for(DWORD i =0;i<EntrySources.size();i++)
        {
            EmptyStringList(EntrySources.at(i)->ASXEntrySourceURLs);
        }
        EntrySources.clear();
    }
    
    if( EntryList.size() > 0)
    {
        for(DWORD i =0;i<EntryList.size();i++)
        {
        EmptyStringList(EntryList.at(i)->ASXEntryParamNames);
        EmptyStringList(EntryList.at(i)->ASXEntryParamNameValues);
        }
        EntryList.clear();
    }
}

HRESULT PlaybackMedia::SaveToXml(HANDLE hXmlFile, int depth)
{
    HRESULT hr = S_OK;
    tstring xml = TEXT("");
    tstring prefix = TEXT("");
    TCHAR szXmlString[TEST_MAX_PATH];
    int nChars =0 ;
    for(int i = 0; i < depth; i++)
        prefix += TEXT("\t");
     
    // Open the XML elements
    xml += prefix;
    xml += _T("<Media>\r\n");

    // Increase depth
    depth++;
    prefix += TEXT("\t");

    TCHAR* szFileName = _tcsrchr(m_szCurrentUrl, '\\');
    szFileName++;
    TCHAR* szExt = _tcsrchr(szFileName, '.');
    int len = szExt - szFileName;
    TCHAR szName[TEST_MAX_PATH];
    errno_t Error = _tcscpy_s(szName, countof(szName), szFileName);
    if(Error != 0)
    {
       LOG(TEXT("FAIL : PlaybackMedia::SaveToXml- _tcscpy_s Error"));
    }
    szName[len] = TEXT('\0');

    // Well-known / friendly name - just the name of the file without the extension
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<Name>%s</Name>\r\n"), szName);
    if(nChars < 0)
    {
       LOG(TEXT("FAIL : PlaybackMedia::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // FileName
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<FileName>%s</FileName>\r\n"), szFileName);
    if(nChars < 0)
    {
       LOG(TEXT("FAIL : PlaybackMedia::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // Description
    // Make up a description
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<Description>%s</Description>\r\n"), m_szCurrentUrl);
    if(nChars < 0)
    {
       LOG(TEXT("FAIL : PlaybackMedia::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // Base Url placeholder
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<BaseUrl></BaseUrl>\r\n"));
    if(nChars < 0)
    {
       LOG(TEXT("FAIL : PlaybackMedia::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // Playback duration
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<PlaybackDuration>%d</PlaybackDuration>\r\n"), TICKS_TO_MSEC(duration));
    if(nChars < 0)
    {
       LOG(TEXT("FAIL : PlaybackMedia::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // preroll
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<Preroll>%I64d</Preroll>\r\n"), preroll);
    if(nChars < 0)
    {
       LOG(TEXT("FAIL : PlaybackMedia::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // Num video and audio streams
    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<VideoStreams>%d</VideoStreams>\r\n"), nVideoStreams);
    if(nChars < 0)
    {
       LOG(TEXT("FAIL : PlaybackMedia::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    nChars  = _stprintf_s (szXmlString,countof(szXmlString), _T("<AudioStreams>%d</AudioStreams>\r\n"), nAudioStreams);
    if(nChars < 0)
    {
       LOG(TEXT("FAIL : PlaybackMedia::SaveToXml - _stprintf_s Error"));
    }
    xml += prefix + szXmlString;

    // BUGBUG: Multi-language
    // BUGBUG: DRM

    // Write the xml string to the file before asking video and audio to write
    DWORD nBytesWritten = 0;
    WriteFile(hXmlFile, (const void*)xml.c_str(), xml.length()*sizeof(TCHAR), &nBytesWritten, NULL);
    xml = TEXT("");

    // Save the system stream info
    if (nSystemStreams)
        systemInfo.SaveToXml(hXmlFile, depth);

    // Save the video info
    if (nVideoStreams)
        videoInfo.SaveToXml(hXmlFile, depth);

    // Save the audio info
    if (nAudioStreams)
        audioInfo.SaveToXml(hXmlFile, depth);

    // BUGBUG: Write the xml string to the file before asking the filters to output their info

    // Close the XML elements
    prefix.erase(prefix.end() - 1);
    xml += prefix;
    xml += _T("</Media>\r\n");

    // Write the xml string to the file
    nBytesWritten = 0;
    WriteFile(hXmlFile, (const void*)xml.c_str(), xml.length()*sizeof(TCHAR), &nBytesWritten, NULL);
    
    return S_OK;
}
