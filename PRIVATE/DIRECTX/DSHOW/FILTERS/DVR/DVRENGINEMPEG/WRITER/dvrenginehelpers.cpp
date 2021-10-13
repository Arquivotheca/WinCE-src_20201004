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
#include "stdafx.h"
#include "writer.h"
#include "..\\reader\\reader.h"
#include "DvrEngineHelpers.h"
#include "..\\plumbing\\sink.h"
#include "PipelineInterfaces.h"
#include "DVREngine.h"

using std::vector;
using std::wstring;

#define DELETED_REC_SUFFIX	L".deleted"

namespace MSDvr
{

static VOID DeleteSentinelForDeletedRecording(const WCHAR *wszFileName);

CDVREngineHelpersMgr g_cDVREngineHelpersMgr;

typedef vector<CReader *>::iterator READER_IT;
typedef vector<CWriter *>::iterator WRITER_IT;
typedef vector<ISampleConsumer *>::iterator CONSUMER_IT;

HRESULT CDVREngineHelpersImpl::DeleteRecording(LPCOLESTR wszFileName)
{
	return g_cDVREngineHelpersMgr.DeleteRecording(wszFileName);
}

HRESULT CDVREngineHelpersImpl::CleanupOrphanedRecordings(LPCOLESTR wszDirName)
{
	return g_cDVREngineHelpersMgr.CleanupOrphanedRecordings(wszDirName);
}

HRESULT CDVREngineHelpersImpl::GetRecordingSizeOnDisk(LPCOLESTR pszRecordingName, LONGLONG *pllBytes)
{
	return g_cDVREngineHelpersMgr.GetRecordingSizeOnDisk(pszRecordingName, pllBytes);
}

HRESULT CDVREngineHelpersImpl::RegisterActivityCallback(IDVREngineHelpersDeleteMonitor *piDVREngineHelpersDeleteMonitor)
{
	return g_cDVREngineHelpersMgr.RegisterActivityCallback(piDVREngineHelpersDeleteMonitor);
}

HRESULT CDVREngineHelpersImpl::UnregisterActivityCallback(IDVREngineHelpersDeleteMonitor *piDVREngineHelpersDeleteMonitor)
{
	return g_cDVREngineHelpersMgr.UnregisterActivityCallback(piDVREngineHelpersDeleteMonitor);
}

void CDVREngineHelpersMgr::RegisterReader(CReader *pReader)
{
	CAutoLock lock(&m_cLock);

	// Make sure the reader doesn't exist in our list yet
	for (READER_IT it = m_vecReaders.begin(); it != m_vecReaders.end(); it++)
	{
		if (*it == pReader)
			return;
	}

	m_vecReaders.push_back(pReader);
}

void CDVREngineHelpersMgr::RegisterWriter(CWriter *pWriter)
{
	CAutoLock lock(&m_cLock);

	// Make sure the writer doesn't exist in our list yet
	for (WRITER_IT it = m_vecWriters.begin(); it != m_vecWriters.end(); it++)
	{
		if (*it == pWriter)
			return;
	}

	m_vecWriters.push_back(pWriter);
}
 void CDVREngineHelpersMgr::RegisterConsumer(ISampleConsumer *pSampleConsumer)
{
	CAutoLock lock(&m_cLock);

	// Make sure the consumer doesn't exist in our list yet
	for (CONSUMER_IT it = m_vecConsumers.begin(); it != m_vecConsumers.end(); it++)
	{
		if (*it == pSampleConsumer)
			return;
	}

	m_vecConsumers.push_back(pSampleConsumer);
}

void CDVREngineHelpersMgr::UnregisterReader(CReader *pReader)
{
	CAutoLock lock(&m_cLock);

	// Search the list for pReader and remove it
	for (READER_IT it = m_vecReaders.begin(); it != m_vecReaders.end(); it++)
	{
		if (*it == pReader)
		{
			m_vecReaders.erase(it);
			return;
		}
	}
}

void CDVREngineHelpersMgr::UnregisterWriter(CWriter *pWriter)
{
	CAutoLock lock(&m_cLock);

	// Search the list for pWriter and remove it
	for (WRITER_IT it = m_vecWriters.begin(); it != m_vecWriters.end(); it++)
	{
		if (*it == pWriter)
		{
			m_vecWriters.erase(it);
			return;
		}
	}
}

void CDVREngineHelpersMgr::UnregisterConsumer(ISampleConsumer *pSampleConsumer)
{
	CAutoLock lock(&m_cLock);

	// Search the list for pWriter and remove it
	for (CONSUMER_IT it = m_vecConsumers.begin(); it != m_vecConsumers.end(); it++)
	{
		if (*it == pSampleConsumer)
		{
			m_vecConsumers.erase(it);
			return;
		}
	}
}

struct HFindCloser
{
	HFindCloser(HANDLE hFind)
		: m_hFind(hFind) 
	{}

	~HFindCloser()
	{
		FindClose(m_hFind);
	}

private:
	HANDLE m_hFind;
};

HRESULT CDVREngineHelpersMgr::DeleteRecording(LPCOLESTR wszFileName)
{
	if (!wszFileName || !*wszFileName)
		return E_INVALIDARG;

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CDVREngineHelpersMgr::DeleteRecording(%s) entry\n"), 
				wszFileName ));

	{
		CAutoLock lock(&m_cLock);

		if (IsRecordingDeleted(wszFileName))
		{
			DbgLog((LOG_FILE_OTHER, 3, TEXT("CDVREngineHelpersMgr::DeleteRecording(%s) -- the file is marked for async deletion\n"), wszFileName ));
			return ERROR_FILE_NOT_FOUND;
		}

	// We can't delete this recording if any reader is bound to it.
	// So first walk through the readers and see if anyone is playing
	// it back.
	for (READER_IT it = m_vecReaders.begin(); it != m_vecReaders.end(); it++)
	{
		LPOLESTR wsz = NULL;
		if (SUCCEEDED((*it)->GetCurFile(&wsz, NULL)))
		{
			BOOL fFound = wsz && (_wcsicmp(wszFileName, wsz) == 0);
			CoTaskMemFree(wsz);
		
			// It's in use, bail
			if (fFound)
				{
					DbgLog((LOG_FILE_OTHER, 3, TEXT("CDVREngineHelpersMgr::DeleteRecording(%s) bailing, in use by reader\n"), 
							wszFileName ));

					return E_ACCESSDENIED;
				}
			}
		}

		// We also can't delete this recording if it is currently in progress AND
		// it is permanent.
		for (WRITER_IT itW = m_vecWriters.begin(); itW != m_vecWriters.end(); itW++)
		{
			LPOLESTR wsz = NULL;
			if (SUCCEEDED((*itW)->GetCurFile(&wsz, NULL)))
			{

				BOOL fFound = wsz && (_wcsicmp(wszFileName, wsz) == 0);
				CoTaskMemFree(wsz);
		
				// It's in progress, check the mode and return
				if (fFound)
				{
					if ((*itW)->GetCaptureMode() == STRMBUF_TEMPORARY_RECORDING)
					{
						DbgLog((LOG_FILE_OTHER, 3, TEXT("CDVREngineHelpersMgr::DeleteRecording(%s) bailing, in as temporary recording\n"), 
							wszFileName ));

						return S_OK;
					}
					else
					{
						DbgLog((LOG_FILE_OTHER, 3, TEXT("CDVREngineHelpersMgr::DeleteRecording(%s) bailing, in use as permanent recording\n"), 
								wszFileName ));

						return E_ACCESSDENIED;
					}
				}
			}
		}

		// Now we know noone is playing it back or writing to it

		// Next see if any writers know about it in their pause buffers.
		for (itW = m_vecWriters.begin(); itW != m_vecWriters.end(); itW++)
		{
			HRESULT hr = (*itW)->GetFilter().ConvertToTemporaryRecording(wszFileName);
			if (hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
			{
				if (FAILED(hr))
				{
					DbgLog((LOG_FILE_OTHER, 3, TEXT("CDVREngineHelpersMgr::DeleteRecording(%s) bailing, failed convert-to-temporary 0x%x\n"), 
							wszFileName,hr ));
				}

				return hr;
			}
		}

		// Make sure that any consumers playing in this recording go somewhere else -- where it
		// is safe to play:
		for (CONSUMER_IT itConsumer = m_vecConsumers.begin(); itConsumer != m_vecConsumers.end(); itConsumer++)
		{
			HRESULT hr = (*itConsumer)->ExitRecordingIfOrphan(wszFileName);
			if (FAILED(hr))
			{
				DbgLog((LOG_FILE_OTHER, 3, TEXT("CDVREngineHelpersMgr::DeleteRecording(%s) bailing, consumer cannot stop playing, error 0x%x\n"), 
							wszFileName,hr ));

				return hr;
			}
			else if (S_OK == hr)
			{
				DbgLog((LOG_FILE_OTHER, 3, TEXT("CDVREngineHelpersMgr::DeleteRecording(%s) --  consumer stopped playing this recording\n"), 
							wszFileName ));
			}
			// other success codes mean that the consumer didn't have to do anything.
		}
	}

	// Not found in any writer.  Now try to simply blow it away.

	return DeleteRecordingInternal(wszFileName, true);
}

HRESULT CDVREngineHelpersMgr::DeleteRecordingInternal(const WCHAR *wszFileName, bool fCanBeAsync)
{
	// To minimize interference with playback and capture, we will
	// only delete as many files as needed to bring us to at least
	// twice the maximum temporary file size. If there is more,
	// we will spin off a thread to complete the deletion as a
	// background activity. As a sentinel that the delete is incomplete,
	// we will create a file named RECORDING_NAME.deleted

	// Build the search string.  For "foo" we'll search for "foo*.dvr-*"
	wstring strSearch = wszFileName;
	strSearch += L"*.dvr-*";

	// Retrieve the path from the search string
	// ie Turn "c:\recordings\temp*.*" to "c:\recordings\"
	wstring strPath = strSearch.substr(0, strSearch.rfind(L"\\")+1);

	// Search for the first file and bail if we didn't find one
	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFileW(strSearch.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		_CDW(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));

	HFindCloser hFindCloser(hFind);
	DWORD dw = ERROR_SUCCESS;
	LONGLONG ullFreedDiskBytes = 0;

	do
	{
		if (fCanBeAsync && (ullFreedDiskBytes >= 2LL * g_uhyMaxTemporaryRecordingFileSize))
		{
			// Time to spin off a thread and create our sentinel.  If we can't,
			// go ahead and do normal deletion:

			if (StartAsyncDeletion(wszFileName))
			{
				// Return here skips the deletion of the done-deleting sentinel
				return ERROR_SUCCESS;
			}
			fCanBeAsync = false;
		}

		// Recombine the found filename and the path
		wstring strFile = strPath + fd.cFileName;

		// Try to blow the file away
		if (!DeleteFileW(strFile.c_str()))
		{
			// Someone likely has the file still open.  This could actually
			// be the writer if m_pcRecPrev is non-null.
			dw = GetLastError();
		}
		// Update our total
		LARGE_INTEGER li;
		li.LowPart = fd.nFileSizeLow;
		li.HighPart = fd.nFileSizeHigh;
		ullFreedDiskBytes += li.QuadPart;

		if (m_piDVREngineHelpersDeleteMonitor)
		{
			m_piDVREngineHelpersDeleteMonitor->DeleteInProgress();
		}
	} while (FindNextFileW(hFind, &fd));

	DeleteSentinelForDeletedRecording(wszFileName);

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CDVREngineHelpersMgr::DeleteRecording(%s) exit, status 0x%x\n"), 
				wszFileName,HRESULT_FROM_WIN32(dw) ));

	return HRESULT_FROM_WIN32(dw);
}

HRESULT CDVREngineHelpersMgr::CleanupOrphanedRecordings(LPCOLESTR wszDirName)
{
	if (!wszDirName)
		return E_POINTER;

	CAutoLock lock(&m_cLock);

	// Add a trailing backslash if necessary
	wstring strPath = wszDirName;
	if (strPath.empty() || (strPath[strPath.length()-1] != '\\'))
		strPath += '\\';

	DWORD dwRetVal = ERROR_SUCCESS;

	// CleanupOrphanedRecordingsInDir() returns ERROR_FILE_NOT_FOUND if there
	// are no recordings in the given directory. We want to return ERROR_FILE_NOT_FOUND
	// only if all directories report no files:

	DWORD dwOneDirRet = CleanupOrphanedRecordingsInDir(strPath);
	dwRetVal = dwOneDirRet;

	const std::wstring strPathChars = L"0123456789ABCDEF";
	size_t iLevelOne, iLevelTwo;

	for (iLevelOne = 0; iLevelOne < 16; ++iLevelOne)
	{
		std::wstring strLevelOneDir = strPath + strPathChars[iLevelOne] + L"\\";

		for (iLevelTwo = 0; iLevelTwo < 16; ++iLevelTwo)
		{
			std::wstring strLevelTwoDir = strLevelOneDir + strPathChars[iLevelTwo] + L"\\";

			dwOneDirRet = CleanupOrphanedRecordingsInDir(strLevelTwoDir);
			if (dwOneDirRet == ERROR_SUCCESS)
			{
				// At least one directory had something that was validly deleted.
				// The overall error must therefore be either success or some
				// error that isn't ERROR_FILE_NOT_FOUND:
				if (dwRetVal == ERROR_FILE_NOT_FOUND)
				{
					// We had one successful directory, so it is no longer true
					// that all directories were empty of deletable stuff:

					dwRetVal = dwOneDirRet;
				}
			}
			else if (dwOneDirRet != ERROR_FILE_NOT_FOUND)
			{
				// We had a genuine error in this directory -- report that error back out:

				dwRetVal = dwOneDirRet;
			}
			// Otherwise there wasn't anything to delete in this directory
			// so we just retain memory of what happened earlier (e.g. a genuine
			// error earlier;  one or more files with deletable stuff earlier;
			// all files with nothing deletable).
		}
	}

	return HRESULT_FROM_WIN32(dwRetVal);
}

DWORD CDVREngineHelpersMgr::CleanupOrphanedRecordingsInDir(std::wstring &strPath)
{
	DWORD dwRetVal = ERROR_SUCCESS;

	// Build the search string.
	wstring strSearch = strPath + L"*.dvr-*";

	// Search for the first file and bail if we didn't find one
	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFileW(strSearch.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		_CDW(ERROR_FILE_NOT_FOUND);

	HFindCloser hFindCloser(hFind);
	do
	{
		// Recombine the found filename and the path
		wstring strFile = strPath + fd.cFileName;

		// Remove the file extension to get the recording file pair
		wstring strFilePair = strFile.substr(0, strFile.rfind(L"."));

		// We have to make sure that the file is not in any writer's pause buffer
		bool fFoundFile = false;
		for (	WRITER_IT itW = m_vecWriters.begin();
				!fFoundFile && (itW != m_vecWriters.end());
				itW++)
		{
			// Walk the pause buffer searching for strFilePair
			CPauseBufferData *pPB = (*itW)->GetPauseBuffer();
			if (pPB)
			{
				for (size_t i = 0; i < pPB->GetCount(); i++)
				{
					const SPauseBufferEntry &s = (*pPB)[i];
					if (!_wcsicmp((*pPB)[i].strFilename.c_str(), strFilePair.c_str()))
					{
						fFoundFile = true;
						break;
					}
				}
				pPB->Release();
			}
		}

		// If we didn't find the file in a pause buffer...
		if (!fFoundFile)
		{
			bool fPermanent = false;

			// Open the recording file pair and verify that it's a temp recording.
			CRecordingFile recFile;
			if (ERROR_SUCCESS == recFile.Open(strFilePair.c_str(), NULL, NULL, NULL))
			{
				// Check the header
				fPermanent = !recFile.GetHeader().fTemporary;
			}
			recFile.Close();
			
			// If there was an error opening the recording, then 2 things may have
			// happened.  Likely, in a previous iteration of this loop we blew away
			// "foo_0.dvr-dat" because it was a temp recording.  Now we're looping
			// for "foo_0.dvr-idx.  Of course the above will fail since the corresponding
			// data file is gone.  This is ok.  There's also the chance that the
			// recording is corrupted and failed to open.  So we'll blow that one away
			// too.

			if (!fPermanent)
			{
				dwRetVal = ERROR_SUCCESS;

				// Try to blow the file away
				if (!DeleteFileW(strFile.c_str()))
				{
					// Someone likely has the file still open.  This could actually
					// be the writer if m_pcRecPrev is non-null.
					dwRetVal = GetLastError();
				}
			}
		}

	} while (FindNextFileW(hFind, &fd));

	// Now also finish any pending recording deletes:

	strSearch = strPath + L"*" + DELETED_REC_SUFFIX;

	// Search for the first deleted file sentineland bail if we didn't find one
	HANDLE hFindDeleted = FindFirstFileW(strSearch.c_str(), &fd);
	if (hFindDeleted != INVALID_HANDLE_VALUE)
	{
		// Okay, there are deleted recordings to clean up:

		HFindCloser hFindDeletedCloser(hFindDeleted);

		do
		{
			// Recombine the found filename and the path
			wstring strFile = strPath + fd.cFileName;

			// Remove the file extension to get the recording name:
			wstring strFilePair = strFile.substr(0, strFile.rfind(L"."));

			HRESULT hr = DeleteRecordingInternal(strFilePair.c_str(), false);
			if (FAILED(hr))
			{
				DbgLog((LOG_ERROR, 3, 
					TEXT("CDVREngineHelpersMgr::CleanupOrphanedRecordingsInDir():  failed to fully delete recording at %s\n"),
					strFilePair.c_str() ));
			}

		} while (FindNextFileW(hFind, &fd));
	}
	return dwRetVal;
}

HRESULT CDVREngineHelpersMgr::GetRecordingSizeOnDisk(LPCOLESTR pszRecordingName, LONGLONG *pllBytes)
{
	if (!pszRecordingName || !pllBytes)
		return E_POINTER;

	*pllBytes = 0;

	// Build the search string.
	wstring strSearch = pszRecordingName;
	strSearch += L"_*.dvr-*";

	// Search for the first file and bail if we didn't find one
	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFileW(strSearch.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		return E_FAIL;

	HFindCloser hFindCloser(hFind);
	do
	{
		// Update our total
		LARGE_INTEGER li;
		li.LowPart = fd.nFileSizeLow;
		li.HighPart = fd.nFileSizeHigh;
		*pllBytes += li.QuadPart;

		// Find the next file
	} while (FindNextFileW(hFind, &fd));

	return S_OK;
}

HRESULT CDVREngineHelpersMgr::RegisterActivityCallback(IDVREngineHelpersDeleteMonitor *piDVREngineHelpersDeleteMonitor)
{
	m_piDVREngineHelpersDeleteMonitor = piDVREngineHelpersDeleteMonitor;
	return S_OK;
}

HRESULT CDVREngineHelpersMgr::UnregisterActivityCallback(IDVREngineHelpersDeleteMonitor *piDVREngineHelpersDeleteMonitor)
{
	if (m_piDVREngineHelpersDeleteMonitor == piDVREngineHelpersDeleteMonitor)
	{
		m_piDVREngineHelpersDeleteMonitor = NULL;
	}
	return S_OK;
}

BOOL CDVREngineHelpersMgr::StartAsyncDeletion(const WCHAR *wszFileName)
{
	// We'll need a copy of the file name so that it will survive until the thread is done:

	WCHAR *wszFileNameCopy = _wcsdup(wszFileName);
	if (!wszFileNameCopy)
		return FALSE;

	DWORD dwThreadID;
	HANDLE hThread = CreateThread(	NULL,
								0,
								AsyncDeletionThreadMain,
								wszFileNameCopy,
								0,
								&dwThreadID);
	if (!hThread)
	{
		free(wszFileNameCopy);
		return FALSE;
	}
	CloseHandle(hThread);		// we're not going to monitor this thread

	return TRUE;
}

DWORD CDVREngineHelpersMgr::AsyncDeletionThreadMain(PVOID pFileName)
{
	WCHAR *wszFileName = static_cast<WCHAR*>(pFileName);

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CDVREngineHelpersMgr::AsyncDeletionThreadMain(%s) entry\n"), wszFileName ));

#ifdef _WIN32_WCE
	CeSetThreadPriority(GetCurrentThread(), g_dwLazyDeletePriority);
#endif // _WIN32_WCE

	HRESULT hr = g_cDVREngineHelpersMgr.DeleteRecordingInternal(wszFileName, false);
	free(wszFileName);

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CDVREngineHelpersMgr::AsyncDeletionThreadMain(%s) exit, delete status 0x%x\n"), wszFileName, hr ));
	return (DWORD) hr;
} // CDVREngineHelpersMgr::AsyncDeletionThreadMain

BOOL MSDvr::IsRecordingDeleted(const WCHAR *wszFileName)
{
	wstring strSearch = wszFileName;
	strSearch += DELETED_REC_SUFFIX;

	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFileW(strSearch.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	FindClose(hFind);
	return TRUE;
} // MSDvr::IsRecordingDeleted

VOID MSDvr::DeleteSentinelForDeletedRecording(const WCHAR *wszFileName)
{
	wstring strSearch = wszFileName;
	strSearch += DELETED_REC_SUFFIX;

	DeleteFileW(strSearch.c_str());
} // MSDvr::DeleteSentinelForDeletedRecording


}