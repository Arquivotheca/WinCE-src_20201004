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
#include "PerfTest.h"
#include "utility.h"

PerfTestConfig::PerfTestConfig()
{
	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	szPerflog = NULL;
	szPlaylist = NULL;
	bDRM = false; //if DRM is not specified it means non-DRMd file
	dwVRMode = -1;
	iBackBuffers = -1;
	iSampleInterval = 120; //arbitrary number taken as default

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
}

PerfTestConfig::~PerfTestConfig()
{
	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	for(int i = 0; i < protocols.size(); i++)
		delete protocols[i];
	protocols.clear();

	for(int i = 0; i < clipIDs.size(); i++)
		delete clipIDs[i];
	clipIDs.clear();
	
	for(int i = 0; i < statuses.size(); i++)
		delete statuses[i];
	statuses.clear();

	if (szPlaylist)
		delete szPlaylist;

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);	
}

TCHAR* PerfTestConfig::Clone(const TCHAR* string)
{
	LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	int len = _tcslen(string);
	TCHAR* clone = new TCHAR[len + 1];
	if (!clone) {
		LOG(TEXT("%S: EXCEPTION: Unable to allocate memory for string %d@%S"), __FUNCTION__, __LINE__, __FILE__);
		throw PerfTestException(TEXT("Unable to allocate memory for string"));
	}
	_tcsncpy(clone, string, len + 1);

	LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
	
	return clone;
}

void PerfTestConfig::AddProtocol(TCHAR* string)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %s"), __FUNCTION__, __LINE__, __FILE__, string);
	protocols.push_back(Clone(string));
}

int PerfTestConfig::GetNumProtocols()
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %d"), __FUNCTION__, __LINE__, __FILE__, protocols.size());
	return protocols.size();
}
	
void PerfTestConfig::GetProtocol(int seq, TCHAR* to)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S seq: %d is %s"), __FUNCTION__, __LINE__, __FILE__, seq, protocols.at(seq));
	const TCHAR* &string = const_cast<const TCHAR *&>(protocols.at(seq));
	_tcsncpy(to, string, _tcslen(string) + 1);
}

const TCHAR* PerfTestConfig::GetProtocol(int seq)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S seq: %d is %s"), __FUNCTION__, __LINE__, __FILE__, seq, protocols.at(seq));
	return protocols.at(seq);
}

void PerfTestConfig::AddClipID(TCHAR* string)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %s"), __FUNCTION__, __LINE__, __FILE__, string);
	clipIDs.push_back(Clone(string));
}

int PerfTestConfig::GetNumClipIDs()
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %d"), __FUNCTION__, __LINE__, __FILE__, clipIDs.size());
	return clipIDs.size();
}
	
void PerfTestConfig::GetClipID(int seq, TCHAR* to)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S seq: %d is %s"), __FUNCTION__, __LINE__, __FILE__, seq, clipIDs.at(seq));
	const TCHAR* &string = const_cast<const TCHAR *&>(clipIDs.at(seq));
	_tcsncpy(to, string, _tcslen(string));
}

const TCHAR* PerfTestConfig::GetClipID(int seq)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S seq: %d is %s"), __FUNCTION__, __LINE__, __FILE__, seq, clipIDs.at(seq));
	return clipIDs.at(seq);
}

	
void PerfTestConfig::AddStatusToReport(TCHAR* string)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %s"), __FUNCTION__, __LINE__, __FILE__, string);
	statuses.push_back(Clone(string));
}

int PerfTestConfig::GetNumStatusToReport()
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %d"), __FUNCTION__, __LINE__, __FILE__, statuses.size());
	return statuses.size();
}
	
bool PerfTestConfig::StatusToReport(const TCHAR* status)
{
	LOGZONES(TEXT("%S: ENTER %d@%S %s"), __FUNCTION__, __LINE__, __FILE__, status);
	
	bool found = false;
	for(int i = 0; i < statuses.size(); i++)
	{
		if (found = !_tcsncmp(statuses[i], status, _tcslen(statuses[i])))
			break;
	}
	
	LOGZONES(TEXT("%S: EXIT %d@%S %u"), __FUNCTION__, __LINE__, __FILE__, found);	
	
	return found;
}

void PerfTestConfig::SetSampleInterval(int interval) 
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %d"), __FUNCTION__, __LINE__, __FILE__, interval);

	iSampleInterval = interval;
}

int PerfTestConfig::GetSampleInterval()
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %d"), __FUNCTION__, __LINE__, __FILE__, iSampleInterval);

	return iSampleInterval;
}
void PerfTestConfig::SetPlaylist(TCHAR* playlist)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %s"), __FUNCTION__, __LINE__, __FILE__, playlist);
	
	szPlaylist = Clone(playlist);
}

TCHAR* PerfTestConfig::GetPlaylist()
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %s"), __FUNCTION__, __LINE__, __FILE__, szPlaylist);
	return szPlaylist;
}

void PerfTestConfig::SetPerflog(TCHAR* strPerflog)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %s"), __FUNCTION__, __LINE__, __FILE__, strPerflog);
	
	if (strPerflog)
	{
		szPerflog = Clone(strPerflog);
	}
	perflog = true;
}

TCHAR* PerfTestConfig::GetPerflog()
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %s"), __FUNCTION__, __LINE__, __FILE__, szPerflog);
	return szPerflog;
}

void PerfTestConfig::GetPerflog(TCHAR* to)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %s"), __FUNCTION__, __LINE__, __FILE__, szPerflog);
	if (szPerflog)
		_tcsncpy(to, szPerflog, _tcslen(szPerflog));
}

bool PerfTestConfig::UsePerflog()
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %u"), __FUNCTION__, __LINE__, __FILE__, perflog);
	return perflog;
}

void PerfTestConfig::SetDRM(bool drm)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %u"), __FUNCTION__, __LINE__, __FILE__, drm);
	bDRM = drm;
}

bool PerfTestConfig::UseDRM()
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %u"), __FUNCTION__, __LINE__, __FILE__, bDRM);
	return bDRM;
}

void PerfTestConfig::SetRepeat(int repeat)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %d"), __FUNCTION__, __LINE__, __FILE__, repeat);
	nRepeat = repeat;
}

int PerfTestConfig::GetRepeat()
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %d"), __FUNCTION__, __LINE__, __FILE__, nRepeat);
	return nRepeat;
}
void PerfTestConfig::SetVRMode(DWORD vrMode)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %d"), __FUNCTION__, __LINE__, __FILE__, vrMode);
	dwVRMode = vrMode;
}

DWORD PerfTestConfig::GetVRMode()
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %d"), __FUNCTION__, __LINE__, __FILE__, dwVRMode);
	return dwVRMode;
}

void PerfTestConfig::SetMaxbackBuffers(int maxBackBuffers)
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %d"), __FUNCTION__, __LINE__, __FILE__, maxBackBuffers);
	iBackBuffers = maxBackBuffers;
}

DWORD PerfTestConfig::GetMaxbackBuffers()
{
	LOGZONES(TEXT("%S: ENTER/EXIT %d@%S %d"), __FUNCTION__, __LINE__, __FILE__, iBackBuffers);
	return iBackBuffers;
}

