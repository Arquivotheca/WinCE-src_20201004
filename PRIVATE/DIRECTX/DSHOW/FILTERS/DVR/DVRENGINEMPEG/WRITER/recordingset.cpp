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
#include "recordingset.h"
#include "ComponentWorkerThread.h"
#include "DVREngine.h"
#include "DVRInterfaces.h"

#ifndef SHIP_BUILD
#define DEBUG_FILE_POSITION 1
#endif

#ifdef DEBUG_FILE_POSITION
#define DFP(x) DbgLog(x)
#else
#define DFP(x)
#endif

#define ROUND_UP(_value, _boundary) (((_value) + ((_boundary) - 1)) & ~((_boundary) - 1))

using std::wstring;
using std::vector;
using std::map;

#ifndef MONITOR_WRITE_FILE
#define WRITEFILE(hFile,pBuf,dwBytesIn,pdwBytesWritten,unused)	WriteFile(hFile,pBuf,dwBytesIn,pdwBytesWritten,unused)
#else
static BOOL WRITEFILE(HANDLE hFile, PCVOID pBuf, DWORD dwBytesIn, PDWORD pdwBytesWritten, OVERLAPPED *unused);
#endif

namespace MSDvr
{

enum
{
	DATA_LINE_LEN			= 24,			// Minimum bytes of each line of the
											// data file.  Lines may be longer but
											// reading this much will yield the
											// info the parse the remaining data.
	DEFAULT_MAX_FILE_SIZE	= 100000000,	// 100 MB
	FILE_VER				= 103,			// File version 1.03
	FILE_VER_101			= 101,			// File version 1.01 is also supported
	FILE_VER_102			= 102,			// File version 1.02 is also supported
	WAIT_FOR_WRITE_DURATION	= 25,			// When reading, amount of time to wait
											// to see if the file is being written.
	CUSTOM_DATA_VER			= 100,			// Custom Data version 1.00
	MEDIA_TYPE_VER			= 100,			// Media Type version 1.00
	SECTOR_SIZE				= 512,			// Size of a sector
	EXTRA_PADDING_FOR_LAST_SAMPLE = 132000	// Number of extra bytes of padding to allow for the sample that pushes us over the top
};

struct FileIOTimeLogger
{
	static const DWORD WORRISOME_FILE_IO_DURATION = 100;
	static const DWORD TROUBLESOME_FILE_IO_DURATION = 500;

	FileIOTimeLogger(const _TCHAR *pszAction)
		: m_dwStartTick(GetTickCount())
		, m_pszAction(pszAction)
	{
	}

	~FileIOTimeLogger()
	{
		DWORD dwStopTick = GetTickCount();
		DWORD dwElapsedTicks = 0;

		if (dwStopTick < m_dwStartTick)
		{
			dwElapsedTicks = dwStopTick + 1 + (0xffffffff - m_dwStartTick);
		}
		else
		{
			dwElapsedTicks = dwStopTick - m_dwStartTick;
		}
		if (dwElapsedTicks >= WORRISOME_FILE_IO_DURATION)
		{
			DbgLog((LOG_FILE_OTHER, 3, TEXT("%s:  elapsed i/o time of %u ms\n"), m_pszAction, dwElapsedTicks));
		}
		if (dwElapsedTicks >= TROUBLESOME_FILE_IO_DURATION)
		{
            DbgLog((LOG_FILE_OTHER, 3, TEXT("### DISK BOTTLENECK -- %s:  elapsed i/o time of %u ms ###\n"), m_pszAction, dwElapsedTicks));
		}
	}

private:
	DWORD m_dwStartTick;
	const _TCHAR *m_pszAction;
};

// ################################################
//      CRecordingSetWriteIOThread
// ################################################

class CRecordingSetWriteIOThread : public CComponentWorkerThread
{
public:
	CRecordingSetWriteIOThread(CRecordingSetWrite &rcRecordingSetWrite);
	~CRecordingSetWriteIOThread();

	virtual DWORD ThreadMain();
	virtual void OnThreadExit(DWORD &dwOutcome);

	void PrepareFileInRecording();
	void PrepareNextRecording(std::wstring &strRecordingName);

	CRecordingFile *GetNextFileInRecording();
	CRecordingFile *GetNextRecording(std::wstring &strRecordingName);

    void SignalWakeup() { SetEvent(m_hWakeupEvent); }

	void ClearPreparedFileInRecording();
	void ClearPreparedRecording();

protected:
	void OpenFileInRecording();
	void OpenFileInNewRecording();
	void DiscardFileInRecording();
	void DiscardNextRecording();

	CRecordingSetWrite &m_rcRecordingSetWrite;
	bool m_fNeedFileInRecording;
	bool m_fNeedNextRecording;
	bool m_fFileInRecordingNeedsPadding;
	bool m_fFileInNextRecordingNeedsPadding;
	std::wstring m_strRecordingName;
	CRecordingFile *m_pcRecordingFileInRecording;
	CRecordingFile *m_pcRecordingFileNextRecording;
	HANDLE m_hWakeupEvent;
	int m_iLastPrepCounter;
};

CRecordingSetWriteIOThread::CRecordingSetWriteIOThread(CRecordingSetWrite &rcRecordingSetWrite)
	: CComponentWorkerThread()
	, m_rcRecordingSetWrite(rcRecordingSetWrite)
	, m_pcRecordingFileInRecording(NULL)
	, m_pcRecordingFileNextRecording(NULL)
	, m_fNeedFileInRecording(false)
	, m_fNeedNextRecording(false)
	, m_hWakeupEvent(CreateEvent(NULL, FALSE, FALSE, NULL))
	, m_iLastPrepCounter(0)
	, m_strRecordingName()
	, m_fFileInRecordingNeedsPadding(false)
	, m_fFileInNextRecordingNeedsPadding(false)
{
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread:  constructor\n") ));
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::CRecordingSetWriteIOThread called but not pre-creating files!\n"));
        return;
    }
} // CRecordingSetWriteIOThread::CRecordingSetWriteIOThread

CRecordingSetWriteIOThread::~CRecordingSetWriteIOThread()
{
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread:  destructor entry\n") ));
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::~CRecordingSetWriteIOThread called but not pre-creating files!\n"));
        return;
    }

	CloseHandle(m_hWakeupEvent);

	// These are temporary recordings so it is okay to simply delete the info:

	DiscardFileInRecording();
	DiscardNextRecording();

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread:  destructor exit\n") ));
} // CRecordingSetWriteIOThread::~CRecordingSetWriteIOThread

void CRecordingSetWriteIOThread::DiscardFileInRecording()
{
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::DiscardFileInRecording called but not pre-creating files!\n"));
        return;
    }
	if (m_pcRecordingFileInRecording)
	{
		std::wstring strFileName = m_pcRecordingFileInRecording->GetFileName();
		m_pcRecordingFileInRecording->Close();
		CRecordingFile::DeleteFile(strFileName);

		delete m_pcRecordingFileInRecording;
		m_pcRecordingFileInRecording = NULL;
	}
	m_fFileInRecordingNeedsPadding = false;
} // CRecordingSetWriteIOThread::DiscardFileInRecording

void CRecordingSetWriteIOThread::DiscardNextRecording()
{
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::DiscardNextRecording called but not pre-creating files!\n"));
        return;
    }
	if (m_pcRecordingFileNextRecording)
	{
		std::wstring strFileName = m_pcRecordingFileNextRecording->GetFileName();
		m_pcRecordingFileNextRecording->Close();
		CRecordingFile::DeleteFile(strFileName);

		delete m_pcRecordingFileNextRecording;
		m_pcRecordingFileNextRecording = NULL;
	}
	m_strRecordingName = L"";
	m_fFileInNextRecordingNeedsPadding = false;
} // CRecordingSetWriteIOThread::DiscardNextRecording

DWORD CRecordingSetWriteIOThread::ThreadMain()
{
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::ThreadMain called but not pre-creating files!\n"));
        return ERROR_INVALID_STATE;
    }
	// Empirically, if this thread runs at normal priority, it can be starved
	// by startup and/or UI actions enough to hog some critical resource needed
	// for file opens and, for whatever reason, it does not get priority-inverted
	// [enough] to speed up if the writer should happen to need a new file asap.
	// Empirically, the following priority setting is good enough:

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::ThreadMain():  entry\n") ));

#ifdef _WIN32_WCE
	BOOL fResult = CeSetThreadPriority(GetCurrentThread(), g_dwWritePreCreatePriority );
	ASSERT(fResult);
#endif // _WIN32_WCE

	while (WAIT_OBJECT_0 == WaitForSingleObject(m_hWakeupEvent, INFINITE))
	{
		// The thread needs to exit if shutdown has been requested:

		if (m_fShutdownSignal)
		{
			break;
		}

		// At the moment the event is signalled, there is often a lot going on
		// -- for example, the playback graph may be processing a pause buffer
		// and opening this recording.  Or this might be during startup when
		// lots of stuff is going on. It is better to wait awhile.  Since
		// each file needs to cover several minutes of a/v in order to avoid the
		// performance hit of too many files, this bit of code assumes that we'll
		// be nicely mid-file after 1 minute:

		DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::ThreadMain():  wake-up for request %d, waiting for good time\n"), m_iLastPrepCounter ));

		int iLastPrep = m_iLastPrepCounter;
		while (true)
		{
			WaitForSingleObject(m_hWakeupEvent, 60000);		/* 1 minute = 60 seconds = 60,000 ms */
			if (m_fShutdownSignal || (iLastPrep == m_iLastPrepCounter))
			{
				break;
			}
			iLastPrep = m_iLastPrepCounter;
		};
		if (m_fShutdownSignal)
		{
			break;
		}

		DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::ThreadMain(): good time is here\n") ));
		{
			CAutoLock cAutoLock(&m_rcRecordingSetWrite.m_cCritSecDiskOpt);

			if (m_fNeedFileInRecording)
			{
				DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::ThreadMain(): about to prepare the next file:\n") ));

				OpenFileInRecording();
			}
    		m_fFileInRecordingNeedsPadding = (m_pcRecordingFileInRecording != NULL);
		}
		if (m_fShutdownSignal)
		{
			break;
		}

		{
			CAutoLock cAutoLock(&m_rcRecordingSetWrite.m_cCritSecDiskOpt);

			if (m_fNeedNextRecording)
			{
				DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::ThreadMain(): about to prepare the first file in a new recording:\n") ));

				OpenFileInNewRecording();
			}
			m_fFileInNextRecordingNeedsPadding = (m_pcRecordingFileNextRecording != NULL);
		}
		if (m_fShutdownSignal)
		{
			break;
		}

        if (m_fFileInRecordingNeedsPadding  ||  m_fFileInNextRecordingNeedsPadding)
        {
		    DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::ThreadMain():  about to pad out files to minimize later disk i/o glitches\n") ));

		    while (!m_fShutdownSignal)
		    {
			    CAutoLock cAutoLock(&m_rcRecordingSetWrite.m_cCritSecDiskOpt);

			    if (m_fNeedNextRecording || m_fNeedFileInRecording)
			    {
				    // Go work on the requested recording or new file:

				    break;
			    }
			    else if (m_fFileInRecordingNeedsPadding)
			    {
				    if (m_pcRecordingFileInRecording)
				    {
					    if (m_pcRecordingFileInRecording->PadMoreUpToSize(m_rcRecordingSetWrite.m_ullMaxFileSize + EXTRA_PADDING_FOR_LAST_SAMPLE))
					    {
						    // We've either achieve our goal or the file is as big as we can make it:

						    m_fFileInRecordingNeedsPadding = false;
					    }
				    }
				    else
				    {
					    m_fFileInRecordingNeedsPadding = false;
				    }
			    }
			    else if (m_fFileInNextRecordingNeedsPadding)
			    {
				    if (m_pcRecordingFileNextRecording)
				    {
					    // Pad out to the maximum of temporary and permanent recording size
					    // so that a scheduled recording (if started) won't glitch immediately:
					    
					    ULONGLONG uPaddingSize = m_rcRecordingSetWrite.m_ullMaxFileSize;
					    if (uPaddingSize < g_uhyMaxPermanentRecordingFileSize)
						    uPaddingSize = g_uhyMaxPermanentRecordingFileSize;
					    if (uPaddingSize < g_uhyMaxTemporaryRecordingFileSize)
						    uPaddingSize = g_uhyMaxTemporaryRecordingFileSize;
					    if (m_pcRecordingFileNextRecording->PadMoreUpToSize(uPaddingSize + EXTRA_PADDING_FOR_LAST_SAMPLE))
					    {
						    // We've either achieve our goal or the file is as big as we can make it:

						    m_fFileInNextRecordingNeedsPadding = false;
					    }
				    }
				    else
				    {
					    m_fFileInNextRecordingNeedsPadding = false;
				    }
			    }
			    else
			    {
				    // No longer anything to do on this pass:

				    break;
			    }
		    }
		    DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::ThreadMain():  done padding out files\n") ));
        }

		if (m_fShutdownSignal)
		{
			break;
		}
	};

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::ThreadMain():  exit\n") ));

	return 0;
} // CRecordingSetWriteIOThread::ThreadMain

void CRecordingSetWriteIOThread::OnThreadExit(DWORD &dwOutcome)
{
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::OnThreadExit called but not pre-creating files!\n"));
        return;
    }
	m_rcRecordingSetWrite.m_pcRecordingSetWriteIOThread = NULL;
} // CRecordingSetWriteIOThread::OnThreadExit

void CRecordingSetWriteIOThread::PrepareFileInRecording()
{
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::PrepareFileInRecording called but not pre-creating files!\n"));
        return;
    }
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::PrepareFileInRecording(): requested to prepare the next file in advance\n") ));

	CAutoLock cAutoLock(&m_rcRecordingSetWrite.m_cCritSecDiskOpt);

	try {
		DiscardFileInRecording();

		m_fNeedFileInRecording = true;
		++m_iLastPrepCounter;
		SignalWakeup();
	} catch (const std::exception &) {};

} // CRecordingSetWriteIOThread::PrepareFileInRecording

void CRecordingSetWriteIOThread::PrepareNextRecording(std::wstring &strRecordingName)
{
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::PrepareNextRecording called but not pre-creating files!\n"));
        return;
    }
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::PrepareNextRecording(): requested to prepare recording in advance\n") ));

	CAutoLock cAutoLock(&m_rcRecordingSetWrite.m_cCritSecDiskOpt);

	try {
		DiscardNextRecording();

		m_strRecordingName = strRecordingName;
		m_fNeedNextRecording = true;
		++m_iLastPrepCounter;
		SignalWakeup();
	} catch (const std::exception &) {};

} // CRecordingSetWriteIOThread::PrepareNextRecording

CRecordingFile *CRecordingSetWriteIOThread::GetNextFileInRecording()
{
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::GetNextFileInRecording called but not pre-creating files!\n"));
        return NULL;
    }
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::GetNextFileInRecording(): need the next file now\n") ));

	CAutoLock cAutoLock(&m_rcRecordingSetWrite.m_cCritSecDiskOpt);

	if (m_fNeedFileInRecording && !m_pcRecordingFileInRecording)
	{
		DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::GetNextFileInRecording(): must create it now\n") ));
		OpenFileInRecording();
	}
	CRecordingFile *pcRecordingFile = m_pcRecordingFileInRecording;
	m_pcRecordingFileInRecording = NULL;
	m_fNeedFileInRecording = false;
	m_fFileInRecordingNeedsPadding = false;

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::GetNextFileInRecording(): returning\n") ));

	return pcRecordingFile;
} // CRecordingSetWriteIOThread::GetNextFileInRecording

CRecordingFile *CRecordingSetWriteIOThread::GetNextRecording(std::wstring &strRecordingName)
{
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::GetNextRecording called but not pre-creating files!\n"));
        return NULL;
    }
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::GetNextRecording(): need the next file now\n") ));

	strRecordingName = L"";

	CAutoLock cAutoLock(&m_rcRecordingSetWrite.m_cCritSecDiskOpt);

	m_fFileInNextRecordingNeedsPadding = false;
	if (!m_pcRecordingFileNextRecording)
	{
		DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::GetNextRecording(): no prepped recording\n") ));
		return NULL;
	}
	CRecordingFile *pcRecordingFile = m_pcRecordingFileNextRecording;
	strRecordingName = m_strRecordingName;
	m_pcRecordingFileNextRecording = NULL;
	m_strRecordingName = L"";
	m_fNeedNextRecording = false;

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::GetNextRecording(): returning\n") ));

	return pcRecordingFile;
} // CRecordingSetWriteIOThread::GetNextRecording

void CRecordingSetWriteIOThread::OpenFileInRecording()
{
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::OpenFileInRecording called but not pre-creating files!\n"));
        return;
    }
	try
	{
		m_pcRecordingFileInRecording = new CRecordingFile();
		wstring strNextFile = m_rcRecordingSetWrite.GetNextFileName();
	
		DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::OpenFileInRecording() --about to prep file %s\n"), strNextFile.c_str() ));

		DWORD dwStatus = m_pcRecordingFileInRecording->CreateNew(strNextFile.c_str(),
									m_rcRecordingSetWrite.m_dwNegotiatedBufferSize,
									m_rcRecordingSetWrite.m_rgMediaTypes,
									m_rcRecordingSetWrite.m_dwNumStreams,
									m_rcRecordingSetWrite.m_pbCustomData,
									m_rcRecordingSetWrite.m_dwCustomDataLen,
									true,
									m_rcRecordingSetWrite.m_ullMaxFileSize);
		if (ERROR_SUCCESS != dwStatus)
		{
			DbgLog((LOG_ERROR, 3, TEXT("CRecordingSetWriteIOThread::OpenFileInRecording(): error %d\n"), dwStatus ));

			DiscardFileInRecording();
		}
	
	} catch (const std::exception &) {
		DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::OpenFileInRecording(): failed due to exception\n") ));
		delete m_pcRecordingFileInRecording;
		m_pcRecordingFileInRecording = NULL;
	};
	m_fNeedFileInRecording = false;
} // CRecordingSetWriteIOThread::OpenFileInRecording

void CRecordingSetWriteIOThread::OpenFileInNewRecording()
{
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::OpenFileInNewRecording called but not pre-creating files!\n"));
        return;
    }
	try
	{
		m_pcRecordingFileNextRecording = new CRecordingFile();
	
		DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::OpenFileInNewRecording() --about to create first file for recording %s\n"), m_strRecordingName.c_str() ));

		std::wstring strFileName = m_strRecordingName + L"_0";
		DWORD dwStatus = m_pcRecordingFileNextRecording->CreateNew(strFileName.c_str(),
									m_rcRecordingSetWrite.m_dwNegotiatedBufferSize,
									m_rcRecordingSetWrite.m_rgMediaTypes,
									m_rcRecordingSetWrite.m_dwNumStreams,
									m_rcRecordingSetWrite.m_pbCustomData,
									m_rcRecordingSetWrite.m_dwCustomDataLen,
									true,
									m_rcRecordingSetWrite.m_ullMaxFileSize);
		if (ERROR_SUCCESS != dwStatus)
		{
			DbgLog((LOG_ERROR, 3, TEXT("CRecordingSetWriteIOThread::OpenFileInNewRecording(): error %d\n"), dwStatus ));

			DiscardNextRecording();
		}
	
	} catch (const std::exception &) {
		DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::OpenFileInNewRecording(): failed due to exception\n") ));
		delete m_pcRecordingFileNextRecording;
		m_pcRecordingFileNextRecording = NULL;
	};


	m_fNeedNextRecording = false;
} // CRecordingSetWriteIOThread::OpenFileInNewRecording

void CRecordingSetWriteIOThread::ClearPreparedFileInRecording()
{
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::ClearPreparedFileInRecording called but not pre-creating files!\n"));
        return;
    }
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::ClearPreparedFileInRecording(): discarding prepared file\n") ));

	CAutoLock cAutoLock(&m_rcRecordingSetWrite.m_cCritSecDiskOpt);

	DiscardFileInRecording();
	m_fNeedFileInRecording = false;
} // CRecordingSetWriteIOThread::ClearPreparedFileInRecording

void CRecordingSetWriteIOThread::ClearPreparedRecording()
{
    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWriteIOThread::ClearPreparedRecording called but not pre-creating files!\n"));
        return;
    }
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWriteIOThread::ClearPreparedRecording(): discarding prepared file\n") ));

	CAutoLock cAutoLock(&m_rcRecordingSetWrite.m_cCritSecDiskOpt);

	DiscardNextRecording();
	m_fNeedNextRecording = false;
} // CRecordingSetWriteIOThread::ClearPreparedRecording

// ################################################
//      CBinFile
// ################################################

CBinFile::CBinFile() :
	m_hFile(INVALID_HANDLE_VALUE),
	m_ullActualFilePos(0),
	m_ullDesiredFilePos(0)
{
}

CBinFile::CBinFile(HANDLE hFile)
	: m_hFile(hFile)
{
}

CBinFile::~CBinFile()
{
	Close();
}

HANDLE CBinFile::ReleaseHandle()
{
	HANDLE h = m_hFile;
	m_hFile = INVALID_HANDLE_VALUE;
	return h;
}

void CBinFile::Close()
{
	// Close the file
	if (m_hFile != INVALID_HANDLE_VALUE)
	{
		FileIOTimeLogger ioLogger(TEXT("CBinFile::Close()"));

		::CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;
	}
}

DWORD CBinFile::Open(const WCHAR *wszFileName, bool fOpenForRead)
{
	// Make sure we're not already open
	if (IsOpen())
		_CDW(ERROR_INVALID_STATE);

	// Open the file
	FileIOTimeLogger ioLogger(TEXT("CBinFile::Open()"));

	m_hFile = CreateFileW(	wszFileName,
							GENERIC_READ | (fOpenForRead ? 0 : GENERIC_WRITE),
							FILE_SHARE_READ | FILE_SHARE_WRITE,
							NULL,
							fOpenForRead ? OPEN_EXISTING : CREATE_NEW,
							fOpenForRead ? 0 : FILE_FLAG_WRITE_THROUGH,
							NULL);

	if (m_hFile == INVALID_HANDLE_VALUE)
	{
		DWORD dw = GetLastError();
		Close();
		return dw;
	}

	m_fOpenedForRead = fOpenForRead;
	m_ullActualFilePos = 0;
	m_ullDesiredFilePos = 0;

	return ERROR_SUCCESS;
}

ULONGLONG CBinFile::GetFileSize()
{
	FileIOTimeLogger ioLogger(TEXT("CBinFile::GetFileSize()"));

	ASSERT(m_hFile != INVALID_HANDLE_VALUE);

	ULARGE_INTEGER li;
	li.QuadPart = 0;
	li.LowPart = ::GetFileSize(m_hFile, (DWORD *) &li.HighPart);
	return li.QuadPart;
}


DWORD CBinFile::Write(BYTE *pBuf, DWORD dwBytes)
{
	DFP((LOG_FILE_IO, 5, _T("CBinFile[%p]::Write():  writing %u bytes starting at %I64d\n"), this, dwBytes, GetFilePos() ));

	// Now perform the lazy update of the file pos
	if (m_ullActualFilePos != m_ullDesiredFilePos)
		_CDW(CommitLazyFilePos());

	DWORD dwWritten;
	FileIOTimeLogger ioLogger(TEXT("CBinFile::Write()"));
    if (WRITEFILE(m_hFile, pBuf, dwBytes, &dwWritten, NULL) == FALSE  ||
        dwWritten < dwBytes)
    {
		DFP((LOG_ERROR, 5, _T("CBinFile[%p]::Write():  write failed\n"), this ));
		ASSERT(0);
		_CDW(ERROR_DISK_FULL);
    }

	m_ullActualFilePos += dwBytes;
	m_ullDesiredFilePos += dwBytes;
	DFP((LOG_FILE_IO, 5, _T("CBinFile[%p]::Write():  now at position %I64d\n"), this, GetFilePos()));

	return ERROR_SUCCESS;
}

DWORD CBinFile::SetFilePos(ULONGLONG pos)
{
	// Do this lazily
	m_ullDesiredFilePos = pos;
	return ERROR_SUCCESS;
}

DWORD CBinFile::CommitLazyFilePos()
{
	LARGE_INTEGER li;
	li.QuadPart = m_ullDesiredFilePos;
	FileIOTimeLogger ioLogger(TEXT("CBinFile:CommitLazyFilePos()"));

	ASSERT(m_hFile != INVALID_HANDLE_VALUE);

	if (SetFilePointer(m_hFile, li.LowPart, &li.HighPart, FILE_BEGIN)
			== INVALID_SET_FILE_POINTER)
	{
		DFP((LOG_ERROR, 5, _T("CBinFile[%p]::CommitLazyFilePos():  failed\n"), this ));
		ASSERT(0);
		return GetLastError();
	}
	m_ullActualFilePos = m_ullDesiredFilePos;
	DFP((LOG_FILE_IO, 5, _T("CBinFile[%p]::CommitLazyFilePos(%I64d)\n"), this, m_ullDesiredFilePos));
	return ERROR_SUCCESS;
}

ULONGLONG CBinFile::GetFilePos()
{
	return m_ullDesiredFilePos;
}

DWORD CBinFile::TruncateFile(ULONGLONG uTruncationPosition)
{
	ASSERT(m_hFile != INVALID_HANDLE_VALUE);

	ULONGLONG uCurPos = GetFilePos();
	_CDW(SetFilePos(uTruncationPosition));
	_CDW(CommitLazyFilePos());
	DWORD dwRet = (SetEndOfFile(m_hFile) == 0) ? GetLastError() : ERROR_SUCCESS;
	_CDW(SetFilePos(uCurPos));
	return dwRet;
}

DWORD CBinFile::Read(BYTE *pBuf, DWORD dwBytes)
{
	DFP((LOG_FILE_IO, 5, _T("CBinFile[%p]::Read(%u) called at file position\n"), this, dwBytes, GetFilePos() ));

	ASSERT(m_hFile != INVALID_HANDLE_VALUE);

	// Now perform the lazy update of the file pos
	if (m_ullActualFilePos != m_ullDesiredFilePos)
		_CDW(CommitLazyFilePos());

	DWORD n = 0;
	FileIOTimeLogger ioLogger(TEXT("CBinFile::ReadFile()"));

	if (!ReadFile(m_hFile, pBuf, dwBytes, &n, NULL) || !n)
	{
		DWORD dw = GetLastError();
		DFP((LOG_FILE_IO, 5, _T("CBinFile[%p]::Read(%u) -- ReadFile() failed\n"), this ));
		return ERROR_HANDLE_EOF;
	}

	if (n < dwBytes)
		return ERROR_HANDLE_EOF;

	m_ullActualFilePos += dwBytes;
	m_ullDesiredFilePos += dwBytes;

	/*

	// First make sure there's enough data in the file to perform the read.
	ULONGLONG ullRemaining = GetFileSize() - GetFilePos();
	while (ullRemaining < dwBytes)
	{
		// Starved, sleeping a bit to see if the file is currently being
		// written to.
		Sleep(WAIT_FOR_WRITE_DURATION);
		ULONGLONG ullLeft = GetFileSize() - GetFilePos();
		if (ullLeft == ullRemaining)
		{
			DFP((LOG_TRACE, 5, _T("CBinFile[%p]::Read(%u) failed\n"), this ));
			return ERROR_HANDLE_EOF;
		}

		ullRemaining = ullLeft;
	}

	// The file is now large enough so read the buffer.
	DWORD n = 0;
	while (dwBytes > 0)
	{
		if (!ReadFile(m_hFile, pBuf, dwBytes, &n, NULL) || !n)
		{
			DWORD dw = GetLastError();
			DFP((LOG_TRACE, 5, _T("CBinFile[%p]::Read(%u) -- ReadFile() failed\n"), this ));
			return ERROR_HANDLE_EOF;
		}
		m_ullActualFilePos += dwBytes;
		m_ullDesiredFilePos += dwBytes;
		dwBytes -= n;
		pBuf += n;
	} */

	DFP((LOG_FILE_IO, 5, _T("CBinFile[%p]::Read() succeeded, new file position is %I64d\n"), this, GetFilePos() ));
	return ERROR_SUCCESS;
}

DWORD CBinFile::Flush()
{

    ASSERT(m_hFile != INVALID_HANDLE_VALUE);

    if (!FlushFileBuffers(m_hFile))
        {
        return GetLastError();
        }

    return ERROR_SUCCESS;
}

// ################################################
//      CRecordingFile
// ################################################

CRecordingFile::CRecordingFile() :	m_fWroteFirstSample(false),
									m_dwLastSampleDataLen(0),
									m_pIdxCache(0),
									m_ullIndexFileSize(0),
									m_ullDataFileSize(0),
									m_fOptimizedOpen(false),
									m_iFileCreationIndex(-1),
									m_ullMaxDataFileSize(0LL)
{
	memset(&m_header, 0, sizeof(m_header));
    memset (&m_sampleHeader, 0, sizeof(m_sampleHeader));
	m_header.tMaxSeekPos = -1;
}

CRecordingFile::~CRecordingFile()
{
	// We've seen 'leaked' files in the list of to-be-deleted files held by the
	// reader on error paths and codepaths that prune a recordingset by deleting
	// one of CRecordingFile's and then erasing the file from the vector. To
	// stem the leaks, implement a destructor that makes sure that the files are
	// closed.

	Close();
} // CRecordingFile::~CRecordingFile

DWORD CRecordingFile::CreateNew(const WCHAR *wszFileName,
								DWORD dwNegotiatedBufferSize,
								CMediaType rgMediaTypes[],
								DWORD dwNumStreams,
								BYTE rgbCustom[],
								DWORD dwNumCustomBytes,
								bool fTemporary,
								ULONGLONG uPaddedFileSize)
{
	_CP(rgMediaTypes);

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingFile::CreateNew(%s)\n"), wszFileName));

	ASSERT(dwNegotiatedBufferSize);
	Close();

	// Fill in the header
	m_header.dwNumStreams = dwNumStreams;
	m_header.dwVersion = FILE_VER;
	m_header.dwMaxSampleSize = 0;
	m_header.dwCustomDataSize = dwNumCustomBytes;
	m_header.tVirtualStartTime = -1;
	m_header.tTrueStartTime = -1;
	m_header.dwNegotiatedBufferSize = dwNegotiatedBufferSize;
	m_header.fTemporary = fTemporary;
	m_header.tMaxSeekPos = -1;
	m_header.fInProgress = true;
	m_header.fFileInProgress = true;
	m_header.fPaddedFile = (uPaddedFileSize != 0);

#ifdef DEBUG
	memset(m_header.szReserved, '.', sizeof(m_header.szReserved));
	sprintf(m_header.szReserved,	"\r\nVERSION=%d"
									"\r\n# PINS=%d"
									"\r\nMAX SAMPLE SIZE = %d"
									"\r\nVIRTUAL START =%I64d"
									"\r\nMAX SEEK POS = %I64d"
									"\r\nCUSTOM size=%d",
									m_header.dwVersion,
									m_header.dwNumStreams,
									m_header.dwMaxSampleSize,
									m_header.tVirtualStartTime,
									m_header.tMaxSeekPos,
									m_header.dwCustomDataSize);
#endif

	// Build our file names
	m_strFilename = wszFileName;
	wstring strDataFileName = m_strFilename + DATA_FILE_SUFFIX;
	wstring strIndexFileName = m_strFilename + INDEX_FILE_SUFFIX;

	const WCHAR *pswzUnderscore = wcsrchr(wszFileName, '_');
	if (!pswzUnderscore)
	{
		return ERROR_INVALID_DATA;
	}
	m_iFileCreationIndex = _wtoi(pswzUnderscore + 1);

	// Create new data and index files
	_CDW(m_dataFile.Open(strDataFileName.c_str(), false));
	_CDW(m_indexFile.Open(strIndexFileName.c_str(), false));

#ifndef SHIP_BUILD
	DbgLog((LOG_ERROR, 4, _T("CRecordingFile::CreateNew\n")));
#endif

#ifdef UNICODE
	DFP((LOG_FILE_OTHER, 5, _T("CRecordingFile[%p]::CreateNew(%s) ... about to write header etc\n"), this, wszFileName ));
#else
	DFP((LOG_FILE_OTHER, 5, _T("CRecordingFile[%p]::CreateNew(%S) ... about to write header etc\n"), this, wszFileName ));
#endif
	// Write the header to the file
	_CDW(m_dataFile.Write((BYTE *) &m_header, sizeof(m_header)));

	// Write the custom data to the file
	if (rgbCustom)
		_CDW(WriteCustom(rgbCustom, dwNumCustomBytes));

	// Write the media format of each connected pin
	for (DWORD i = 0; i < dwNumStreams; i++)
	{
		_CDW(WriteMediaType(i, &rgMediaTypes[i]));
	}

	m_ullDataFileSize = m_dataFile.GetFileSize();

#ifdef UNICODE
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::CreateNew(%s) ... wrote header\n"), this, wszFileName ));
#else
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::CreateNew(%S) ... wrote header\n"), this, wszFileName ));
#endif
	return ERROR_SUCCESS;
}

BOOL CRecordingFile::PadMoreUpToSize(ULONGLONG uPaddedFileSize)
{
	BOOL fFutileToPadMore = TRUE;

	if (!m_dataFile.IsOpen() || m_dataFile.IsOpenedForRead() || !m_header.fPaddedFile)
	{
		return fFutileToPadMore;		// cannot add more padding
	}

	ULONGLONG uCurrentSize = m_dataFile.GetFileSize();
	if (uCurrentSize >= uPaddedFileSize)
	{
		// The file is already as big as requested.
		return fFutileToPadMore;
	}
	if (SetFilePos(uCurrentSize) == ERROR_SUCCESS)
	{
		// Pad the file with content containing zeros:

		static BYTE bData[32 * 1024];  // 32 KB matches cluster size
			// static moves this off the stack. Because this buffer is always zero,
			// it doesn't matter if there are multiple concurrent uses.
		memset(bData, 0, 32 * 1024);
		size_t uWriteSize = uPaddedFileSize - uCurrentSize;
		if (uWriteSize > 32 * 1024)
		{
			uWriteSize = 32 * 1024;
		}
		if (ERROR_SUCCESS == m_dataFile.Write(bData, uWriteSize))
		{
			if (uCurrentSize + uWriteSize < uPaddedFileSize)
			{
				fFutileToPadMore = FALSE;
			}
		}
	}
	SetFilePos(m_ullDataFileSize);
	return fFutileToPadMore;
}

DWORD CRecordingFile::Open(	const WCHAR *wszFileName,
							DWORD *pdwNumStreams,
							DWORD *pdwMaxSampleSize,
							DWORD *pdwCustomDataSize)
{
	_CP(wszFileName);

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingFile::Open(%s)\n"), wszFileName));

	// Accept null pointers as parameters
	DWORD dwNumStreams, dwMaxSampleSize, dwCustomDataSize;
	if (!pdwNumStreams)
		pdwNumStreams = &dwNumStreams;
	if (!pdwMaxSampleSize)
		pdwMaxSampleSize = &dwMaxSampleSize;
	if (!pdwCustomDataSize)
		pdwCustomDataSize = &dwCustomDataSize;

	Close();

	// Build our file names
	m_strFilename = wszFileName;
	wstring strDataFileName = m_strFilename + DATA_FILE_SUFFIX;
	wstring strIndexFileName = m_strFilename + INDEX_FILE_SUFFIX;

	const WCHAR *pswzUnderscore = wcsrchr(wszFileName, '_');
	if (!pswzUnderscore)
	{
		return ERROR_INVALID_DATA;
	}
	m_iFileCreationIndex = _wtoi(pswzUnderscore + 1);

	// Open the actual files
	_CDW(m_dataFile.Open(strDataFileName.c_str(), true));
	_CDW(m_indexFile.Open(strIndexFileName.c_str(), true));

#ifdef UNICODE
	DFP((LOG_FILE_OTHER, 5, _T("CRecordingFile[%p]::Open(%s) ... readering header\n"), this, wszFileName ));
#else
	DFP((LOG_FILE_OTHER, 5, _T("CRecordingFile[%p]::Open(%S) ... readering header\n"), this, wszFileName ));
#endif

	// Read in the file header
	_CDW(m_dataFile.Read((BYTE *) &m_header, sizeof(m_header)));

	// Verify the version
	if ((m_header.dwVersion != FILE_VER) && (m_header.dwVersion != FILE_VER_101)  &&  (m_header.dwVersion != FILE_VER_102))
	{
		// Wrong file version or unrecognized format!
		_CDW(ERROR_INVALID_DATA);
	}

	*pdwNumStreams = m_header.dwNumStreams;
	*pdwMaxSampleSize = max(m_header.dwMaxSampleSize, m_header.dwNegotiatedBufferSize);
	*pdwCustomDataSize = m_header.dwCustomDataSize;
	ASSERT(*pdwMaxSampleSize || m_header.fTemporary);

	return ERROR_SUCCESS;
}

DWORD CRecordingFile::OptimizedOpen( const WCHAR *wszFileName)
{
	DFP((LOG_FILE_OTHER, 3, _T("CRecordingFile[%p]::OptimizedOpen(%s) ... entry\n"), this, wszFileName ));

	_CP(wszFileName);

	Close();

	// Build our file names
	m_strFilename = wszFileName;

	const WCHAR *pswzUnderscore = wcsrchr(wszFileName, '_');
	if (!pswzUnderscore)
	{
		return ERROR_INVALID_DATA;
	}
	m_iFileCreationIndex = _wtoi(pswzUnderscore + 1);
	m_fOptimizedOpen = true;

	return ERROR_SUCCESS;
} // CRecordingFile::OptimizedOpen

DWORD CRecordingFile::FullyOpen(DWORD *pdwNumStreams,
								DWORD *pdwMaxSampleSize,
								DWORD *pdwCustomDataSize)
{
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingFile::FullyOpen()  %s\n"), m_strFilename.c_str() ));

	// Accept null pointers as parameters
	DWORD dwNumStreams, dwMaxSampleSize, dwCustomDataSize;
	if (!pdwNumStreams)
		pdwNumStreams = &dwNumStreams;
	if (!pdwMaxSampleSize)
		pdwMaxSampleSize = &dwMaxSampleSize;
	if (!pdwCustomDataSize)
		pdwCustomDataSize = &dwCustomDataSize;

	if (!m_fOptimizedOpen)
	{
		*pdwNumStreams = m_header.dwNumStreams;
		*pdwMaxSampleSize = max(m_header.dwMaxSampleSize, m_header.dwNegotiatedBufferSize);
		*pdwCustomDataSize = m_header.dwCustomDataSize;
		return ERROR_SUCCESS;
	}

	// Build our file names
	wstring strDataFileName = m_strFilename + DATA_FILE_SUFFIX;
	wstring strIndexFileName = m_strFilename + INDEX_FILE_SUFFIX;

	// Open the actual files
	_CDW(m_dataFile.Open(strDataFileName.c_str(), true));
	_CDW(m_indexFile.Open(strIndexFileName.c_str(), true));

#ifdef UNICODE
	DFP((LOG_FILE_OTHER, 5, _T("CRecordingFile[%p]::FullyOpen(%s) ... reading header\n"), this, m_strFilename.c_str() ));
#else
	DFP((LOG_FILE_OTHER, 5, _T("CRecordingFile[%p]::FullyOpen(%S) ... reading header\n"), this, m_strFilename.c_str() ));
#endif

	// Read in the file header
	_CDW(m_dataFile.Read((BYTE *) &m_header, sizeof(m_header)));

	// Verify the version
	if ((m_header.dwVersion != FILE_VER) && (m_header.dwVersion != FILE_VER_101)  &&  (m_header.dwVersion != FILE_VER_102))
	{
		// Wrong file version or unrecognized format!
		_CDW(ERROR_INVALID_DATA);
	}

	if (!m_header.dwNumStreams)
	{
		_CDW(ERROR_INVALID_DATA);
	}

	m_fOptimizedOpen = false;
	*pdwNumStreams = m_header.dwNumStreams;
	*pdwMaxSampleSize = max(m_header.dwMaxSampleSize, m_header.dwNegotiatedBufferSize);
	*pdwCustomDataSize = m_header.dwCustomDataSize;

	ASSERT(*pdwMaxSampleSize || m_header.fTemporary);

	// We need to set up our file position as if we had done a read-custom:

	ULONGLONG ullFilePos = GetFilePos();

	// Take our current position and factor in the meat of the custom data,
	// then do the round-to-next-sector padding:

	ULONGLONG ullCustomDataSize = sizeof(CUSTOM_DATA_HEADER) + m_header.dwCustomDataSize;
	ullFilePos += ullCustomDataSize;
	if (0 != (ullFilePos % SECTOR_SIZE))
	{
		ullFilePos = ROUND_UP(ullFilePos, SECTOR_SIZE);
	}
	_CDW(m_dataFile.SetFilePos(ullFilePos));

	// Now factor in the amount of disk dedicated to the media types:

	// Read in/verify the media types in the data file. This is complicated
	// enough that we really do need to call the routine. The arguments will
	// optimize the i/o, though:

	_CDW(ReadMediaTypes(NULL, m_header.dwNumStreams, TRUE));

	return ERROR_SUCCESS;
} // CRecordingFile::FullyOpen

DWORD CRecordingFile::WriteHeader()
{
#ifdef DEBUG
	// todo: pull out the szReserved member of FILE_HEADER altogether
	memset(m_header.szReserved, '.', sizeof(m_header.szReserved));
	sprintf(m_header.szReserved,	"\r\nVERSION=%d"
									"\r\n# PINS=%d"
									"\r\nMAX SAMPLE SIZE = %d"
									"\r\nVIRTUAL START = %I64d"
									"\r\nMAX SEEK POS = %I64d"
									"\r\nCUSTOM size=%d",
									m_header.dwVersion,
									m_header.dwNumStreams,
									m_header.dwMaxSampleSize,
									m_header.tVirtualStartTime,
									m_header.tMaxSeekPos,
									m_header.dwCustomDataSize);
#endif

	ASSERT(m_header.fTemporary ||
		   max(m_header.dwMaxSampleSize, m_header.dwNegotiatedBufferSize));

	ULONGLONG ullOldPos = m_dataFile.GetFilePos();
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::WriteHeader() ... updating header, will return to %I64d\n"), &m_dataFile, ullOldPos ));
	m_dataFile.SetFilePos(0);
	_CDW(m_dataFile.Write((BYTE *) &m_header, sizeof(m_header) - FILE_HEADER_RESERVED_SIZE_UNUSED_SECTORS));
	_CDW(m_dataFile.SetFilePos(ullOldPos));

	return ERROR_SUCCESS;
}

DWORD CRecordingFile::ReadHeader()
{
	if (m_fOptimizedOpen)
	{
		_CDW(FullyOpen());
	}
	ULONGLONG ullOldPos = m_dataFile.GetFilePos();
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile::ReaderHeader(), from file %p at %I64d\n"), &m_dataFile, ullOldPos ));
	m_dataFile.SetFilePos(0);
	_CDW(m_dataFile.Read((BYTE *) &m_header, sizeof(m_header) - FILE_HEADER_RESERVED_SIZE_UNUSED_SECTORS));
	_CDW(m_dataFile.SetFilePos(ullOldPos));
	if (m_header.dwVersion == FILE_VER_101)
	{
		m_header.fPaddedFile = false;
		m_header.fFileInProgress = m_header.fInProgress;
	}

	ASSERT(m_header.fTemporary ||
		   max(m_header.dwMaxSampleSize, m_header.dwNegotiatedBufferSize));

	return ERROR_SUCCESS;
}

DWORD CRecordingFile::SetTempPermStatus(LONGLONG tVirtualStart,
										bool fTemporary)
{
	DFP((LOG_FILE_OTHER, 5, _T("CRecordingFile[%p]::SetTempPermStatus() ... entry\n"), &m_dataFile ));

	if (fTemporary)
		tVirtualStart = -1;
	GetHeader().tVirtualStartTime = tVirtualStart;
	GetHeader().fTemporary = fTemporary;
	return WriteHeader();
}

DWORD CRecordingFile::WriteMediaType(DWORD dwStreamNum, AM_MEDIA_TYPE *pmt)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::WriteMediaType() ... entry\n"), &m_dataFile ));

	// Write out the media type header
	MEDIA_TYPE_HEADER header;
	header.dwVersion = MEDIA_TYPE_VER;
	header.dwSize = sizeof(AM_MEDIA_TYPE) + pmt->cbFormat;
	ZeroMemory(&(header.reserved), sizeof(header.reserved));
	_CDW(m_dataFile.Write((BYTE*)&header, sizeof(header)));
	m_ullDataFileSize += sizeof(header);

	// Write out the media type
	_CDW(m_dataFile.Write((BYTE *) pmt, sizeof(AM_MEDIA_TYPE)));
	if (pmt->cbFormat)
	{
		ASSERT(pmt->pbFormat);
		_CDW(m_dataFile.Write(pmt->pbFormat, pmt->cbFormat));
		m_ullDataFileSize += pmt->cbFormat;
	}

	// Pad out the media type to a sector
	ULONG cbPadding = SECTOR_SIZE - ((sizeof(header) + sizeof(AM_MEDIA_TYPE) + pmt->cbFormat) % SECTOR_SIZE);
	if (cbPadding > 0)
	{
		ASSERT(cbPadding < SECTOR_SIZE);
		BYTE rgbDummy[SECTOR_SIZE];
		ZeroMemory(rgbDummy, cbPadding);
		_CDW(m_dataFile.Write(rgbDummy, cbPadding));
		m_ullDataFileSize += cbPadding;
	}

	return S_OK;
}

DWORD CRecordingFile::WriteCustom(BYTE *pbCustom, DWORD dwCustomLen)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::WriteCustomData() ... entry\n"), &m_dataFile ));

	// Write out the custom data header
	CUSTOM_DATA_HEADER header;
	header.dwVersion = CUSTOM_DATA_VER;
	header.dwSize = dwCustomLen;
	ZeroMemory(&(header.reserved), sizeof(header.reserved));
	_CDW(m_dataFile.Write((BYTE*)&header, sizeof(header)));
	m_ullDataFileSize += sizeof(header);

	// Write out the custom data
	_CDW(m_dataFile.Write((BYTE *) pbCustom, dwCustomLen));
	m_ullDataFileSize += dwCustomLen;

	// Pad out the custom data to a sector boundary
	ULONG cbPadding = SECTOR_SIZE - ((sizeof(header) + dwCustomLen) % SECTOR_SIZE);
	if (cbPadding > 0)
	{
		ASSERT(cbPadding < SECTOR_SIZE);
		BYTE rgbDummy[SECTOR_SIZE];
		ZeroMemory(rgbDummy, cbPadding);
		_CDW(m_dataFile.Write(rgbDummy, cbPadding));
 		m_ullDataFileSize += cbPadding;
}

	return S_OK;
}

DWORD CRecordingFile::WriteSample(	IMediaSample &rSample,
									DWORD dwStreamNum,
									LONGLONG llDataOffset,
									bool fCreateIndex)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::WriteSample() ... entry\n"), &m_dataFile ));
	FileIOTimeLogger ioLogger(TEXT("CRecordingFile::WriteSample()"));

	// Make sure that we're not getting a sample larger than expected.
	if (rSample.GetActualDataLength() > (long) m_header.dwNegotiatedBufferSize)
	{
		// It's no ok to continue.  We might be reading back this file right now.
		// The reader won't be able to handle a sample bigger than what was
		// initially expected.
		ASSERT(FALSE);
		_CDW(ERROR_INVALID_DATA);
	}

	// Update the max sample size in the header if necessary
	if (rSample.GetActualDataLength() > (long) m_header.dwMaxSampleSize)
	{
		m_header.dwMaxSampleSize = rSample.GetActualDataLength();
		DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::WriteSample() ... updating header\n"), &m_dataFile ));
		_CDW(WriteHeader());
	}

	// Get the sample buffer
	BYTE *pBuf;
	if (FAILED(rSample.GetPointer(&pBuf)))
	{
		ASSERT(FALSE);
		_CDW(ERROR_INVALID_PARAMETER);
	}

	// Fill in the sample header info
	m_sampleHeader.dwDataLen = rSample.GetActualDataLength();
	m_sampleHeader.dwStreamNum = dwStreamNum;
	m_sampleHeader.fSyncPoint = rSample.IsSyncPoint() == S_OK;
	m_sampleHeader.fDiscontinuity = rSample.IsDiscontinuity() == S_OK;
	m_sampleHeader.dwLastSampleDataLen = m_dwLastSampleDataLen;
	m_sampleHeader.llStreamOffset = llDataOffset;

	if (FAILED(rSample.GetTime(&m_sampleHeader.tPTStart, &m_sampleHeader.tPTEnd)))
	{
		// Not there, just use -1's.
		m_sampleHeader.tPTStart = -1;
		m_sampleHeader.tPTEnd = -1;
	}

	if (FAILED(rSample.GetMediaTime(&m_sampleHeader.tStart, &m_sampleHeader.tEnd)))
	{
		// Just toss this sample - it's probably the ASF header.
		return ERROR_SUCCESS;
	}

	// Build the index if necessary.  This MUST be done before
	// writing the sample to disk or else GetFilePos will return
	// the position of the next sample, not this one.
	INDEX idx;
	if (fCreateIndex || !m_fWroteFirstSample)
	{
		idx.tStart = m_sampleHeader.tStart;
		idx.offset = m_dataFile.GetFilePos();
		idx.llStreamOffset = llDataOffset;
	}

	// If this is the first sample then store this first media time in the header.
	if (!m_fWroteFirstSample)
	{
		ASSERT(m_sampleHeader.tStart != -1);
		m_header.tTrueStartTime = m_sampleHeader.tStart;
		_CDW(WriteHeader());
	}

	// Write the header and actual sample data
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::WriteSample() ... writing sample header\n"), &m_dataFile ));
	_CDW(m_dataFile.Write((BYTE *) &m_sampleHeader, sizeof(m_sampleHeader)));
	m_ullDataFileSize += sizeof(m_sampleHeader);
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::WriteSample() ... writing %u bytes of data\n"), &m_dataFile, rSample.GetActualDataLength() ));
	_CDW(m_dataFile.Write(pBuf, rSample.GetActualDataLength()));
	m_ullDataFileSize += rSample.GetActualDataLength();

	// Write the index if necessary
	if (fCreateIndex || !m_fWroteFirstSample)
	{
		DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::WriteSample() ... writing index entry (media pos %I64d, file pos %I64d, stream offset %I64d\n"),
			&m_dataFile, idx.tStart, idx.offset, idx.llStreamOffset ));
		_CDW(m_indexFile.Write((BYTE *) &idx, sizeof(idx)));
	}

	DFP((LOG_FILE_IO, 5, _T("CRecordingFile::WriteSample():  wrote sample [%I64d,%I64d] maybe offset %I64d\n"), m_sampleHeader.tStart, m_sampleHeader.tEnd, idx.offset));

	// Update our state variables
	m_fWroteFirstSample = true;
	m_dwLastSampleDataLen = m_sampleHeader.dwDataLen;
	m_header.tMaxSeekPos = m_sampleHeader.tStart;

	return ERROR_SUCCESS;
}

DWORD CRecordingFile::CommitFinalFileState(bool fRecordingInProgress)
{
	DFP((LOG_FILE_IO, 3, _T("CRecordingFile::CommitFinalFileState():  file %s done, recording %s\n"),
		m_strFilename.c_str(), fRecordingInProgress ? TEXT("in-progress") : TEXT("complete") ));

	DWORD dwStatus = ERROR_SUCCESS;

	// Truncate the file:
	if (m_header.fPaddedFile)
	{
		if (ERROR_SUCCESS == (dwStatus = m_dataFile.TruncateFile(m_ullDataFileSize)))
		{
			m_header.fPaddedFile = false;
		}
	}

	// Now update the file:
	ASSERT(m_header.dwMaxSampleSize <= m_header.dwNegotiatedBufferSize);
	if (!fRecordingInProgress)
	{
		m_header.dwNegotiatedBufferSize = 0;
		m_header.fInProgress = false;
	}
	m_header.fFileInProgress = false;
	DWORD dwHeader = WriteHeader();
	if (ERROR_SUCCESS == dwStatus)
		dwStatus = dwHeader;

    if (fRecordingInProgress)
        {
        // Flush any unwritten data (and directory status) to file
        m_dataFile.Flush();
        m_indexFile.Flush();
        }

	return dwStatus;
}

void CRecordingFile::Close()
{
	// Now that we're closing the file, the theoretical max sample size is
	// no longer important.  We're really only concerned with the max sample
	// size encountered.  This also makes sure our max seek pos gets
	// propogated to disk.
	if (m_dataFile.IsOpen())
	{
		DFP((LOG_FILE_OTHER, 5, _T("CRecordingFile[%p]::Close() ... entry [opened for %s]\n"), &m_dataFile,
			m_dataFile.IsOpenedForRead() ? TEXT("read") : TEXT("write") ));
	}

	if (m_dataFile.IsOpen() && !m_dataFile.IsOpenedForRead())
	{
		CommitFinalFileState(false);
	}

	m_dataFile.Close();
	m_indexFile.Close();
	m_strFilename = L"";
	m_fWroteFirstSample = false;
	m_dwLastSampleDataLen = 0;
	m_pIdxCache = NULL;
	m_ullDataFileSize = 0;
	m_ullIndexFileSize = 0;
	m_fOptimizedOpen = false;

}

DWORD CRecordingFile::ReadCustom(BYTE *pbCustom, DWORD dwCustomLen)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::ReadCustom() ... entry\n"), &m_dataFile ));

	if (m_fOptimizedOpen)
	{
		_CDW(ERROR_INVALID_DATA);
	}

	// Handle the trivial case
	if (dwCustomLen == 0)
		return ERROR_SUCCESS;

	// Verify the custom data header
	CUSTOM_DATA_HEADER headerCustom;
	_CDW(m_dataFile.Read((BYTE *)&headerCustom, sizeof(CUSTOM_DATA_HEADER)));

	// Verify the header of the custom data
	if ((headerCustom.dwVersion != CUSTOM_DATA_VER) || (headerCustom.dwSize != dwCustomLen))
	{
		_CDW(ERROR_INVALID_DATA);
	}

	if (!pbCustom)
	{
		// Skip over the custom data
		_CDW(m_dataFile.SetFilePos(m_dataFile.GetFilePos() + dwCustomLen));
	}
	else
	{
		// Read in the custom data
		_CDW(m_dataFile.Read(pbCustom, dwCustomLen));
	}

	// Round up to the next sector
	ULONGLONG llNewOffset = m_dataFile.GetFilePos();
	if (0 != (llNewOffset % SECTOR_SIZE))
	{
		llNewOffset = ROUND_UP(llNewOffset, SECTOR_SIZE);
		_CDW(m_dataFile.SetFilePos(llNewOffset));
	}

	return ERROR_SUCCESS;
}

DWORD CRecordingFile::ReadNextMediaType(CMediaType *pMediaType)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::ReadNextMediaType() ... entry\n"), &m_dataFile ));

	if (m_fOptimizedOpen)
	{
		_CDW(ERROR_INVALID_DATA);
	}
	// Get the file position before doing anything
	ULONGLONG llStartOffset = m_dataFile.GetFilePos();
	ASSERT(0 == (llStartOffset % SECTOR_SIZE));

	// If the caller didn't supply a media type, use a local one
	CMediaType tempMT;
	if (!pMediaType)
		pMediaType = &tempMT;

	// Read in media type header data
	MEDIA_TYPE_HEADER header;
	_CDW(m_dataFile.Read((BYTE*) &header, sizeof(header)));

	// Validate the header
	if ((header.dwVersion != MEDIA_TYPE_VER) || (header.dwSize < sizeof(AM_MEDIA_TYPE)))
		_CDW(ERROR_INVALID_DATA);

	// Read the data
	_CDW(m_dataFile.Read((BYTE *) (AM_MEDIA_TYPE *) pMediaType, sizeof(AM_MEDIA_TYPE)));

	// Retrieve the format length and then clear the one in the struct.
	// That way it isn't confused internally about whether it's already
	// allocated a buffer or not.
	DWORD dwFormatLength = pMediaType->FormatLength();
	pMediaType->cbFormat = 0;
	pMediaType->pbFormat = 0;

	// Make sure our lengths check out
	if (header.dwSize != (sizeof(AM_MEDIA_TYPE) + dwFormatLength))
		_CDW(ERROR_INVALID_DATA);

	// Read the format block if it exists
	if (dwFormatLength)
	{
		// Allocate a new format buffer
		BYTE *pBuf = pMediaType->AllocFormatBuffer(dwFormatLength);
		_CMEM(pBuf);

		// Read the format data
		_CDW(m_dataFile.Read(pBuf, dwFormatLength));
	}

	// Round up to the next sector
	ULONGLONG llNewOffset = llStartOffset + sizeof(MEDIA_TYPE_HEADER) + header.dwSize;
	if (0 != (llNewOffset % SECTOR_SIZE))
	{
		llNewOffset = ROUND_UP(llNewOffset, SECTOR_SIZE);
		_CDW(m_dataFile.SetFilePos(llNewOffset));
	}

	return ERROR_SUCCESS;
}

DWORD CRecordingFile::ReadMediaTypes(	CMediaType *rgTypes,
										DWORD dwNumTypes,
										bool fVerifyOnly)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::ReadMediaTypes() ... entry\n"), &m_dataFile ));

	if (m_fOptimizedOpen)
	{
		_CDW(ERROR_INVALID_DATA);
	}
	if (!rgTypes && !fVerifyOnly)
	{
		_CDW(ERROR_INVALID_DATA);
	}

	for (DWORD dw = 0; dw < dwNumTypes; dw++)
	{
		if (fVerifyOnly)
		{
			// Read the next type from the file
			CMediaType tempMT;
			_CDW(ReadNextMediaType(&tempMT));

			// Make sure it matches what we already have
			if (rgTypes && (rgTypes[dw] != tempMT))
			{
				_CDW(ERROR_INVALID_DATA);
			}
		}
		else
		{
			_CDW(ReadNextMediaType(&rgTypes[dw]));
		}
	}

	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::ReadCustom() ... exit\n"), &m_dataFile ));

	return ERROR_SUCCESS;
}

DWORD CRecordingFile::ReadSampleHeader(	SAMPLE_HEADER &rHeader,
										bool fResetFilePos)
{
	if (m_fOptimizedOpen)
	{
		_CDW(FullyOpen());
	}

	ULONGLONG uiCurPos = m_dataFile.GetFilePos();

	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::ReadSampleHeader() ... initially at %I64d\n"), &m_dataFile, uiCurPos ));

	// Read the sample header data
	_CDW(m_dataFile.Read((BYTE *) &rHeader, sizeof(SAMPLE_HEADER)));

	// Restore the previous file position
	if (fResetFilePos)
	{
		DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::ReadSampleHeader() ... restoring file position %I64d\n"), &m_dataFile, uiCurPos ));
		_CDW(m_dataFile.SetFilePos(uiCurPos));
	}

	// We need to distinguish real samples from empty garbage in the file:
	if ((m_header.dwVersion == FILE_VER) && m_header.fPaddedFile)
	{
		if (0 == rHeader.dwDataLen)
		{
			// This is really a read past EOF:

			return ERROR_HANDLE_EOF;
		}
	}
	return ERROR_SUCCESS;
}

DWORD CRecordingFile::ReadSample(SAMPLE_HEADER &rHeader, IMediaSample &rSample)
{
	if (m_fOptimizedOpen)
	{
		_CDW(ERROR_INVALID_DATA);
	}

	long lMaxSize = rSample.GetSize();

	// Read the sample data
	BYTE *pBuf;
	if (FAILED(rSample.GetPointer(&pBuf)))
	{
		_CDW(ERROR_INVALID_PARAMETER);
	}

	DWORD cbRead;
	_CDW(ReadSampleData(rHeader, pBuf, lMaxSize, &cbRead));
	ASSERT(rHeader.dwDataLen == cbRead);

	// Set the various sample fields
	rSample.SetSyncPoint(rHeader.fSyncPoint);
	rSample.SetPreroll(FALSE);
    if (m_header.dwVersion <= FILE_VER_102)
    {
        rSample.SetDiscontinuity(FALSE);
    }
    else
    {
    	rSample.SetDiscontinuity(rHeader.fDiscontinuity);
    }
	rSample.SetMediaTime(&rHeader.tStart, &rHeader.tEnd);
	rSample.SetTime(&rHeader.tPTStart, &rHeader.tPTEnd);
	rSample.SetActualDataLength(rHeader.dwDataLen);

	return ERROR_SUCCESS;
}

DWORD CRecordingFile::ReadSampleData(SAMPLE_HEADER &rHeader, BYTE *pBuf, ULONG cbBuf, ULONG* pcbRead)
{
	if (m_fOptimizedOpen)
	{
		_CDW(ERROR_INVALID_DATA);
	}
	if (cbBuf < rHeader.dwDataLen)
	{
		DFP((LOG_FILE_IO, 4, _T("CRecordingFile::ReadSample():	returning error -- sample buffer size %ld < header-specified sample size %u\n"),
			cbBuf, rHeader.dwDataLen));
		ASSERT(0);
		_CDW(ERROR_INVALID_PARAMETER);
	}

	// Read the sample data
#ifdef DEBUG_FILE_POSITION
	ULONGLONG uiCurPos = m_dataFile.GetFilePos();
#endif
	_CDW(m_dataFile.Read(pBuf, rHeader.dwDataLen));

#ifdef UNICODE
	DFP((LOG_SOURCE_DISPATCH, 4, _T("CRecordingFile::ReadSample():	file %s, pos %I64d, sample [%I64d, %I64d] has %u bytes\n"),
		GetFileName().c_str(), uiCurPos, rHeader.tStart, rHeader.tEnd, rHeader.dwDataLen));
#else
	DFP((LOG_SOURCE_DISPATCH, 4, _T("CRecordingFile::ReadSample():	file %S, pos %I64d, sample [%I64d, %I64d] has %u bytes\n"),
		GetFileName().c_str(), uiCurPos, rHeader.tStart, rHeader.tEnd, rHeader.dwDataLen));
#endif

	// Sanity check to make sure data isn't totally hosed
	// todo: Update these checks in the future if our product changes.
	ASSERT(rHeader.dwStreamNum == 0);
	ASSERT(rHeader.dwDataLen <= (512 * 1024));

	*pcbRead = rHeader.dwDataLen;
	
	return ERROR_SUCCESS;
}

BOOL CRecordingFile::DeleteFile(std::wstring &strFileName)
{
	BOOL fDeleteIsDone = TRUE;

	RETAILMSG(LOG_LOCKING, (L"## RecFile deleting %s.\n", strFileName.c_str()));
	if (!::DeleteFileW((strFileName + INDEX_FILE_SUFFIX).c_str()))
	{
		DWORD dwError = GetLastError();
		if (ERROR_FILE_NOT_FOUND == dwError)
		{
			DFP((LOG_FILE_OTHER, 3, _T("CRecordingFile::DeleteFile() -- index file already deleted\n")));
		}
		else
		{
			DEBUGMSG(1, (L"CRecordingFile::Delete index file failed.  Err %d\n", dwError));
			fDeleteIsDone = FALSE;
		}
	}
	if (!::DeleteFileW((strFileName + DATA_FILE_SUFFIX).c_str()))
	{
		DWORD dwError = GetLastError();
		if (ERROR_FILE_NOT_FOUND == dwError)
		{
			DFP((LOG_FILE_OTHER, 3, _T("CRecordingFile::DeleteFile() -- data file already deleted\n")));
		}
		else
		{
			DEBUGMSG(1, (L"CRecordingFile::Delete data file failed.  Err %d\n", dwError));
			fDeleteIsDone = FALSE;
		}
	}
	return fDeleteIsDone;
}

LONGLONG CRecordingFile::GetMinSampleTime(bool fIgnoreVirtualStartTime)
{
	if (m_fOptimizedOpen)
	{
		if (ERROR_SUCCESS != FullyOpen())
		{
			return -1;
		}
	}

	// Make sure our file is big enough
	if (GetIndexFileSize() < sizeof(INDEX))
		return -1;

	// Make sure it's header is up to date.
	if (ReadHeader())
		return -1;

	DbgLog((LOG_FILE_IO, 3, TEXT("CRecordingFile::GetMinSampleTime() -- file %s, virtual start %I64d, true start %I64d\n"),
				m_strFilename.c_str(), m_header.tVirtualStartTime, m_header.tTrueStartTime ));

	// Return the virtual start time if applicable
	if (!fIgnoreVirtualStartTime && (m_header.tVirtualStartTime != -1))
		return m_header.tVirtualStartTime;

	ASSERT(m_header.tTrueStartTime != -1);
	return m_header.tTrueStartTime;
}

LONGLONG CRecordingFile::GetMaxSampleTime()
{
	if (m_fOptimizedOpen)
	{
		if (ERROR_SUCCESS != FullyOpen())
		{
			return -1;
		}
	}

	DbgLog((LOG_FILE_IO, 3, TEXT("CRecordingFile::GetMaxSampleTime() -- file %s, %s, max seek pos %I64d\n"),
		m_strFilename.c_str(),
		m_header.fFileInProgress ? TEXT("file-and-recording-in-progress") :
			(m_header.fInProgress ? TEXT("recording-in-progress/file-complete") : TEXT("recording-complete")),
			m_header.tMaxSeekPos ));

	// Return our stored value if we have one.
	if (!m_header.fFileInProgress && (m_header.tMaxSeekPos != -1))
		return m_header.tMaxSeekPos;

	// Go to disk for the max seek pos

	// Just in case the last index object is for some reason only partially
	// committed to disk at this point, truncate any extra bytes.
	LONGLONG llFileSize = GetIndexFileSize();
	llFileSize -= llFileSize % sizeof(INDEX);

	// Make sure our file is big enough
	if (llFileSize < sizeof(INDEX))
		return -1;

	INDEX idx;
	if (ReadIndexAtFileOffset(llFileSize - sizeof(INDEX), idx))
		return -1;

	return idx.tStart;
}

DWORD CRecordingFile::SeekToSampleTime(LONGLONG tStart, bool fSeekToKeyframe, bool fAllowEOF)
{
	DFP((LOG_FILE_IO, 5, 	_T("CRecordingFile[%p]::SeekToSampleTime(%I64d, %s) ... entry with index file %p\n"),
		&m_dataFile, tStart, fSeekToKeyframe ? _T("key-frame") : _T("any-frame"), &m_indexFile ));

	if (m_fOptimizedOpen)
	{
		_CDW(FullyOpen());
	}

	LONGLONG llFileSize = GetIndexFileSize();
	if (llFileSize < sizeof(INDEX))
	{
		DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::SeekToSampleTime() ... EOF exit\n"), &m_dataFile ));
		return ERROR_HANDLE_EOF;
	}

	// todo - change from binary to interpolating search

	DWORD dwMin = 0;
	DWORD dwMax = (DWORD) ((llFileSize - 1) / sizeof(INDEX));

	// Get our initial search position and read the index
	INDEX idx;
	DWORD dwSearch = (dwMax + dwMin) / 2;
	_CDW(ReadIndexAtFileOffset(dwSearch * sizeof(INDEX), idx));

	// Loop until we've hit the end of our search
	while (dwMin != dwMax)
	{
		if (idx.tStart > tStart)
		{
			// Handle an edge case that can occur
			if (dwMax == dwSearch)
				dwMax--;
			else
				dwMax = dwSearch;
		}
		else if (idx.tStart < tStart)
		{
			// Handle an edge case than can occur
			if (dwMin == dwSearch)
				dwMin++;
			else
				dwMin = dwSearch;
		}
		else
			break;	// Found it!

		// Update our search and read the new index
		dwSearch = (dwMax + dwMin) / 2;
		_CDW(ReadIndexAtFileOffset(dwSearch * sizeof(INDEX), idx));

	}

	// We found the closest keyframe.  This might be too far though
	// if we're trying to seek to an exact position.
	if (!fSeekToKeyframe && (dwSearch > 0) && (tStart < idx.tStart))
	{
		// Back up 1 index position
		dwSearch--;
		_CDW(ReadIndexAtFileOffset(dwSearch * sizeof(INDEX), idx));
	}

	// Another boundary case.  We might be searching for a position beyond the last
	// keyframe.  In that case, we want to end up at the EOF.  If we don't do this
	// then we'll end up at a key frame earlier than what we're searching for.  Being
	// at the EOF here is ok because the next sample read will be the key frame in
	// the next recording file which is what we want.
	if (fAllowEOF && fSeekToKeyframe && (tStart > idx.tStart))
	{
		// Careful here -- if we have a padded file we cannot just jump to end-of-file.
		// We need to move to just after the last valid file.
		if (m_header.fPaddedFile)
		{
			// idx holds the position of the closest key frame.  So we will start there and
			// scan forward until we reach the end of valid data:

			DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::SeekToSampleTime() ... scanning for EOF\n"), &m_dataFile ));

			ULONGLONG uEOFPos = idx.offset;
			DWORD dwReadError = ERROR_SUCCESS;

			while (ERROR_SUCCESS == dwReadError)
			{
				if (ERROR_SUCCESS == m_dataFile.SetFilePos(uEOFPos))
				{
					// Read the current sample header
					SAMPLE_HEADER sh;
					dwReadError = ReadSampleHeader(sh, true);
					if (dwReadError == ERROR_SUCCESS)
					{
						// We'll have to try again after this sample:
						uEOFPos = m_dataFile.GetFilePos() + sizeof(sh) + sh.dwDataLen;
					}
				}
			}

			_CDW(m_dataFile.SetFilePos(uEOFPos));
		}
		else
		{
			m_dataFile.SetFilePos(m_dataFile.GetFileSize());
		}
		DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::SeekToSampleTime() ... success exit [key frame]\n"), &m_dataFile ));
		return ERROR_SUCCESS;
	}

	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::SeekToSampleTime() ... chose key frame tStart %I64d, file pos %I64d, stream offset %I64d\n"),
		&m_dataFile,  idx.tStart, idx.offset, idx.llStreamOffset));

	// Seek to the right spot
	_CDW(m_dataFile.SetFilePos(idx.offset));

	// Bail if we just want to be at the correct key frame
	if (fSeekToKeyframe)
	{
		DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::SeekToSampleTime() ... success exit [key frame]\n"), &m_dataFile ));
		return ERROR_SUCCESS;
	}

	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::SeekToSampleTime() ... scanning for nearest sample\n"), &m_dataFile ));

	// Read the current sample header
	SAMPLE_HEADER sh;
	_CDW(ReadSampleHeader(sh, true));

	// Loop until we find our sample
	while (sh.tStart < tStart)
	{
		// Store our current location
		LONGLONG llCurPos = m_dataFile.GetFilePos();

		// Advance to the next sample and read it
		DWORD dw = m_dataFile.SetFilePos(llCurPos + sizeof(sh) + sh.dwDataLen);
		if (dw == ERROR_SUCCESS)
			dw = ReadSampleHeader(sh, true);

		if (dw != ERROR_SUCCESS)
		{
			// We are likely seeking too far, so just go to the last good
			// sample header and then break which will return success.
			_CDW(m_dataFile.SetFilePos(llCurPos));
			break;
		}
	}

	// The following two lines are useful for testing/debugging.  The make sure
	// that a valid sample time is being supplied by the sample consumer.
	// However, they should be commented out for proper IStreamBufferMediaSeeking
	// behavior.

	// Not found, return an error
	//	if (sh.tStart != tStart)
	//		return ERROR_SEEK;

	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::SeekToSampleTime() ... success exit [any frame]\n"), &m_dataFile ));
	return ERROR_SUCCESS;
}

ULONGLONG CRecordingFile::GetIndexFileSize()
{
	if (m_fOptimizedOpen)
	{
		if (ERROR_SUCCESS != FullyOpen())
		{
			return -1LL;
		}
	}

	if (m_header.fFileInProgress)
		return m_indexFile.GetFileSize();

	if (!m_ullIndexFileSize)
		m_ullIndexFileSize = m_indexFile.GetFileSize();

	return m_ullIndexFileSize;
}

ULONGLONG CRecordingFile::GetFileSize()
{
	if (m_fOptimizedOpen)
	{
		if (ERROR_SUCCESS != FullyOpen())
		{
			return -1LL;
		}
	}

	if (m_dataFile.IsOpenedForRead())
	{
		if (m_header.fFileInProgress)
			return m_dataFile.GetFileSize();

		if (!m_ullDataFileSize)
			m_ullDataFileSize = m_dataFile.GetFileSize();
	}
	else
	{
		ASSERT(m_ullDataFileSize);
	}

	return m_ullDataFileSize;
}

ULONGLONG CRecordingFile::GetIndexFilePos()
{
	if (m_fOptimizedOpen)
	{
		if (ERROR_SUCCESS != FullyOpen())
		{
			return -1LL;
		}
	}

	if (m_pIdxCache)
		return m_ullCacheFilePos;

	return m_indexFile.GetFilePos();
}

void CRecordingFile::ReleaseIndexFileCache()
{
	if (m_fOptimizedOpen)
	{
		FullyOpen();
	}

	// Restore the file pointer
	ASSERT((m_ullCacheFilePos >= 0) && (m_ullCacheFilePos <= m_ullCacheSize));
	m_indexFile.SetFilePos(m_ullCacheSize);

	// Mark us as no longer being cached
	m_pIdxCache = NULL;
}

DWORD CRecordingFile::ReadIndexAtFileOffset(LONGLONG llFileOffset, INDEX &rIdx)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingFile[%p]::ReadIndexAtFileOffset(file pos %I64d)\n"), &m_dataFile, llFileOffset ));

	if (m_fOptimizedOpen)
	{
		_CDW(FullyOpen());
	}

	// Read from the cache if we can
	if (m_pIdxCache)
	{
		// Range check
		if (llFileOffset < 0)
			_CDW(ERROR_INVALID_PARAMETER);

		// See if we're at EOF
		if ((llFileOffset + sizeof(INDEX)) > (LONGLONG) m_ullCacheSize)
		{
			// If we're not in progress, then we're at EOF
			// Get the new index file size to see if it's changed
			ULONGLONG ullNewCacheSize = GetIndexFileSize();
			if (ullNewCacheSize <= m_ullCacheSize)
			{
				ASSERT(ullNewCacheSize == m_ullCacheSize);
				_CDW(ERROR_HANDLE_EOF);
			}

			// We can now add more data to the cache, make sure we don't overflow it
			if (ullNewCacheSize > INDEX_CACHE_SIZE)
			{
				DEBUGMSG(1, (L"*** Cannot cache index file because it's too big.\n"));
				_CDW(ERROR_ACCESS_DENIED);
			}

			// Seek to the end of what we currently have in memory
			_CDW(m_indexFile.SetFilePos(m_ullCacheSize));

			// Read in the rest
			_CDW(m_indexFile.Read(m_pIdxCache + m_ullCacheSize, (DWORD) (ullNewCacheSize - m_ullCacheSize)));

			// Upadate our cache size
			m_ullCacheSize = ullNewCacheSize;

			// Now range check our read again
			if ((llFileOffset + sizeof(INDEX)) > (LONGLONG) m_ullCacheSize)
			{
				_CDW(ERROR_HANDLE_EOF);
			}

			// Success!  Now continue on and read from the cache.
		}

		memcpy(&rIdx, m_pIdxCache + llFileOffset, sizeof(INDEX));
		m_ullCacheFilePos = llFileOffset + sizeof(INDEX);

		return ERROR_SUCCESS;
	}

	_CDW(m_indexFile.SetFilePos(llFileOffset));
	_CDW(m_indexFile.Read((BYTE *) &rIdx, sizeof(rIdx)));
	return ERROR_SUCCESS;
}

DWORD CRecordingFile::CacheIndexFile(BYTE pCache[INDEX_CACHE_SIZE])
{
	if (m_fOptimizedOpen)
	{
		_CDW(FullyOpen());
	}

	if (!pCache)
		_CDW(ERROR_INVALID_PARAMETER);

	ASSERT(!m_pIdxCache);

	// Make sure the index file isn't too big
	m_ullCacheSize = GetIndexFileSize();
	if (m_ullCacheSize > INDEX_CACHE_SIZE)
	{
		DEBUGMSG(1, (L"*** Cannot cache index file because it's too big.\n"));
		_CDW(ERROR_ACCESS_DENIED);
	}

	// Grab the index file seek pos
	m_ullCacheFilePos = m_indexFile.GetFilePos();

	// Seek to the beginning of the index file
	_CDW(m_indexFile.SetFilePos(0));

	// Read the whole index file
	_CDW(m_indexFile.Read(pCache, (DWORD) m_ullCacheSize));

	// Range check the cache pos
	if ((m_ullCacheFilePos < 0) || (m_ullCacheFilePos > m_ullCacheSize))
	{
		// todo: Is it better to fail here?
		ASSERT(FALSE);
		m_ullCacheFilePos = 0;
	}

	// Store the cache
	m_pIdxCache = pCache;

	return ERROR_SUCCESS;
}

// ################################################
//      CRecordingSetWrite
// ################################################


CRecordingSetWrite::CRecordingSetWrite()
	:	m_ullMaxFileSize(DEFAULT_MAX_FILE_SIZE),
		m_rgMediaTypes(NULL),
		m_dwNumStreams((DWORD) -1),
		m_dwMaxFileNum((DWORD) -1),
		m_pbCustomData(NULL),
		m_dwCustomDataLen(0),
		m_fIsOpen(false),
		m_dwNegotiatedBufferSize(0),
		m_cCritSecDiskOpt(),
		m_pcRecordingSetWriteIOThread(NULL)
{
    if (g_bEnableFilePreCreation)
    {
    	StartDiskIOThread();
    }
}

CRecordingSetWrite::~CRecordingSetWrite()
{
    if (g_bEnableFilePreCreation)
    {
        StopDiskIOThread();
    }
    Close();
}

void CRecordingSetWrite::Close()
{
	{
		CAutoLock cAutoLock(&m_cCritSecDiskOpt);

		if (m_pcRecordingSetWriteIOThread)
		{
			m_pcRecordingSetWriteIOThread->ClearPreparedFileInRecording();
		}
	}

	// Empty out the recording list.
	while (!m_vecFiles.empty())
	{
		m_vecFiles.back()->Close();
		delete m_vecFiles.back();
		m_vecFiles.pop_back();
	}

	if (m_rgMediaTypes)
	{
		delete[] m_rgMediaTypes;
		m_rgMediaTypes = NULL;
	}

	if (m_pbCustomData)
	{
		delete[] m_pbCustomData;
		m_pbCustomData = NULL;
	}

	m_strFilename = L"";
	m_dwNumStreams = (DWORD) -1;
	m_dwMaxFileNum = (DWORD) -1;
	m_dwCustomDataLen = 0;
	m_dwNegotiatedBufferSize = 0;
	m_fIsOpen = false;
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWrite::Close() ... setting m_fIsOpen to false\n") ));
}


void CRecordingSetWrite::SetMaxFileSize(ULONGLONG ullMaxSize)
{
    m_ullMaxFileSize = ullMaxSize;  
    if (g_bEnableFilePreCreation)
    {
        BeginPaddingToNewFileSize();
    }
}

DWORD CRecordingSetWrite::CreateNew(const WCHAR *wszFileName,
									DWORD dwNegotiatedBufferSize,
									CMediaType rgMediaTypes[],
									DWORD dwNumStreams,
									BYTE rgbCustom[],
									DWORD dwNumCustomBytes,
									bool fTemporary)
{
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWrite::CreateNew()\n"), wszFileName ));

	_CP(dwNumStreams);
	_CP(wszFileName);
	_CP(rgMediaTypes);

	// Make sure we're not already open
	if (IsOpen())
	{
		DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWrite::CreateNew(%s) -- already open\n"), wszFileName ));
		_CDW(ERROR_INVALID_STATE);
	}

	// Make sure we're in a clean state
	Close();

	// Duplicate the custom data
	if (rgbCustom)
	{
		m_pbCustomData = new BYTE[dwNumCustomBytes];
		memcpy(m_pbCustomData, rgbCustom, dwNumCustomBytes);
		m_dwCustomDataLen = dwNumCustomBytes;
	}

	// Save the filename
	m_strFilename = wszFileName;

	// Hold onto the temporary flag
	m_fTemporary = fTemporary;

	// Save the theoretical max sample size
	ASSERT(dwNegotiatedBufferSize);
	m_dwNegotiatedBufferSize = dwNegotiatedBufferSize;

	// Save the pins array
	if (rgMediaTypes && (dwNumStreams > 0))
	{
		m_rgMediaTypes = new CMediaType[dwNumStreams];
		_CMEM(m_rgMediaTypes);
		m_dwNumStreams = dwNumStreams;

		for (DWORD i = 0; i < dwNumStreams; i++)
			m_rgMediaTypes[i] = rgMediaTypes[i];
	}

	// Now create a new recording file object and add it to the list
	std::auto_ptr<CRecordingFile> pNewFile(new CRecordingFile());
	m_vecFiles.push_back(pNewFile.release());
	CRecordingFile *pFile = m_vecFiles.back();

	// Create the file on disk
	wstring strNextFile = GetNextFileName();
	
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWrite::CreateNew(%s) --about to create first file %s\n"), wszFileName, strNextFile.c_str() ));

    DWORD dwRet = pFile->CreateNew(	strNextFile.c_str(),
							        m_dwNegotiatedBufferSize,
							        m_rgMediaTypes,
							        m_dwNumStreams,
							        m_pbCustomData,
							        m_dwCustomDataLen,
							        m_fTemporary,
							        0);
    if (dwRet != ERROR_SUCCESS)
    {
		DbgLog((LOG_ERROR, 3, TEXT("CRecordingSetWrite::CreateNew(): error %d\n"), dwRet ));
        return dwRet;
    }

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWrite::CreateNew() ... setting m_fIsOpen to true\n") ));
	m_fIsOpen = true;

	{
		CAutoLock cAutoLock(&m_cCritSecDiskOpt);

		if (m_pcRecordingSetWriteIOThread)
		{
			m_pcRecordingSetWriteIOThread->PrepareFileInRecording();
		}
	}

	return ERROR_SUCCESS;
}

DWORD CRecordingSetWrite::UsePreparedNew(DWORD dwNegotiatedBufferSize,
									CMediaType rgMediaTypes[],
									DWORD dwNumStreams,
									BYTE rgbCustom[],
									DWORD dwNumCustomBytes,
									bool fTemporary)
{
	_CP(dwNumStreams);
	_CP(rgMediaTypes);

    ASSERT (g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWrite::UsePreparedNew called but not pre-creating files!\n"));
        return ERROR_INVALID_STATE;
    }

	// Make sure we're not already open
	if (IsOpen())
	{
		DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWrite::UsePreparedNew() -- already open\n") ));
		_CDW(ERROR_INVALID_STATE);
	}

	// Make sure we're in a clean state
	Close();

	CRecordingFile *pPreppedFile = NULL;
	std::wstring strPreppedRecording = L"";

	{
		CAutoLock cAutoLock(&m_cCritSecDiskOpt);

		if (m_pcRecordingSetWriteIOThread)
		{
			pPreppedFile = m_pcRecordingSetWriteIOThread->GetNextRecording(strPreppedRecording);
		}
	}
	if (!pPreppedFile)
		return ERROR_INVALID_STATE;

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWrite::UsePreparedNew() -- using prepped %s, file %s\n"),
		strPreppedRecording.c_str(), pPreppedFile->GetFileName().c_str() ));

	// Duplicate the custom data
	if (rgbCustom)
	{
		m_pbCustomData = new BYTE[dwNumCustomBytes];
		memcpy(m_pbCustomData, rgbCustom, dwNumCustomBytes);
		m_dwCustomDataLen = dwNumCustomBytes;
	}

	// Save the filename
	m_strFilename = strPreppedRecording;
	m_dwMaxFileNum = 0;			

	// Hold onto the temporary flag
	m_fTemporary = fTemporary;

	// Save the theoretical max sample size
	ASSERT(dwNegotiatedBufferSize);
	m_dwNegotiatedBufferSize = dwNegotiatedBufferSize;

	// Save the pins array
	if (rgMediaTypes && (dwNumStreams > 0))
	{
		m_rgMediaTypes = new CMediaType[dwNumStreams];
		_CMEM(m_rgMediaTypes);
		m_dwNumStreams = dwNumStreams;

		for (DWORD i = 0; i < dwNumStreams; i++)
			m_rgMediaTypes[i] = rgMediaTypes[i];
	}

	// Make sure that this file is marked as permanent if the recording
	// is permanent:

	if (!m_fTemporary)
	{
		pPreppedFile->GetHeader().fTemporary = false;
		DWORD dwStatusUpdateHeader = pPreppedFile->WriteHeader();
		if (ERROR_SUCCESS != dwStatusUpdateHeader)
		{
			delete pPreppedFile;
			return dwStatusUpdateHeader;
		}
	}

	// Now create a new recording file object and add it to the list
	m_vecFiles.push_back(pPreppedFile);

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetWrite::UsePreparedNew() ... setting m_fIsOpen to true, recording %s, file %s\n"),
		m_strFilename.c_str(), pPreppedFile->GetFileName().c_str() ));
	m_fIsOpen = true;

	{
		CAutoLock cAutoLock(&m_cCritSecDiskOpt);

		if (m_pcRecordingSetWriteIOThread)
		{
			m_pcRecordingSetWriteIOThread->PrepareFileInRecording();
		}
	}

	return ERROR_SUCCESS;
} // CRecordingSetWrite::UsePreparedNew

void CRecordingSetWrite::PrepareNextRecording(std::wstring &strRecordingName)
{
	CAutoLock cAutoLock(&m_cCritSecDiskOpt);

    ASSERT(g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWrite::PrepareNextRecording called but not pre-creating files!\n"));
        return;
    }

	if (m_pcRecordingSetWriteIOThread)
	{
		m_pcRecordingSetWriteIOThread->PrepareNextRecording(strRecordingName);
	}
} // CRecordingSetWrite::PrepareNextRecording

void CRecordingSetWrite::ClearPreppedRecording()
{
	CAutoLock cAutoLock(&m_cCritSecDiskOpt);

    ASSERT(g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWrite::ClearPreppedRecording called but not pre-creating files!\n"));
        return;
    }

	if (m_pcRecordingSetWriteIOThread)
	{
		m_pcRecordingSetWriteIOThread->ClearPreparedRecording();
	}
} // CRecordingSetWrite::ClearPreppedRecording

CRecordingFile *CRecordingSetWrite::ReleaseLastRecordingFile()
{
	// Handle the trivial case
	if (!IsOpen() || m_vecFiles.empty())
		return NULL;

	CRecordingFile *p = m_vecFiles.back();
	m_vecFiles.pop_back();
	return p;
}


wstring CRecordingSetWrite::GetNextFileName()
{
	WCHAR wszTemp[20];
	swprintf(wszTemp, L"_%d", ++m_dwMaxFileNum);
	return m_strFilename + wszTemp;
}

void CRecordingSetWrite::StartDiskIOThread()
{
    ASSERT(g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWrite::StartDiskIOThread called but not pre-creating files!\n"));
        return;
    }

	try {
		CAutoLock cAutoLock(&m_cCritSecDiskOpt);

		if (m_pcRecordingSetWriteIOThread)
			return;

		m_pcRecordingSetWriteIOThread = new CRecordingSetWriteIOThread(*this);
		if (!m_pcRecordingSetWriteIOThread)
			return;

		m_pcRecordingSetWriteIOThread->StartThread();

	}
	catch (const std::exception &)
	{
		m_pcRecordingSetWriteIOThread = NULL;
	};

} // CRecordingSetWrite::StartDiskIOThread

void CRecordingSetWrite::StopDiskIOThread()
{
	CRecordingSetWriteIOThread *pcRecordingSetWriteIOThread = NULL;

    ASSERT(g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWrite::StopDiskIOThread called but not pre-creating files!\n"));
        return;
    }

	{
		CAutoLock cAutoLock(&m_cCritSecDiskOpt);

		pcRecordingSetWriteIOThread = m_pcRecordingSetWriteIOThread;
		if (!pcRecordingSetWriteIOThread)
			return;

		pcRecordingSetWriteIOThread->SignalThreadToStop();
		pcRecordingSetWriteIOThread->SignalWakeup();
	}
	pcRecordingSetWriteIOThread->SleepUntilStopped();
} // CRecordingSetWrite::StopDiskIOThread

void CRecordingSetWrite::BeginPaddingToNewFileSize()
{
	CRecordingSetWriteIOThread *pcRecordingSetWriteIOThread = NULL;

    ASSERT(g_bEnableFilePreCreation);
    if (!g_bEnableFilePreCreation)
    {
        RETAILMSG(1, (L"CRecordingSetWrite::BeginPaddingToNewFileSize called but not pre-creating files!\n"));
        return;
    }

	{
		CAutoLock cAutoLock(&m_cCritSecDiskOpt);

		if (m_pcRecordingSetWriteIOThread)
		{
			m_pcRecordingSetWriteIOThread->SignalWakeup();
		}
	}
} // CRecordingSetWrite::BeginPaddingToNewFileSize

DWORD CRecordingSetWrite::WriteSample(	IMediaSample &rSample,
										DWORD dwStreamNum,
										LONGLONG llDataOffset,
										bool fCreateIndex,
										bool *pfAddedNewFile)
{
	_CP(pfAddedNewFile);

	*pfAddedNewFile = false;

	// Get the most recent file
	CRecordingFile *pFile = m_vecFiles.back();

	// On index boundaries: see if the current file is to big OR if the index file has
	// reached the cache size
	if (fCreateIndex && ((pFile->GetFileSize() > m_ullMaxFileSize) ||
						 ((pFile->GetSizeLimit() != 0) && (pFile->GetFileSize() > pFile->GetSizeLimit())) ||
						((pFile->GetIndexFilePos()+sizeof(INDEX)) >= INDEX_CACHE_SIZE)) )
	{
		// Add a new file to the recording set

		_CDW(pFile->CommitFinalFileState(true));

		pFile = NULL;

		{
			CAutoLock cAutoLock(&m_cCritSecDiskOpt);

			if (m_pcRecordingSetWriteIOThread)
			{
				pFile = m_pcRecordingSetWriteIOThread->GetNextFileInRecording();
				m_pcRecordingSetWriteIOThread->PrepareFileInRecording();
			}
		}

		if (pFile)
		{
			if (!m_fTemporary)
		{
				pFile->GetHeader().fTemporary = false;
				pFile->WriteHeader();
			}
			m_vecFiles.push_back(pFile);
		}
		else
		{
			// Create a new recording file object and add it to the map
			std::auto_ptr<CRecordingFile> pNewFile(new CRecordingFile());
			m_vecFiles.push_back(pNewFile.release());
			pFile = m_vecFiles.back();

			// Create the files on disk
			wstring strNextFile = GetNextFileName();
			_CDW(pFile->CreateNew(	strNextFile.c_str(),
									m_dwNegotiatedBufferSize,
									m_rgMediaTypes,
									m_dwNumStreams,
									m_pbCustomData,
									m_dwCustomDataLen,
									m_fTemporary,
									0));

			RETAILMSG(LOG_LOCKING, (L"## RecFile creating %s *\n", strNextFile.c_str()));
		}

		*pfAddedNewFile = true;
	}

	// pFile now points to the current active file
	return pFile->WriteSample(rSample, dwStreamNum, llDataOffset, fCreateIndex);
}

bool CRecordingSetWrite::RemoveSubFile(std::wstring &strSubFilename)
{
	for (std::vector<CRecordingFile *>::iterator it = m_vecFiles.begin();
		it != m_vecFiles.end();
		it++)
	{
		// Do a case insensitive string comparison
		if (!_wcsicmp((*it)->GetFileName().c_str(), strSubFilename.c_str()))
		{
			(*it)->Close();
			delete *it;
			m_vecFiles.erase(it);
			return true;
		}
	}
	return false;
}

bool CRecordingSetWrite::SetTempPermStatus(	const wstring &strSubFilename,
											LONGLONG tVirtualStart,
											bool fTemporary)
{
	for (std::vector<CRecordingFile *>::iterator it = m_vecFiles.begin();
		it != m_vecFiles.end();
		it++)
	{
		// Do a case insensitive string comparison
		if (!_wcsicmp((*it)->GetFileName().c_str(), strSubFilename.c_str()))
		{
			return (*it)->SetTempPermStatus(tVirtualStart, fTemporary)
				== ERROR_SUCCESS;
		}
	}
	return false;
}


// ################################################
//      CRecordingSetRead
// ################################################

CRecordingSetRead::CRecordingSetRead()
	:	m_rgMediaTypes(NULL),
		m_dwNumStreams((DWORD) -1),
		m_uiActiveFileNum((DWORD) -1),
		m_dwMaxSampleSize(0),
		m_pbCustomData(NULL),
		m_dwCustomDataLen(0),
		m_llActIdxOffset(-1),
		m_iLastRecFileSeekedTo(0),
		m_fDontOpenTempFiles(false)
{
	for (ULONG idxCacheIdx = 0; idxCacheIdx < INDEX_CACHE_COUNT; idxCacheIdx++)
	{
		m_rgidxCache[idxCacheIdx].uiCachedIdxFile = (DWORD) -1;
	}
}

void CRecordingSetRead::Close()
{
	// Empty out the recording list.  The destructor of each
	// item will automatically close the recording file.
	while (!m_vecFiles.empty())
	{
		delete m_vecFiles.back();
		m_vecFiles.pop_back();
	}

	if (m_rgMediaTypes)
	{
		delete[] m_rgMediaTypes;
		m_rgMediaTypes = NULL;
	}

	if (m_pbCustomData)
	{
		delete[] m_pbCustomData;
		m_pbCustomData = NULL;
	}

	m_strFilename = L"";
	m_dwNumStreams = (DWORD) -1;
	m_uiActiveFileNum = (DWORD) -1;
	m_dwMaxSampleSize = 0;
	m_dwCustomDataLen = 0;
	m_llActIdxOffset = -1;
	m_iLastRecFileSeekedTo = 0;
	m_fDontOpenTempFiles = false;
	for (ULONG idxCacheIdx = 0; idxCacheIdx < INDEX_CACHE_COUNT; idxCacheIdx++)
	{
		m_rgidxCache[idxCacheIdx].uiCachedIdxFile = (DWORD) -1;
	}
}

DWORD CRecordingSetRead::Open(const WCHAR *wszFileName, bool fOpenEmpty, bool fIgnoreTempRecFiles)
{
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetRead::Open(%s, %s, %s)\n"),
		wszFileName, fOpenEmpty ? TEXT("open-empty") : TEXT("dont-open-empty"),
		fIgnoreTempRecFiles ? TEXT("skip-temp-recording") : TEXT("temp-recording-ok") ));

	_CP(wszFileName);

	// Make sure we're not already open
	if (IsOpen())
		_CDW(ERROR_INVALID_STATE);

	// Make sure we're in a clean state
	Close();

	m_strFilename = wszFileName;
//	m_fDontOpenTempFiles = false;				// Use this to allow playing back of temp recording files.
	m_fDontOpenTempFiles = fIgnoreTempRecFiles; // Use this for proper behavior

	// If m_strFilename ends with .dvr-dat, truncate off the extension and file number
	size_t searchPos = m_strFilename.rfind(DATA_FILE_SUFFIX);
	if (searchPos != wstring::npos)
	{
		// Extract the file extension
		m_strFilename = m_strFilename.substr(0, searchPos);

		// Extract the file number
		m_strFilename = m_strFilename.substr(0, m_strFilename.rfind(L"_"));
	}

	// Open the actual files
	if (!fOpenEmpty)
	{
		DWORD dw = OpenNewFiles(true);
		if (dw != ERROR_SUCCESS)
		{
			Close();
			_CDW(dw);
		}
	}

	m_uiActiveFileNum = 0;
	m_llActIdxOffset = -1;
	for (ULONG idxCacheIdx = 0; idxCacheIdx < INDEX_CACHE_COUNT; idxCacheIdx++)
	{
		m_rgidxCache[idxCacheIdx].uiCachedIdxFile = (DWORD) -1;
	}

#ifdef DEBUG
	Dump();
#endif // DEBUG

	return ERROR_SUCCESS;
}


DWORD CRecordingSetRead::AddFile(	const WCHAR *wszFileName,
									bool fUpdateMaxSampleSize,
									bool fOptimizeOpen)
{
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetRead::AddFile(%s) -- %s\n"),
		wszFileName, fOptimizeOpen ? TEXT("optimized-open") : TEXT("full-open") ));

	// See if the file is already in the recording set
	for (std::vector<CRecordingFile *>::iterator it = m_vecFiles.begin();
		it != m_vecFiles.end();
		it++)
	{
		if (!_wcsicmp((*it)->GetFileName().c_str(), wszFileName))
		{
			// It's already in here!
			return ERROR_SUCCESS;
		}
	}

	// Read in the header of the newest file.  Its (highly) possible that
	// the header was updated.
	if (!fOptimizeOpen && (m_vecFiles.size() > 0))
	{
		CRecordingFile *pLastFile = m_vecFiles.back();
		_CDW(pLastFile->ReadHeader());
	}

	// Create a new recording file
	std::auto_ptr<CRecordingFile> pFile(new CRecordingFile());

	if (fOptimizeOpen)
	{
		_CDW(pFile->OptimizedOpen(wszFileName));

		int idx = pFile->GetCreationIndex();
		std::wstring &strNewFileName = pFile->GetFileName();
		std::wstring strNewRecordingName = strNewFileName.substr(0, strNewFileName.rfind(L"_") - 1);


		// Perform a sorted insert based on the creation index (the _# suffix
		// in the file name) for files in the same recording.
		for (	std::vector<CRecordingFile *>::iterator it = m_vecFiles.begin();
				it != m_vecFiles.end();
				it++)
		{
			CRecordingFile *pExistingFile = *it;
			std::wstring &strExistingFileName = pExistingFile->GetFileName();
			std::wstring strExistingRecordingName = strExistingFileName.substr(0, strExistingFileName.rfind(L"_") - 1);

			if ((idx < pExistingFile->GetCreationIndex()) &&
				(strNewRecordingName == strExistingRecordingName))
			{
				m_vecFiles.insert(it, pFile.release());
				return ERROR_SUCCESS;
			}
		}

		// Just put it at the end
		m_vecFiles.push_back(pFile.release());

		return ERROR_SUCCESS;
	}

	// Open the file
	DWORD dwNumStreams, dwTempMax, dwCustomDataLen;
	_CDW(pFile->Open(wszFileName,
					&dwNumStreams,
					&dwTempMax,
					&dwCustomDataLen));

	// If the file is temporary && fIgnoreTempRecFiles is set, ignore it
	if (m_fDontOpenTempFiles && pFile->GetHeader().fTemporary)
	{
		pFile->Close();
		return ERROR_SUCCESS;
	}

	// We don't handle files with 0 streams
	if (dwNumStreams == 0)
	{
		_CDW(ERROR_INVALID_DATA);
	}

	// Update our max sample size
	if (dwTempMax > m_dwMaxSampleSize)
	{
		if (fUpdateMaxSampleSize)
			m_dwMaxSampleSize = dwTempMax;
		else
		{
			// Problem - this means that the graph is likely already
			// running so we can't handle these bigger samples.
			_CDW(ERROR_INVALID_DATA);
		}
	}

	// Store the custom data if we don't have it yet.
	if (!m_pbCustomData)
	{
		// Make a new buffer for the custom data
		m_dwCustomDataLen = dwCustomDataLen;
		m_pbCustomData = new BYTE[m_dwCustomDataLen];

		// Read the custom data
		_CDW(pFile->ReadCustom(m_pbCustomData, m_dwCustomDataLen));
	}
	else
	{
		// Read the custom data
		_CDW(pFile->ReadCustom(NULL, dwCustomDataLen));

		// Verify that the custom data length hasn't changed
		if (dwCustomDataLen != m_dwCustomDataLen)
		{
			// We don't handle dynamic format changes yet.
			_CDW(ERROR_INVALID_DATA);
		}

		// Assume that the actual custom data buffers are the same
	}

	// Check and make sure the number of streams is valid
	if (m_dwNumStreams == -1)
		m_dwNumStreams = dwNumStreams;
	else if (m_dwNumStreams != dwNumStreams)
	{
		// We don't handle dynamic format changes yet. So we
		// must have the same number of streams in each file.
		_CDW(ERROR_INVALID_DATA);
	}

	// Allocate our array of media types if we don't have one yet
	bool fJustAllocatedArray = false;
	if (!m_rgMediaTypes)
	{
		m_rgMediaTypes = new CMediaType[m_dwNumStreams];
		fJustAllocatedArray = true;
	}

	// Read in/verify the media types in the data file
	_CDW(pFile->ReadMediaTypes(m_rgMediaTypes, m_dwNumStreams, !fJustAllocatedArray));

	// Add the file to the map based on min seek position.
	LONGLONG t = pFile->GetMinSampleTime(true);

	// Perform a sorted insert based on t
	for (	std::vector<CRecordingFile *>::iterator it = m_vecFiles.begin();
			it != m_vecFiles.end();
			it++)
	{
		if (t < (*it)->GetMinSampleTime(true))
		{
			m_vecFiles.insert(it, pFile.release());
			return ERROR_SUCCESS;
		}
	}

	// Just put it at the end
	m_vecFiles.push_back(pFile.release());

	return ERROR_SUCCESS;
}


DWORD CRecordingSetRead::FinishOptimizedOpen()
{
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetRead::FinishOptimizedOpen() -- entry for %s\n"), m_strFilename.c_str() ));

	// Fully open the first and last files.  Note that for the
	// first file, we may need to search to find a non-temporary file.

	if (m_vecFiles.empty())
	{
		return ERROR_SUCCESS;
	}

	CRecordingFile *pLastFile = m_vecFiles.back();
	_CDW(FullyOpen(pLastFile, true));

	if (pLastFile->IndexIsEmpty())
	{
		// This is potentially a file prepped in advance -- not yet part of the recording.
		// Don't add it after all.

		DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetRead::FinishOptimizedOpen(%s) -- rejecting empty file %s\n"),
			m_strFilename.c_str(), pLastFile->GetFileName().c_str() ));

		delete m_vecFiles.back();
		m_vecFiles.pop_back();
	
		if (m_vecFiles.empty())
		{
			return ERROR_SUCCESS;
		}
		pLastFile = m_vecFiles.back();
		_CDW(FullyOpen(pLastFile, true));
	}

	if (m_fDontOpenTempFiles && pLastFile->GetHeader().fTemporary)
	{
		// It could be that just the final file is temporary (prepped in advance for
		// a permanent recording that was aborted):

		delete m_vecFiles.back();
		m_vecFiles.pop_back();
	
		if (m_vecFiles.empty())
		{
			return ERROR_SUCCESS;
		}
		pLastFile = m_vecFiles.back();
		_CDW(FullyOpen(pLastFile, true));
	}

	if (m_fDontOpenTempFiles && pLastFile->GetHeader().fTemporary)
	{
		// All of the files must be temporary -- zap them:
		while (!m_vecFiles.empty())
		{
			delete m_vecFiles.back();
			m_vecFiles.pop_back();
		}
	}
	if (m_vecFiles.empty())
	{
		return ERROR_SUCCESS;
	}

	// Okay, at least the final file is not temporary or we allow temporary files.
	// If we disallow temporary files, find the first permanent one:

	CRecordingFile *pFirstFile = m_vecFiles[0];
	if (!pFirstFile->UsedOptimizedOpen())
	{
		return ERROR_SUCCESS;
	}
	_CDW(FullyOpen(pFirstFile, true));

	if (m_fDontOpenTempFiles && pFirstFile->GetHeader().fTemporary)
	{
		// Invariants:  m_vecFiles[uLowerBound] is temporary
		//              uLowerBound is always a valid index
		//				m_vecFiles[i > uUpperBound] is never temporary
		size_t uLowerBound = 0;
		size_t uUpperBound = m_vecFiles.size() - 2;		// We know that the last file i.e [size()-1] is not temporary

		while (uLowerBound < uUpperBound)
		{
			size_t idx = (uLowerBound + uUpperBound + 1) >> 1;		// uLowerBound < idx <= uUpperBound
			pFirstFile = m_vecFiles[idx];
			if (pFirstFile->UsedOptimizedOpen())
			{
				_CDW(FullyOpen(pFirstFile, true));
			}
			if (pFirstFile->GetHeader().fTemporary)
			{
				uLowerBound = idx;
			}
			else
			{
				uUpperBound = idx - 1;
			}
		}  // end while loop searching for the boundary between temporary and permanent
		
		// At this point, uLowerBound is the index of the last temporary file.
		std::vector<CRecordingFile *>::iterator it = m_vecFiles.begin();
		size_t idx;
		for (	idx = 0;
				(idx <= uLowerBound) && (it != m_vecFiles.end());
				++idx )
		{
			pFirstFile = m_vecFiles[0];
			delete pFirstFile;
			it = m_vecFiles.erase(it);
		}
	}

	return ERROR_SUCCESS;
} // CRecordingSetRead::FinishOptimizedOpen

DWORD CRecordingSetRead::FullyOpen(CRecordingFile *pFile, bool fUpdateMaxSampleSize)
{
	// Open the file for real

	_CP(pFile);
	DWORD dwNumStreams, dwTempMax, dwCustomDataLen;
	_CDW(pFile->FullyOpen(&dwNumStreams,
					&dwTempMax,
					&dwCustomDataLen));

	// We don't handle files with 0 streams
	if (dwNumStreams == 0)
	{
		_CDW(ERROR_INVALID_DATA);
	}

	// Update our max sample size
	if (dwTempMax > m_dwMaxSampleSize)
	{
		if (fUpdateMaxSampleSize)
			m_dwMaxSampleSize = dwTempMax;
		else
		{
			// Problem - this means that the graph is likely already
			// running so we can't handle these bigger samples.
			_CDW(ERROR_INVALID_DATA);
		}
	}

	pFile->SetFilePos(sizeof(FILE_HEADER));
	// Store the custom data if we don't have it yet.
	if (!m_pbCustomData)
	{
		// Make a new buffer for the custom data
		m_dwCustomDataLen = dwCustomDataLen;
		m_pbCustomData = new BYTE[m_dwCustomDataLen];

		// Read the custom data
		_CDW(pFile->ReadCustom(m_pbCustomData, m_dwCustomDataLen));
	}
	else
	{
		// Read the custom data
		_CDW(pFile->ReadCustom(NULL, dwCustomDataLen));

		// Verify that the custom data length hasn't changed
		if (dwCustomDataLen != m_dwCustomDataLen)
		{
			// We don't handle dynamic format changes yet.
			_CDW(ERROR_INVALID_DATA);
		}

		// Assume that the actual custom data buffers are the same
	}

	// Check and make sure the number of streams is valid
	if (m_dwNumStreams == -1)
		m_dwNumStreams = dwNumStreams;
	else if (m_dwNumStreams != dwNumStreams)
	{
		// We don't handle dynamic format changes yet. So we
		// must have the same number of streams in each file.
		_CDW(ERROR_INVALID_DATA);
	}

	// Allocate our array of media types if we don't have one yet
	bool fJustAllocatedArray = false;
	if (!m_rgMediaTypes)
	{
		m_rgMediaTypes = new CMediaType[m_dwNumStreams];
		fJustAllocatedArray = true;
	}

	// Read in/verify the media types in the data file
	_CDW(pFile->ReadMediaTypes(m_rgMediaTypes, m_dwNumStreams, !fJustAllocatedArray));

	return ERROR_SUCCESS;
} // CRecordingSetRead::FullyOpen


DWORD CRecordingSetRead::OpenNewFiles(bool fMayOptimize)
{
	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetRead::OpenNewFiles(%s)\n"),
		fMayOptimize ? TEXT("optimized-open") : TEXT("full-open") ));

	// Build the search string
	wstring strSearch = m_strFilename + L"_*" + DATA_FILE_SUFFIX;

	// Iterate through wszFileName_*.dvr-dat to find the first file in the
	// recording set .
	WIN32_FIND_DATAW fd;
	HANDLE hFind = FindFirstFileW(strSearch.c_str(), &fd);

	if (fMayOptimize && !m_vecFiles.empty())
		fMayOptimize = false;

	// Retrieve the path from the search string
	// ie Turn "c:\recordings\temp_*.dvr-dat" to "c:\recordings\"

	try
	{
		wstring strPath = strSearch.substr(0, strSearch.rfind(L"\\")+1);

		if (hFind == INVALID_HANDLE_VALUE)
		{
			_CDW(GetLastError());
		}

		do
		{
			// Remove the .dvr-dat from the file name if it's there
			wstring strFile = strPath + fd.cFileName;
			strFile = strFile.substr(0, strFile.rfind(DATA_FILE_SUFFIX));

			// Now add the file
			DWORD dw = AddFile(strFile.c_str(), true, fMayOptimize);
			if (dw != ERROR_SUCCESS)
			{
				FindClose(hFind);
				_CDW(dw);
			}
			else
			{
				DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetRead::OpenNewFiles() -- added file %s\n"),
					strFile.c_str() ));
			}

		} while (FindNextFileW(hFind, &fd));

		if (fMayOptimize)
		{
			DWORD dwFinishOpen = FinishOptimizedOpen();
			if (dwFinishOpen != ERROR_SUCCESS)
			{
				DbgLog((LOG_ERROR, 1, TEXT("CRecordingSetRead::OpenNewFiles():  error 0x%x from FinishOptimizedOpen()\n"), dwFinishOpen ));

				FindClose(hFind);
				_CDW(dwFinishOpen);
			}
		}
	}
	catch (const std::exception&)
	{
		// In case any of these wstring calls throw
		FindClose(hFind);
		_CDW(ERROR_NOT_ENOUGH_MEMORY);
	}

	FindClose(hFind);

	DbgLog((LOG_FILE_OTHER, 3, TEXT("CRecordingSetRead::OpenNewFiles() -- exit\n") ));

#ifdef DEBUG
	Dump();
#endif // DEBUG

	return ERROR_SUCCESS;
}

DWORD CRecordingSetRead::ReadSample(SAMPLE_HEADER &rHeader,
									IMediaSample &rSample)
{
	// Don't check to see if we're at the end of the file.  That's because
	// ReadSampleHeader should have already advanced us to the next file as
	// necessary.
	CRecordingFile *pFile = m_vecFiles[m_uiActiveFileNum];
	if (pFile->UsedOptimizedOpen())
	{
		_CDW(pFile->FullyOpen());
	}
	return pFile->ReadSample(rHeader, rSample);
}

DWORD CRecordingSetRead::ReadSampleData(SAMPLE_HEADER &rHeader,
                                        BYTE *pBuf,
                                        ULONG cbBuf,
                                        ULONG* pcbRead)
{
	// Don't check to see if we're at the end of the file.  That's because
	// ReadSampleHeader should have already advanced us to the next file as
	// necessary.
	CRecordingFile *pFile = m_vecFiles[m_uiActiveFileNum];
	if (pFile->UsedOptimizedOpen())
	{
		_CDW(pFile->FullyOpen());
	}
	return pFile->ReadSampleData(rHeader, pBuf, cbBuf, pcbRead);
}

bool CRecordingSetRead::RemoveSubFile(std::wstring &strSubFilename)
{
	size_t index = 0;
	for (std::vector<CRecordingFile *>::iterator it = m_vecFiles.begin();
		it != m_vecFiles.end();
		it++, index++)
	{
		// Do a case insensitive string comparison
		if (!_wcsicmp((*it)->GetFileName().c_str(), strSubFilename.c_str()))
		{	
			// We shouldn't be removing a file we're playing back from
			if (index == m_uiActiveFileNum)
			{
				// todo : pull out this debug message
				DbgLog((LOG_FILE_OTHER, 4, TEXT("CRecordingSetWrite::RemoveSubFile - Removing m_iActiveFileNum - reader will require seek\n")));
				m_uiActiveFileNum = 0;
			}
			else if (index < m_uiActiveFileNum)
			{
				// The index is less than m_uiActiveFileNum so shift m_uiActiveFileNum
				m_uiActiveFileNum--;

				// todo : pull out this debug message
				DbgLog((LOG_FILE_OTHER, 4, TEXT("CRecordingSetWrite::RemoveSubFile - Removing non-active file.\n")));
			}

			// Update our cached index file number the same way
			for (ULONG idxFixup = 0; idxFixup < INDEX_CACHE_COUNT; idxFixup++)
			{
				if (index == m_rgidxCache[idxFixup].uiCachedIdxFile)
					m_rgidxCache[idxFixup].uiCachedIdxFile = (DWORD) -1;
				else if ((index < m_rgidxCache[idxFixup].uiCachedIdxFile) && (m_rgidxCache[idxFixup].uiCachedIdxFile != -1))
					m_rgidxCache[idxFixup].uiCachedIdxFile--;
			}

			(*it)->Close();
			delete *it;
			m_vecFiles.erase(it);
			return true;
		}
	}
	return false;
}

LONGLONG CRecordingSetRead::GetMinSampleTime(bool fIgnoreVirtualStartTime)
{
	// Get the first file
	vector<CRecordingFile *>::iterator it = m_vecFiles.begin();
	if (it == m_vecFiles.end())
		return -1;

	return (*it)->GetMinSampleTime(fIgnoreVirtualStartTime);
}

LONGLONG CRecordingSetRead::GetMaxSampleTime()
{
	// Get the last file
	vector<CRecordingFile *>::reverse_iterator it = m_vecFiles.rbegin();
	if (it == m_vecFiles.rend())
		return -1;
	
	// It's possible the last file is empty, if it is, then try
	// the previous file
	while ((*it)->GetIndexFileSize() < sizeof(INDEX))
	{
		it++;
		if (it == m_vecFiles.rend())
			return -1;
	}

	return (*it)->GetMaxSampleTime();
}

DWORD CRecordingSetRead::SeekToSampleTime(LONGLONG tStart, bool fSeekToKeyframe)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::SeekToSampleTime(%I64d, %s)\n"),
		this, tStart, fSeekToKeyframe ? _T("key-frame") : _T("any-frame") ));

	CRecordingFile *pFile = NULL;

    if (!m_vecFiles.size())
    {
        DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::SeekToSampleTime() ... error exit\n"), this ));
        return -1;
    }

	if ((m_iLastRecFileSeekedTo < 0) || (m_iLastRecFileSeekedTo > (m_vecFiles.size() - 1)))
	{
		m_iLastRecFileSeekedTo = m_vecFiles.size() - 1;
	}

	// m_iLastRecFileSeekedTo contains the recording file of our last seek.
	// Perform a linear search through m_vecFiles but do it from our last seek position.
	// This takes advantage of the fact that we very often seek to similar positions.

	if ((tStart < m_vecFiles[m_iLastRecFileSeekedTo]->GetMinSampleTime(true)) && (m_iLastRecFileSeekedTo > 0))
	{
		m_iLastRecFileSeekedTo--;

		// Walk down
		while (!pFile && ((m_iLastRecFileSeekedTo+1) >= 1))
		{
			if (m_vecFiles[m_iLastRecFileSeekedTo]->GetMinSampleTime(true) <= tStart)
			{
				// Found it!
				pFile = m_vecFiles[m_iLastRecFileSeekedTo];
			}
			else
				m_iLastRecFileSeekedTo--;
		}
	}
	else if (tStart >= m_vecFiles[m_iLastRecFileSeekedTo]->GetMinSampleTime(true))
	{
		// Walk up
		while (!pFile && ((m_iLastRecFileSeekedTo+2) <= m_vecFiles.size()))
		{
			if (m_vecFiles[m_iLastRecFileSeekedTo+1]->GetMinSampleTime(true) > tStart)
			{
				// Found it!
				pFile = m_vecFiles[m_iLastRecFileSeekedTo];
			}
			else
				m_iLastRecFileSeekedTo++;
		}

		// If we didn't find it then that means it must be the last file
		if (!pFile)
			pFile = m_vecFiles[m_iLastRecFileSeekedTo];
	}

	if (!pFile)
	{
		DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::SeekToSampleTime() ... error exit\n"), this ));
		return -1;
	}

	if (pFile->UsedOptimizedOpen())
	{
		_CDW(pFile->FullyOpen());
	}

	// Now pass the call on and save the new active file number.
	CacheIndexFile(m_iLastRecFileSeekedTo);
	_CDW(pFile->SeekToSampleTime(tStart, fSeekToKeyframe, (m_iLastRecFileSeekedTo +1) != m_vecFiles.size()));
	if (m_iLastRecFileSeekedTo != m_uiActiveFileNum)
	{
		DbgLog((LOG_FILE_OTHER, 1, _T("### DVR Source Filter:  switching to a different disk file [1] ###\n")));
	}
	m_uiActiveFileNum = m_iLastRecFileSeekedTo;
	m_llActIdxOffset = -1;
	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::SeekToSampleTime() ... success exit\n"), this ));

	return ERROR_SUCCESS;
}

LONGLONG CRecordingSetRead::GetNavPackStreamOffsetFast(LONGLONG tStart)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFast(%I64d) ... entry\n"), this, tStart ));

	if (tStart < m_idxLastNavPackFound.tStart)
	{
		DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFast(%I64d) ... bailing, tested versus %I64d\n"),
				this, tStart, m_idxLastNavPackFound.tStart ));
		return -1;
	}

	if (m_uiActiveFileNum >= m_vecFiles.size())
	{
		DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFast(%I64d) ... bailing, invalid active file index [%d]\n"),
				this, tStart, m_uiActiveFileNum ));
		return -1;
	}

	// Set up some state variables
	INDEX idx;
	CRecordingFile *pFile = m_vecFiles[m_uiActiveFileNum];
	if (pFile->UsedOptimizedOpen())
	{
		if (ERROR_SUCCESS != pFile->FullyOpen())
		{
			return -1;
		}
	}
	LONGLONG llActiveFileSize = pFile->GetIndexFileSize();
	m_llActIdxOffset += sizeof(INDEX);	// Advance to the next one we want to read

	// Keep looping until we hit EOF, hit an error, or find the right nav_pack
	while (1)
	{
		// Update the cached index file
		CacheIndexFile(m_uiActiveFileNum);

		DWORD dw = pFile->ReadIndexAtFileOffset(m_llActIdxOffset, idx);
		if (dw == ERROR_HANDLE_EOF)
		{
			// Increment our active file and see if we're at the last file.
			m_uiActiveFileNum++;
			if (m_uiActiveFileNum >= m_vecFiles.size())
			{
				DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFast(%I64d) ... bailing, post-increment invalid active file index [%d]\n"),
						this, tStart, m_uiActiveFileNum ));
				return -1;
			}

			DFP((LOG_FILE_OTHER, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFast(%I64d) ... updated active file index [now %d]\n"),
				this, tStart, m_uiActiveFileNum ));

			// Update our other state vars
			m_llActIdxOffset = 0;
			pFile = m_vecFiles[m_uiActiveFileNum];
			if (pFile->UsedOptimizedOpen())
			{
				if (ERROR_SUCCESS != pFile->FullyOpen())
				{
					return -1;
				}
			}
			llActiveFileSize = pFile->GetIndexFileSize();
		}
		else if (dw != ERROR_SUCCESS)
		{
			// Some other error occurred
			DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFast(%I64d) ... bailing, failed to read index [error %u]\n"),
				this, tStart, dw ));
			ASSERT(FALSE);
			return -1;
		}
		else
		{
			if (idx.tStart > tStart)
			{
				// We've found where we want to be!
				
				// Unfortunately, the next nav pack searched for might be this one as well.
				// So we need to roll back to the last index/nav pack

				if (m_llActIdxOffset >= sizeof(INDEX))
					m_llActIdxOffset -= sizeof(INDEX);
				else
				{
					// Drop back a file
					m_uiActiveFileNum--;
					if (m_uiActiveFileNum < 0)
					{
						ASSERT(FALSE);
						return -1;
					}

					pFile = m_vecFiles[m_uiActiveFileNum];
					if (pFile->UsedOptimizedOpen())
					{
						if (ERROR_SUCCESS != pFile->FullyOpen())
						{
							return -1;
						}
					}
					m_llActIdxOffset = pFile->GetIndexFileSize() - sizeof(INDEX);
					DFP((LOG_FILE_OTHER, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFast(%I64d) ... decremented file index [now %d]\n"),
							this, tStart, m_uiActiveFileNum ));
				}
				DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFast(%I64d) ... succeeded, returning %I64d\n"),
						this, tStart, m_idxLastNavPackFound.llStreamOffset ));
				return m_idxLastNavPackFound.llStreamOffset;
			}

			// Update our last index and advance our current location
			m_idxLastNavPackFound = idx;
			m_llActIdxOffset += sizeof(INDEX);
		}
	}	// end while(1)

	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFast(%I64d) ... error exit\n"), this, tStart ));
	return -1;
}

LONGLONG CRecordingSetRead::GetNavPackStreamOffsetFullSearch(LONGLONG tStart)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFullSearch(%I64d) ... entry\n"), this, tStart ));
	LONGLONG tSearch = tStart;// - UNITS/2*240;	// tSearch = tStart - 120sec

	// Reset our active file num.
	m_uiActiveFileNum = -1;

	// Start at the newest file and work our way back until we find the file
	// that contains tStart.
	for (size_t i = m_vecFiles.size() - 1; (i+1) >= 1; i--)
	{
		CRecordingFile *pFile = m_vecFiles[i];
		if (pFile->UsedOptimizedOpen())
		{
			if (ERROR_SUCCESS != pFile->FullyOpen())
			{
				return -1;
			}
		}
		FILE_HEADER header = pFile->GetHeader();
		if (header.tTrueStartTime <= tSearch)
		{
			// Update the cached index file
			CacheIndexFile(i);

			// We've now found the file we want to start in.
			// Now search this file for the nav pack <= tSearch

			// todo  - change from binary to interpolating search

			// Get our initial search position and read the index
			DWORD dwMin = 0;
			DWORD dwMax = (DWORD) ((pFile->GetIndexFileSize() - 1) / sizeof(INDEX));
			DWORD dwSearch = (dwMax + dwMin) / 2;
			if (pFile->ReadIndexAtFileOffset(dwSearch * sizeof(INDEX), m_idxLastNavPackFound))
			{
				DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFullSearch(%I64d) ... error exit, failed index read[1]\n"), this, tStart ));
				return -1;
			}

			// Loop until we've hit the end of our search
			while (dwMin != dwMax)
			{
				if (m_idxLastNavPackFound.tStart > tSearch)
				{
					// Handle an edge case that can occur
					if (dwMax == dwSearch)
						dwMax--;
					else
						dwMax = dwSearch;
				}
				else if (m_idxLastNavPackFound.tStart < tSearch)
				{
					// Handle an edge case than can occur
					if (dwMin == dwSearch)
						dwMin++;
					else
						dwMin = dwSearch;
				}
				else
					break;	// Found it!

				// Update our search and read the new index
				dwSearch = (dwMax + dwMin) / 2;
				if (pFile->ReadIndexAtFileOffset(dwSearch * sizeof(INDEX), m_idxLastNavPackFound))
				{
					DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFullSearch(%I64d) ... error exit, failed index read[2]\n"), this, tStart ));
					return -1;
				}
			}

			// We might have to back up 1 position
			// That is.  Suppose we have 3 indexes with times 0, 15, and 30 and we
			// search for 18.  The search will return 30.  We actually want to err
			// on the low side.

			if (m_idxLastNavPackFound.tStart > tSearch)
			{
				if (dwSearch == 0)
				{
					// We can't back up.  This means that tSearch is out of bounds.
					DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFullSearch(%I64d) ... error exit, tSearch out-of-bounds\n"), this, tStart ));
					ASSERT(0);
					return -1;
				}

				dwSearch--;
				if (pFile->ReadIndexAtFileOffset(dwSearch * sizeof(INDEX), m_idxLastNavPackFound))
				{
					DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFullSearch(%I64d) ... error exit, failed index read[3]\n"), this, tStart ));
					return -1;
				}

				ASSERT(m_idxLastNavPackFound.tStart < tSearch);
			}
			// If we're off the end of the last file, then return -1
			if ((i == m_vecFiles.size() - 1)&&
				(m_idxLastNavPackFound.tStart < tSearch) &&
				(dwSearch == (DWORD) ((pFile->GetIndexFileSize() - 1) / sizeof(INDEX))))
			{
				DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFullSearch(%I64d) ... error exit, beyond end of last file\n"), this, tStart ));
				return -1;
			}

			// Found it!
			m_uiActiveFileNum = i;
			if (m_uiActiveFileNum != i)
			{
				DbgLog((LOG_FILE_OTHER, 1, _T("### DVR Source Filter:  switching to a different disk file [5] ###\n")));
			}
			m_llActIdxOffset = dwSearch * sizeof(INDEX);
			DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFullSearch(%I64d) ... sucess exit, file %d, index file pos %d, stream pos %I64d\n"),
						this, tStart, i, m_llActIdxOffset, m_idxLastNavPackFound.llStreamOffset ));

			return m_idxLastNavPackFound.llStreamOffset;
		}
	}
	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackStreamOffsetFullSearch(%I64d) ... error exit\n"), this, tStart ));
	return -1;
}

LONGLONG CRecordingSetRead::GetNavPackMediaTimeFullSearch(ULONGLONG llOffset)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackMediaTimeFullSearch(%I64d) ... entry\n"), this, llOffset ));
	ULONGLONG llSearch = llOffset;// - UNITS/2*240;	// tSearch = tStart - 120sec

	// Reset our active file num.
	m_uiActiveFileNum = -1;

	// Start at the newest file and work our way back until we find the file
	// that contains tStart.
	size_t i = m_vecFiles.size() - 1;
	for(	vector<CRecordingFile *>::reverse_iterator it = m_vecFiles.rbegin();
			it != m_vecFiles.rend();
			it++, i--)
	{
		CRecordingFile *pFile = *it;
		if (pFile->UsedOptimizedOpen())
		{
			if (ERROR_SUCCESS != pFile->FullyOpen())
			{
				return -1;
			}
		}
        INDEX idxFileSearch;
		if (pFile->ReadIndexAtFileOffset(0, idxFileSearch))
		{
			DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackMediaTimeFullSearch(%I64d) ... error exit, failed index read[1]\n"), this, llOffset ));
			return -1;
		}
		if (idxFileSearch.llStreamOffset <= llSearch)
		{
			// Update the cached index file
			CacheIndexFile(i);

			// We've now found the file we want to start in.
			// Now search this file for the nav pack <= tSearch

			// todo  - change from binary to interpolating search

			// Get our initial search position and read the index
			DWORD dwMin = 0;
			DWORD dwMax = (DWORD) ((pFile->GetIndexFileSize() - 1) / sizeof(INDEX));
			DWORD dwSearch = (dwMax + dwMin) / 2;
			if (pFile->ReadIndexAtFileOffset(dwSearch * sizeof(INDEX), m_idxLastNavPackFound))
			{
				DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackMediaTimeFullSearch(%I64d) ... error exit, failed index read[1]\n"), this, llOffset ));
				return -1;
			}

			// Loop until we've hit the end of our search
			while (dwMin != dwMax)
			{
				if (m_idxLastNavPackFound.llStreamOffset > llSearch)
				{
					// Handle an edge case that can occur
					if (dwMax == dwSearch)
						dwMax--;
					else
						dwMax = dwSearch;
				}
				else if (m_idxLastNavPackFound.llStreamOffset < llSearch)
				{
					// Handle an edge case than can occur
					if (dwMin == dwSearch)
						dwMin++;
					else
						dwMin = dwSearch;
				}
				else
					break;	// Found it!

				// Update our search and read the new index
				dwSearch = (dwMax + dwMin) / 2;
				if (pFile->ReadIndexAtFileOffset(dwSearch * sizeof(INDEX), m_idxLastNavPackFound))
				{
					DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackMediaTimeFullSearch(%I64d) ... error exit, failed index read[2]\n"), this, llOffset ));
					return -1;
				}
			}

			// We might have to back up 1 position
			// That is.  Suppose we have 3 indexes with times 0, 15, and 30 and we
			// search for 18.  The search will return 30.  We actually want to err
			// on the low side.

			if (m_idxLastNavPackFound.llStreamOffset > llSearch)
			{
				if (dwSearch == 0)
				{
					// We can't back up.  This means that tSearch is out of bounds.
					DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackMediaTimeFullSearch(%I64d) ... error exit, tSearch out-of-bounds\n"), this, llOffset ));
					ASSERT(0);
					return -1;
				}

				dwSearch--;
				if (pFile->ReadIndexAtFileOffset(dwSearch * sizeof(INDEX), m_idxLastNavPackFound))
				{
					DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackMediaTimeFullSearch(%I64d) ... error exit, failed index read[3]\n"), this, llOffset ));
					return -1;
				}

				ASSERT(m_idxLastNavPackFound.llStreamOffset < llSearch);
			}
			// If we're off the end of the last file, then return -1
			if ((i == m_vecFiles.size() - 1)&&
				(m_idxLastNavPackFound.llStreamOffset < llSearch) &&
				(dwSearch == (DWORD) ((pFile->GetIndexFileSize() - 1) / sizeof(INDEX))))
			{
				DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackMediaTimeFullSearch(%I64d) ... error exit, beyond end of last file\n"), this, llOffset ));
				return -1;
			}

			// Found it!
			m_uiActiveFileNum = i;
			if (m_uiActiveFileNum != i)
			{
				DbgLog((LOG_FILE_OTHER, 1, _T("### DVR Source Filter:  switching to a different disk file [5] ###\n")));
			}
			m_llActIdxOffset = dwSearch * sizeof(INDEX);
			DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackMediaTimeFullSearch(%I64d) ... sucess exit, file %d, index file pos %d, stream pos %I64d\n"),
						this, llOffset, i, m_llActIdxOffset, m_idxLastNavPackFound.llStreamOffset ));
			return m_idxLastNavPackFound.tStart;
		}
	}
	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::GetNavPackMediaTimeFullSearch(%I64d) ... error exit\n"), this, llOffset ));
	return -1;
}

DWORD CRecordingSetRead::ScanToIndexedSampleHeader(bool fForward)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ScanToIndexedSampleHeader(%s) ... entry\n"), this, fForward ? _T("forward") : _T("backward") ));

	// Get the current active file
	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ScanToIndexedSampleHeader(%s) ... initially using file %d\n"),
			this, fForward ? _T("forward") : _T("backward"), m_uiActiveFileNum ));
	CRecordingFile *pFile = m_vecFiles[m_uiActiveFileNum];
	if (pFile->UsedOptimizedOpen())
	{
		_CDW(pFile->FullyOpen());
	}

	SAMPLE_HEADER sh;

	do
	{
		DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ScanToIndexedSampleHeader(%s) ... reading sample header\n"),
				this, fForward ? _T("forward") : _T("backward")  ));
		// Read the current sample header
		DWORD dw = pFile->ReadSampleHeader(sh, false);

		// If we hit EOF, then advance to the next file
		if ((dw == ERROR_HANDLE_EOF) && ((m_uiActiveFileNum + 1) < m_vecFiles.size()))
		{
			pFile = m_vecFiles[++m_uiActiveFileNum];
			if (pFile->UsedOptimizedOpen())
			{
				_CDW(pFile->FullyOpen());
			}
			dw = pFile->ReadSampleHeader(sh, false);
		}

		// See if we can't scan forward any more
		if (dw == ERROR_HANDLE_EOF)
		{
			m_llActIdxOffset = pFile->GetIndexFileSize();
			DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ScanToIndexedSampleHeader(%s) ... eof exit\n"),
					this, fForward ? _T("forward") : _T("backward")  ));
			return S_OK;
		}

		// Check for any other errors
		_CDW(dw);

		DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ScanToIndexedSampleHeader(%s) ... sample %s: media times [%I64d, %I64d], size %u, stream offset %I64d\n"),
				this, fForward ? _T("forward") : _T("backward"),
				sh.fSyncPoint ? _T(" sync") : _T(""), sh.tStart, sh.tEnd, sh.dwDataLen, sh.llStreamOffset));

		// Advance if we didn't hit the sync point or are not past the scan boundary
		if (!sh.fSyncPoint)
		{
			// Jump to the next sample header
			if (fForward)
			{
				DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ScanToIndexedSampleHeader(%s) ... skipping over this sample's data\n"),
						this, fForward ? _T("forward") : _T("backward") ));
				_CDW(pFile->SetFilePos(pFile->GetFilePos() + sh.dwDataLen))
			}
			else
			{
				// See if we can't scan backwards any more
				if (sh.dwLastSampleDataLen == 0)
				{
					m_llActIdxOffset = 0;
					DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ScanToIndexedSampleHeader(%s) ... exit, beginning of file\n"),
						this, fForward ? _T("forward") : _T("backward") ));
					return S_OK;
				}

				DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ScanToIndexedSampleHeader(%s) ... skipping to previous sample (%u bytes buffer data)\n"),
						this, fForward ? _T("forward") : _T("backward"), sh.dwLastSampleDataLen ));

				_CDW(pFile->SetFilePos(	pFile->GetFilePos()
										- sh.dwLastSampleDataLen
										- 2 * sizeof(SAMPLE_HEADER)));
			}
		}
	} while (!sh.fSyncPoint);

	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ScanToIndexedSampleHeader(%s) ... selected sample at %I64d, will seek\n"),
						this, fForward ? _T("forward") : _T("backward"), sh.tStart ));

	// We've found the next keyframe, it's in sh.
	// So jump everything there.
	_CDW(pFile->SeekToSampleTime(sh.tStart, false, false));

	// Update our active index offset
	m_llActIdxOffset = pFile->GetIndexFileSize() - pFile->GetRemainingIndexFileSize();

	// (Sorry, I know this is confusing).  Now correct the active index offset
	// 1 position depending on which way we're reading.
	if (fForward)
		m_llActIdxOffset -= sizeof(INDEX);
	else
		m_llActIdxOffset += sizeof(INDEX);

	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ScanToIndexedSampleHeader(%s) ... success exit\n"),
						this, fForward ? _T("forward") : _T("backward") ));
	return ERROR_SUCCESS;
}


DWORD CRecordingSetRead::ReadSampleHeaderHyperFast(	SAMPLE_HEADER &rHeader,
													bool fCheckForNewFiles,
													LONGLONG llSkipBoundary,
													bool fForward)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ReadSampleHeaderHyperFast(%s, %I64d, %s) ... entry\n"),
		this, fCheckForNewFiles ? _T("check-for-new-files") : _T("don't-check-for-new-files"),
		llSkipBoundary, fForward ? _T("forward") : _T("backward") ));

	// I know, cheesy to use infinite loop...less duplicate code though.
	while (1)
	{
		// Read the next sample header
		_CDW(ReadSampleHeader(	rHeader,
								fCheckForNewFiles,
								false,				// Reset the file pos?
								true,				// Skip to an index?
								fForward));

		// See if we crossed llSkipBoundary
		if ((fForward && (rHeader.tStart >= llSkipBoundary)) ||
			(!fForward && (rHeader.tStart <= llSkipBoundary)))
		{
			DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ReadSampleHeaderHyperFast() ... success exit [1]\n"),
					this ));
			return ERROR_SUCCESS;
		}

		// Skip over the sample data to put the file pos at the next sample header
		CRecordingFile *pFile = m_vecFiles[m_uiActiveFileNum];
		ASSERT(pFile);
		if (pFile->UsedOptimizedOpen())
		{
			_CDW(pFile->FullyOpen());
		}
		_CDW(pFile->SetFilePos(pFile->GetFilePos() + rHeader.dwDataLen));
	}

	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ReadSampleHeaderHyperFast() ... success exit [2]\n"), this ));
	return ERROR_SUCCESS;
}

DWORD CRecordingSetRead::ReadSampleHeader(	SAMPLE_HEADER &rHeader,
											bool fCheckForNewFiles,
											bool fResetFilePos,
											bool fSkipToIndex,
											bool fForward)
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader(%s, %s, %s, %s) ... entry\n"),
		this, fCheckForNewFiles ? _T("check-for-new-files") : _T("don't-check-for-new-files"),
		fResetFilePos ? _T("reset-file-pos") : _T("don't-reset-file-pos"),
		fSkipToIndex ? _T("skip-to-index") : _T("don't-skip-to-index"),
		fForward ? _T("forward") : _T("backward") ));

	// We don't support regular backwards play
	if (!fSkipToIndex && !fForward)
	{
		DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... error exit, regular backward play\n"), this ));
		_CDW(ERROR_INVALID_PARAMETER);
	}

	// Make sure that we have a non-empty recording
	if (!IsOpen() || (m_uiActiveFileNum == -1))
	{
		DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... error exit, empty recording\n"), this ));
		_CDW(ERROR_INVALID_STATE);
	}

	if (fSkipToIndex)
	{
		// If we are just getting into this frame skipping mode
		if (m_llActIdxOffset == -1)
		{
			// Jump to the next keyframe sample header.
			// m_llActIdxOffset is updated as a side effect.
			DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... index offset unknown, will scan\n"), this ));
			_CDW(ScanToIndexedSampleHeader(fForward));
		}
		else
		{
			DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... using index offset %I64d\n"), this, m_llActIdxOffset ));
		}
	}
	else
	{
		// If we had an active index offset, it is now hosed.
		m_llActIdxOffset = -1;
	}

	// Get the current active file
	CRecordingFile *pFile = m_vecFiles[m_uiActiveFileNum];
	if (pFile->UsedOptimizedOpen())
	{
		_CDW(pFile->FullyOpen());
	}

	// At this point, if we are skipping to an index then we must have a
	// valid active index offset.
	ASSERT(!fSkipToIndex || (m_llActIdxOffset >= 0));

	// If we're going forward, we might need to advance to the next file
	if (fForward)
	{
		// If we're at the end of the active file, move to the next one
		LONGLONG llFileRemaining = fSkipToIndex ?
				pFile->GetIndexFileSize() - m_llActIdxOffset - (sizeof(INDEX)-1)
				: pFile->GetRemainingFileSize();
		if (llFileRemaining <= 0)
		{
			// Attempt to advance to the next file

			DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... attempting to advance to the next file (from %d)\n"), this, m_uiActiveFileNum ));

			// See if we're at the last file
			if (m_uiActiveFileNum >= (m_vecFiles.size() - 1))
			{
				// Again, see if we're at the last file
				if (m_uiActiveFileNum >= (m_vecFiles.size() - 1))
				{
					DFP((LOG_EVENT_DETECTED, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... eof exit\n"), this ));
					return ERROR_HANDLE_EOF;
				}
			}

			// Grab the next file in the list and start reading from its beginning
			pFile = m_vecFiles[++m_uiActiveFileNum];
			if (pFile->UsedOptimizedOpen())
			{
				_CDW(pFile->FullyOpen());
			}
			DbgLog((LOG_FILE_OTHER, 1, _T("### DVR Source Filter:  switching to a different disk file [6] ###\n")));
			// TODO:  please check the 3 lines below to see if this is
			//                the right way to ensure that the next file chunk
			//                is positioned to read a tht right place and not
			//                wherever it was left after a potential previous
			//                partial or full playback of the chunk.  Also, I didn't
			//                know if there were other places in the code that might
			//                have a similar bug.
			pFile->SetFilePos(sizeof(FILE_HEADER));
			_CDW(pFile->ReadCustom(NULL, m_dwCustomDataLen));
			_CDW(pFile->ReadMediaTypes(m_rgMediaTypes, m_dwNumStreams, true));

			// Update our active index offset
			if (fSkipToIndex)
				m_llActIdxOffset = 0;

			DFP((LOG_FILE_OTHER, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... now in next file\n"), this ));
		}
	}
	else
	{
		// We're playing back just keyframes in reverse.  The offset points at the
		// index AFTER the one we just read.  So the next index we want to read is
		// actually 2 indices before the one we're pointing at.

		if (m_llActIdxOffset < (2 * sizeof(INDEX)))
		{
			DFP((LOG_FILE_OTHER, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... moving to previous file [now %d]\n"), this, m_uiActiveFileNum ));

			// We need to move to a previous file
			if (m_uiActiveFileNum == 0)
			{
				DFP((LOG_EVENT_DETECTED, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... eof exit\n"), this ));
				return ERROR_HANDLE_EOF;
			}

			// Grab the previous file in the list
			pFile = m_vecFiles[--m_uiActiveFileNum];
			if (pFile->UsedOptimizedOpen())
			{
				_CDW(pFile->FullyOpen());
			}
			DbgLog((LOG_FILE_OTHER, 1, _T("### DVR Source Filter:  switching to a different disk file [7] ###\n")));

			// Update the active index position.  Yes this extends beyond the
			// bounds of the index file but it is consistent with our definition
			// of m_llActIdxOffset - in that m_llActIdxOffset points to the index
			// immediatly in the file after the one we just read.
			m_llActIdxOffset = pFile->GetIndexFileSize() + sizeof(INDEX);

			if ((m_llActIdxOffset % sizeof(INDEX)) != 0)
			{
				// We have a corrupted file.
				ASSERT(FALSE);
				DFP((LOG_ERROR, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... corrupt eof exit\n"), this ));
				return ERROR_HANDLE_EOF;
			}
		}

		DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... prior index offset %I64d\n"), this, m_llActIdxOffset ));

		// Now do our check again since we might have moved to an earlier file
		if (m_llActIdxOffset < (2 * sizeof(INDEX)))
		{
			DFP((LOG_EVENT_DETECTED, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... eof exit\n"), this ));
			return ERROR_HANDLE_EOF;
		}

		// Advance m_llActIdxOffset to point to the index of the next sample that
		// should be delivered.
		m_llActIdxOffset -= 2 * sizeof(INDEX);
	}

	// At this point...
	// If fSkipToIndex is true, then m_llActIdxOffset contains the location in the
	// index file of the index of the next sample we want to read else pFile is
	// already seeked to the location of the next sample.

	if (fSkipToIndex)
	{
		DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... reading from index offset %I64d\n"),
			this, m_llActIdxOffset ));
		ASSERT(m_llActIdxOffset != -1);

		// Update the cached index file
		CacheIndexFile(m_uiActiveFileNum);

		// Read the index
		INDEX idx;
		_CDW(pFile->ReadIndexAtFileOffset(m_llActIdxOffset, idx));

		// Advance the active index if we don't want a reset
		if (!fResetFilePos)
			m_llActIdxOffset += sizeof(INDEX);
		else
		{
			// Reset the index offset if we're playing backwards.
			// (If we're playing forwards m_llActIdxOffset is already
			// at the right spot.)
			if (!fForward)
				m_llActIdxOffset += 2 * sizeof(INDEX);
		}

		DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ReadSampleHeader() ... read from file pos %I64d, new index offset %I64d\n"),
			this, idx.offset, m_llActIdxOffset ));

		// Advance pFile to the right offset
		_CDW(pFile->SetFilePos(idx.offset));
	}

	return pFile->ReadSampleHeader(rHeader, fResetFilePos);
}

DWORD CRecordingSetRead::ResetSampleHeaderRead()
{
	DFP((LOG_FILE_IO, 5, _T("CRecordingSetRead[%p]::ResetSampleHeaderRead()\n"), this ));

	// Make sure that we have a non-empty recording
	if (!IsOpen() || (m_uiActiveFileNum == -1))
	{
		_CDW(ERROR_INVALID_STATE);
	}

	CRecordingFile *pFile = m_vecFiles[m_uiActiveFileNum];
	ASSERT(pFile);
	if (pFile->UsedOptimizedOpen())
	{
		_CDW(pFile->FullyOpen());
	}

	// Seek backwards exactly sizeof(SAMPLE_HEADER)
	_CDW(pFile->SetFilePos(pFile->GetFilePos() - sizeof(SAMPLE_HEADER)));

	return ERROR_SUCCESS;
}

DWORD CRecordingSetRead::CacheIndexFile(size_t iFileNum)
{
	// See if we already have the file entry
	for (ULONG idxSearch = 0; idxSearch < INDEX_CACHE_COUNT; idxSearch++)
	{
		// If we already have the correct file cached, great!
		if (iFileNum == m_rgidxCache[idxSearch].uiCachedIdxFile)
		{
			m_rgidxCache[idxSearch].dwLastAccessed = GetTickCount();
			return ERROR_SUCCESS;
		}
	}

	// Make sure we're in bounds
	if ((iFileNum < 0) || (iFileNum >= m_vecFiles.size()))
		_CDW(ERROR_INVALID_PARAMETER);

	// Figure out which entry to throw out
	ULONG idxEntry = 0;
	for (ULONG idxSearch = 0; idxSearch < INDEX_CACHE_COUNT; idxSearch++)
	{
		if (m_rgidxCache[idxSearch].uiCachedIdxFile == -1)
		{
			idxEntry = idxSearch;
			break;
		}
		else if ((m_rgidxCache[idxEntry].dwLastAccessed - m_rgidxCache[idxSearch].dwLastAccessed) > 0)
		{
			idxEntry = idxSearch;
		}
	}

	// Release the old cached file
	if (m_rgidxCache[idxEntry].uiCachedIdxFile != -1)
	{
		CRecordingFile *pOldFile = m_vecFiles[m_rgidxCache[idxEntry].uiCachedIdxFile];
		pOldFile->ReleaseIndexFileCache();
		m_rgidxCache[idxEntry].uiCachedIdxFile = -1;
	}

	// Cache the new file
	CRecordingFile *pFile = m_vecFiles[iFileNum];
	if (pFile->UsedOptimizedOpen())
	{
		_CDW(pFile->FullyOpen());
	}
	_CDW(pFile->CacheIndexFile(m_rgidxCacheData[idxEntry]));

	// Store the new cached file number and get out of dodge
	m_rgidxCache[idxEntry].uiCachedIdxFile = iFileNum;
	m_rgidxCache[idxEntry].dwLastAccessed = GetTickCount();

	return ERROR_SUCCESS;
}

void CRecordingSetRead::MarkSeekPosition()
{
	m_markedSeekPos.uiActiveFileNum = m_uiActiveFileNum;
	m_markedSeekPos.llActIdxOffset = m_llActIdxOffset;
}

void CRecordingSetRead::RestoreSeekPostion()
{
	m_uiActiveFileNum = m_markedSeekPos.uiActiveFileNum;
	m_llActIdxOffset = m_markedSeekPos.llActIdxOffset;
}

#ifdef DEBUG

void CRecordingSetRead::Dump()
{
	DbgLog((LOG_PAUSE_BUFFER, 3, TEXT("## READER'S %s: %s ##\n"),
			m_fDontOpenTempFiles ? TEXT("RECORDING") : TEXT("PAUSE BUFFER"),
			m_strFilename.c_str() ));

	if (!IsOpen())
	{
		DbgLog((LOG_PAUSE_BUFFER, 3, TEXT("-- not open --\n") ));
		return;
	}
	
	size_t idx = 0;
	for (std::vector<CRecordingFile *>::iterator it = m_vecFiles.begin();
			it != m_vecFiles.end();
			it++)
	{
		CRecordingFile *pFile = *it;

		DbgLog((LOG_PAUSE_BUFFER, 3, TEXT("  %s:  %s%s%s [%I64d (%I64d), %I64d] %s\n"),
			pFile->GetFileName().c_str(),
			pFile->UsedOptimizedOpen() ? TEXT("Optimized-Open") : TEXT("Fully-Open"),
			!pFile->UsedOptimizedOpen() ?
				(pFile->GetHeader().fTemporary ? TEXT(" Temporary") : TEXT(" Permanent")) : TEXT(""),
			!pFile->UsedOptimizedOpen() ?
				(pFile->GetHeader().fFileInProgress ? TEXT(" File-In-Progress") :
					(pFile->GetHeader().fInProgress ? TEXT(" Recording-In-Progress") : TEXT(" Recording-Complete"))) : TEXT(""),
			!pFile->UsedOptimizedOpen() ?
				pFile->GetHeader().tTrueStartTime :-1LL,
			!pFile->UsedOptimizedOpen() ?
				pFile->GetHeader().tVirtualStartTime : -1LL,
			!pFile->UsedOptimizedOpen() ?
				pFile->GetHeader().tMaxSeekPos : -1LL,
			(idx == m_uiActiveFileNum) ? TEXT("<<< CURRENT FILE") : TEXT("") ));
		++idx;

	}
} // CRecordingSetRead::Dump

#endif // DEBUG


} // namespace MSDvr

#ifdef MONITOR_WRITE_FILE

static long g_lInWriteCount = 0;
static long g_lWriteID = 0;

static BOOL WRITEFILE(HANDLE hFile, PCVOID pBuf, DWORD dwBytesIn, PDWORD pdwBytesWritten, OVERLAPPED *unused)
{
	InterlockedIncrement(&g_lInWriteCount);
	InterlockedIncrement(&g_lWriteID);
	BOOL fResult = WriteFile(hFile,pBuf,dwBytesIn,pdwBytesWritten,unused);
	InterlockedDecrement(&g_lInWriteCount);
	return fResult;
}

static HANDLE g_hThreadFileWriteMonitor = 0;
static bool g_fMonitorThreadWrites = false;
static bool g_fBreakOnLongWrite = true;

#define THRESHOLD_IN_100_MS		10

static DWORD WINAPI MonitorFileWrite(void *p)
{
	long lLastWriteID = g_lWriteID;
	DWORD dwCountdown = THRESHOLD_IN_100_MS;
	while (g_fMonitorThreadWrites)
	{
		if (g_lInWriteCount > 0)
		{
			if (g_lWriteID != lLastWriteID)
			{
				dwCountdown = THRESHOLD_IN_100_MS;
				lLastWriteID = g_lWriteID;
				g_fBreakOnLongWrite = true;
			}
			else if (dwCountdown == 0)
			{
				if (g_fBreakOnLongWrite)
				{
//					g_fBreakOnLongWrite = false;
					OutputDebugString(TEXT("### DISK BLOCKAGE IN PROGRESS ###\n"));
					dwCountdown = 0x7fffffff;
					try {
						DebugBreak();
					} catch (...) {};

					OutputDebugString(TEXT("### DISK BLOCKAGE IN PROGRESS ###\n"));
					OutputDebugString(TEXT("### DISK BLOCKAGE IN PROGRESS ###\n"));
					OutputDebugString(TEXT("### DISK BLOCKAGE IN PROGRESS ###\n"));
					OutputDebugString(TEXT("### DISK BLOCKAGE IN PROGRESS ###\n"));
					OutputDebugString(TEXT("### DISK BLOCKAGE IN PROGRESS ###\n"));
					OutputDebugString(TEXT("### DISK BLOCKAGE IN PROGRESS ###\n"));
				}
			}
			else
			{
				--dwCountdown;
			}
		}
		Sleep(100);
	}
	return 0;
}

void BeginFileWriteMonitoring()
{
	if (!g_hThreadFileWriteMonitor)
	{
		DWORD dwThreadID;

		g_fMonitorThreadWrites = true;
		g_hThreadFileWriteMonitor = CreateThread(	NULL,
								0,
								MonitorFileWrite,
								NULL,
								0,
								&dwThreadID);
		if (g_hThreadFileWriteMonitor)
		{
			CeSetThreadPriority(g_hThreadFileWriteMonitor, /* 249 ok */ 50 /* 252 too low */);
			OutputDebugString(TEXT("BeginFileWriteMonitoring():  created thread\n"));
		}
		else
		{
			OutputDebugString(TEXT("BeginFileWriteMonitoring():  unable to create the thread\n"));
			g_fMonitorThreadWrites = false;
		}
	}
}

void EndFileWriteMonitoring()
{
	if (g_hThreadFileWriteMonitor)
	{
		OutputDebugString(TEXT("EndFileWriteMonitoring():  signalling thread ...\n"));

		g_fMonitorThreadWrites = false;
		WaitForSingleObject(g_hThreadFileWriteMonitor, INFINITE);
		CloseHandle(g_hThreadFileWriteMonitor);
		g_hThreadFileWriteMonitor = 0;

		OutputDebugString(TEXT("EndFileWriteMonitoring():  thread dead\n"));
	}
}

#endif // MONITOR_WRITE_FILE

