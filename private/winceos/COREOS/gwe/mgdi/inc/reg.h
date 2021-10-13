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
/*


*/

#ifndef __REG_H__
#define __REG_H__

//
// GDI_REG_KEY is used to generate registry key names all based
// on the same MGDI-specific path.  Usage example:
//
//      g_fv1PaletteIndex = FRegKeyExists(GDI_REG_KEY("V1PALETTEINDEX"));
//
#define GDI_REG_KEY(str)    TEXT("SYSTEM\\GDI\\") TEXT(str)

//
// Return TRUE iff the given registry key exists.  Callers should
// usually use the GDI_REG_KEY macro to construct the key's name.
//
BOOL
FRegKeyExists(
	const	WCHAR*	pwszKey
	);

// Obtains a DWORD value from the registry.
BOOL
FGetRegDword(
	const	WCHAR*	pwszKey,
	const	WCHAR*	pwszValueName,
			DWORD*	pdwValue
	);

// Obtains a string value from the registry. Returns true iff it succesffuly
// read a string from the registry.
BOOL
FGetRegSZ(
	const	WCHAR*	pwszKey,
	const	WCHAR*	pwszValueName,
			WCHAR*	pwszBuf,
			DWORD*	pcbBufSize
	);

#endif  // !__REG_H__
