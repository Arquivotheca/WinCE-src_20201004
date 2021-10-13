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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef MEDIA_H
#define MEDIA_H

#include <vector>
#include <windows.h>
#include <stdlib.h>

class SystemInfo
{
public:
	TCHAR szType[256];
	DWORD lSampleSize;
	DWORD dwBitRate;

public:
	SystemInfo();
	~SystemInfo();
	HRESULT SaveToXml(HANDLE hXmlFile, int depth);
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


// Common properties for both video and audio
enum TimeFormat {
	TIME_MEDIA_TIME,
	TIME_FIELD,
	TIME_SAMPLE,
	TIME_BYTE,
	TIME_FRAME
};

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

class VideoInfo : public CMediaProp
{
public:
	TCHAR szIndexFile[MAX_PATH];
	TCHAR szType[256];
	DWORD nFrames;
	bool bCompressed;
	DWORD lSampleSize;
	DWORD dwBitRate;
	REFERENCE_TIME dwAvgTimePerFrame;
	int height;
	int width;
	bool bInterlaced;
	DWORD arx;
	DWORD ary;
	union {
		MPEG2VideoInfo mpeg2VideoInfo;
	} decSpecificInfo;

	// For performance information from IQualProp interface
	int m_nQualPropAvgFrameRate;
	int m_nQualPropAvgSyncOffset;
	int m_nQualPropDevSyncOffset;
	int m_nQualPropFramesDrawn;
	int m_nQualPropFramesDroppedInRenderer;
	int m_nQualPropJitter;

public:
	VideoInfo();
	virtual ~VideoInfo();
	HRESULT SaveToXml(HANDLE hXmlFile, int depth);
};

class AudioInfo : public CMediaProp
{
public:
	TCHAR szIndexFile[MAX_PATH];
	TCHAR szType[256];
	DWORD nFrames;
	bool bCompressed;
	DWORD lSampleSize;
	DWORD dwBitRate;
	WORD  nChannels;       
    DWORD nSamplesPerSec;
	union {
		MPEG1AudioInfo mpeg1AudioInfo;
	} decSpecificInfo;
	
public:
	AudioInfo();
	~AudioInfo();
	HRESULT SaveToXml(HANDLE hXmlFile, int depth);
};

enum {
	MEDIA_NAME_LENGTH = 64,
	PROTOCOL_NAME_LENGTH = 64
};

using namespace std;
class Media;

typedef vector<Media*> MediaList;
class FilterDesc;
typedef vector<FilterDesc*> FilterDescList;
typedef vector<TCHAR*> StringList;

#define MAX_PROTOCOLS 8

class BaseUrl
{
public:
	TCHAR szProtocolName[PROTOCOL_NAME_LENGTH];
	TCHAR szBaseUrl[MAX_PATH];
};

// Media encapsualtes info about the media. Properties include
// - url
// - list of filters needed to handle the media
// - media properties such as dimension, bitrate etc
// - known metadata present in the media
class Media
{
public:
	// The file name for the media
	TCHAR szFileName[MAX_PATH];

	// The well known name
	TCHAR szName[64];

	// Description of the clip
	TCHAR szDescription[128];

	// The possible base urls
	BaseUrl baseUrls[MAX_PROTOCOLS];

	// List of names of filters that are necessary to playback the media
	StringList knownFilterList;

	// The duration of the playback in milli-seconds
	LONGLONG duration;

	LONGLONG preroll;

	DWORD nSystemStreams;

	DWORD nVideoStreams;

	DWORD nAudioStreams;

	// video/audio flag
	bool m_bIsVideo;

	// System stream info
	SystemInfo systemInfo;

	// Video specific info
	VideoInfo videoInfo;

	// Audio specific info
	AudioInfo audioInfo;

private:
	// The currently used url
	TCHAR szCurrentUrl[MAX_PATH];

	bool bProtocolSet;

	// Given protocol find base url
	const TCHAR* FindBaseUrl(const TCHAR* szProtocol);

	// Was this downloaded?
	bool m_bDownloaded;

public:
	Media(const TCHAR* szUrl);
	Media();
	~Media();
	HRESULT SetProtocol(const TCHAR* szProtocol);
	HRESULT SetDownloadLocation(const TCHAR* szDownloadLocation);
	
	const TCHAR* GetUrl(const TCHAR* szProtocol = NULL);
	const TCHAR* GetName();
	LONGLONG GetDuration();

	int GetNumVideoStreams();
	int GetNumAudioStreams();
	int GetNumTextStreams();
	void Reset();
	
	HRESULT SaveToXml(HANDLE hXmlFile, int depth); 
};

Media* FindMedia(MediaList* pMediaList, TCHAR* szMediaName, int length);
HRESULT GetProtocolName(TCHAR* szMediaProtocol, TCHAR* szProtocol, int length);
HRESULT GetMediaName(TCHAR* szMediaProtocol, TCHAR* szMedia, int length);

#endif
