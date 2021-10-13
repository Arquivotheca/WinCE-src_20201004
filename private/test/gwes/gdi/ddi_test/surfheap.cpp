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
#include "precomp.h"

#define ZONE_SURFACEHEAP	0

DWORD SurfaceHeap::Available()
{
	SurfaceHeap *pNode;
	DWORD available = 0;

	for( pNode = this; pNode; pNode = pNode->m_pNext )
		if( !pNode->m_fUsed )
			available += pNode->m_nSize;

	return available;
}

SurfaceHeap::SurfaceHeap( DWORD size, ADDRESS base, SurfaceHeap *pNext, SurfaceHeap *pPrev )
{
	DEBUGMSG(ZONE_SURFACEHEAP,(TEXT("SurfaceHeap::SurfaceHeap size %d address 0x%08x pPrev %08x pNext %08x this %08x\r\n"),
		size, base, pPrev, pNext, this ));
	if( m_pNext = pNext )	// deliberate assign
		m_pNext->m_pPrev = this;
	if( m_pPrev = pPrev )	// deliberate assign
		m_pPrev->m_pNext = this;
	m_pStart = base;
	m_nSize = size;
	m_fUsed = 0;
}

SurfaceHeap::~SurfaceHeap()
{
	DEBUGMSG(ZONE_SURFACEHEAP,(TEXT("SurfaceHeap::~SurfaceHeap 0x%08x (size=%d)\r\n"),this,m_nSize));
	if( !m_pPrev )
	{
		DEBUGMSG(ZONE_SURFACEHEAP,(TEXT("Deleting heap!\r\n")));
		// if this is the base, then we are deleting the heap,
		SurfaceHeap *pLast = this;
		while( pLast->m_pNext )
			pLast = pLast->m_pNext;
		while( pLast != this )
		{
			pLast = pLast->m_pPrev;
			pLast->m_pNext->m_pPrev = (SurfaceHeap *)NULL;	// prevent SurfaceHeap::~SurfaceHeap from doing anything
			delete pLast->m_pNext;
			pLast->m_pNext = (SurfaceHeap *)NULL;
		}
	}
	else
	{
		DEBUGMSG(ZONE_SURFACEHEAP,(TEXT("Deleting node only\r\n")));
		// otherwise, we are just freeing this node
		m_pPrev->m_nSize += m_nSize;
		m_pPrev->m_pNext = m_pNext;
		if( m_pNext )
			m_pNext->m_pPrev = m_pPrev;
	}
}

SurfaceHeap *SurfaceHeap::Alloc( DWORD size )
{
	SurfaceHeap *pNode = this;
	DEBUGMSG(ZONE_SURFACEHEAP,(TEXT("SurfaceHeap::Alloc(%d)\r\n"),size));
	while( pNode && ( ( pNode->m_fUsed ) || ( pNode->m_nSize < size ) ) )
		pNode = pNode->m_pNext;
	if( pNode && ( pNode->m_nSize > size ) )
	{
		DEBUGMSG(ZONE_SURFACEHEAP,(TEXT("Splitting %d byte node at 0x%08x\r\n"), pNode->m_nSize, pNode ));
		// Ok, have to split into used & unused section
		SurfaceHeap *pFreeNode = new
			SurfaceHeap(
				pNode->m_nSize - size,								// size
				(ADDRESS)(size + (unsigned long)pNode->m_pStart),	// start
				pNode->m_pNext,										// next
				pNode												// prev
				);
		if( !pFreeNode )
		{
			pNode = (SurfaceHeap *)NULL;	// out of memory for new node
			DEBUGMSG(ZONE_SURFACEHEAP,(TEXT("Failed to allocate new node\r\n")));
		}
		else
		{
			pNode->m_nSize = size;
		}
	}
	if( pNode )
	{
		pNode->m_fUsed = 1;
		DEBUGMSG(ZONE_SURFACEHEAP,(TEXT("Marking node at 0x%08x as used (offset = 0x08x)\r\n"),pNode, pNode->Address()));
	}
	return pNode;
}

void SurfaceHeap::Free()
{
	SurfaceHeap *pMerge;
	SurfaceHeap *pNode = this;

	pNode->m_fUsed = 0;
	DEBUGMSG(ZONE_SURFACEHEAP,(TEXT("SurfaceHeap::Free 0x%08x (size=%d)\r\n"),pNode,pNode->m_nSize));
	pMerge = pNode->m_pPrev;
	if( pMerge && !pMerge->m_fUsed )
	{
		DEBUGMSG(ZONE_SURFACEHEAP,(TEXT("SurfaceHeap::Free - merging node %08x with prior node (%08x)\r\n"),
			pNode, pMerge ));
		delete pNode;			// Merge pNode with prior node
		pNode = pMerge;
	}
	pMerge = pNode->m_pNext;
	if( pMerge && !pMerge->m_fUsed )
	{
		DEBUGMSG(ZONE_SURFACEHEAP,(TEXT("SurfaceHeap::Free - merging %08x with subsequent node (%08x)\r\n"),
			pNode, pMerge ));
		delete pMerge;			// Merge pNode with subsequent node
	}
}

DWORD SurfaceHeap::Size()
{
	// this shouldn't be called too often, - just walk through accumulating total size;
	SurfaceHeap *pNode;
	DWORD size = 0;

	for( pNode=this; pNode; pNode = pNode->m_pNext )
		size += pNode->m_nSize;

	return size;
}
