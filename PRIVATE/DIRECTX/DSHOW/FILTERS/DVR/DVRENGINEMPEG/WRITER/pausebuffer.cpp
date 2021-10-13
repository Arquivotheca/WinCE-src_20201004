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
#include "..\\PipelineInterfaces.h"
#include "..\\PauseBuffer.h"
#include "..\\SampleProducer\\ProducerConsumerUtilities.h"  // to pick up CSmartRefPtr

using std::vector;

namespace MSDvr
{

// ################################################
//      CPauseBufferMgrImpl
// ################################################

typedef std::list<IPauseBufferCallback *>::iterator PauseBufferCallbackIterator;

CPauseBufferMgrImpl::~CPauseBufferMgrImpl()
{
	ClearPauseBufferCallbacks();
}

void CPauseBufferMgrImpl::CancelNotifications(IPauseBufferCallback *pCallback)
{
    for (PauseBufferCallbackIterator iter = m_lPauseBufferCallbacks.begin();
         iter != m_lPauseBufferCallbacks.end();
         ++iter)
    {
        if (*iter == pCallback)
        {
            m_lPauseBufferCallbacks.erase(iter);
            return;
        }
    }
}

void CPauseBufferMgrImpl::RegisterForNotifications(IPauseBufferCallback *pCallback)
{
    for (PauseBufferCallbackIterator iter = m_lPauseBufferCallbacks.begin();
         iter != m_lPauseBufferCallbacks.end();
         ++iter)
    {
        if (*iter == pCallback)
            return;
    }
    m_lPauseBufferCallbacks.push_back(pCallback);
}

void CPauseBufferMgrImpl::FirePauseBufferUpdateEvent(CPauseBufferData *pData)
{
    for (PauseBufferCallbackIterator iter = m_lPauseBufferCallbacks.begin();
         iter != m_lPauseBufferCallbacks.end();
         ++iter)
    {
        CSmartRefPtr<CPauseBufferData> holder(pData);
        (*iter)->PauseBufferUpdated(pData);
    }
}

void CPauseBufferMgrImpl::ClearPauseBufferCallbacks()
{
	m_lPauseBufferCallbacks.clear();
}


// ################################################
//      CPauseBufferData
// ################################################

CPauseBufferData::CPauseBufferData()
	:	m_lRefCount(1),
		m_llActualBuffer_100nsec(0),
		m_llMaxBuffer_100nsec(MAXLONGLONG),
		m_llLastSampleTime_100nsec(0)
{
}

void CPauseBufferData::Clear()
{
	m_vecFiles.clear();
	m_lRefCount = 1;
	m_llActualBuffer_100nsec = 0;
	m_llMaxBuffer_100nsec = MAXLONGLONG;
	m_llLastSampleTime_100nsec = 0;
}

ULONG CPauseBufferData::Release()
{
	ASSERT(m_lRefCount > 0);
	if (InterlockedDecrement(&m_lRefCount) <= 0)
	{
		delete this;
		return 0;
	}

	return m_lRefCount;
}

ULONG CPauseBufferData::AddRef()
{
	return InterlockedIncrement(&m_lRefCount);
}

CPauseBufferData *CPauseBufferData::CreateNew()
{
	return new CPauseBufferData();
};

CPauseBufferData *CPauseBufferData::Duplicate()
{
	CPauseBufferData *pcPB = CreateNew();

	// Simply add the items 1 at a time to the new PB
	for (std::vector<SPauseBufferEntry>::iterator it = m_vecFiles.begin();
		it != m_vecFiles.end();
		it++)
	{
		pcPB->m_vecFiles.push_back(*it);
		pcPB->m_vecFiles.back().pOriginal = &(*it);
	}
	
	// And copy over the common info:
	pcPB->m_llActualBuffer_100nsec = m_llActualBuffer_100nsec;
	pcPB->m_llMaxBuffer_100nsec = m_llMaxBuffer_100nsec;
	pcPB->m_llLastSampleTime_100nsec = m_llLastSampleTime_100nsec;

	return pcPB;
}

size_t CPauseBufferData::GetEntryPositionAtTime(LONGLONG llTime)
{
	// Walk through list in reverse until we've found it.
	size_t index = GetCount() - 1;
	for (std::vector<SPauseBufferEntry>::reverse_iterator it = m_vecFiles.rbegin();
		it != m_vecFiles.rend();
		it++, index--)
	{
		if (llTime > it->tStart)
			return index;
	}

	return -1;
}

const SPauseBufferEntry &CPauseBufferData::GetEntryAtTime(LONGLONG llTime)
{
	size_t index = GetEntryPositionAtTime(llTime);
	if (index == -1)
	{
		// Check and make sure there isn't a bug in the calling code.
		ASSERT(FALSE);
		index = 0;
	}
	return m_vecFiles[index];
}

size_t CPauseBufferData::SortedInsert(SPauseBufferEntry &newEntry)
{
	size_t index = 0;
	for (	std::vector<SPauseBufferEntry>::iterator it = m_vecFiles.begin();
			it != m_vecFiles.end();
			it++, index++)
	{
		// Check to see if we've found the right insertion point.
		if (newEntry.tStart < it->tStart)
		{
			m_vecFiles.insert(it, newEntry);
			return index;
		}
	}

	// We didn't find the reight insertion point, so insert at the end.
	m_vecFiles.push_back(newEntry);
	return m_vecFiles.size() - 1;
}

} // namespace MSDvr
