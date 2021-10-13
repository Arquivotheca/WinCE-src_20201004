//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

namespace litexml
{

#define LXML_MAKE_ERROR(__code)		(LXML_EXTRACT_ERROR(__code) | 0x80000000)
#define LXML_EXTRACT_ERROR(__err)	((__err)&0x0FFFFFFF)

struct Status
{
	typedef long Code;

	// A positive status code denotes special non-error cases
	static const long Ignore					= 0x00000001;
	static const long TraversalAborted			= 0x00000002;

	// Success
	static const long Success					= 0x00000000;

	// Lexical and syntactic analysis errors
	static const long ErrorEmptyDocument		= LXML_MAKE_ERROR(0x0001);
	static const long ErrorUnclosedComment		= LXML_MAKE_ERROR(0x0002);
	static const long ErrorUnclosedCData		= LXML_MAKE_ERROR(0x0003);
	static const long ErrorUnclosedTag			= LXML_MAKE_ERROR(0x0004);
	static const long ErrorStringExpected		= LXML_MAKE_ERROR(0x0005);
	static const long ErrorIdentifierExpected	= LXML_MAKE_ERROR(0x0006);
	static const long ErrorOpenTagExpected		= LXML_MAKE_ERROR(0x0007);
	static const long ErrorEndTagExpected		= LXML_MAKE_ERROR(0x0008);
	static const long ErrorEndMetaTagExpected	= LXML_MAKE_ERROR(0x0009);
	static const long ErrorAttributeExpected	= LXML_MAKE_ERROR(0x000A);
	static const long ErrorTagMismatch			= LXML_MAKE_ERROR(0x000B);
	static const long ErrorDuplicateAttribute	= LXML_MAKE_ERROR(0x000C);

	// General errors
	static const long Error						= LXML_MAKE_ERROR(0x000D);
	static const long ErrorNoMemory				= LXML_MAKE_ERROR(0x000E);
	static const long ErrorBadCast				= LXML_MAKE_ERROR(0x000F);
	static const long ErrorFile					= LXML_MAKE_ERROR(0x0010);
	static const long ErrorFileNotFound			= LXML_MAKE_ERROR(0x0011);
	static const long ErrorInvalidArg			= LXML_MAKE_ERROR(0x0012);
	static const long ErrorTranslation			= LXML_MAKE_ERROR(0x0013);
	static const long ErrorUnknownEncoding		= LXML_MAKE_ERROR(0x0014);
	static const long ErrorUnsupported			= LXML_MAKE_ERROR(0x0015);
};

#define LXML_FAILED(__v)	((__v) < 0L)
#define LXML_SUCCEEDED(__v)	((__v) >= 0L)

}	// namespace litexml