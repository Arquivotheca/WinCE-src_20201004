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
// @doc
// HResultError.h : class for throwing hresults as exceptions.
//

#pragma once

namespace MSDvr
{

// @class CHResultError |
// Class for throwing an hresult as an exception. Always use this class instead
// of throwing a raw long. Always throw instances of this class by value.
class CHResultError : public std::runtime_error
{
// @access Public Interface
public:
	// @cmember Constructor
	CHResultError(
		// @parm HRESULT error code to be reported
		HRESULT hrError,
		// @parm optional explanation, appended to error message
		const std::string& siMessage = "");

	// @mfunc Error string reporting override
	virtual const char* what() const;

	const HRESULT			m_hrError;

// @access Private Members
private:
	mutable std::string		m_siFullMessage;
};

}
