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

class CDVRAllocator : public CMemAllocator
{
public:
	CDVRAllocator(HRESULT *pHR) : CMemAllocator(TEXT("CDVRAllocator"), NULL, pHR) {}

	HRESULT Alloc()
	{
		CAutoLock lck(this);

		/* Check he has called SetProperties */
		HRESULT hr = CBaseAllocator::Alloc();
		if (FAILED(hr)) {
			return hr;
		}

		/* If the requirements haven't changed then don't reallocate */
		if (hr == S_FALSE) {
			ASSERT(m_pBuffer);
			return NOERROR;
		}
		ASSERT(hr == S_OK); // we use this fact in the loop below

		/* Free the old resources */
		if (m_pBuffer) {
			ReallyFree();
		}

		/* Compute the aligned size */
		LONG lAlignedSize = m_lSize + m_lPrefix;
		if (m_lAlignment > 1) {
			LONG lRemainder = lAlignedSize % m_lAlignment;
			if (lRemainder != 0) {
				lAlignedSize += (m_lAlignment - lRemainder);
			}
		}

		/* Create the contiguous memory block for the samples
		making sure it's properly aligned (64K should be enough!)
		*/
		ASSERT(lAlignedSize % m_lAlignment == 0);

		// Reserve the memory from the global shared region.
		// Make sure we reserve at least 2 MB.
		DWORD dwSize = m_lCount * lAlignedSize;
		m_pBuffer = (PBYTE) VirtualAlloc(	NULL,
											max(1024*2048, dwSize),
											MEM_RESERVE,
											PAGE_NOACCESS);

		if (m_pBuffer == NULL) {
			DWORD dw = GetLastError();
			return E_OUTOFMEMORY;
		}

		// Commit the memory
		m_pBuffer = (PBYTE) VirtualAlloc(	m_pBuffer,
											dwSize,
											MEM_COMMIT,
											PAGE_READWRITE);
		if (m_pBuffer == NULL) {
			DWORD dw = GetLastError();
			return E_OUTOFMEMORY;
		}

		LPBYTE pNext = m_pBuffer;
		CMediaSample *pSample;

		ASSERT(m_lAllocated == 0);

		// Create the new samples - we have allocated m_lSize bytes for each sample
		// plus m_lPrefix bytes per sample as a prefix. We set the pointer to
		// the memory after the prefix - so that GetPointer() will return a pointer
		// to m_lSize bytes.
		for (; m_lAllocated < m_lCount; m_lAllocated++, pNext += lAlignedSize) {


			pSample = new CMediaSample(
								NAME("Default memory media sample"),
					this,
								&hr,
								pNext + m_lPrefix,      // GetPointer() value
								m_lSize);               // not including prefix

				ASSERT(SUCCEEDED(hr));
			if (pSample == NULL) {
				return E_OUTOFMEMORY;
			}

			// This CANNOT fail
			m_lFree.Add(pSample);
		}

		m_bChanged = FALSE;
		return NOERROR;
	}
};
