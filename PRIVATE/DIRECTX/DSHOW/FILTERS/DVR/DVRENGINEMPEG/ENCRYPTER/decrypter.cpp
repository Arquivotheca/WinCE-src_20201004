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
#include "..\\plumbing\\pipelinemanagers.h"
#include "Decrypter.h"
#include "..\\CopyProtectionPolicies.h"

namespace MSDvr
{
	
CDecrypter::CDecrypter()
{
	m_pPipelineMgr = NULL;
	m_pReader = NULL;
	m_pNextFileSource = NULL;
	m_fNeedsNewKey = true;
	m_hKey = NULL;

	// Load a CSP and create a new keyset
	if (!CryptAcquireContext(	&m_hProv,
								L"mycontainer",			// Default key container
								NULL,					// Default provider
								PROV_RSA_AES,
								CRYPT_NEWKEYSET | CRYPT_SILENT))
	{
		DWORD dw = GetLastError();
		if (dw != NTE_EXISTS)
		{
			RETAILMSG(1, (L"*** Decrypter failed to load AES enabled CSP and create keyset - trying again.  Error 0x%08x.\n", dw));
			ASSERT(FALSE);
			return;
		}

		// Try again, this time acquire the existing keyset
		if (!CryptAcquireContext(	&m_hProv,
									L"mycontainer",		// Default key container
									NULL,				// Default provider
									PROV_RSA_AES,
									CRYPT_SILENT))		// No flags
		{
			dw = GetLastError();
			RETAILMSG(1, (L"*** Decrypter failed to load AES enabled CSP.  Error 0x%08x.\n", dw));
			ASSERT(FALSE);
			return;
		}
	}
}

CDecrypter::~CDecrypter()
{
	if (m_hKey)
		CryptDestroyKey(m_hKey);

	if (m_hProv)
		CryptReleaseContext(m_hProv, NULL);
}

unsigned char CDecrypter::AddToPipeline(IPlaybackPipelineManager& rcManager)
{
	// Keep the pipeline mgr
	m_pPipelineMgr = &rcManager;

	// Get a router
	CPipelineRouter router = m_pPipelineMgr->GetRouter(NULL, false, false);

	// If I call m_pPipelineMgr->IPipelineManager::RegisterCOMInterface without
	// going throug this temporary variable I get an unresolved external symbol.
	IPipelineManager *pBasePipelineMgr = m_pPipelineMgr;

	// Register our com interfaces
	m_pNextFileSource = (IFileSourceFilter *)
						pBasePipelineMgr->RegisterCOMInterface(
						*((TRegisterableCOMInterface<IFileSourceFilter> *) this));

	// Find the reader
	ASSERT(!m_pReader);
	void *p = NULL;
	ROUTE eRoute = router.GetPrivateInterface(IID_IReader, p);
	if ((eRoute == HANDLED_STOP) || (eRoute == HANDLED_CONTINUE))
		m_pReader = static_cast<IReader*>(p);

	if (!m_pReader)
	{
		ASSERT(FALSE);
		RETAILMSG(1, (L"*** Decrypter failed to find reader.\n"));
	}

	return 0;
}

STDMETHODIMP CDecrypter::GetCurFile(LPOLESTR *ppszFileName, AM_MEDIA_TYPE *pmt)
{
	if (m_pNextFileSource)
		return m_pNextFileSource->GetCurFile(ppszFileName, pmt);
	return E_NOTIMPL;
}

HRESULT CDecrypter::Load(LPCOLESTR wszFileName, const AM_MEDIA_TYPE *pmt)
{
	// Loading a new file means we need a new key.
	m_fNeedsNewKey = true;

	if (m_pNextFileSource)
		return m_pNextFileSource->Load(wszFileName, pmt);
	return E_NOTIMPL;
}

HRESULT CDecrypter::LoadKey()
{
	if (!m_pReader)
	{
		ASSERT(FALSE);
		RETAILMSG(1, (L"*** Decrypter failed to find reader.\n"));
		return E_FAIL;
	}

	if (m_hKey)
	{
		CryptDestroyKey(m_hKey);
		m_hKey = NULL;
	}

	BYTE buf[1024];
	DATA_BLOB dataIn = { m_pReader->GetCustomDataSize(), m_pReader->GetCustomData() };
	DATA_BLOB dataOut = {sizeof(buf), buf};
	if (!CryptUnprotectData(&dataIn,
							NULL,						// receives description string
							NULL,						// optional entropy
							NULL,						// reserved
							NULL,						// prompt struct
							CRYPTPROTECT_UI_FORBIDDEN,
							&dataOut))
	{
		HRESULT hr = GetLastError();
		ASSERT(FALSE);
		RETAILMSG(1, (L"*** Decrypter failed to unprotect AES key.  Error 0x%08x.\n", hr));
		return hr;
	}

	// WARNING!  dataOut.pbData is in plaintext in memory right now.  Clean it ASAP

	// Import the key
	BOOL b = CryptImportKey(m_hProv,
							dataOut.pbData,
							dataOut.cbData,
							NULL,
							0,
							&m_hKey);
	
	// Clean dataOut
	memset(dataOut.pbData, 0, dataOut.cbData);

	// Now check for success
	if (!b)
	{
		HRESULT hr = GetLastError();
		ASSERT(FALSE);
		RETAILMSG(1, (L"*** Decrypter failed to import AES key.  Error 0x%08x.\n", hr));
		return hr;
	}
	
	// Put the key in ECB mode
	DWORD dw = CRYPT_MODE_ECB;
	if (!CryptSetKeyParam(m_hKey, KP_MODE, (BYTE *) &dw, 0))
	{
		HRESULT hr = GetLastError();
		ASSERT(FALSE);
		RETAILMSG(1, (L"*** Decrypter failed to put AES key in ECB mode.  Error 0x%08x.\n", hr));
		return hr;
	}

	m_fNeedsNewKey = false;

	return S_OK;
}

ROUTE CDecrypter::ProcessOutputSample(	IMediaSample& riSample,
										CDVROutputPin& rcOutputPin)
{
	// Bail if we shouldn't decrypt
	if (!CCopyProtectionExtractor::IsEncrypted(riSample))
		return UNHANDLED_CONTINUE;

	if (m_fNeedsNewKey)
	{
		if (FAILED(LoadKey()))
			return UNHANDLED_CONTINUE;
	}

	BYTE *pData;
	riSample.GetPointer(&pData);
	DWORD dwDataLen = riSample.GetActualDataLength();

	if (!CryptDecrypt(m_hKey, NULL, FALSE, 0, pData, &dwDataLen))
	{
		DWORD dw = GetLastError();
		RETAILMSG(1, (L"*** Decrypter failed on sample.  Error 0x%08x.\n", dw));
		ASSERT(FALSE);
		return UNHANDLED_CONTINUE;
	}

	// Store the new data len and get out of here.
	riSample.SetActualDataLength(dwDataLen);

	return HANDLED_CONTINUE;
}

} // namespace MSDvr

