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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#ifndef __FNT_FIX_HPP_INCLUDED__
#define __FNT_FIX_HPP_INCLUDED__

class Font_t;

struct LOGFONTWITHATOM;

//  Functions in the "fntfix" component to change fonts based on the version ID of the caller

class FntFix
{

public:

	static
	BOOL
	FixUpLogFont(
		const	LOGFONT*			plfIn,
				LOGFONTWITHATOM*	plfOut,
				WCHAR*				szAliasedName,
				DWORD				nChar
				);

	static
	void
	CreateV1StockFont(
		void
        );

	static
	Font_t*
	GetV1StockFont(
		void
		);

};

#endif

