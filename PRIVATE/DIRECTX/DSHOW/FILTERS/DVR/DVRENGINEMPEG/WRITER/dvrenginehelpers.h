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
#pragma once
#include "DvrInterfaces.h"

namespace MSDvr
{

class CReader;
class CWriter;
struct ISampleConsumer;

class CDVREngineHelpersImpl : public IDVREngineHelpers
{
public:
	// IDVREngineHelpers
	STDMETHOD(DeleteRecording)(LPCOLESTR pszFileName);
	STDMETHOD(CleanupOrphanedRecordings)(LPCOLESTR pszDirName);
	STDMETHOD(GetRecordingSizeOnDisk)(LPCOLESTR pszRecordingName, LONGLONG *pllBytes);
	STDMETHOD(RegisterActivityCallback)(IDVREngineHelpersDeleteMonitor *piDVREngineHelpersDeleteMonitor);
	STDMETHOD(UnregisterActivityCallback)(IDVREngineHelpersDeleteMonitor *piDVREngineHelpersDeleteMonitor);
};

class CDVREngineHelpersMgr
{
public:
	void RegisterReader(CReader *pReader);
	void RegisterWriter(CWriter *pWriter);
	void RegisterConsumer(ISampleConsumer *pSampleConsumer);
	void UnregisterReader(CReader *pReader);
	void UnregisterWriter(CWriter *pWriter);
	void UnregisterConsumer(ISampleConsumer *pSampleConsumer);

	// IDVREngineHelpers
	HRESULT DeleteRecording(LPCOLESTR pszFileName);
	HRESULT CleanupOrphanedRecordings(LPCOLESTR pszDirName);
	HRESULT GetRecordingSizeOnDisk(LPCOLESTR pszRecordingName, LONGLONG *pllBytes);
	HRESULT RegisterActivityCallback(IDVREngineHelpersDeleteMonitor *piDVREngineHelpersDeleteMonitor);
	HRESULT UnregisterActivityCallback(IDVREngineHelpersDeleteMonitor *piDVREngineHelpersDeleteMonitor);

private:

	HRESULT DeleteRecordingInternal(const WCHAR *wszFileName, bool fCanBeAsync);
	DWORD CleanupOrphanedRecordingsInDir(std::wstring &strDirName);
	BOOL StartAsyncDeletion(const WCHAR *wszFileName);
	static DWORD WINAPI AsyncDeletionThreadMain(PVOID pFileName);

	CCritSec m_cLock;
	std::vector<CReader *> m_vecReaders;
	std::vector<CWriter *> m_vecWriters;
	std::vector<ISampleConsumer *> m_vecConsumers;

	IDVREngineHelpersDeleteMonitor *m_piDVREngineHelpersDeleteMonitor;
};

extern CDVREngineHelpersMgr g_cDVREngineHelpersMgr;

extern BOOL IsRecordingDeleted(const WCHAR *wszFileName);
 
}
