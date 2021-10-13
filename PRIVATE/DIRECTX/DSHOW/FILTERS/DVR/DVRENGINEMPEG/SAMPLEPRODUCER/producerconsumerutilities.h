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

#include "../HResultError.h"

namespace MSDvr 
{
	HRESULT WStringToOleString(const std::wstring &wstringSource, LPOLESTR *ppszDestinationString);

	// For copying media samples -- for performance reasons, throw an exception in
	// the rare case that something is wrong.
	void CopySample(IMediaSample *pSrcSample, IMediaSample *pDestSample);

	template<class T>
	class CSmartRefPtr
	{
	public:
		CSmartRefPtr(T *pT = NULL) 
			: m_pT(0)
			{
				if (pT)
					pT->AddRef();
				m_pT = pT;
			}

		CSmartRefPtr(const CSmartRefPtr &rcSmartRefPtr)
			: m_pT(0)
			{
				if (rcSmartRefPtr.m_pT)
					rcSmartRefPtr.m_pT->AddRef();
				m_pT = rcSmartRefPtr.m_pT;
			}

		~CSmartRefPtr()
			{
				if (m_pT)
					m_pT->Release();
			}

		CSmartRefPtr & operator=(const CSmartRefPtr &rcSmartRefPtr) throw()
			{
				if (rcSmartRefPtr.m_pT)
					rcSmartRefPtr.m_pT->AddRef();
				if (m_pT)
					m_pT->Release();
				m_pT = rcSmartRefPtr.m_pT;
				return *this;
			}

		CSmartRefPtr & operator=(T *pT) throw()
			{
				if (pT)
					pT->AddRef();
				if (m_pT)
					m_pT->Release();
				m_pT = pT;
				return *this;
			}
		operator T*() const throw()
			{
				return m_pT;
			}

		T& operator*() const throw()
		{
			ASSERT(m_pT!=NULL);
			return *m_pT;
		}

		//The assert on operator& usually indicates a bug.  If this is really
		//what is needed, however, take the address of the p member explicitly.
		T** operator&() throw()
		{
			return &m_pT;
		}

		T* operator->() const throw()
		{
			return m_pT;
		}

		bool operator!() const throw()
		{
			return (m_pT == NULL);
		}

		bool operator<(T* pT) const throw()
		{
			return m_pT < pT;
		}

		bool operator==(T* pT) const throw()
		{
			return m_pT == pT;
		}

		void Release() throw()
		{
			T* pTemp = m_pT;
			if (pTemp)
			{
				m_pT = NULL;
				pTemp->Release();
			}
		}

		// Attach to an existing interface (does not AddRef)
		void Attach(T* p2) throw()
		{
			if (m_pT)
				m_pT->Release();
			m_pT = p2;
		}

		// Detach the interface (does not Release)
		T* Detach() throw()
		{
			T* pt = m_pT;
			m_pT = NULL;
			return pt;
		}

		T *m_pT;
	};

	template<typename T>
	struct OleMemHolder
	{
		OleMemHolder(T pMem = 0)
			: m_pMem(pMem)
		{
		}

		~OleMemHolder()
		{
			if (m_pMem)
				CoTaskMemFree(m_pMem);
		}

		T m_pMem;
	};

	class CAutoUnlock 
	{
	public:
		CAutoUnlock(CCritSec &rcCritSec)
			: m_rcCritSec(rcCritSec)
		{
			m_rcCritSec.Unlock();
		}

		~CAutoUnlock()
		{
			m_rcCritSec.Lock();
		}

	protected:
		CCritSec &m_rcCritSec;
	};

	class CEventHandle
	{
	public:
		CEventHandle(BOOL bManualReset, BOOL bInitialState) 
			: m_hEvent(CreateEvent(NULL, bManualReset, bInitialState, NULL))
			{
			}

		~CEventHandle()
			{
				if (m_hEvent != NULL)
				{
					EXECUTE_ASSERT(CloseHandle(m_hEvent));
				}
			}

		operator HANDLE() const throw()
			{
				return m_hEvent;
			}

		bool operator!() const throw()
		{
			return (m_hEvent == NULL);
		}

	private:
		// deliberately not implemented:
		CEventHandle & operator=(const CEventHandle &);
		CEventHandle(const CEventHandle &);

		const HANDLE m_hEvent;
	};

}

