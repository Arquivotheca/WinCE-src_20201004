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
#include "Encrypter.h"
#include "..\\plumbing\\sink.h"
#include "..\\CopyProtectionPolicies.h"

namespace MSDvr
{

CEncrypter::CEncrypter()
{
	m_hProv = NULL;
	m_hKey = NULL;
	m_pbEncKey = NULL;
	m_dwEncKeyLen = 0;

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
			RETAILMSG(1, (L"*** Encrypter failed to load AES enabled CSP and create keyset - trying again.  Error 0x%08x.\n", dw));
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
			RETAILMSG(1, (L"*** Encrypter failed to load AES enabled CSP.  Error 0x%08x.\n", dw));
			ASSERT(FALSE);
			return;
		}
	}

	// Create an AES key
	if (!CryptGenKey(	m_hProv,
						CALG_AES_128,
						CRYPT_EXPORTABLE,
						&m_hKey))
	{
		DWORD dw = GetLastError();
		RETAILMSG(1, (L"*** Encrypter failed to create AES key.  Error 0x%08x.\n", dw));
		ASSERT(FALSE);
		return;
	}

	// Put the key in ECB mode
	DWORD dw = CRYPT_MODE_ECB;
	if (!CryptSetKeyParam(m_hKey, KP_MODE, (BYTE *) &dw, 0))
	{
		dw = GetLastError();
		RETAILMSG(1, (L"*** Encrypter failed to put AES key in ECB mode.  Error 0x%08x.\n", dw));
		ASSERT(FALSE);
		return;
	}

	// Export the AES key
	BYTE pbTemp[1024];
	DWORD dwLen = sizeof(pbTemp);
	if (!CryptExportKey(m_hKey,
						NULL,
						PLAINTEXTKEYBLOB,
						0,	// No flags
						pbTemp,
						&dwLen))
	{
		dw = GetLastError();
		RETAILMSG(1, (L"*** Encrypter failed to export AES key.  Error 0x%08x.\n", dw));
		ASSERT(FALSE);
		return;
	}

	// WARNING!  pbTemp is in plaintext in memory right now.  Clean it ASAP

	// Protect the key
	DATA_BLOB dataOut = {0, 0}, dataIn = { dwLen, pbTemp};
	if (!CryptProtectData(	&dataIn,
							L"Description string.",
							NULL,	// optional entropy not used
							NULL,	// reserved
							NULL,	// no prompt structure
							CRYPTPROTECT_UI_FORBIDDEN,
						&dataOut))
	{
		dw = GetLastError();
		RETAILMSG(1, (L"*** Encrypter failed to protect AES key.  Error 0x%08x.\n", dw));
		ASSERT(FALSE);
		return;
	}

	m_dwEncKeyLen = dataOut.cbData;
	m_pbEncKey = dataOut.pbData;

	// Clean the plaintext key from memory
	memset(pbTemp, 0, dwLen);
}

CEncrypter::~CEncrypter()
{
	if (m_pbEncKey)
		LocalFree(m_pbEncKey);

	if (m_hKey)
		CryptDestroyKey(m_hKey);

	if (m_hProv)
		CryptReleaseContext(m_hProv, NULL);
}

ROUTE CEncrypter::ProcessInputSample(	IMediaSample& riSample,
										CDVRInputPin& rcInputPin)
{
	// Bail if we shouldn't encrypt
	if (!CCopyProtectionExtractor::IsEncrypted(riSample))
		return UNHANDLED_CONTINUE;

	BYTE *pData;
	riSample.GetPointer(&pData);
	DWORD dwDataLen = riSample.GetActualDataLength();
	DWORD dwBufLen = riSample.GetSize();

	// Encrypt the whole sample.  Don't chain samples together.
	if (!CryptEncrypt(m_hKey, NULL, FALSE, 0, pData, &dwDataLen, dwBufLen))
	{
		DWORD dw = GetLastError();
		RETAILMSG(1, (L"*** Encrypter failed on sample.  Error 0x%08x.\n", dw));
		ASSERT(FALSE);
		// todo flag sample as unencrypted
	}
	else
	{
		riSample.SetActualDataLength(dwDataLen);
		// todo flag sample as encrypted
	}

	return HANDLED_CONTINUE;
}

} // namespace MSDvr

