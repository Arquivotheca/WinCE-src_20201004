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

class CEncrypter : public ICapturePipelineComponent
{
public:
	// CEncrypter
	CEncrypter();
	~CEncrypter();
	void Cleanup() {}
	DWORD GetCustomDataLen() { return m_dwEncKeyLen; }
	BYTE *GetCustomData() { return m_pbEncKey; }

	// IPipelineComponent
	void RemoveFromPipeline() { Cleanup(); }
	ROUTE ConfigurePipeline(UCHAR iNumPins,
							CMediaType cMediaTypes[],
							UINT iSizeCustom,
							BYTE pbCustom[]) { return HANDLED_CONTINUE; }
	
	// ICapturePipelineComponent
	unsigned char AddToPipeline(ICapturePipelineManager& rcManager) { return 0; }
	ROUTE ProcessInputSample(	IMediaSample& riSample,
								CDVRInputPin& rcInputPin);

private:
	HCRYPTPROV m_hProv;								// Cryptographic provider handle
	HCRYPTKEY m_hKey;								// Symmetric encryption key
	BYTE *m_pbEncKey;								// Buffer holding m_hKey in encrypted form
	DWORD m_dwEncKeyLen;							// Count of bytes in m_pbEncKeyBuf
};

} // namespace MSDvr
