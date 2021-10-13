//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

