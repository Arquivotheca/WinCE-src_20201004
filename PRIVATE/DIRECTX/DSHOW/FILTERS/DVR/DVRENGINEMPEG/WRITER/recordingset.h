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

// #define DEBUG_FILE_POSITION	1

namespace MSDvr
{

static const WCHAR *INDEX_FILE_SUFFIX = L".dvr-idx";
static const WCHAR *DATA_FILE_SUFFIX = L".dvr-dat";
static const DWORD INDEX_CACHE_SIZE = (128 * 1024);
static const DWORD INDEX_CACHE_COUNT = 2;

#define _CDW(p)    {DWORD _d=(p);if(_d)return _d;}
#define _CHR(p)    {HRESULT _h=(p);if (FAILED(_h)) return _h;}
#define _CGLE(p)   {BOOL  _b=(p)==0;if(_b)return GetLastError();}
#define _CMEM(p)   {BOOL  _b=(p)==0;if(_b)return ERROR_NOT_ENOUGH_MEMORY;}
#define _CP(p)     {BOOL  _b=(p)==0;if(_b)return ERROR_INVALID_PARAMETER;}

#pragma pack(push, 1)
struct SAMPLE_HEADER
{
	LONGLONG tStart;			// @field Start media time of the sample
	LONGLONG tEnd;				// @field End media time of the sample
	LONGLONG tPTStart;			// @field Start presentation time of the sample
	LONGLONG tPTEnd;			// @field End presentation time of the sample
	DWORD fSyncPoint;			// @field Is this sample a sync point?
	DWORD dwStreamNum;			// @field Stream number this sample is on
	DWORD dwDataLen;			// @field Size of sample data.
	DWORD dwLastSampleDataLen;	// @field Size of the previous sample's data
								// or 0 if there was no previous sample.
	LONGLONG llStreamOffset;	// @field Treating the sample data ON ALL STREAMS
								// like a linear byte stream, this is the offset.
								// So use this with care when multiple streams
								// exist.
    DWORD fDiscontinuity;       // @field Is this sample a discontinuity?
	BYTE reserved[452];			// @field Makes the struct exactly 512 bytes.
};

struct INDEX
{
	LONGLONG tStart;					// Media time of sample
	LONGLONG offset;					// Seek pos of sample in file.
	LONGLONG llStreamOffset;			// Treating the sample data ON ALL STREAMS like a
										// linear byte stream, this is the offset.  So use
										// this with care when multiple streams exist.
};

#define FILE_HEADER_RESERVED_SIZE		4048
		// amount of space reserved for future use
#define FILE_HEADER_RESERVED_SIZE_UNUSED_SECTORS	3584
		// subset of the reserved space that is a multiple of 512 byte sectors

struct FILE_HEADER
{
	DWORD dwVersion;
	DWORD dwNumStreams;
	DWORD dwMaxSampleSize;
	DWORD dwNegotiatedBufferSize;
	DWORD dwCustomDataSize;
	LONGLONG tMaxSeekPos;		// Max seek position of this recording file.
	LONGLONG tVirtualStartTime;	// The true start time for this file
								// is the time stored in the first index.
								// We can't front truncate a recording file
								// so this value lets us logically front truncate
								// for converting temporary recordings to perm.
								// A value of -1 signifies that it isn't set.
	LONGLONG tTrueStartTime;	// The true start time for this file regardless of
								// whether the recording is temporary or permanent.
								// A value of -1 signifies that it isn't set.
	bool fTemporary;			// True iff this is part of a temporary recording.
	bool fInProgress;			// True iff media samples are still being written to this file
								// or some other file in this recording.
	bool fPaddedFile;			// Applicable only to version 1.02 or later -- true iff this
								// file contains padding to typical maximum size
	bool fFileInProgress;		// True iff media samples are still being written to this file.
	char szReserved[FILE_HEADER_RESERVED_SIZE];
								// This is here for forward compatability of the
								// file format.  That is, if in a future version
								// we need to add additional fields to the header,
								// we can add those fields in this reserved section.
								// Then old recordings can be 'upgraded' to the new
								// format without having to move all that recording
								// data.  Makes the structure exactly 4096 bytes.
};

struct CUSTOM_DATA_HEADER
{
	DWORD dwVersion;
	DWORD dwSize;
	BYTE reserved[8];			// Makes the structure exactly 16 bytes.
};

struct MEDIA_TYPE_HEADER
{
	DWORD dwVersion;
	DWORD dwSize;
	BYTE reserved[8];			// Makes the structure exactly 16 bytes.
};

#pragma pack(pop)

// Class that handles basic file I/O.  It exposes virtual methods for reading
// and writing more advanced objects such as IMediaSamples, INDEXes, file
// headers, etc.  Classes that want to implement a particular file format
// should derive from this one.
class CBinFile
{
public:
	CBinFile();
	CBinFile(HANDLE hFile);
	~CBinFile();

	// Opens a file for reading and writing.  If fOpenForRead is true, then the file
	// is opened OPEN_EXISTING otherwise is is opened CREATE_NEW.
	DWORD Open(const WCHAR *wszFileName, bool fOpenForRead);

	// Close an open file.
	void Close();

	// Is the file currently open?
	bool IsOpen() {return m_hFile != INVALID_HANDLE_VALUE;}

	// Seek the file pointer to an offset in the file.  The file pointer is updated
	// lazily for perf reasons.
	DWORD SetFilePos(ULONGLONG pos);

	// Retrieves the current seek position or -1 on error.
	ULONGLONG GetFilePos();

	// Retrieves the open file size of -1 on error.
	ULONGLONG GetFileSize();

	// Write a byte array at the current position.
	DWORD Write(BYTE *pBuf, DWORD dwBytes);

	// Read from the current position.  If the number of bytes specified aren't
	// available, then the function returns an error.
	DWORD Read(BYTE *pBuf, DWORD dwBytes);

	// @cmember Release the file handle associated with this CBinFile
	HANDLE ReleaseHandle();

	// @cmember Returns whether the file was opened for read or write.
	// The return value of this function is undefined if the file is not open.
	bool IsOpenedForRead() { return m_fOpenedForRead; }

	DWORD TruncateFile(ULONGLONG uTruncationPosition);

    DWORD Flush();

protected:

	// Truly seek the file pointer to an offset in the file.  This is used for
	// the lazy implementation of SetFilePos
	DWORD CommitLazyFilePos();

	CBinFile(const CBinFile *rhs);		// Not implemented

	HANDLE m_hFile;	// File handle
	bool m_fOpenedForRead;
	ULONGLONG m_ullActualFilePos;
	ULONGLONG m_ullDesiredFilePos;
};

// ################################################

// Class which represents a single logical recoring file.  Right now this class
// 'mates' a data file with an index file to represent 1 logical recording file.
class CRecordingFile
{
public:

	CRecordingFile();
	~CRecordingFile();

	DWORD CreateNew(const WCHAR *wszFileName,
					DWORD dwNegotiatedBufferSize,
					CMediaType rgMediaTypes[],
					DWORD dwNumStreams,
					BYTE rgbCustom[],
					DWORD dwNumCustomBytes,
					bool fTemporary,
					ULONGLONG uPaddedFileSize);
	BOOL PadMoreUpToSize(ULONGLONG uPaddedFileSize);		
		// TRUE means fully padded (or unable to pad more), FALSE means more padding can be added
		// without exceeding the maximum.  This routine is responsible for doing an amount of
		// padding that won't interfere a lot with other I/O needs if the caller is at a reasonable
		// priority and isn't pounding away too much on this call.

	DWORD Open(	const WCHAR *wszFileName,
				DWORD *pdwNumStreams,
				DWORD *pdwMaxSampleSize,
				DWORD *pdwCustomDataSize);
	DWORD OptimizedOpen( const WCHAR *wszFileName);
	DWORD FullyOpen(DWORD *pdwNumStreams = NULL,
				DWORD *pdwMaxSampleSize = NULL,
				DWORD *pdwCustomDataSize = NULL);

	void Close();
	ULONGLONG GetIndexFileSize();
	ULONGLONG GetIndexFilePos();
	ULONGLONG GetFileSize();
	ULONGLONG GetFilePos() {return m_dataFile.GetFilePos();}
	DWORD SetFilePos(ULONGLONG pos) {return m_dataFile.SetFilePos(pos);}
	ULONGLONG GetRemainingFileSize() { return GetFileSize() - GetFilePos(); }
	ULONGLONG GetRemainingIndexFileSize() { return GetIndexFileSize() - GetIndexFilePos(); }
	DWORD WriteSample(IMediaSample &rSample, DWORD dwStreamNum, LONGLONG llDataOffset, bool fCreateIndex);
	DWORD WriteCustom(BYTE *pbCustom, DWORD dwCustomLen);
	DWORD WriteMediaType(DWORD dwStreamNum, AM_MEDIA_TYPE *pmt);

	// @cmember Seeks to the beginning of the file, writes m_header to the file,
	// and then seeks back to the previous file position.  This allows the
	// header to be written to the fly with no apparent change in the file position.
	DWORD WriteHeader();

	// @cmember Seeks to the beginning of the file, reads m_header from the file,
	// and then seeks back to the previous file position.  This allows the
	// header to be read to the fly with no apparent change in the file position.
	DWORD ReadHeader();

	// @cmember We can't front truncate a file so this method lets us assign
	// a virtual start time of a recording file to provide the illusion of
	// front truncation.  This virtual start time will be used by the reader
	// when playing back permanently recorded shows.  Conversely, by setting
	// fTemporary to true we can convert a permanent recording file to
	// temporary which resets the virtual start time to -1;
	DWORD SetTempPermStatus(LONGLONG tVirtualStart,
							bool fTemporary);

	// @cmember Retrieve the sample header for the ensuing sample.  This function
	// must be called exactly once before calling ReadSample.  Iff fResetFilePos
	// is set, the the file pointer is reset back to it's previous position after
	// reading the sample header.  Iff fSkipToIndex is set, then the position
	// will advance either forwards or backwards to the next index position.
	DWORD ReadSampleHeader(	SAMPLE_HEADER &rHeader,
							bool fResetFilePos);

	// @cmember Read a media sample from the current position in the data file.
	// rHeader must contain the sample header info retrieved from the preceding
	// ReadSampleHeader call.
	DWORD ReadSample(SAMPLE_HEADER &rHeader, IMediaSample &rSample);

	// @cmember Read the sample data from the current position in the data file.
	// rHeader must contain the sample header info retrieved from the preceding
	// ReadSampleHeader call.
	DWORD ReadSampleData(SAMPLE_HEADER &rHeader, BYTE *pBuf, ULONG cbBuf, ULONG* pcbRead);

	// @cmember Reads the custom data from the current file location.  This function
	// MUST be called after Open but before ReadMediaTypes.  If pbCustom is NULL,
	// then the file pointer is advanced past the custom data to the media types
	// section.
	DWORD ReadCustom(BYTE *pbCustom, DWORD dwCustomLen);

	// @cmember Reads a specified number of media types from the current data
	// file location.  rgTypes must be an array already allocated by the caller
	// of this function.  If fVerifyOnly is 'false', then the media types are
	// read from the file and stored in rgTypes.  If fVerifyOnly is 'true', then
	// each media type in the file is compared to the corresponding media type in
	// rgTypes.  If any media types don't match up, then ERROR_INVALID_DATA is
	// returned.  This second mode of functionality is to make sure that multiple
	// files all belonging to the same recording set all have the same media types
	// stored within.  Dynamic format changes are not supported at this time.
	DWORD ReadMediaTypes(CMediaType *rgTypes, DWORD dwNumTypes, bool fVerifyOnly);

	// @cmember Retrieve the cached file header.  Modifying this header will not
	// update the header stored in the file.  You must call WriteHeader after
	// changing the header.
	FILE_HEADER &GetHeader() {if (m_fOptimizedOpen) { FullyOpen(); }; return m_header;}

	// @cmember Returns a reference to the recording's filename.
	std::wstring& GetFileName() { return m_strFilename; }

	// @cmember Retrieves the minimum starting sample time - that is, the earliest
	// sample to which an application may seek.  This value is taken from the first
	// index of the first file.  If the first file has a tVirtualStartTime other
	// than -1 in its header AND fIgnoreVirtualStartTime==false, then
	// tVirtualStartTime is returned rather than the time of the first index.
	// Iff the recording set is empty or an error occurs, -1 is returned.
	LONGLONG GetMinSampleTime(bool fIgnoreVirtualStartTime);

	// @cmember Retrieves the maximum sample time - that is, the last sample to
	// which an application may seek.  This value is taken from the last index of
	// the last recording file.
	// Iff the recording set is empty or an error occurs, -1 is returned.
	LONGLONG GetMaxSampleTime();

	// @cmember Seek in the data file to the specified sample time.  The correct
	// seek point is found by searching through the index file.  If fAllowEOF
	// is set to true, and tStart is beyond the last keyframe, and fSeekToKeyframe
	// is true then the seek position in the data file will set to EOF.  If
	// fAllowEOF is false, tStart is beyond the last keyframe, and fSeekToKeyfram
	// is true then the seek position will be set to the last keyframe.
	DWORD SeekToSampleTime(LONGLONG tStart, bool fSeekToKeyframe, bool fAllowEOF);

	// @cmember Seeks to llFileOffset in the index file and reads out the index
	// at that location to rIdx.
	DWORD ReadIndexAtFileOffset(LONGLONG llFileOffset, INDEX &rIdx);

	// @cmember Physically deletes a recording file pair off disk - that is both
	// the data and index file are removed.  So DeleteFile("foo_0") would delete
	// both "foo_0.dvr-dat" and "foo_0.dvr-idx".
	static BOOL DeleteFile(std::wstring &strFileName);

	// @cmember Release the cache in use by this recording file
	void ReleaseIndexFileCache();

	// @cmember Cache the index file and store it in the supplied buffer.  A
	// reference to that buffer is kept in m_pIdxCache
	DWORD CacheIndexFile(BYTE pCache[INDEX_CACHE_SIZE]);

	// @cmember Accessor function
	bool HasWroteFirstSample() { return m_fWroteFirstSample; }

	int GetCreationIndex() { return m_iFileCreationIndex; }

	bool UsedOptimizedOpen() { return m_fOptimizedOpen; }

	bool IndexIsEmpty() { return (0 == m_indexFile.GetFileSize()); }

	DWORD CommitFinalFileState(bool fRecordingInProgress);

	void SetSizeLimit(ULONGLONG ullFileSizeLimit) { m_ullMaxDataFileSize = ullFileSizeLimit; }

	ULONGLONG GetSizeLimit() const { return m_ullMaxDataFileSize; }

protected:

	// @cmember Read a media type from the current location in the file and store
	// it into the supplied media type pointer.  pMediaType may be NULL which will
	// effectively advance the file pointer past current media type in the file.
	DWORD ReadNextMediaType(CMediaType *pMediaType);

	CBinFile m_dataFile;
	CBinFile m_indexFile;
	FILE_HEADER m_header;
	std::wstring m_strFilename;
	int m_iFileCreationIndex;
	DWORD m_dwLastSampleDataLen;	// @cmember Length of last sample written to
									// the recording file.
	bool m_fWroteFirstSample;		// @cmember Set to true when the first sample
									// is written to the recording file.  It allows
									// us to force an index object to be created
									// on the first sample.
	bool m_fOptimizedOpen;			// @cmember false for files being written. True
									// for files that are to be read but that haven't
									// been properly opened yet.
	ULONGLONG m_ullMaxDataFileSize;	// @cmember When this file reaches this size (if not 0)
									// start a new file. Used to limit how badly a/v will
									// glitch if you start with a permanent recording or
									// use an impulse recording;

	BYTE *m_pIdxCache;				// @cmember Complete cache of the index file.
									// NULL if not cached.

	ULONGLONG m_ullCacheSize;		// @cmember Actual number of bytes chached.

	ULONGLONG m_ullCacheFilePos;	// @cmember Simulated position of the file pointer
									// in the cache.

	ULONGLONG m_ullDataFileSize;	// @cmember Size of the data file in bytes.  Only
									// valid once m_header.fFileInProgress is FALSE for
									// files opened in read mode.
	ULONGLONG m_ullIndexFileSize;	// @cmember Size of the index file in bytes.  Only
									// valid once m_header.fFileInProgress is FALSE.
    SAMPLE_HEADER m_sampleHeader;   // @cmember Sample header that should be used for writing. 
                                    // Reserved area is initialized at construction time.
};

// ################################################

class CRecordingSetWriteIOThread;

class CRecordingSetWrite
{
	friend class CWriter;

public:

	// @cmember Constructor - just initializes members.
	CRecordingSetWrite();

	// @cmember Destructor
    ~CRecordingSetWrite();

	// @cmember Create a new recording set for writing with the specified
	// stream types.
	DWORD CreateNew(const WCHAR *wszFileName,
					DWORD dwNegotiatedBufferSize,
					CMediaType rgMediaTypes[],
					DWORD dwNumStreams,
					BYTE rgbCustom[],
					DWORD dwNumCustomBytes,
					bool fTemporary);

	// @cmember If a new recording has been prepared asynchronously,
	// does the final steps to make it the current recording:
	DWORD UsePreparedNew(DWORD dwNegotiatedBufferSize,
					CMediaType rgMediaTypes[],
					DWORD dwNumStreams,
					BYTE rgbCustom[],
					DWORD dwNumCustomBytes,
					bool fTemporary);

	// @cmember Kicks off preparation of the next recording -- to
	// be done asynchronously so that the Writer is not blocked
	// for a significant amount of time when the Writer does need
	// to start another recording:
	void PrepareNextRecording(std::wstring &strRecordingName);

	// @cmember Clears the prepped recording (if any):
	void ClearPreppedRecording();

	// @cmember Closes the recording set.
	void Close();

	// @cmember Returns true if and only iff the recording set is open.
	bool IsOpen() {return m_fIsOpen;}	
	
	// @cmember Set the max file of the recording set.  Any files which are
	// already part of the set aren't truncated.  This can be set while a
	// recording set is opened for write.  This property has no effect for
	// recording sets opened for read.
    void SetMaxFileSize(ULONGLONG ullMaxSize);

	// @cmember Write a media sample to the end of the recording set.  If the
	// currently opened data file is larger than m_ullMaxFileSize then that
	// file is closed and a new data/index file pair is opened first.  Iff a new
	// file is added to the recording set, then *pfAddedNewFile is set to true.
	DWORD WriteSample(	IMediaSample &rSample,
						DWORD dwStreamNum,
						LONGLONG llDataOffset,
						bool fCreateIndex,
						bool *pfAddedNewFile);

	// @cmember Returns a reference to the recording set's filename.
	std::wstring& GetFileName() { return m_strFilename; }

	// @cmember Removes the specified sub file from the recording set.  This
	// allows the recording set to be 'front truncated' when watching Live TV
	// with some fixed size buffer.  The removed file is not deleted, but just
	// closed and removed from the recording set.  If the file was successfully
	// removed, true is returned.  On an error (such as the file not being
	// part of the set), false is returned.
	bool RemoveSubFile(std::wstring &strSubFilename);

	// @cmember Simply looks up the specified recording file and passes the call on.
	bool SetTempPermStatus(	const std::wstring &strSubFilename,
							LONGLONG tVirtualStart,
							bool fTemporary);

	// @cmember Releases the last recording file in the set.  A pointer to that
	// object is returned.  It is the responsibility of the caller to delete that
	// pointer eventually.  Iff the recording set is empty or closed when this
	// function is called, then NULL is returned.
	CRecordingFile *ReleaseLastRecordingFile();

	// @cmember Sets the recording mode as either permanent or temporary.  This
	// does not affect any files currently in the recording set but will affect
	// future created files.
	void SetRecordingMode(bool fTemporary) { m_fTemporary = fTemporary; }

protected:

	// @cmember Returns a string containing the next filename to be included
	// in the recording set.  This is used when writing to the set and a new
	// file needs to be added.
	std::wstring GetNextFileName();

 	// APIs that prepare for doing some tasks asynchronously that otherwise
	// would lock out capturing a/v long enough to create a significant risk
	// of putting enough backpressure on the A/V capture pipeline to cause
	// a/v glitches:

	void StartDiskIOThread();
	void StopDiskIOThread();
	void BeginPaddingToNewFileSize();

	std::wstring m_strFilename;	// @cmember The root filename of this recording set.
								// For example, the root of 125_0.dvr-dat is "125"

	DWORD m_dwNegotiatedBufferSize;	// @cmember The theoretical max media sample size.

	ULONGLONG m_ullMaxFileSize;	// @cmember Once a file excedes this size when
								// writing to a recording set, a new file is opened.

	DWORD m_dwMaxFileNum;		// @cmember The maximum file number that is part of
								// this recording set.

	CMediaType* m_rgMediaTypes;	// @cmember Array of media types that this recording
								// set stores.

	DWORD m_dwNumStreams;		// @cmember Number of streams (and media types) that
								// are stored in this recording set.

	BYTE *m_pbCustomData;		// @cmember Custom file data.  This memory for this
								// array must be managed outside this CRecordingSet.

	DWORD m_dwCustomDataLen;	// @cmember Length of custom data.

	bool m_fTemporary;			// @cmember True if the temporary flag should be set
								// in subsequent recording file headers.

	bool m_fIsOpen;				// @cmember True if and only if the recording set was
								// opened successfully via CreateNew.

	std::vector<CRecordingFile *> m_vecFiles;	// @cmember Vector containing the
												// recording  files being written to.

	// Optimization stuff:

	friend class CRecordingSetWriteIOThread;

	CCritSec m_cCritSecDiskOpt;
	CRecordingSetWriteIOThread *m_pcRecordingSetWriteIOThread;
};

class CRecordingSetRead
{
	friend class CReader;

public:
	// @cmember Constructor - just initializes members.
	CRecordingSetRead();

	// @cmember Destructor
	~CRecordingSetRead() {Close();}

	// @cmember Retrieve the number of streams in this recording set.
	DWORD GetNumStreams() { return m_dwNumStreams; }

	// @cmember Retrieve an array of stream types in this recording set.
	CMediaType *GetStreamTypes() { return m_rgMediaTypes; }

	// @cmember Retrieves the max sample size in any of the opened recording files.
	DWORD GetMaxSampleSize() { return m_dwMaxSampleSize; }

	// @cmember Retrieves custom data buffer.
	BYTE *GetCustomData() { return m_pbCustomData; }

	// @cmember Retrieves custom data buffer size.
	DWORD GetCustomDataSize() { return m_dwCustomDataLen; }

	// @cmember Open a recording set for reading.  If fOpenEmpty is specified, then
	// no files are actually added to the recording set.  Files can be added at any
	// time using the AddFile method.  Iff fIgnoreTempRecFiles is set, then any
	// files flagged temporary (via fTemporary in the file header) are ignored.
	// Likewise, temporary files will then NEVER be added to the recording set in
	// AddFile or OpenNewFiles.
	DWORD Open(const WCHAR *wszFileName, bool fOpenEmpty, bool fIgnoreTempRecFiles);

	// @cmember Adds a file to the recording set.  Iff fUpdateMaxSampleSize is
	// true, then m_dwMaxSampleSize is updated if necessary.  Iff
	// fUpdateMaxSampleSize is false, then an error returned if the file being
	// added has a larger max sample size than m_dwMaxSampleSize.
	DWORD AddFile(	const WCHAR *wszFileName,
					bool fUpdateMaxSampleSize,
					bool fOptimizeOpen = false);

	// @cmember Reads the header info in for the first and last files in an optimized-open
	// recording set.
	DWORD FinishOptimizedOpen();

	// @cmember Opens any new files which are part of the recording set which
	// may not have existed when the recording set was originally opened.  Iff
	// fIgnoreTempRecFiles is set, then any files flagged temporary (via fTemporary
	// in the file header) are ignored.
	DWORD OpenNewFiles(bool fMayOptimizeOpen);

	// @cmember Returns true if and only iff the recording set is open.
	bool IsOpen() {return m_uiActiveFileNum != -1;}

	// @cmember Closes the recording set.
	void Close();

	// @cmember Retrieve the stream number for the ensuing sample.  This function
	// must be called exactly once before calling ReadSample.  Iff fCheckForNewFiles
	// and the current file pos is EOF, then this call checks for and opens and
	// files that may have been added to the recording set since it was opened.
	// Iff fResetFilePos is set then this function reads the sample header and then
	// sets the file pointer back to where it was before this call.  It essentially
	// allows someone to 'peek' at the next sample.  Iff fSkipToIndex is set, then
	// the position will advance either forwards or backwards to the next index
	// position.
	DWORD ReadSampleHeader(	SAMPLE_HEADER &rHeader,
							bool fCheckForNewFiles,
							bool fResetFilePos,
							bool fSkipToIndex,
							bool fForward);

	// @cmember Version of ReadSampleHeader which is for implementing hyper fast
	// read modes.  It loops calling ReadSampleHeader until llSkipBoundary is
	// crossed, and then returns.
	DWORD ReadSampleHeaderHyperFast(SAMPLE_HEADER &rHeader,
									bool fCheckForNewFiles,
									LONGLONG llSkipBoundary,
									bool fForward);
									

	// @cmember This function seeks back sizeof(SAMPLE_HEADER).  It can be called
	// immediately after calling ReadSampleHeader
	// This means that:
	//     ReadSampleHeader(foo, false, true, false, true);
	// Is equivalent to:
	//     ReadSampleHeader(foo, false, false, false, true);
	//     ResetSampleHeaderRead();
	DWORD ResetSampleHeaderRead();

	// @cmember Read a media sample from the current position in the data file.
	// rHeader must contain the sample header info retrieved from the preceding
	// ReadSampleHeader call.  If there are no more samples in the current file,
	// then the current position in the recording set is moved to the next file.
	DWORD ReadSample(SAMPLE_HEADER &rHeader, IMediaSample &rSample);

	// @cmember Read the sample data from the current position in the data file.
	// rHeader must contain the sample header info retrieved from the preceding
	// ReadSampleHeader call.
	DWORD ReadSampleData(SAMPLE_HEADER &rHeader, BYTE *pBuf, ULONG cbBuf, ULONG* pcbRead);

	// @cmember Returns a reference to the recording set's filename.
	std::wstring& GetFileName() { return m_strFilename; }

	// @cmember Removes the specified sub file from the recording set.  This
	// allows the recording set to be 'front truncated' when watching Live TV
	// with some fixed size buffer.  The removed file is not deleted, but just
	// closed and removed from the recording set.  If the file was successfully
	// removed, true is returned.  On an error (such as the file not being
	// part of the set), false is returned.
	bool RemoveSubFile(std::wstring &strSubFilename);

	// @cmember Retrieves the minimum starting sample time - that is, the earliest
	// sample to which an application may seek.  This value is taken from the first
	// index of the first file.  If the first file has a tVirtualStartTime other
	// than -1 in its header AND fIgnoreVirtualStartTime==false, then
	// tVirtualStartTime is returned rather than the time of the first index.
	// Iff the recording set is empty or an error occurs, -1 is returned.
	LONGLONG GetMinSampleTime(bool fIgnoreVirtualStartTime);

	// @cmember Retrieves the maximum sample time - that is, the last sample to
	// which an application may seek.  This value is taken from the last index of
	// the last recording file.
	// Iff the recording set is empty or an error occurs, -1 is returned.
	LONGLONG GetMaxSampleTime();

	// @cmember Seek in the recording set to the specified sample time.  The call
	// is simply passed onto the appropriate recording file.
	DWORD SeekToSampleTime(LONGLONG tStart, bool fSeekToKeyframe);

	// @cmember Returns the stream offset of the closest indexed nav pack which
	// starts <= tStart.  This function does a full search.
	LONGLONG GetNavPackStreamOffsetFullSearch(LONGLONG tStart);

	// @cmember Returns the stream offset of the closest indexed nav pack which
	// starts <= tStart.  This function uses the results of GetNavPackStreamOffsetFullSearch
	// and simply scans forward until he finds the right nav pack.
	LONGLONG GetNavPackStreamOffsetFast(LONGLONG tStart);

	// @cmember Returns the media sample time of the closest indexed nav pack which
	// starts <= llOffset.  This function does a full search.
    LONGLONG GetNavPackMediaTimeFullSearch(ULONGLONG llOffset);

	// @cmember These mark/restore the position of the current active file
	// to allow safe mucking with the index files.  Use with care.
	void MarkSeekPosition();
	void RestoreSeekPostion();
#ifdef DEBUG
	void Dump();
#endif // DEBUG


protected:

	struct MARKED_SEEK_POS
	{
		size_t uiActiveFileNum;
		LONGLONG llActIdxOffset;
	};

	struct INDEX_CACHE
	{
		size_t uiCachedIdxFile;
		DWORD dwLastAccessed;
	};

	// @cmember Function which scans either forward or backwards through the active
	// recording file until a keyframe is hit.  Then m_llActIdxOffset is updated
	// with the location in the corresponding index file of the index of that
	// keyframe.  If eof is hit in either direction before a keyframe, then
	// ERROR_SUCCESS is returned and m_llActIdxOffset is set to either 0 or the
	// index file size.
	DWORD ScanToIndexedSampleHeader(bool fForward);
	DWORD FullyOpen(CRecordingFile *pFile, bool fUpdateMaxSampleSize);

	// @cmember Releases the previously cached index file and stores the new index
	// file in m_idxCache for quick access
	DWORD CacheIndexFile(size_t iFileNum);

	std::wstring m_strFilename;	// @cmember The root filename of this recording set.
								// For example, the root of 125_0.dvr-dat is "125"

	DWORD m_dwMaxSampleSize;	// @cmember The maximum media sample size stored in
								// this recording set.

	size_t m_uiActiveFileNum;	// @cmember The file which is currently being
								// written to or read from.  A value of -1 indicates
								// that the recording set isn't open.

	LONGLONG m_llActIdxOffset;	// @cmember Offset into the recording index file
								// just after where the last sample was read from
								// when reading keyframes only.  A value of -1
								// indicates none.  This value gets cleared on
								// seeks.

	CMediaType* m_rgMediaTypes;	// @cmember Array of media types that this recording
								// set stores.

	DWORD m_dwNumStreams;		// @cmember Number of streams (and media types) that
								// are stored in this recording set.

	BYTE *m_pbCustomData;		// @cmember Custom file data.  This memory for this
								// array must be managed outside this CRecordingSet.

	DWORD m_dwCustomDataLen;	// @cmember Length of custom data.

	bool m_fDontOpenTempFiles;	// @cmember Iff true, then any files marked temporary
								// in their file header are opened or added to the
								// recording set - in any of the Add/Open calls.

	std::vector<CRecordingFile *> m_vecFiles;	// @cmember Hashtable storing the
												// recording files with their
												// respective numbers.

	INDEX m_idxLastNavPackFound;// @cmember Index of the last nav pack found by either
								// of the GetNavPackStreamOffset functions

	INDEX_CACHE m_rgidxCache[INDEX_CACHE_COUNT]; 	// @cmember In memory copy of the index file
													// at position m_iCachedIdxFile

	__declspec(align(4096))
	BYTE m_rgidxCacheData[INDEX_CACHE_COUNT][INDEX_CACHE_SIZE]; // @cmember In memory copy of the index file
																// at position m_iCachedIdxFile
																// align this on a page boundary

	MARKED_SEEK_POS m_markedSeekPos;	// @cmember Bookmarked seek position.

	size_t m_iLastRecFileSeekedTo;	// @cmember Used to help speed up seeks during hyper-fast modes

};


} // namespace MSDvr

// Diagnostics -- don't turn on unless a developer wants to do so in a personal build:
//
// #define MONITOR_WRITE_FILE

#ifdef MONITOR_WRITE_FILE

extern "C" void BeginFileWriteMonitoring();
extern "C" void EndFileWriteMonitoring();

#endif
