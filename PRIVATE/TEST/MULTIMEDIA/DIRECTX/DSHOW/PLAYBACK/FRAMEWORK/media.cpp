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

#include "globals.h"
#include "logging.h"
#include "globals.h"
#include "TestGraph.h"
#include "Media.h"
#include "utility.h"

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

AudioInfo::AudioInfo() : CMediaProp()
{
}

AudioInfo::~AudioInfo()
{
}

HRESULT AudioInfo::SaveToXml(HANDLE hXmlFile, int depth)
{
	tstring xml = TEXT("");
	tstring prefix = TEXT("");
	TCHAR szXmlString[256];

	for(int i = 0; i < depth; i++)
		prefix += TEXT("\t");
	 
	// Open the XML elements
	xml += prefix;
	xml += _T("<Audio>\r\n");

	// Increase depth
	prefix += TEXT("\t");

	// Type
	_stprintf(szXmlString, _T("<Type>%s</Type>\r\n"), szType);
	xml += prefix + szXmlString;
	
	// Compressed?
	_stprintf(szXmlString, _T("<Compressed>%s</Compressed>\r\n"), (bCompressed) ? TEXT("TRUE") : TEXT("FALSE"));
	xml += prefix + szXmlString;

	// Bitrate
	if (dwBitRate)
		_stprintf(szXmlString, _T("<BitRate>%d</BitRate>\r\n"), dwBitRate);
	else 
		_stprintf(szXmlString, _T("<BitRate>Unknown/Variable</BitRate>\r\n"));
	xml += prefix + szXmlString;

	// Sample Size
	if (lSampleSize)
		_stprintf(szXmlString, _T("<SampleSize>%d</SampleSize>\r\n"), lSampleSize);
	else 
		_stprintf(szXmlString, _T("<SampleSize>Unknown/Variable</SampleSize>\r\n"));
	xml += prefix + szXmlString;

	// nChannels
	_stprintf(szXmlString, _T("<nChannels>%d</nChannels>\r\n"), nChannels);
	xml += prefix + szXmlString;

	// Sampling rate
	_stprintf(szXmlString, _T("<nSamplesPerSec>%d</nSamplesPerSec>\r\n"), nSamplesPerSec);
	xml += prefix + szXmlString;
		
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

VideoInfo::VideoInfo() : CMediaProp()
{	
	// Performance information
	m_nQualPropAvgFrameRate = 0;
	m_nQualPropAvgSyncOffset = 0;
	m_nQualPropDevSyncOffset = 0;
	m_nQualPropFramesDrawn = 0;
	m_nQualPropFramesDroppedInRenderer = 0;
	m_nQualPropJitter = 0;
}

VideoInfo::~VideoInfo()
{
}

HRESULT VideoInfo::SaveToXml(HANDLE hXmlFile, int depth)
{
	tstring xml = TEXT("");
	tstring prefix = TEXT("");
	TCHAR szXmlString[256];

	for(int i = 0; i < depth; i++)
		prefix += TEXT("\t");
	 
	// Open the XML elements
	xml += prefix;
	xml += _T("<Video>\r\n");

	// Increase depth
	prefix += TEXT("\t");

	// Type
	_stprintf(szXmlString, _T("<Type>%s</Type>\r\n"), szType);
	xml += prefix + szXmlString;

	// Compressed?
	_stprintf(szXmlString, _T("<Compressed>%s</Compressed>\r\n"), (bCompressed) ? TEXT("TRUE") : TEXT("FALSE"));
	xml += prefix + szXmlString;

	// Bitrate
	if (dwBitRate)
		_stprintf(szXmlString, _T("<BitRate>%d</BitRate>\r\n"), dwBitRate);
	else 
		_stprintf(szXmlString, _T("<BitRate>Unknown/Variable</BitRate>\r\n"));
	xml += prefix + szXmlString;

	// Sample Size
	if (lSampleSize)
		_stprintf(szXmlString, _T("<SampleSize>%d</SampleSize>\r\n"), lSampleSize);
	else 
		_stprintf(szXmlString, _T("<SampleSize>Unknown/Variable</SampleSize>\r\n"));
	xml += prefix + szXmlString;

	// Frame rate
	_stprintf(szXmlString, _T("<dwAvgTimePerFrame>%d</dwAvgTimePerFrame>\r\n"), dwAvgTimePerFrame);
	xml += prefix + szXmlString;

	// Width
	_stprintf(szXmlString, _T("<Width>%d</Width>\r\n"), width);
	xml += prefix + szXmlString;

	// Height
	_stprintf(szXmlString, _T("<Height>%d</Height>\r\n"), height);
	xml += prefix + szXmlString;

	// Interlaced?
	_stprintf(szXmlString, _T("<Interlaced>%s</Interlaced>\r\n"), (bInterlaced) ? TEXT("TRUE") : TEXT("FALSE"));
	xml += prefix + szXmlString;

	// AspectRatio?
	if (arx && ary)
	{
		_stprintf(szXmlString, _T("<AspectRatio>%d:%d</AspectRatio>\r\n"), arx, ary);
		xml += prefix + szXmlString;
	}

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
	TCHAR szXmlString[256];

	for(int i = 0; i < depth; i++)
		prefix += TEXT("\t");
	 
	// Open the XML elements
	xml += prefix;
	xml += _T("<System>\r\n");

	// Increase depth
	prefix += TEXT("\t");

	// Type
	_stprintf(szXmlString, _T("<Type>%s</Type>\r\n"), szType);
	xml += prefix + szXmlString;

	// Close the element
	xml += _T("</System>\r\n");

	// Write the xml string to the file
	DWORD nBytesWritten = 0;
	WriteFile(hXmlFile, (const void*)xml.c_str(), xml.length()*sizeof(TCHAR), &nBytesWritten, NULL);

	return S_OK;
}


Media::Media()
{
	// BUGBUG: this messes up vtables if any on CE?
	memset(this, 0, sizeof(Media));
	m_bIsVideo = false;
}

Media::Media(const TCHAR* szUrl)
{
	memset(this, 0, sizeof(Media));
	_tcsncpy(szCurrentUrl, szUrl, countof(szCurrentUrl));
}

Media::~Media()
{
	// Delete any downloaded file
	if (m_bDownloaded)
	{
		if (!DeleteFile(szCurrentUrl))
			LOG(TEXT("%S: ERROR %d@%S - failed to delete downloaded file %s."), __FUNCTION__, __LINE__, __FILE__, szCurrentUrl);
		m_bDownloaded = false;
	}
}

const TCHAR* Media::FindBaseUrl(const TCHAR* szProtocol)
{
	for(int i = 0; i < MAX_PROTOCOLS; i++)
	{
		if (!_tcsncmp(szProtocol, baseUrls[i].szProtocolName, countof(baseUrls[i].szProtocolName)))
			return baseUrls[i].szBaseUrl;
	}
	return NULL;
}


HRESULT Media::SetProtocol(const TCHAR* szProtocol)
{
	HRESULT hr = S_OK;

	const TCHAR* szBaseUrl = FindBaseUrl(szProtocol);
	if (!szBaseUrl)
		return E_FAIL;

	// Copy the base url to the current url
	_tcsncpy(szCurrentUrl, szBaseUrl, countof(szCurrentUrl));

	// Concat the base url and file name
	int length = _tcslen(szFileName);
	_tcsncat(szCurrentUrl, szFileName, length); 

	bProtocolSet = true;

	// Return a pointer to the private szCurrentUrl - this should not be modified by the caller
	return hr;
}

HRESULT Media::SetDownloadLocation(const TCHAR* szDownloadLocation)
{
	HRESULT hr = S_OK;

	// If the protocol for accesing this media has not been set, return error
	if (!bProtocolSet)
		return E_FAIL;

	// Form the local file url
	TCHAR szLocalFile[MAX_PATH] = TEXT("");
	// For safety, null-terminate the last character
	szLocalFile[MAX_PATH - 1] = TEXT('\0');
	_tcsncpy(szLocalFile, szDownloadLocation, countof(szLocalFile) - 1);

	int length = _tcslen(szFileName);
	_tcsncat(szLocalFile, szFileName, length);

	// Download the file from the currently set url to the download location
	LOG(TEXT("Downloading media: %s to %s"), szCurrentUrl, szLocalFile);
	HANDLE hFile = INVALID_HANDLE_VALUE;
	hr = UrlDownloadFile(szCurrentUrl, szLocalFile, &hFile);
	if (SUCCEEDED_F(hr))
	{
		LOG(TEXT("Downloaded!"));
		
		// Close the handle to the file
		CloseHandle(hFile);

		// Copy the local file url to szCurrentUrl
		_tcsncpy(szCurrentUrl, szLocalFile, countof(szLocalFile));

		// Set the flag
		m_bDownloaded = true;
	}
	else {
		LOG(TEXT("Error while downloading!"));
	}
	return hr;
}

const TCHAR* Media::GetUrl(const TCHAR* szProtocol)
{
	if (!szProtocol)
	{
		return (bProtocolSet || _tcscmp(szCurrentUrl, TEXT(""))) ? szCurrentUrl : NULL;
	}
	
	const TCHAR* szBaseUrl = FindBaseUrl(szProtocol);
	if (!szBaseUrl)
		return NULL;

	// Copy the base url to the current url
	_tcsncpy(szCurrentUrl, szBaseUrl, countof(szCurrentUrl));

	// Concat the base url and file name
	_tcsncat(szCurrentUrl, szFileName, countof(szFileName)); 

	// Return a pointer to the private szCurrentUrl - this should not be modified by the caller
	return szCurrentUrl;
} 

int Media::GetNumAudioStreams()
{
	return nAudioStreams;
}

int Media::GetNumVideoStreams()
{
	return nVideoStreams;
}

LONGLONG Media::GetDuration()
{
	return -1;
}

const TCHAR* Media::GetName()
{
	return szName;
}

// Utility method
Media* FindMedia(MediaList* pMediaList, TCHAR* szMediaName, int len)
{
	for (int i = 0; i < pMediaList->size(); i++)
	{
		Media* pMedia = pMediaList->at(i);

		if (!_tcsncmp(pMedia->GetName(), szMediaName, len))
			return pMedia;
	}
	return NULL;
}

HRESULT GetProtocolName(TCHAR* szMediaProtocol, TCHAR* szProtocol, int length)
{
	if (length <= 0 || !szProtocol || !szMediaProtocol)
		return E_INVALIDARG;

	const TCHAR* szTempProtocol = _tcschr(szMediaProtocol, TEXT(':'));
	if (!szTempProtocol)
		return E_FAIL;

	_tcsncpy(szProtocol, szTempProtocol + 1, length);
	return S_OK;
}

HRESULT GetMediaName(TCHAR* szMediaProtocol, TCHAR* szMedia, int length)
{
	// How to handle length
	int nFields = _stscanf(szMediaProtocol, TEXT("%[^:]s"), szMedia);
	return (nFields) ? S_OK : E_FAIL;
}

void Media::Reset()
{
	// Delete any downloaded file
	if (m_bDownloaded)
	{
		if (!DeleteFile(szCurrentUrl))
			LOG(TEXT("%S: ERROR %d@%S - failed to delete downloaded file %s."), __FUNCTION__, __LINE__, __FILE__, szCurrentUrl);
		else m_bDownloaded = false;
	}

	bProtocolSet = false;
}

HRESULT Media::SaveToXml(HANDLE hXmlFile, int depth)
{
	HRESULT hr = S_OK;
	tstring xml = TEXT("");
	tstring prefix = TEXT("");
	TCHAR szXmlString[MAX_PATH];

	for(int i = 0; i < depth; i++)
		prefix += TEXT("\t");
	 
	// Open the XML elements
	xml += prefix;
	xml += _T("<Media>\r\n");

	// Increase depth
	depth++;
	prefix += TEXT("\t");

	TCHAR* szFileName = _tcsrchr(szCurrentUrl, '\\');
	szFileName++;
	TCHAR* szExt = _tcsrchr(szFileName, '.');
	int len = szExt - szFileName;
	TCHAR szName[MAX_PATH];
	_tcsncpy(szName, szFileName, len);
	szName[len] = TEXT('\0');

	// Well-known / friendly name - just the name of the file without the extension
	_stprintf(szXmlString, _T("<Name>%s</Name>\r\n"), szName);
	xml += prefix + szXmlString;

	// FileName
	_stprintf(szXmlString, _T("<FileName>%s</FileName>\r\n"), szFileName);
	xml += prefix + szXmlString;

	// Description
	// Make up a description
	_stprintf(szXmlString, _T("<Description>%s</Description>\r\n"), szCurrentUrl);
	xml += prefix + szXmlString;

	// Base Url placeholder
	_stprintf(szXmlString, _T("<BaseUrl></BaseUrl>\r\n"));
	xml += prefix + szXmlString;

	// Playback duration
	_stprintf(szXmlString, _T("<PlaybackDuration>%d</PlaybackDuration>\r\n"), TICKS_TO_MSEC(duration));
	xml += prefix + szXmlString;

	// preroll
	_stprintf(szXmlString, _T("<Preroll>%I64d</Preroll>\r\n"), preroll);
	xml += prefix + szXmlString;

	// Num video and audio streams
	_stprintf(szXmlString, _T("<VideoStreams>%d</VideoStreams>\r\n"), nVideoStreams);
	xml += prefix + szXmlString;

	_stprintf(szXmlString, _T("<AudioStreams>%d</AudioStreams>\r\n"), nAudioStreams);
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