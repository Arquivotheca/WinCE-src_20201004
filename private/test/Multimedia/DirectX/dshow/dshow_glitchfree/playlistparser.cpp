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
#include "PlaylistParser.h"

//
// CPlaylistParser::CPlaylistParser
//
CPlaylistParser::CPlaylistParser()
	: m_eParserState(PS_NONE),
	  m_pMediaContent(NULL)
{
}

//
// CPlaylistParser::CPlaylistParser
//
CPlaylistParser::~CPlaylistParser()
{
	typedef map<string,CMediaContent*>::iterator CI;
	for(CI i = m_MediaList.begin(); i != m_MediaList.end(); i++) 
	{
		CMediaContent* foo = i->second;
		delete i->second;
		i->second = NULL;
	}
	if (m_pMediaContent != NULL)
	{
		delete m_pMediaContent;
		m_pMediaContent = NULL;
	}
}

//
// CPlaylistParser::CPlaylistParser
//
bool CPlaylistParser::StartDocument()
{
	return true;
}

//
// CPlaylistParser::StartElement. Parse the XML element provided, based on where we are in the 
// the file right now and what we expect
//
bool CPlaylistParser::StartElement(const char *szElement, int cchElement)
{
	switch (m_eParserState)
	{
		case PS_NONE:
		{
			if (strncmp("Playlist", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST; 
				return true;
			}
		}
		break;

		case PS_PLAYLIST:
		{
			if (strncmp("MediaContent", szElement, cchElement) == 0)
			{
				if (m_pMediaContent != NULL)
				{
					delete m_pMediaContent;
					m_pMediaContent = NULL;
				}

				m_pMediaContent = new CMediaContent;

				if (m_pMediaContent == NULL)
				{
					return false;
				}

				m_eParserState = PS_PLAYLIST_MEDIACONTENT;

				return true;
			}
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT:
		{
			if (strncmp("Description", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_DESCRIPTION;
				return true;
			}
			else if (strncmp("ClipName", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_CLIPNAME;
				return true;
			} 
			else if (strncmp("DRMClipName", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_DRMCLIPNAME;
				return true;
			}
			else if (strncmp("Codec", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_CODEC;
				return true;
			}
			else if (strncmp("Locations", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_LOCATIONS;
				return true;
			} 
			else if (strncmp("VideoStats", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS;
				return true;
			}
			else if (strncmp("AudioStats", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS;
				return true;
			}
			else if (strncmp("QualityControl", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL;
				return true;
			}
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS:
		{
			if (strncmp("Local", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_LOCATIONS_LOCAL;
				return true;
			}
			else if (strncmp("HTTP", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_LOCATIONS_HTTP;
				return true;
			}
			else if (strncmp("MMS", szElement, cchElement) == 0)
			{
				// MMS must be compared before MMST & MMSU since
				// cchElement is 3 and MMST & MMSU would match
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_LOCATIONS_MMS;
				return true;
			}
			else if (strncmp("MMSU", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_LOCATIONS_MMSU;
				return true;
			}
			else if (strncmp("MMST", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_LOCATIONS_MMST;
				return true;
			} 
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS:
		{
			if (strncmp("BitRate", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_BITRATE;
				return true;
			} 
			else if (strncmp("ContainsAudio", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_CONTAINSAUDIO;
				return true;
			}
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS:
		{
			if (strncmp("BitRate", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS_BITRATE;
				return true;
			} 
			else if (strncmp("Hz", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS_HZ;
				return true;
			}
			else if (strncmp("Channels", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS_CHANNELS;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL:
		{
			if (strncmp("BitRate", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_BITRATE;
				return true;
			} 
			else if (strncmp("Duration", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DURATION;
				return true;
			}
			else if (strncmp("AvgFrameRate", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGFRAMERATE;
				return true;
			}
			else if (strncmp("AvgSyncOffset", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGSYNCOFFSET;
				return true;
			} 
			else if (strncmp("DevSyncOffset", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DEVSYNCOFFSET;
				return true;
			}
			else if (strncmp("FramesDrawn", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDRAWN;
				return true;
			}
			else if (strncmp("FramesDroppedInRenderer", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINRENDERER;
				return true;
			} 
			else if (strncmp("FramesDroppedInDecoder", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINDECODER;
				return true;
			}
			else if (strncmp("AudioDiscontinuities", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AUDIODISCONTINUITIES;
				return true;
			}
			else if (strncmp("Jitter", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_JITTER;
				return true;
			}
		}

	}

	return false;
}

//
// CPlaylistParser::Attribute
//
bool CPlaylistParser::Attribute(const char *szAttribute, int cchAttribute)
{
	switch (m_eParserState)
	{
		case PS_PLAYLIST_MEDIACONTENT:
		{
			if (strncmp("id", szAttribute, cchAttribute) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_ID; 
				return true;
			}
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS:
		{
			if (strncmp("height", szAttribute, cchAttribute) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_HEIGHT; 
				return true;
			} 
			else if (strncmp("width", szAttribute, cchAttribute) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_WIDTH; 
				return true;
			} 
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DURATION:
		{
			if (strncmp("MarginOfError", szAttribute, cchAttribute) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DURATION_MARGIN_OF_ERROR; 
				return true;
			} 
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGFRAMERATE:
		{
			if (strncmp("MarginOfError", szAttribute, cchAttribute) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGFRAMERATE_MARGIN_OF_ERROR; 
				return true;
			} 
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGSYNCOFFSET:
		{
			if (strncmp("MarginOfError", szAttribute, cchAttribute) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGSYNCOFFSET_MARGIN_OF_ERROR; 
				return true;
			} 
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DEVSYNCOFFSET:
		{
			if (strncmp("MarginOfError", szAttribute, cchAttribute) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DEVSYNCOFFSET_MARGIN_OF_ERROR; 
				return true;
			} 
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDRAWN:
		{
			if (strncmp("MarginOfError", szAttribute, cchAttribute) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDRAWN_MARGIN_OF_ERROR; 
				return true;
			} 
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINRENDERER:
		{
			if (strncmp("MarginOfError", szAttribute, cchAttribute) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINRENDERER_MARGIN_OF_ERROR; 
				return true;
			} 
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINDECODER:
		{
			if (strncmp("MarginOfError", szAttribute, cchAttribute) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINDECODER_MARGIN_OF_ERROR; 
				return true;
			} 
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AUDIODISCONTINUITIES:
		{
			if (strncmp("MarginOfError", szAttribute, cchAttribute) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AUDIODISCONTINUITIES_MARGIN_OF_ERROR; 
				return true;
			} 
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_JITTER:
		{
			if (strncmp("MarginOfError", szAttribute, cchAttribute) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_JITTER_MARGIN_OF_ERROR; 
				return true;
			} 
		}
		break;
	}

	return false;
}

//
// CPlaylistParser::AttributeValue
//
bool CPlaylistParser::AttributeValue(const char *szAttributeValue, int cchAttributeValue)
{
	switch (m_eParserState)
	{
		case PS_PLAYLIST_MEDIACONTENT_ID:
		{
			m_pMediaContent->m_id = szAttributeValue;
			m_eParserState = PS_PLAYLIST_MEDIACONTENT; 
			return true;
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_HEIGHT:
		{
			m_pMediaContent->m_VideoStats.m_iHeight = atoi(szAttributeValue);
			m_eParserState = PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS;
			return true;
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_WIDTH:
		{
			m_pMediaContent->m_VideoStats.m_iWidth = atoi(szAttributeValue);
			m_eParserState = PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS;
			return true;
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DURATION_MARGIN_OF_ERROR:
		{
			m_pMediaContent->m_QualityControl.m_dDurationMarginOfError = atof(szAttributeValue);
			m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DURATION;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGFRAMERATE_MARGIN_OF_ERROR:
		{
			m_pMediaContent->m_QualityControl.m_iAvgFrameRateMarginOfError = atoi(szAttributeValue);
			m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGFRAMERATE;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGSYNCOFFSET_MARGIN_OF_ERROR:
		{
			m_pMediaContent->m_QualityControl.m_iAvgSyncOffsetMarginOfError = atoi(szAttributeValue);
			m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGSYNCOFFSET;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DEVSYNCOFFSET_MARGIN_OF_ERROR:
		{
			m_pMediaContent->m_QualityControl.m_iDevSyncOffsetMarginOfError = atoi(szAttributeValue);
			m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DEVSYNCOFFSET;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDRAWN_MARGIN_OF_ERROR:
		{
			m_pMediaContent->m_QualityControl.m_lFramesDrawnMarginOfError = atol(szAttributeValue);
			m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDRAWN;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINRENDERER_MARGIN_OF_ERROR:
		{
			m_pMediaContent->m_QualityControl.m_lFramesDroppedInRendererMarginOfError = atol(szAttributeValue);
			m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINRENDERER;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINDECODER_MARGIN_OF_ERROR:
		{
			m_pMediaContent->m_QualityControl.m_lFramesDroppedInDecoderMarginOfError = atol(szAttributeValue);
			m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINDECODER;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AUDIODISCONTINUITIES_MARGIN_OF_ERROR:
		{
			m_pMediaContent->m_QualityControl.m_iJitterMarginOfError = atoi(szAttributeValue);
			m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_JITTER;
			return true;
		}
		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_JITTER_MARGIN_OF_ERROR:
		{
			m_pMediaContent->m_QualityControl.m_iJitterMarginOfError = atoi(szAttributeValue);
			m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_JITTER;
			return true;
		}

	}

	return false;
}

//
// CPlaylistParser::Characters
//
bool CPlaylistParser::Characters(const char *szCharacters, int cchCharacters)
{
	switch (m_eParserState)
	{
		case PS_PLAYLIST:
		case PS_PLAYLIST_MEDIACONTENT:
		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS:
		case PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS:
		case PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS:
		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL:
		{
			return true;
		}
		case PS_PLAYLIST_MEDIACONTENT_DESCRIPTION:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_Descripton = szCharacters;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_CLIPNAME:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_ClipName = szCharacters;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_DRMCLIPNAME:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_DRMClipName = szCharacters;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_CODEC:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_Codec = szCharacters;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS_LOCAL:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_Locations.m_Local = szCharacters;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS_HTTP:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_Locations.m_HTTP = szCharacters;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS_MMSU:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_Locations.m_MMSU = szCharacters;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS_MMST:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_Locations.m_MMST = szCharacters;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS_MMS:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_Locations.m_MMS = szCharacters;
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_BITRATE:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_VideoStats.m_lBitRate = atol(szCharacters);
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_CONTAINSAUDIO:
		{
			if(NULL != szCharacters) {
				if(strncmp("true", szCharacters, cchCharacters) == 0)
					m_pMediaContent->m_VideoStats.m_bContainsAudio = true;
				else
					m_pMediaContent->m_VideoStats.m_bContainsAudio = false;
			}
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS_BITRATE:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_AudioStats.m_lBitRate = atol(szCharacters);
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS_HZ:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_AudioStats.m_iHz = atoi(szCharacters);
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS_CHANNELS:
		{
			if(NULL != szCharacters)
			{
				if(strncmp("Stereo", szCharacters, cchCharacters) == 0)
					m_pMediaContent->m_AudioStats.m_Channels = CAudioStats::Stereo;
				else 
					m_pMediaContent->m_AudioStats.m_Channels = CAudioStats::Mono;
			}
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_BITRATE:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_QualityControl.m_lBitRate = atol(szCharacters);
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DURATION:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_QualityControl.m_dDuration = atof(szCharacters);
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGFRAMERATE:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_QualityControl.m_iAvgFrameRate = atoi(szCharacters);
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGSYNCOFFSET:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_QualityControl.m_iAvgSyncOffset = atoi(szCharacters);
			return true;
		}
	
		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DEVSYNCOFFSET:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_QualityControl.m_iDevSyncOffset = atoi(szCharacters);
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDRAWN:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_QualityControl.m_lFramesDrawn = atol(szCharacters);
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINRENDERER:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_QualityControl.m_lFramesDroppedInRenderer = atol(szCharacters);
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINDECODER:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_QualityControl.m_lFramesDroppedInDecoder = atol(szCharacters);
			return true;
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_JITTER:
		{
			if(NULL != szCharacters)
				m_pMediaContent->m_QualityControl.m_iJitter = atoi(szCharacters);
			return true;
		}
	}

	return false;
}

//
// CPlaylistParser::EndElement
//
bool CPlaylistParser::EndElement(const char *szElement, int cchElement)
{
	switch (m_eParserState)
	{
		case PS_PLAYLIST:
		{
			if (strncmp("Playlist", szElement, cchElement) == 0)
			{
				m_eParserState = PS_DONE; 
				return true;
			}
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT:
		{
			if (strncmp("MediaContent", szElement, cchElement) == 0)
			{
				if (m_pMediaContent != NULL)
				{
					m_MediaList[m_pMediaContent->m_id] = m_pMediaContent;
					// Store the clip id and then increment the number of clips
					m_ClipIDList[m_nMediaClips] = m_pMediaContent->m_id;
					m_nMediaClips++;
					m_pMediaContent = NULL;
				}

				m_eParserState = PS_PLAYLIST;
				return true;
			}
		}
		break;

		case PS_PLAYLIST_MEDIACONTENT_DESCRIPTION:
		{
			if (strncmp("Description", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_CLIPNAME:
		{
			if (strncmp("ClipName", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_DRMCLIPNAME:
		{
			if (strncmp("DRMClipName", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_CODEC:
		{
			if (strncmp("Codec", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS_LOCAL:
		{
			if (strncmp("Local", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_LOCATIONS;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS_HTTP:
		{
			if (strncmp("HTTP", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_LOCATIONS;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS_MMSU:
		{
			if (strncmp("MMSU", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_LOCATIONS;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS_MMST:
		{
			if (strncmp("MMST", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_LOCATIONS;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS_MMS:
		{
			if (strncmp("MMS", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_LOCATIONS;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_LOCATIONS:
		{
			if (strncmp("Locations", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_BITRATE:
		{
			if (strncmp("BitRate", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS_CONTAINSAUDIO:
		{
			if (strncmp("ContainsAudio", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_VIDEOSTATS:
		{
			if (strncmp("VideoStats", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS_BITRATE:
		{
			if (strncmp("BitRate", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS_HZ:
		{
			if (strncmp("Hz", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS_CHANNELS:
		{
			if (strncmp("Channels", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_AUDIOSTATS:
		{
			if (strncmp("AudioStats", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_BITRATE:
		{
			if (strncmp("BitRate", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DURATION:
		{
			if (strncmp("Duration", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGFRAMERATE:
		{
			if (strncmp("AvgFrameRate", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_AVGSYNCOFFSET:
		{
			if (strncmp("AvgSyncOffset", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_DEVSYNCOFFSET:
		{
			if (strncmp("DevSyncOffset", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDRAWN:
		{
			if (strncmp("FramesDrawn", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINRENDERER:
		{
			if (strncmp("FramesDroppedInRenderer", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_FRAMESDROPPEDINDECODER:
		{
			if (strncmp("FramesDroppedInDecoder", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL_JITTER:
		{
			if (strncmp("Jitter", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL;
				return true;
			}
		}

		case PS_PLAYLIST_MEDIACONTENT_QUALITYCONTROL:
		{
			if (strncmp("QualityControl", szElement, cchElement) == 0)
			{
				m_eParserState = PS_PLAYLIST_MEDIACONTENT;
				return true;
			}
		}
	}

	return false;
}

//
// CPlaylistParser::EndDocument
//
bool CPlaylistParser::EndDocument()
{
	return true;
}


int CPlaylistParser::GetNumMediaClips()
{
	return m_nMediaClips;
}

void CPlaylistParser::GetMediaClipID(int seq, TCHAR* szClipID, int szCount)
{
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, m_ClipIDList[seq].c_str(), -1, szClipID, szCount);
}

CMediaContent* CPlaylistParser::GetMediaContent(const TCHAR* clipID)
{
	char szClipID[MAX_PATH];
	WideCharToMultiByte(CP_ACP, 0, clipID, -1, szClipID, sizeof(szClipID), NULL, NULL);
	string s = szClipID;
	return m_MediaList[s];
}
