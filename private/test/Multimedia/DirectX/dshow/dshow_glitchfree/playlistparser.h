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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#pragma once

#include "SAXParser.h"
#include "MediaContent.h"
#include <map>

class CPlaylistParser :
	public SAXParser
{
public:
	CPlaylistParser();
	virtual ~CPlaylistParser();

	// Implementation of SAXParser pure virtual methods
	bool StartDocument();
	bool StartElement(const char *szElement, int cchElement);
	bool Attribute(const char *szAttribute, int cchAttribute);
	bool AttributeValue(const char *szAttributeValue, int cchAttributeValue);
	bool Characters(const char *szCharacters, int cchCharacters);
	bool EndElement(const char *szElement, int cchElement);
	bool EndDocument();

	// Playlist methods
	int GetNumMediaClips();
	CMediaContent* GetMediaContent(const TCHAR* clipID);
	//const TCHAR* GetMediaClipID(int seq);
	void CPlaylistParser::GetMediaClipID(int index, TCHAR* szClipId, int szCount);

private:
	enum EParserState
	{
		PS_NONE = 0,
		PS_PLAYLIST,
		PS_PLAYLIST_MEDIACONTENT,
		PS_PLAYLIST_MEDIACONTENT_ID,
		PS_PLAYLIST_MEDIACONTENT_DESCRIPTION,
		PS_PLAYLIST_MEDIACONTENT_CLIPNAME,
		PS_PLAYLIST_MEDIACONTENT_DRMCLIPNAME,
		PS_PLAYLIST_MEDIACONTENT_CODEC,
		PS_PLAYLIST_MEDIACONTENT_LOCATIONS,
		PS_PLAYLIST_MEDIACONTENT_LOCATIONS_LOCAL,
		PS_PLAYLIST_MEDIACONTENT_LOCATIONS_HTTP,
		PS_PLAYLIST_MEDIACONTENT_LOCATIONS_MMSU,
		PS_PLAYLIST_MEDIACONTENT_LOCATIONS_MMST,
		PS_PLAYLIST_MEDIACONTENT_LOCATIONS_MMS,
		PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS,
		PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_HEIGHT,
		PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_WIDTH,
		PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_BITRATE,
		PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_CONTAINSAUDIO,
		PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS,
		PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS_BITRATE,
		PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS_HZ,
		PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS_CHANNELS,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_BITRATE,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DURATION,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGFRAMERATE,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGSYNCOFFSET,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DEVSYNCOFFSET,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDRAWN,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINRENDERER,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINDECODER,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_JITTER,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AUDIODISCONTINUITIES,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DURATION_MARGIN_OF_ERROR,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGFRAMERATE_MARGIN_OF_ERROR,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGSYNCOFFSET_MARGIN_OF_ERROR,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DEVSYNCOFFSET_MARGIN_OF_ERROR,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDRAWN_MARGIN_OF_ERROR,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINRENDERER_MARGIN_OF_ERROR,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINDECODER_MARGIN_OF_ERROR,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AUDIODISCONTINUITIES_MARGIN_OF_ERROR,
		PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_JITTER_MARGIN_OF_ERROR,
		PS_DONE
	};

private:
	int m_nMediaClips;
	typedef basic_string<TCHAR> tstring;
	map<string,CMediaContent*> m_MediaList;
	map<int,string> m_ClipIDList;
	map<tstring,CMediaContent*> m_tMediaList;
	EParserState   m_eParserState;
	CMediaContent *m_pMediaContent;
};