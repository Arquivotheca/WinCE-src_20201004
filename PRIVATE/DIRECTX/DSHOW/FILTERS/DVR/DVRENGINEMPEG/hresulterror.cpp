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
// HResultError.cpp : class for throwing hresults as exceptions.
//

#include "stdafx.h"
#include "HResultError.h"

namespace MSDvr
{

//////////////////////////////////////////////////////////////////////////////
//	HResultError exception class.											//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//	Construction.															//
//////////////////////////////////////////////////////////////////////////////

CHResultError::CHResultError(HRESULT hrError, const std::string& siMessage /*= ""*/) :
	std::runtime_error	(siMessage),
	m_hrError			(hrError)
{
	ASSERT (FAILED(hrError));
}

//////////////////////////////////////////////////////////////////////////////
//	Implementation.															//
//////////////////////////////////////////////////////////////////////////////

const char* CHResultError::what() const
{
	// Error message prefix
	const char* szPrefix = "Runtime error 0x%X has occurred";

	// Optional suffix
	const char* szBaseMessage = std::runtime_error::what();

	// Concatenation buffer
	try {
		char* szFullMessage = reinterpret_cast <char*>
			(_alloca(strlen(szPrefix) + (szBaseMessage ? strlen(szBaseMessage) : 0) + 11));

		sprintf(szFullMessage, szPrefix, m_hrError);

		// Concatenate with base, if there is one
		if (szBaseMessage && strlen(szBaseMessage))
		{
			strcat(szFullMessage, ", ");  // hence "11" above
			strcat(szFullMessage, szBaseMessage);
		}

		// Copy into this object, and return from there
		m_siFullMessage = szFullMessage;
		return m_siFullMessage.c_str();
	} catch (std::exception &) {
	}
	return szBaseMessage;
}

}
