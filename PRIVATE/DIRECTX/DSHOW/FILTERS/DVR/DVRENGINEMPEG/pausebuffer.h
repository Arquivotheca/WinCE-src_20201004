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

namespace MSDvr
{

	namespace SampleProducer {
		class CPauseBufferSegment;
		class CPauseBufferHistory;
	}

struct SPauseBufferEntry
{
	std::wstring strFilename;
	std::wstring strRecordingName;
	LONGLONG tStart;			// In 100 nanosecs
	bool fPermanent;

	// If this entry was duplicated, pointer to the entry from which
	// this entry was made.  Use this with great care (if at all).
	SPauseBufferEntry *pOriginal;

};


// @interface CPauseBufferData |
// Class which will ultimately expose the contents of the pause buffer.
class CPauseBufferData
{
	friend class CWriter;
	friend class CSampleConsumer;
	friend class CSampleProducer;
	friend class SampleProducer::CPauseBufferSegment;
	friend class SampleProducer::CPauseBufferHistory;

public:
	//@cmember Increment the object's ref count.  The updated ref count is
	// returned.
	ULONG AddRef(void);

	// @cmember Decrement the ref count.  When the ref count drops to 0 any
	// resources consumed by the object are automatically cleaned up.  The
	// updated ref count is returned.
	ULONG Release(void);

	// @cmember Create a new CPauseBufferData.  This ensures that this class
	// is only created on the heap.
	static CPauseBufferData *CreateNew();

	// @cmember Duplicates a pause buffer object.  The new object will have
	// a ref count of 1 and contain the same list contents as the original
	// pause buffer object.
	CPauseBufferData *Duplicate();

	// @cmember Inserts a copy the supplied SPauseBufferEntry into the pause
	// buffer based on its tStart.  It is the responsibility of the caller
	// of this function to protect the contents of the pause buffer across
	// multiple threads.  The index of the inserted item is returned.
	size_t SortedInsert(SPauseBufferEntry &newEntry);

	// @cmember Accesses an entry in the pause buffer.  The indices are 0
	// based and range from 0 to GetCount()-1.
	const SPauseBufferEntry &operator[](size_t i) { return m_vecFiles[i]; }

	// @cmember Returns the numer of entries in the pause buffer.
	size_t GetCount() { return m_vecFiles.size(); }

	// @cmember Returns the index of the entry which contains the specified
	// time.  A given entry contains all times from it's tStart to tStart of
	// the next entry.  If llTime is less than tStart of the first entry,
	// then -1 is returned.
	size_t GetEntryPositionAtTime(LONGLONG llTime);

	// @cmember Returns the entry which contains the specified time.  A given
	// entry contains all times from it's tStart to tStart of the next entry.
	// It is the responsibility of the caller of this function to ensure that
	// the pause buffer contains an entry which spans llTime.
	const SPauseBufferEntry &GetEntryAtTime(LONGLONG llTime);

	// @cmember Returns the maximum size of the pause buffer in 100 nanosecs.
	// The pause buffer may not actually contain this much data if it hasn't
	// had time to fill.  Also, the pause buffer may actually contain MORE
	// data since the oldest file cannot be removed until it is 100% stale.
	LONGLONG GetMaxBufferDuration() { return m_llMaxBuffer_100nsec; }

	// @cmember Returns the actual size of the pause buffer in 100 nanosecs.
	// This value is correct when the PauseBufferUpdated event is fired but
	// may be obsolete by the time it is retrieved.  Clients should keep that
	// in mind when using this value.
	LONGLONG GetActualBufferDuration() { return m_llActualBuffer_100nsec; }

	// @cmember Returns the start time (media time) of the most recent sample
	// contained in the pause buffer in 100 nsec units.  This value is correct
	// when the PauseBufferUpdated event is fired but may be obsolete by the
	// time it is retrieved.  Clients should keep that in mind when using this
	// value.
	LONGLONG GetLastSampleTime() { return m_llLastSampleTime_100nsec; }

protected:

	// @cmember Constructor - intentionally private to force use of CreateNew.
	CPauseBufferData();

	// @cmember Destructor - intentionally private to force use of Release.
	~CPauseBufferData() { Clear(); }

	// @cmember Cleans out the pause buffer.
	void Clear();

	// @cmember Vector containing a list of pause buffer entries sorted
	// increasingly by their start times.
	std::vector<SPauseBufferEntry> m_vecFiles;

	// @cmember Refcount - when it drops to 0 the CPauseBufferData object
	// automatically cleans itself up.
	LONG m_lRefCount;

	// @cmember Maximum length of the pause buffer.
	LONGLONG m_llMaxBuffer_100nsec;

	// @cmember Actual length of the pause buffer.
	LONGLONG m_llActualBuffer_100nsec;

	// @cmember Media time of the most recent sample written to the pause buffer.
	LONGLONG m_llLastSampleTime_100nsec;
};

}
