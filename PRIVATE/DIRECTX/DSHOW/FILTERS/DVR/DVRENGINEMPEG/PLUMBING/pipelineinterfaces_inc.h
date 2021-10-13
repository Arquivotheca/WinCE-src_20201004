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
// PipelineInterfaces_Inc.h : Template implementations
//

//////////////////////////////////////////////////////////////////////////////
//	Implement IUnknown analogous to DECLARE_IUNKNOWN: delegate to the		//
//	owner of that CUnknown that we have registered with.					//
//////////////////////////////////////////////////////////////////////////////

template <class T>
STDMETHODIMP TRegisterableCOMInterface<T>::QueryInterface(REFIID riid, void** ppv)
{
	ASSERT( m_pcUnknown );
	return m_pcUnknown->GetOwner()->QueryInterface(riid, ppv);
}

template <class T>
STDMETHODIMP_(ULONG) TRegisterableCOMInterface<T>::AddRef()
{
	ASSERT( m_pcUnknown );
	return m_pcUnknown->GetOwner()->AddRef();
}

template <class T>
STDMETHODIMP_(ULONG) TRegisterableCOMInterface<T>::Release()
{
	ASSERT( m_pcUnknown );
	return m_pcUnknown->GetOwner()->Release();
}

//////////////////////////////////////////////////////////////////////////////
//	Interface information functions for the pipeline manager.				//
//////////////////////////////////////////////////////////////////////////////

template <class T>
IUnknown& TRegisterableCOMInterface<T>::GetRegistrationInterface()
{
	return *this;
}

template <class T>
const IID& TRegisterableCOMInterface<T>::GetRegistrationIID()
{
	return __uuidof(T);
}
