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
#include "..\\PipelineInterfaces.h"
#include "DvrInterfaces.h"

namespace MSDvr
{

class CDecrypter :	public IPlaybackPipelineComponent,
					public TRegisterableCOMInterface<IFileSourceFilter>
{
public:
	// CDecrypter
	CDecrypter();
	~CDecrypter();

	// IPipelineComponent
	void RemoveFromPipeline() { m_fNeedsNewKey = true; }
	ROUTE ConfigurePipeline(UCHAR iNumPins,
							CMediaType cMediaTypes[],
							UINT iSizeCustom,
							BYTE pbCustom[]) { return HANDLED_CONTINUE; }
	
	// IPlaybackPipelineComponent
	unsigned char AddToPipeline(IPlaybackPipelineManager& rcManager);
	ROUTE ProcessOutputSample(	IMediaSample& riSample,
								CDVROutputPin& rcOutputPin);

	// IFileSourceFilter
	STDMETHODIMP Load(LPCOLESTR wszFileName, const AM_MEDIA_TYPE *);
	STDMETHODIMP GetCurFile(LPOLESTR *ppszFileName,	AM_MEDIA_TYPE *pmt);

private:
	HRESULT LoadKey();								// Import the decryption key from the reader

	HCRYPTPROV m_hProv;								// Cryptographic provider handle
	HCRYPTKEY m_hKey;								// Symmetric decryption key
	IPlaybackPipelineManager *m_pPipelineMgr;		// The capture pipeline manager
	IReader *m_pReader;								// Reader component
	IFileSourceFilter *m_pNextFileSource;			// Next IFileSourceFilter in pipeline
	bool m_fNeedsNewKey;							// True iff a new key is needed from the reader.
};

} // namespace MSDvr
