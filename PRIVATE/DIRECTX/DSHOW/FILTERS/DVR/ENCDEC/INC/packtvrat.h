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
// --------------------------------------------------
//  PackTvRat.h
//
//		TvRating is a private but well defined 'wire' format
//		for XDS ratings infomation.  It is persisted in the PVR
//		buffer.
//
//		This file contains methods for convering between the
//		3-part (system, rating, attribute) format and the packed format.
//
// ----------------------------------------------------


#ifndef __PACKTvRat_H__
#define __PACKTvRat_H__

	// totally private rating system that's persisted as a 'PackedTvRating' in the
	//   pvr file. Can't change once the first file gets saved.  
typedef struct 
{
	byte s_System;
	byte s_Level;
	byte s_Attributes;
	byte s_Reserved;
} struct_PackedTvRating;

	// union to help convering
typedef union  
{
	PackedTvRating			pr;
	struct_PackedTvRating	sr; 
} UTvRating;


HRESULT 
UnpackTvRating(
			IN	PackedTvRating              TvRating,
			OUT	EnTvRat_System              *pEnSystem,
			OUT	EnTvRat_GenericLevel        *pEnLevel,
			OUT	LONG                    	*plbffEnAttributes  // BfEnTvRat_GenericAttributes
			);


HRESULT
PackTvRating(
			IN	EnTvRat_System              enSystem,
			IN	EnTvRat_GenericLevel        enLevel,
			IN	LONG                        lbfEnAttributes, // BfEnTvRat_GenericAttributes
			OUT PackedTvRating              *pTvRating
			);

// development only code, remove eventually
HRESULT
RatingToString( IN	EnTvRat_System          enSystem,
				IN	EnTvRat_GenericLevel    enLevel,
				IN	LONG                    lbfEnAttributes, // BfEnTvRat_GenericAttributes	
				IN  TCHAR	                *pszBuff,        // allocated by caller
				IN  int		                cBuff);		     // size of above buffer must be >= 64        // 

#endif
