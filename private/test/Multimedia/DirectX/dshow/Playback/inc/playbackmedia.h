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

#ifndef _PLAYBACK_MEDIA_H
#define _PLAYBACK_MEDIA_H

#include <windows.h>
#include <vector>
#include <stdlib.h>
#include "media.h"

struct ASXEntrySource
{
	DWORD ASXEntrySourceCount;
	DWORD ASXEntryFlags;
	StringList ASXEntrySourceURLs;
};

struct ASXEntryData
{
	DWORD ASXEntryIndex;
	DWORD ASXEntryParamCount;
	StringList ASXEntryParamNames;
	StringList ASXEntryParamNameValues;
};

typedef std::vector<ASXEntryData*> ASXEntryList;
typedef std::vector<ASXEntrySource*> ASXEntrySources;

class SystemInfo
{
public:
	TCHAR szType[256];
	DWORD lSampleSize;
	DWORD dwBitRate;

public:
	SystemInfo();
	~SystemInfo();
	HRESULT SaveToXml( HANDLE hXmlFile, int depth );
};

struct MPEG1AudioInfo
{
	WORD layer;
	WORD mode;
};

struct MPEG2VideoInfo
{
	DWORD dwProfile;     
    DWORD dwLevel;            
};

// Time Format Helper class
class CTimeFormat
{
public:
	CTimeFormat( GUID formatID = TIME_FORMAT_NONE, 
				 LONGLONG llSetGetTolerance = 0,
				 LONGLONG llStopTolerance = 0 );
	CTimeFormat( const CTimeFormat &data );
	CTimeFormat &operator= ( const CTimeFormat &data );

	bool  IsEqual( GUID Guid );
	TCHAR *GetName() { return m_szGuid; }
	const GUID *GetGuid() { return &m_Guid; }

public:
	LONGLONG  m_llSetGetTolerance;
	LONGLONG  m_llStopTolerance;

protected:
	GUID m_Guid;
	TCHAR	m_szGuid[32];
};

typedef std::vector<CTimeFormat> TimeFormatList;

// This class captures basic media properties under different time
// format.  The legacy dshow tests use predefined media properties a 
// lot and we use it to accommodate that.
class CMediaProp
{
public:
	CMediaProp();
	virtual ~CMediaProp();

	CMediaProp &operator= (const CMediaProp &prop);

	// data members
	bool m_bSeekBackwards;
	bool m_bSeekForwards;
	LONGLONG m_llDurations;
	bool m_bPrerollSupported;
	LONGLONG m_llPreroll;

	// Seeking Capabilities
	DWORD m_dwSeekCaps;

	// MEDIA_TIME
	LONGLONG m_llMediaTimeEarliest;
	LONGLONG m_llMediaTimeLatest;
	LONGLONG m_llMediaTimeDuration;
	bool	 m_bMediaTimePreroll;
	LONGLONG m_llMediaTimePreroll;

	// FIELD
	LONGLONG m_llFieldEarliest;
	LONGLONG m_llFieldLatest;
	LONGLONG m_llFieldDuration;
	bool     m_bFieldPreroll;
	LONGLONG m_llFieldPreroll;

	// SAMPLE
	LONGLONG m_llSampleEarliest;
	LONGLONG m_llSampleLatest;
	LONGLONG m_llSampleDuration;
	bool     m_bSamplePreroll;
	LONGLONG m_llSamplePreroll;

	// BYTE
	LONGLONG m_llByteEarliest;
	LONGLONG m_llByteLatest;
	LONGLONG m_llByteDuration;
	bool     m_bBytePreroll;
	LONGLONG m_llBytePreroll;

	// Frame
	LONGLONG m_llFrameEarliest;
	LONGLONG m_llFrameLatest;
	LONGLONG m_llFrameDuration;
	bool     m_bFramePreroll;
	LONGLONG m_llFramePreroll;
};

class PlaybackVideoInfo : public VideoInfo, public CMediaProp
{
public:
	bool bCompressed;
	DWORD dwBitRate;
	REFERENCE_TIME dwAvgTimePerFrame;
	union {
		MPEG2VideoInfo mpeg2VideoInfo;
	} decSpecificInfo;

public:
	PlaybackVideoInfo();
	virtual ~PlaybackVideoInfo();
	HRESULT SaveToXml( HANDLE hXmlFile, int depth );
};

class PlaybackAudioInfo : public AudioInfo, public CMediaProp
{
public:
	bool bCompressed;
	DWORD dwBitRate;
	union {
		MPEG1AudioInfo mpeg1AudioInfo;
	} decSpecificInfo;
	
public:
	PlaybackAudioInfo();
	~PlaybackAudioInfo();
	HRESULT SaveToXml( HANDLE hXmlFile, int depth );
};


class PlaybackMedia;

class FilterDesc;
typedef std::vector<FilterDesc*> FilterDescList;

// Media encapsualtes info about the media. Properties include
// - url
// - list of filters needed to handle the media
// - media properties such as dimension, bitrate etc
// - known metadata present in the media
class PlaybackMedia : public Media
{
public:
	PlaybackMedia(const TCHAR* szUrl);
	PlaybackMedia();
	virtual ~PlaybackMedia();

	HRESULT SetDownloadLocation(const TCHAR* szDownloadLocation);	
	LONGLONG GetDuration();
	int GetNumVideoStreams();
	int GetNumAudioStreams();
	int GetNumTextStreams();
	void Reset();	
	HRESULT SaveToXml(HANDLE hXmlFile, int depth); 

public:
	// List of names of filters that are necessary to playback the media
	StringList knownFilterList;

	// The duration of the playback in milli-seconds
	LONGLONG duration;
	LONGLONG preroll;

	DWORD nSystemStreams;
	DWORD nVideoStreams;
	DWORD nAudioStreams;

	// System stream info
	SystemInfo systemInfo;
	// Video specific info
	PlaybackVideoInfo videoInfo;
	// Audio specific info
	PlaybackAudioInfo audioInfo;

	//Media content meta data,supported by IAMMediaContent
	TCHAR szAutherName[MAX_PATH];
	BOOL bAutherName;
	
	TCHAR szTitle[MAX_PATH];
	BOOL bTitle;

	TCHAR szRating[MAX_PATH];
	BOOL bRating;

	TCHAR szDescription[MAX_PATH];
	BOOL bDescription;

	TCHAR szCopyright[MAX_PATH];
	BOOL bCopyright;

	TCHAR szBaseURL[MAX_PATH];
	BOOL bBaseURL;

	TCHAR szLogoURL[MAX_PATH];
	BOOL bLogoURL;

	TCHAR szLogoIconURL[MAX_PATH];
	BOOL bLogoIconURL;

	TCHAR szWatermarkURL[MAX_PATH];
	BOOL bWatermarkURL;

	TCHAR szMoreInfoURL[MAX_PATH];
	BOOL bMoreInfoURL;

	TCHAR szMoreInfoBannerImage[MAX_PATH];
	BOOL bMoreInfoBannerImage;

	TCHAR szMoreInfoBannerURL[MAX_PATH];
	BOOL bMoreInfoBannerURL;

	TCHAR szMoreInfoText[MAX_PATH];
	BOOL bMoreInfoText;

	//for playlist and playlistitem interfact test.
	DWORD nItemCount;
	BOOL bItemCount;
	
	DWORD nRepeatCount;
	BOOL bRepeatCount;
	
	DWORD nRepeatStart;
	BOOL bRepeatStart;
		
	DWORD nRepeatEnd;
	BOOL bRepeatEnd;

	// for graphplaylist test.
	ASXEntrySources EntrySources;

	// for mediacontentex test.
	ASXEntryList EntryList;

	DWORD nFlags;
	BOOL bFlags;
	
};

typedef std::vector<PlaybackMedia*> PBMediaList;

#endif

